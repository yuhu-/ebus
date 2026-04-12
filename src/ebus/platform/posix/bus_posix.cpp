/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if defined(POSIX)
#include "platform/posix/bus_posix.hpp"

#include <algorithm>

#include "utils/common.hpp"

ebus::BusPosix::BusPosix(const BusConfig& config, const RuntimeConfig& runtime,
                         Request* request)
    : device_(config.device),
      simulate_(config.simulate),
      runtime_(runtime),
      request_(request),
      fd_(-1),
      open_(false),
      byteQueue_(new Queue<BusEvent>()),
      thread_(),
      running_(false) {}

ebus::BusPosix::~BusPosix() { stop(); }

void ebus::BusPosix::start() {
  if (open_) return;

  if (simulate_) {
    virtualLine_ = std::make_unique<VirtualLine>();
    fd_ = -1;
    open_ = true;
  } else {
    struct termios newSettings;
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0 || isatty(fd_) == 0)
      throw std::runtime_error("Failed to open ebus device: " + device_);

    tcgetattr(fd_, &oldSettings_);
    ::memset(&newSettings, 0, sizeof(newSettings));
    newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
    newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newSettings.c_iflag |= IGNPAR;
    newSettings.c_oflag &= ~OPOST;
    newSettings.c_cc[VMIN] = 1;
    newSettings.c_cc[VTIME] = 0;

    tcflush(fd_, TCIFLUSH);
    tcsetattr(fd_, TCSAFLUSH, &newSettings);
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) & ~O_NONBLOCK);

    open_ = true;
  }

  running_.store(true);
  thread_ = std::thread(&BusPosix::readerThread, this);

  // start SYN generator if enabled in config
  statsUptime_.markBegin();
  if (runtime_.enable_syn) {
    synBaseMsDur_ = std::chrono::milliseconds(runtime_.syn_base_ms);
    synToleranceMsDur_ = std::chrono::milliseconds(runtime_.syn_tolerance_ms);
    currentTunique_ = synBaseMsDur_ +
                      std::chrono::milliseconds(runtime_.address * 10) +
                      synToleranceMsDur_;
    nextSynExpiry_ = std::chrono::steady_clock::now() + currentTunique_;

    synActive_ = false;
    synRunning_.store(true);
    synThread_ = std::thread(&BusPosix::synThread, this);
  }
}

void ebus::BusPosix::stop() {
  if (!open_) return;
  statsUptime_.markEnd();

  running_.store(false);
  synRunning_.store(false);

  {
    std::unique_lock<std::mutex> lock(synMutex_);
    synCv_.notify_all();
  }

  if (!simulate_ && fd_ != -1) ::tcflush(fd_, TCIOFLUSH);

  if (synThread_.joinable()) synThread_.join();

  if (thread_.joinable()) thread_.join();

  if (!simulate_ && fd_ != -1) {
    ::tcsetattr(fd_, TCSANOW, &oldSettings_);
    ::close(fd_);
  }

  fd_ = -1;
  open_ = false;
}

ebus::Queue<ebus::BusEvent>* ebus::BusPosix::getQueue() const {
  return byteQueue_.get();
}

void ebus::BusPosix::writeByte(const uint8_t byte) {
  {
    std::lock_guard<std::mutex> lock(synMutex_);
    auto now = std::chrono::steady_clock::now();
    lastActivityTime_ = now;
    // Postpone automated SYN generation. Add 4ms to account for serialization.
    if (byte != sym_syn) {
      synActive_ = false;
    }
    nextSynExpiry_ = now + currentTunique_ + std::chrono::milliseconds(4);
    synCv_.notify_one();
  }

  statsTransmit_.markBegin();
  recordUtilization(byte);

  if (simulate_) {
    if (virtualLine_) {
      // Notify write listeners for local simulation feedback
      for (const auto& listener : writeListeners_) listener(byte);
      virtualLine_->write(byte);
    }

  } else {
    ensureOpen();
    for (const auto& listener : writeListeners_) listener(byte);
    if (::write(fd_, &byte, 1) == -1)
      throw std::runtime_error("BusPosix: write error");
  }
  statsTransmit_.markEnd();
}

void ebus::BusPosix::setWindow(const uint16_t window) {
  runtime_.window = window;
}

void ebus::BusPosix::setOffset(const uint16_t offset) {
  runtime_.offset = offset;
}

void ebus::BusPosix::setRuntimeConfig(const RuntimeConfig& runtime) {
  bool shouldStart = false;
  bool shouldStop = false;

  {
    std::lock_guard<std::mutex> lock(synMutex_);
    bool wasEnabled = runtime_.enable_syn;
    runtime_ = runtime;

    // Always recalculate timing durations based on the new configuration
    synBaseMsDur_ = std::chrono::milliseconds(runtime_.syn_base_ms);
    synToleranceMsDur_ = std::chrono::milliseconds(runtime_.syn_tolerance_ms);
    currentTunique_ = synBaseMsDur_ +
                      std::chrono::milliseconds(runtime_.address * 10) +
                      synToleranceMsDur_;

    // Manage thread transitions only if the bus is currently active
    if (open_ && running_.load()) {
      if (runtime_.enable_syn && !wasEnabled && !synRunning_.load()) {
        shouldStart = true;
        synRunning_.store(true);
        nextSynExpiry_ = std::chrono::steady_clock::now() + currentTunique_;
        synActive_ = false;
      } else if (!runtime_.enable_syn && wasEnabled && synRunning_.load()) {
        shouldStop = true;
        synRunning_.store(false);
        synCv_.notify_all();
      }
    }
  }

  // Execute thread lifecycle actions outside of the lock to prevent deadlocks
  if (shouldStart) {
    if (synThread_.joinable()) synThread_.join();
    synThread_ = std::thread(&BusPosix::synThread, this);
  } else if (shouldStop) {
    if (synThread_.joinable()) synThread_.join();
  }
}

