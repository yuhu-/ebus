/*
 * Copyright (C) 2012-2025 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include "../include/ebus/EbusStack.h"

#include <bits/types/struct_timespec.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>

#include "Device.h"
#include "NQueue.h"
#include "Notify.h"
#include "Sequence.h"
#include "Telegram.h"
#include "runtime_warning.h"

#define EBUS_ERR_MASTER -1    // sending is only as master possible
#define EBUS_ERR_SEQUENCE -2  // the passed sequence contains an error
#define EBUS_ERR_TRANSMIT -3  // a data error occurred during sending
#define EBUS_ERR_DEVICE -4    // a device error occurred
#define EBUS_ERR_OFFLINE -5   // ebus service is offline

std::map<int, const char *> EbusErrors = {
    {EBUS_ERR_MASTER, "sending is only as master possible"},
    {EBUS_ERR_SEQUENCE, "the passed sequence contains an error"},
    {EBUS_ERR_TRANSMIT, "a data error occurred during sending"},
    {EBUS_ERR_DEVICE, "a device error occurred"},
    {EBUS_ERR_OFFLINE, "ebus service is offline"}};

namespace ebus {

static const char *info_dev_open = "device opened";
static const char *info_dev_close = "device closed";
static const char *info_ebus_lock = "ebus locked";
static const char *info_ebus_free = "ebus freed";
static const char *info_msg_ignore = "message ignored";
static const char *info_dev_flush = "device flushed";
static const char *info_not_def = "message not defined";
static const char *info_no_func = "no function registered";

static const char *warn_byte_dif = "written/read byte difference";
static const char *warn_arb_lost = "arbitration lost";
static const char *warn_pri_fit = "priority class fit -> retry";
static const char *warn_pri_lost = "priority class lost";
static const char *warn_ack_neg =
    "received acknowledge byte is negative -> retry";
static const char *warn_recv_resp = "received response is invalid -> retry";
static const char *warn_recv_msg = "message is invalid";

static const char *error_open_fail = "opening ebus failed";
static const char *error_close_fail = "closing ebus failed";
static const char *error_ack_neg =
    "received acknowledge byte is negative -> failed";
static const char *error_ack_wrong = "received acknowledge byte is wrong";
static const char *error_nn_wrong = "received size byte is wrong";
static const char *error_recv_resp = "received response is invalid -> failed";
static const char *error_resp_crea = "creating response failed";
static const char *error_resp_send = "sending response failed";
static const char *error_bad_type = "received type does not allow an answer";

struct Message : public Notify {
  explicit Message(Telegram &tel) : Notify(), m_telegram(tel) {}

  Telegram &m_telegram;
  int m_state = 0;
};

enum class State {
  IdleSystem,
  OpenDevice,
  MonitorBus,
  ReceiveMessage,
  ProcessMessage,
  SendResponse,
  LockBus,
  SendMessage,
  ReceiveResponse,
  FreeBus
};

}  // namespace ebus

class ebus::EbusStack::EbusImpl : private Notify {
 public:
  EbusImpl(const uint8_t address, const std::string &device);

  ~EbusImpl();

  void open();
  void close();

  bool online();

  int transmit(const std::vector<uint8_t> &message,
               std::vector<uint8_t> &response);

  const std::string error_text(const int error) const;

  void register_logger(std::shared_ptr<ILogger> logger);

  void register_process(
      std::function<Reaction(const std::vector<uint8_t> &message,
                             std::vector<uint8_t> &response)>
          process);

  void register_publish(
      std::function<void(const std::vector<uint8_t> &message,
                         const std::vector<uint8_t> &response)>
          publish);

  void register_rawdata(std::function<void(const uint8_t &byte)> rawdata);

  void set_access_timeout(const uint16_t &access_timeout);
  void set_lock_counter_max(const uint8_t &lock_counter_max);

  void set_open_counter_max(const uint8_t &open_counter_max);

  static const std::vector<uint8_t> range(const std::vector<uint8_t> &seq,
                                          const size_t index, const size_t len);
  static const std::vector<uint8_t> to_vector(const std::string &str);
  static const std::string to_string(const std::vector<uint8_t> &seq);

 private:
  std::thread m_thread;

  bool m_running = true;
  bool m_online = false;
  bool m_close = false;

  const uint8_t m_address;
  const uint8_t m_slaveAddress;

  uint16_t m_access_timeout = 4400L;

  uint8_t m_lock_counter_max = 5;
  uint8_t m_lock_counter = 0;

  uint8_t m_open_counter_max = 10;
  uint8_t m_open_counter = 0;

  NQueue<std::shared_ptr<Message>> m_messageQueue;

  std::unique_ptr<Device> m_device = nullptr;

  std::shared_ptr<ILogger> m_logger = nullptr;

  std::function<Reaction(const std::vector<uint8_t> &message,
                         std::vector<uint8_t> &response)>
      m_process;

  std::vector<std::function<void(const std::vector<uint8_t> &message,
                                 const std::vector<uint8_t> &response)>>
      m_publish;

  std::vector<std::function<void(const uint8_t &byte)>> m_rawdata;

  Sequence m_sequence;
  std::shared_ptr<Message> m_activeMessage = nullptr;
  std::shared_ptr<Message> m_passiveMessage = nullptr;

  int transmit(Telegram &tel);

  void read(uint8_t &byte, const uint8_t sec, const uint16_t nsec);
  void write(const uint8_t &byte);
  void write_read(const uint8_t &byte, const uint8_t sec, const uint16_t nsec);

  void reset();

  void run();

  State idleSystem();
  State openDevice();
  State monitorBus();
  State receiveMessage();
  State processMessage();
  State sendResponse();
  State lockBus();
  State sendMessage();
  State receiveResponse();
  State freeBus();

  State handleDeviceError(bool error, const std::string &message);

  Reaction process(const std::vector<uint8_t> &message,
                   std::vector<uint8_t> &response);

  void publish(const std::vector<uint8_t> &message,
               const std::vector<uint8_t> &response);

  void rawdata(const uint8_t &byte);

  void logError(const std::string &message);
  void logWarn(const std::string &message);
  void logInfo(const std::string &message);
  void logDebug(const std::string &message);
  void logTrace(const std::string &message);
};

ebus::EbusStack::EbusStack(const uint8_t address, const std::string &device)
    : impl{std::make_unique<EbusImpl>(address, device)} {}

// move functions
ebus::EbusStack &ebus::EbusStack::operator=(EbusStack &&) = default;
ebus::EbusStack::EbusStack(EbusStack &&) = default;

ebus::EbusStack::~EbusStack() = default;

void ebus::EbusStack::open() { this->impl->open(); }

void ebus::EbusStack::close() { this->impl->close(); }

bool ebus::EbusStack::online() { return this->impl->online(); }

int ebus::EbusStack::transmit(const std::vector<uint8_t> &message,
                              std::vector<uint8_t> &response) {
  return this->impl->transmit(message, response);
}

const std::string ebus::EbusStack::error_text(const int error) const {
  return this->impl->error_text(error);
}

void ebus::EbusStack::register_logger(std::shared_ptr<ILogger> logger) {
  this->impl->register_logger(logger);
}

void ebus::EbusStack::register_process(
    std::function<Reaction(const std::vector<uint8_t> &message,
                           std::vector<uint8_t> &response)>
        process) {
  this->impl->register_process(process);
}

void ebus::EbusStack::register_publish(
    std::function<void(const std::vector<uint8_t> &message,
                       const std::vector<uint8_t> &response)>
        publish) {
  this->impl->register_publish(publish);
}

void ebus::EbusStack::register_rawdata(
    std::function<void(const uint8_t &byte)> rawdata) {
  this->impl->register_rawdata(rawdata);
}

void ebus::EbusStack::set_access_timeout(const uint16_t &access_timeout) {
  this->impl->set_access_timeout(access_timeout);
}

void ebus::EbusStack::set_lock_counter_max(const uint8_t &lock_counter_max) {
  this->impl->set_lock_counter_max(lock_counter_max);
}

void ebus::EbusStack::set_open_counter_max(const uint8_t &open_counter_max) {
  this->impl->set_open_counter_max(open_counter_max);
}

const std::vector<uint8_t> ebus::EbusStack::range(
    const std::vector<uint8_t> &seq, const size_t index, const size_t len) {
  return EbusImpl::range(seq, index, len);
}

const std::vector<uint8_t> ebus::EbusStack::to_vector(const std::string &str) {
  return EbusImpl::to_vector(str);
}

const std::string ebus::EbusStack::to_string(const std::vector<uint8_t> &vec) {
  return EbusImpl::to_string(vec);
}

ebus::EbusStack::EbusImpl::EbusImpl(const uint8_t address,
                                    const std::string &device)
    : Notify(),
      m_address(address),
      m_slaveAddress(Telegram::slaveAddress(address)),
      m_device(std::make_unique<Device>(device)) {
  m_thread = std::thread(&EbusImpl::run, this);
}

ebus::EbusStack::EbusImpl::~EbusImpl() {
  close();

  struct timespec req = {0, 10000L};

  while (m_online) nanosleep(&req, (struct timespec *)NULL);

  m_running = false;
  nanosleep(&req, (struct timespec *)NULL);

  notify();
  m_thread.join();

  while (m_messageQueue.size() > 0) m_messageQueue.dequeue().reset();
}

void ebus::EbusStack::EbusImpl::open() { notify(); }

void ebus::EbusStack::EbusImpl::close() { m_close = true; }

bool ebus::EbusStack::EbusImpl::online() { return m_online; }

int ebus::EbusStack::EbusImpl::transmit(const std::vector<uint8_t> &message,
                                        std::vector<uint8_t> &response) {
  Telegram tel;
  tel.createMaster(m_address, message);

  int result = transmit(tel);
  response = tel.getSlave().to_vector();

  return result;
}

const std::string ebus::EbusStack::EbusImpl::error_text(const int error) const {
  return EbusErrors[error];
}

void ebus::EbusStack::EbusImpl::register_logger(
    std::shared_ptr<ILogger> logger) {
  m_logger = logger;
}

void ebus::EbusStack::EbusImpl::register_process(
    std::function<Reaction(const std::vector<uint8_t> &message,
                           std::vector<uint8_t> &response)>
        process) {
  m_process = process;
}

void ebus::EbusStack::EbusImpl::register_publish(
    std::function<void(const std::vector<uint8_t> &message,
                       const std::vector<uint8_t> &response)>
        publish) {
  m_publish.push_back(publish);
}

void ebus::EbusStack::EbusImpl::register_rawdata(
    std::function<void(const uint8_t &byte)> rawdata) {
  m_rawdata.push_back(rawdata);
}

void ebus::EbusStack::EbusImpl::set_access_timeout(
    const uint16_t &access_timeout) {
  m_access_timeout = access_timeout;
}

void ebus::EbusStack::EbusImpl::set_lock_counter_max(
    const uint8_t &lock_counter_max) {
  m_lock_counter_max = lock_counter_max;
}

void ebus::EbusStack::EbusImpl::set_open_counter_max(
    const uint8_t &open_counter_max) {
  m_open_counter_max = open_counter_max;
}

const std::vector<uint8_t> ebus::EbusStack::EbusImpl::range(
    const std::vector<uint8_t> &seq, const size_t index, const size_t len) {
  return Sequence::range(seq, index, len);
}

const std::vector<uint8_t> ebus::EbusStack::EbusImpl::to_vector(
    const std::string &str) {
  return ebus::Sequence::to_vector(str);
}

const std::string ebus::EbusStack::EbusImpl::to_string(
    const std::vector<uint8_t> &vec) {
  return ebus::Sequence::to_string(vec);
}

int ebus::EbusStack::EbusImpl::transmit(Telegram &tel) {
  int result = SEQ_OK;

  if (tel.getMasterState() != SEQ_OK) {
    result = EBUS_ERR_SEQUENCE;
  } else if (!Telegram::isMaster(m_address)) {
    result = EBUS_ERR_MASTER;
  } else if (!online()) {
    result = EBUS_ERR_OFFLINE;
  } else {
    std::shared_ptr<Message> message = std::make_shared<Message>(tel);
    m_messageQueue.enqueue(message);
    message->wait();
    result = message->m_state;
    message.reset();
  }

  return result;
}

void ebus::EbusStack::EbusImpl::read(uint8_t &byte, const uint8_t sec,
                                     const uint16_t nsec) {
  m_device->recv(byte, sec, nsec);

  rawdata(byte);

  std::ostringstream ostr;
  ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned>(byte) << std::nouppercase << std::setw(0);
  logTrace("<" + ostr.str());
}

void ebus::EbusStack::EbusImpl::write(const uint8_t &byte) {
  m_device->send(byte);

  std::ostringstream ostr;
  ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned>(byte) << std::nouppercase << std::setw(0);
  logTrace(">" + ostr.str());
}

void ebus::EbusStack::EbusImpl::write_read(const uint8_t &byte,
                                           const uint8_t sec,
                                           const uint16_t nsec) {
  write(byte);

  uint8_t readByte;
  read(readByte, sec, nsec);

  if (readByte != byte) logDebug(warn_byte_dif);
}

void ebus::EbusStack::EbusImpl::reset() {
  m_open_counter = 0;
  m_lock_counter = m_lock_counter_max;

  m_sequence.clear();

  if (m_activeMessage != nullptr) {
    publish(m_activeMessage->m_telegram.getMaster().to_vector(),
            m_activeMessage->m_telegram.getSlave().to_vector());

    m_activeMessage->notify();
    m_activeMessage = nullptr;
  }

  if (m_passiveMessage != nullptr) {
    std::shared_ptr<Message> message = m_passiveMessage;
    m_passiveMessage = nullptr;
  }
}

void ebus::EbusStack::EbusImpl::run() {
  logInfo("Ebus started");

  State state = State::OpenDevice;

  while (m_running) {
    try {
      switch (state) {
        case State::IdleSystem:
          state = idleSystem();
          break;
        case State::OpenDevice:
          state = openDevice();
          break;
        case State::MonitorBus:
          state = monitorBus();
          break;
        case State::ReceiveMessage:
          state = receiveMessage();
          break;
        case State::ProcessMessage:
          state = processMessage();
          break;
        case State::SendResponse:
          state = sendResponse();
          break;
        case State::LockBus:
          state = lockBus();
          break;
        case State::SendMessage:
          state = sendMessage();
          break;
        case State::ReceiveResponse:
          state = receiveResponse();
          break;
        case State::FreeBus:
          state = freeBus();
          break;
        default:
          break;
      }
    } catch (const ebus::runtime_warning &ex) {
      state = handleDeviceError(false, ex.what());
    } catch (const std::runtime_error &ex) {
      state = handleDeviceError(true, ex.what());
    }

    if (m_close) state = State::IdleSystem;
  }

  logInfo("Ebus stopped");
}

ebus::State ebus::EbusStack::EbusImpl::idleSystem() {
  logDebug("idleSystem");

  if (m_device->isOpen()) {
    m_device->close();

    if (!m_device->isOpen())
      logInfo(info_dev_close);
    else
      logWarn(error_close_fail);
  }

  reset();

  m_online = false;
  m_close = false;

  wait();

  return State::OpenDevice;
}

ebus::State ebus::EbusStack::EbusImpl::openDevice() {
  logDebug("openDevice");

  uint8_t byte = sym_zero;

  if (!m_device->isOpen()) {
    m_device->open();

    if (!m_device->isOpen()) {
      logWarn(error_open_fail);

      m_open_counter++;
      if (m_open_counter > m_open_counter_max) return (State::IdleSystem);

      sleep(1);
      return State::OpenDevice;
    }
  }

  logInfo(info_dev_open);

  do {
    read(byte, 1, 0);
  } while (byte != sym_syn);

  reset();

  m_online = true;

  logInfo(info_dev_flush);

  return State::MonitorBus;
}

ebus::State ebus::EbusStack::EbusImpl::monitorBus() {
  logDebug("monitorBus");

  uint8_t byte = sym_zero;

  read(byte, 1, 0);

  if (byte == sym_syn) {
    if (m_lock_counter != 0) {
      m_lock_counter--;
      logDebug("m_lock_counter: " + std::to_string(m_lock_counter));
    }

    // decode Sequence
    if (m_sequence.size() != 0) {
      logDebug(m_sequence.to_string());

      Telegram tel(m_sequence);
      logInfo(tel.to_string());

      if (tel.isValid())
        publish(tel.getMaster().to_vector(), tel.getSlave().to_vector());

      if (m_sequence.size() == 1 && m_lock_counter < 2) m_lock_counter = 2;

      tel.clear();
      m_sequence.clear();
    }

    // check for new Message
    if (m_activeMessage == nullptr && m_messageQueue.size() > 0)
      m_activeMessage = m_messageQueue.dequeue();

    // handle Message
    if (m_activeMessage != nullptr && m_lock_counter == 0)
      return State::LockBus;
  } else {
    m_sequence.push_back(byte);

    // handle broadcast and at me addressed messages
    if (m_sequence.size() == 2 &&
        (m_sequence[1] == sym_broad || m_sequence[1] == m_address ||
         m_sequence[1] == m_slaveAddress))
      return State::ReceiveMessage;
  }

  return State::MonitorBus;
}

ebus::State ebus::EbusStack::EbusImpl::receiveMessage() {
  logDebug("receiveMessage");

  uint8_t byte;

  // receive Header PBSBNN
  for (int i = 0; i < 3; i++) {
    byte = sym_zero;

    read(byte, 1, 0);

    m_sequence.push_back(byte);
  }

  // maximum data bytes
  if (m_sequence[4] > max_bytes) {
    logWarn(error_nn_wrong);
    m_activeMessage->m_state = EBUS_ERR_TRANSMIT;

    reset();

    return State::MonitorBus;
  }

  // bytes to receive
  uint8_t bytes = m_sequence[4];

  // receive Data Dx
  for (uint8_t i = 0; i < bytes; i++) {
    byte = sym_zero;

    read(byte, 1, 0);

    m_sequence.push_back(byte);

    if (byte == sym_exp) bytes++;
  }

  // 1 for CRC
  bytes = 1;

  // receive CRC
  for (uint8_t i = 0; i < bytes; i++) {
    read(byte, 1, 0);

    m_sequence.push_back(byte);

    if (byte == sym_exp) bytes++;
  }

  logDebug(m_sequence.to_string());
  // TODO check CRC of sequence
  Telegram tel;
  tel.createMaster(m_sequence);

  if (m_sequence[1] != sym_broad) {
    if (tel.getMasterState() == SEQ_OK) {
      byte = sym_ack;
    } else {
      byte = sym_nak;
      logInfo(warn_recv_msg);
    }

    // send ACK
    write_read(byte, 0, 0);

    tel.setSlaveACK(byte);
  }

  if (tel.getMasterState() == SEQ_OK) {
    if (tel.get_type() != Type::MS) {
      logInfo(tel.to_string());
      publish(tel.getMaster().to_vector(), tel.getSlave().to_vector());
    }

    return State::ProcessMessage;
  }

  m_sequence.clear();

  return State::MonitorBus;
}

ebus::State ebus::EbusStack::EbusImpl::processMessage() {
  logDebug("processMessage");

  Telegram tel;
  tel.createMaster(m_sequence);

  std::vector<uint8_t> response;

  Reaction reaction = process(tel.getMaster().to_vector(), response);

  switch (reaction) {
    case Reaction::nofunction:
      logDebug(info_no_func);
      break;
    case Reaction::undefined:
      logDebug(info_not_def);
      break;
    case Reaction::ignore:
      logInfo(info_msg_ignore);
      break;
    case Reaction::response:
      if (tel.get_type() == Type::MS) {
        tel.createSlave(response);

        if (tel.getSlaveState() == SEQ_OK) {
          logInfo("response: " + tel.toStringSlave());
          m_passiveMessage = std::make_shared<Message>(tel);

          return State::SendResponse;
        } else {
          logWarn(error_resp_crea);
        }
      } else {
        logWarn(error_bad_type);
      }

      break;
    default:
      break;
  }

  m_sequence.clear();

  return State::MonitorBus;
}

ebus::State ebus::EbusStack::EbusImpl::sendResponse() {
  logDebug("sendResponse");

  Telegram &tel = m_passiveMessage->m_telegram;
  uint8_t byte;

  // TODO expand, calc CRC of sequence and send

  for (int retry = 1; retry >= 0; retry--) {
    // send Message
    for (size_t i = 0; i < tel.getSlave().size(); i++)
      write_read(tel.getSlave()[i], 0, 0);

    // send CRC
    write_read(tel.getSlaveCRC(), 0, 0);

    // receive ACK
    read(byte, 0, 10000L);

    if (byte != sym_ack && byte != sym_nak) {
      logInfo(error_ack_wrong);
      break;
    } else if (byte == sym_ack) {
      break;
    } else {
      if (retry == 1) {
        logInfo(warn_ack_neg);
      } else {
        logInfo(error_ack_neg);
        logInfo(error_resp_send);
      }
    }
  }

  tel.setMasterACK(byte);

  logInfo(tel.to_string());
  publish(tel.getMaster().to_vector(), tel.getSlave().to_vector());

  reset();

  return State::MonitorBus;
}

ebus::State ebus::EbusStack::EbusImpl::lockBus() {
  logDebug("lockBus");

  Telegram &tel = m_activeMessage->m_telegram;
  uint8_t byte = tel.getSourceAddress();

  write(byte);

  struct timespec req = {0, m_access_timeout * 1000L};
  nanosleep(&req, (struct timespec *)NULL);

  byte = sym_zero;

  read(byte, 0, 10000L);

  if (byte != tel.getSourceAddress()) {
    logDebug(warn_arb_lost);

    if ((byte & uint8_t(0x0f)) != (tel.getSourceAddress() & uint8_t(0x0f))) {
      m_lock_counter = m_lock_counter_max;
      logDebug(warn_pri_lost);
    } else {
      m_lock_counter = 1;
      logDebug(warn_pri_fit);
    }

    return State::MonitorBus;
  }

  logDebug(info_ebus_lock);

  return State::SendMessage;
}

ebus::State ebus::EbusStack::EbusImpl::sendMessage() {
  logDebug("sendMessage");

  Telegram &tel = m_activeMessage->m_telegram;

  // TODO expand, calc CRC of sequence and send

  for (int retry = 1; retry >= 0; retry--) {
    // send Message
    for (size_t i = retry; i < tel.getMaster().size(); i++)
      write_read(tel.getMaster()[i], 0, 0);

    // send CRC
    write_read(tel.getMasterCRC(), 0, 0);

    // Broadcast ends here
    if (tel.get_type() == Type::BC) {
      logInfo(tel.to_string() + " transmitted");
      return State::FreeBus;
    }

    uint8_t byte;

    // receive ACK
    read(byte, 0, 10000L);

    tel.setSlaveACK(byte);

    if (byte != sym_ack && byte != sym_nak) {
      logWarn(error_ack_wrong);
      m_activeMessage->m_state = EBUS_ERR_TRANSMIT;

      return State::FreeBus;
    } else if (byte == sym_ack) {
      // Master Master ends here
      if (tel.get_type() == Type::MM) {
        logInfo(tel.to_string() + " transmitted");
        return State::FreeBus;
      } else {
        return State::ReceiveResponse;
      }
    } else {
      if (retry == 1) {
        logDebug(warn_ack_neg);
      } else {
        logWarn(error_ack_neg);
        m_activeMessage->m_state = EBUS_ERR_TRANSMIT;
      }
    }
  }

  return State::FreeBus;
}

ebus::State ebus::EbusStack::EbusImpl::receiveResponse() {
  logDebug("receiveResponse");

  Telegram &tel = m_activeMessage->m_telegram;
  uint8_t byte;
  Sequence seq;

  for (int retry = 1; retry >= 0; retry--) {
    // receive NN
    read(byte, 1, 0);

    // maximum data bytes
    if (byte > max_bytes) {
      logWarn(error_nn_wrong);
      m_activeMessage->m_state = EBUS_ERR_TRANSMIT;

      reset();

      return State::MonitorBus;
    }

    seq.push_back(byte);

    // +1 for CRC
    uint8_t bytes = byte + 1;

    for (size_t i = 0; i < bytes; i++) {
      read(byte, 1, 0);

      seq.push_back(byte);

      if (byte == sym_exp) bytes++;
    }
    // TODO check CRC of sequence
    // create slave data
    tel.createSlave(seq);

    if (tel.getSlaveState() == SEQ_OK)
      byte = sym_ack;
    else
      byte = sym_nak;

    // send ACK
    write_read(byte, 0, 0);

    tel.setMasterACK(byte);

    if (tel.getSlaveState() == SEQ_OK) {
      logInfo(tel.to_string() + " transmitted");
      break;
    }

    if (retry == 1) {
      seq.clear();
      logDebug(warn_recv_resp);
    } else {
      logWarn(error_recv_resp);
      m_activeMessage->m_state = EBUS_ERR_TRANSMIT;
    }
  }

  return State::FreeBus;
}

ebus::State ebus::EbusStack::EbusImpl::freeBus() {
  logDebug("freeBus");

  uint8_t byte = sym_syn;

  write_read(byte, 0, 0);

  logDebug(info_ebus_free);

  reset();

  return State::MonitorBus;
}

ebus::State ebus::EbusStack::EbusImpl::handleDeviceError(
    bool error, const std::string &message) {
  if (m_activeMessage != nullptr) m_activeMessage->m_state = EBUS_ERR_DEVICE;

  reset();

  if (error) {
    logError(message);

    m_device->close();

    if (!m_device->isOpen()) logInfo(info_dev_close);

    return State::OpenDevice;
  }

  logWarn(message);
  return State::MonitorBus;
}

ebus::Reaction ebus::EbusStack::EbusImpl::process(
    const std::vector<uint8_t> &message, std::vector<uint8_t> &response) {
  if (m_process != nullptr)
    return m_process(message, response);
  else
    return Reaction::nofunction;
}

void ebus::EbusStack::EbusImpl::publish(const std::vector<uint8_t> &message,
                                        const std::vector<uint8_t> &response) {
  if (!m_publish.empty()) {
    for (const auto &publish : m_publish) publish(message, response);
  }
}

void ebus::EbusStack::EbusImpl::rawdata(const uint8_t &byte) {
  if (!m_rawdata.empty()) {
    for (const auto &rawdata : m_rawdata) rawdata(byte);
  }
}

void ebus::EbusStack::EbusImpl::logError(const std::string &message) {
  if (m_logger != nullptr) m_logger->error(message);
}

void ebus::EbusStack::EbusImpl::logWarn(const std::string &message) {
  if (m_logger != nullptr) m_logger->warn(message);
}

void ebus::EbusStack::EbusImpl::logInfo(const std::string &message) {
  if (m_logger != nullptr) m_logger->info(message);
}

void ebus::EbusStack::EbusImpl::logDebug(const std::string &message) {
  if (m_logger != nullptr) m_logger->debug(message);
}

void ebus::EbusStack::EbusImpl::logTrace(const std::string &message) {
  if (m_logger != nullptr) m_logger->trace(message);
}
