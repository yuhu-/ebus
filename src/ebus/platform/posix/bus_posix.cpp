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
      byte_queue_(new Queue<BusEvent>()),
      thread_(),
      running_(false) {}

ebus::BusPosix::~BusPosix() { stop(); }

void ebus::BusPosix::start() {
  if (open_) return;

  if (simulate_) {
    virtual_line_ = std::make_unique<VirtualLine>();
    fd_ = -1;
    open_ = true;
  } else {
    struct termios newSettings;
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0 || isatty(fd_) == 0)
      throw std::runtime_error("Failed to open ebus device: " + device_);

    tcgetattr(fd_, &old_settings_);
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
  stats_uptime_.markBegin();
  if (runtime_.enable_syn) {
    syn_base_ms_dur_ = std::chrono::milliseconds(runtime_.syn_base_ms);
    syn_tolerance_ms_dur_ =
        std::chrono::milliseconds(runtime_.syn_tolerance_ms);
    current_t_unique_ = syn_base_ms_dur_ +
                        std::chrono::milliseconds(runtime_.address * 10) +
                        syn_tolerance_ms_dur_;
    next_syn_expiry_ = std::chrono::steady_clock::now() + current_t_unique_;

    syn_active_ = false;
    syn_running_.store(true);
    syn_thread_ = std::thread(&BusPosix::synThread, this);
  }
}

void ebus::BusPosix::stop() {
  if (!open_) return;
  stats_uptime_.markEnd();

  running_.store(false);
  syn_running_.store(false);

  {
    std::unique_lock<std::mutex> lock(syn_mutex_);
    syn_cv_.notify_all();
  }

  if (!simulate_ && fd_ != -1) ::tcflush(fd_, TCIOFLUSH);

  if (syn_thread_.joinable()) syn_thread_.join();

  if (thread_.joinable()) thread_.join();

  if (!simulate_ && fd_ != -1) {
    ::tcsetattr(fd_, TCSANOW, &old_settings_);
    ::close(fd_);
  }

  fd_ = -1;
  open_ = false;
}

ebus::Queue<ebus::BusEvent>* ebus::BusPosix::getQueue() const {
  return byte_queue_.get();
}

void ebus::BusPosix::writeByte(const uint8_t byte) {
  {
    std::lock_guard<std::mutex> lock(syn_mutex_);
    auto now = std::chrono::steady_clock::now();
    last_activity_time_ = now;
    // Postpone automated SYN generation. Add 4ms to account for serialization.
    if (byte != sym_syn) {
      syn_active_ = false;
    }
    next_syn_expiry_ = now + current_t_unique_ + std::chrono::milliseconds(4);
    syn_cv_.notify_one();
  }

  stats_transmit_.markBegin();
  recordUtilization(byte);

  if (simulate_) {
    if (virtual_line_) {
      // Notify write listeners for local simulation feedback
      for (const auto& listener : write_listeners_) listener(byte);
      virtual_line_->write(byte);
    }

  } else {
    ensureOpen();
    for (const auto& listener : write_listeners_) listener(byte);
    if (::write(fd_, &byte, 1) == -1)
      throw std::runtime_error("BusPosix: write error");
  }
  stats_transmit_.markEnd();
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
    std::lock_guard<std::mutex> lock(syn_mutex_);
    bool wasEnabled = runtime_.enable_syn;
    runtime_ = runtime;

    // Always recalculate timing durations based on the new configuration
    syn_base_ms_dur_ = std::chrono::milliseconds(runtime_.syn_base_ms);
    syn_tolerance_ms_dur_ =
        std::chrono::milliseconds(runtime_.syn_tolerance_ms);
    current_t_unique_ = syn_base_ms_dur_ +
                        std::chrono::milliseconds(runtime_.address * 10) +
                        syn_tolerance_ms_dur_;

    // Manage thread transitions only if the bus is currently active
    if (open_ && running_.load()) {
      if (runtime_.enable_syn && !wasEnabled && !syn_running_.load()) {
        shouldStart = true;
        syn_running_.store(true);
        next_syn_expiry_ = std::chrono::steady_clock::now() + current_t_unique_;
        syn_active_ = false;
      } else if (!runtime_.enable_syn && wasEnabled && syn_running_.load()) {
        shouldStop = true;
        syn_running_.store(false);
        syn_cv_.notify_all();
      }
    }
  }

  // Execute thread lifecycle actions outside of the lock to prevent deadlocks
  if (shouldStart) {
    if (syn_thread_.joinable()) syn_thread_.join();
    syn_thread_ = std::thread(&BusPosix::synThread, this);
  } else if (shouldStop) {
    if (syn_thread_.joinable()) syn_thread_.join();
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
  m["bus.timing.delay"] = stats_delay_.getValues();
  m["bus.timing.window"] = stats_window_.getValues();
  m["bus.timing.transmit"] = stats_transmit_.getValues();

  m["bus.uptime"] = stats_uptime_.getValues();

  // Calculate Physical Utilization (%)
  double totalUptime =
      stats_uptime_.getValues().last;  // assuming uptime tracks total run time
  if (totalUptime > 0) {
    double utilPercent = (stats_utilization_.getSum() / totalUptime) * 100.0;
    m["bus.utilization"] = {utilPercent, utilPercent, utilPercent,
                            utilPercent, 0.0,         1};
  }

  return m;
}