void ebus::BusPosix::resetMetrics() {
#define X(name) counter_.name##_ = 0;
  EBUS_BUS_COUNTER_LIST
#undef X

#define X(name) name##_.reset();
  EBUS_BUS_TIMING_LIST
#undef X
}

std::map<std::string, ebus::MetricValues> ebus::BusPosix::getMetrics() const {
  std::map<std::string, MetricValues> m;
  auto addCounter = [&](const std::string& name, uint32_t val) {
    m["bus.counter." + name] = {static_cast<double>(val),  0, 0, 0, 0,
                                static_cast<uint64_t>(val)};
  };

  // 1. Calculate and map Counters
  Counter c = counter_;

#define X(name) addCounter(#name, c.name##_);
  EBUS_BUS_COUNTER_LIST
#undef X

  // 2. Map the explicit TimingStats members
  m["bus.timing.delay"] = statsDelay_.getValues();
  m["bus.timing.window"] = statsWindow_.getValues();
  m["bus.timing.transmit"] = statsTransmit_.getValues();

  m["bus.uptime"] = statsUptime_.getValues();

  // Calculate Physical Utilization (%)
  double totalUptime =
      statsUptime_.getValues().last;  // assuming uptime tracks total run time
  if (totalUptime > 0) {
    double utilPercent = (statsUtilization_.getSum() / totalUptime) * 100.0;
    m["bus.utilization"] = {utilPercent, utilPercent, utilPercent,
                            utilPercent, 0.0,         1};
  }

  return m;
}

void ebus::BusPosix::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data. eBUS bit time is ~416.67us
  double lowTime = (countZeroBits(byte) + 1) * (1000000.0 / 2400.0);
  statsUtilization_.addSample(lowTime);
}

void ebus::BusPosix::addReadListener(ReadListener listener) {
  readListeners_.push_back(listener);
}

void ebus::BusPosix::addWriteListener(WriteListener listener) {
  writeListeners_.push_back(listener);
}

void ebus::BusPosix::addSynListener(SynListener listener) {
  synListeners_.push_back(listener);
}

void ebus::BusPosix::ensureOpen() const {
  if (!open_ || fd_ < 0) throw std::runtime_error("BusPosix: device not open");
}

void ebus::BusPosix::readerThread() {
  while (running_.load()) {
    uint8_t byte;
    ssize_t n = 0;

    if (simulate_) {
      // Use the memory-based simulation queue
      if (virtualLine_->read(byte, 100))
        n = 1;
      else
        continue;  // timeout, check running_ flag again
    } else {
      n = ::read(fd_, &byte, 1);
    }

    if (n == 1) {
      auto arrivalTime = std::chrono::steady_clock::now();
      for (const auto& listener : readListeners_) listener(byte);

      // Notify SYN generator that a symbol was recognised (end of char)
      resetSynTimer(byte);

      BusEvent event;
      event.byte = byte;
      event.busRequest =
          busRequestFlag_.exchange(false, std::memory_order_acq_rel);
      event.startBit = false;
      event.timestamp = arrivalTime;

      if (byteQueue_) byteQueue_->push(event);

      // Hit the 4300-4456us window (approx 200us after SYN reception)
      if (byte == sym_syn && request_->busRequestPending()) {
        usleep(200);
        writeByte(request_->busRequestAddress());
        busRequestFlag_.store(true, std::memory_order_release);
      }
    } else if (n == 0) {
      // EOF - stop thread
      break;
    } else {
      // read error, optionally break or continue after short sleep
      if (errno == EINTR) continue;
      break;
    }
  }
  running_.store(false);
}

void ebus::BusPosix::resetSynTimer(uint8_t byte) {
  std::lock_guard<std::mutex> lock(synMutex_);
  auto now = std::chrono::steady_clock::now();
  lastActivityTime_ = now;

  // Arbitration Logic:
  // If we are the active SYN generator (synActive_ is true) and we receive
  // the echo of our own SYN (byte == sym_syn), we "won" arbitration or the bus
  // is idle. We continue generating SYNs at the fast rate (synBaseMs_).
  if (synActive_ && byte == sym_syn) {
    nextSynExpiry_ = now + synBaseMsDur_;
  } else {
    synActive_ = false;
    nextSynExpiry_ = now + currentTunique_;
  }

  synCv_.notify_one();
}

void ebus::BusPosix::synThread() {
  while (synRunning_.load()) {
    std::unique_lock<std::mutex> lock(synMutex_);

    auto now = std::chrono::steady_clock::now();
    if (nextSynExpiry_ > now) {
      synCv_.wait_until(lock, nextSynExpiry_);
      continue;
    }

    // Carrier Sense: If the bus was active very recently (e.g. a write
    // started), postpone generation to avoid colliding with the byte being
    // serialized.
    if (now - lastActivityTime_ < std::chrono::milliseconds(5)) {
      nextSynExpiry_ = now + std::chrono::milliseconds(2);
      continue;
    }

    // We are about to generate a SYN, mark ourselves as active
    synActive_ = true;
    lock.unlock();

    for (const auto& listener : synListeners_) listener();
    writeByte(sym_syn);

    lock.lock();
    // Safety Fallback:
    // If the timer hasn't been updated by readerThread (receiving the echo),
    // reset it to the unique (long) value as a fallback.
    // This handles cases where our write failed or the echo was corrupted.
    if (nextSynExpiry_ <= std::chrono::steady_clock::now()) {
      nextSynExpiry_ = std::chrono::steady_clock::now() + currentTunique_;
    }
  }
}

#endif