void ebus::BusPosix::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data. eBUS bit time is ~416.67us
  double lowTime = (countZeroBits(byte) + 1) * (1000000.0 / 2400.0);
  stats_utilization_.addSample(lowTime);
}

void ebus::BusPosix::addReadListener(ReadListener listener) {
  read_listeners_.push_back(listener);
}

void ebus::BusPosix::addWriteListener(WriteListener listener) {
  write_listeners_.push_back(listener);
}

void ebus::BusPosix::addSynListener(SynListener listener) {
  syn_listeners_.push_back(listener);
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
      if (virtual_line_->read(byte, 100))
        n = 1;
      else
        continue;  // timeout, check running_ flag again
    } else {
      n = ::read(fd_, &byte, 1);
    }

    if (n == 1) {
      auto arrivalTime = std::chrono::steady_clock::now();
      for (const auto& listener : read_listeners_) listener(byte);

      // Notify SYN generator that a symbol was recognised (end of char)
      resetSynTimer(byte);

      BusEvent event;
      event.byte = byte;
      event.busRequest =
          bus_request_flag_.exchange(false, std::memory_order_acq_rel);
      event.startBit = false;
      event.timestamp = arrivalTime;

      if (byte_queue_) byte_queue_->push(event);

      // Hit the 4300-4456us window (approx 200us after SYN reception)
      if (byte == sym_syn && request_->busRequestPending()) {
        usleep(200);
        writeByte(request_->busRequestAddress());
        bus_request_flag_.store(true, std::memory_order_release);
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
  std::lock_guard<std::mutex> lock(syn_mutex_);
  auto now = std::chrono::steady_clock::now();
  last_activity_time_ = now;

  // Arbitration Logic:
  // If we are the active SYN generator (synActive_ is true) and we receive
  // the echo of our own SYN (byte == sym_syn), we "won" arbitration or the bus
  // is idle. We continue generating SYNs at the fast rate (synBaseMs_).
  if (syn_active_ && byte == sym_syn) {
    next_syn_expiry_ = now + syn_base_ms_dur_;
  } else {
    syn_active_ = false;
    next_syn_expiry_ = now + current_t_unique_;
  }

  syn_cv_.notify_one();
}

void ebus::BusPosix::synThread() {
  while (syn_running_.load()) {
    std::unique_lock<std::mutex> lock(syn_mutex_);

    auto now = std::chrono::steady_clock::now();
    if (next_syn_expiry_ > now) {
      syn_cv_.wait_until(lock, next_syn_expiry_);
      continue;
    }

    // Carrier Sense: If the bus was active very recently (e.g. a write
    // started), postpone generation to avoid colliding with the byte being
    // serialized.
    if (now - last_activity_time_ < std::chrono::milliseconds(5)) {
      next_syn_expiry_ = now + std::chrono::milliseconds(2);
      continue;
    }

    // We are about to generate a SYN, mark ourselves as active
    syn_active_ = true;
    lock.unlock();

    for (const auto& listener : syn_listeners_) listener();
    writeByte(sym_syn);

    lock.lock();
    // Safety Fallback:
    // If the timer hasn't been updated by readerThread (receiving the echo),
    // reset it to the unique (long) value as a fallback.
    // This handles cases where our write failed or the echo was corrupted.
    if (next_syn_expiry_ <= std::chrono::steady_clock::now()) {
      next_syn_expiry_ = std::chrono::steady_clock::now() + current_t_unique_;
    }
  }
}

#endif
