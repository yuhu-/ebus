/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#include "../include/ebus/Ebus.h"

#include <bits/types/struct_timespec.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>

#include "Device.h"
#include "Notify.h"
#include "NQueue.h"
#include "runtime_warning.h"
#include "Sequence.h"
#include "Telegram.h"

#define EBUS_ERR_MASTER       -1 // sending is only as master possible
#define EBUS_ERR_SEQUENCE     -2 // the passed sequence contains an error
#define EBUS_ERR_TRANSMIT     -3 // a data error occurred during sending
#define EBUS_ERR_DEVICE       -4 // a device error occurred
#define EBUS_ERR_OFFLINE      -5 // ebus service is offline

#define STATE_INF_DEV_OPEN     1 // device opened
#define STATE_INF_DEV_CLOSE    2 // device closed
#define STATE_INF_EBUS_LOCK    3 // ebus locked
#define STATE_INF_EBUS_FREE    4 // ebus freed
#define STATE_INF_MSG_INGORE   5 // message ignored
#define STATE_INF_DEV_FLUSH    6 // device flushed
#define STATE_INF_NOT_DEF      7 // message not defined
#define STATE_INF_NO_FUNC      8 // function not implemented

#define STATE_WRN_BYTE_DIF    11 // written/read byte difference
#define STATE_WRN_ARB_LOST    12 // arbitration lost
#define STATE_WRN_PRI_FIT     13 // priority class fit -> retry
#define STATE_WRN_PRI_LOST    14 // priority class lost
#define STATE_WRN_ACK_NEG     15 // received acknowledge byte is negative -> retry
#define STATE_WRN_RECV_RESP   16 // received response is invalid -> retry
#define STATE_WRN_RECV_MSG    17 // received message is invalid

#define STATE_ERR_OPEN_FAIL   21 // opening ebus failed
#define STATE_ERR_CLOSE_FAIL  22 // closing ebus failed
#define STATE_ERR_ACK_NEG     23 // received acknowledge byte is negative -> failed
#define STATE_ERR_ACK_WRONG   24 // received acknowledge byte is wrong
#define STATE_ERR_NN_WRONG    25 // received size byte is wrong
#define STATE_ERR_RECV_RESP   26 // received response is invalid -> failed
#define STATE_ERR_RESP_CREA   27 // creating response failed
#define STATE_ERR_RESP_SEND   28 // sending response failed
#define STATE_ERR_BAD_TYPE    29 // received type does not allow an answer

std::map<int, std::string> EbusErrors =
{
{ EBUS_ERR_MASTER, "sending is only as master possible" },
{ EBUS_ERR_SEQUENCE, "the passed sequence contains an error" },
{ EBUS_ERR_TRANSMIT, "a data error occurred during sending" },
{ EBUS_ERR_DEVICE, "a device error occurred" },
{ EBUS_ERR_OFFLINE, "ebus service is offline" } };

std::map<int, std::string> StateMessages =
{
{ STATE_INF_DEV_OPEN, "device opened" },
{ STATE_INF_DEV_CLOSE, "device closed" },
{ STATE_INF_EBUS_LOCK, "ebus locked" },
{ STATE_INF_EBUS_FREE, "ebus freed" },
{ STATE_INF_MSG_INGORE, "message ignored" },
{ STATE_INF_DEV_FLUSH, "device flushed" },
{ STATE_INF_NOT_DEF, "message not defined" },
{ STATE_INF_NO_FUNC, "function not implemented" },

{ STATE_WRN_BYTE_DIF, "written/read byte difference" },
{ STATE_WRN_ARB_LOST, "arbitration lost" },
{ STATE_WRN_PRI_FIT, "priority class fit -> retry" },
{ STATE_WRN_PRI_LOST, "priority class lost" },
{ STATE_WRN_ACK_NEG, "received acknowledge byte is negative -> retry" },
{ STATE_WRN_RECV_RESP, "received response is invalid -> retry" },
{ STATE_WRN_RECV_MSG, "message is invalid" },

{ STATE_ERR_OPEN_FAIL, "opening ebus failed" },
{ STATE_ERR_CLOSE_FAIL, "closing ebus failed" },
{ STATE_ERR_ACK_NEG, "received acknowledge byte is negative -> failed" },
{ STATE_ERR_ACK_WRONG, "received acknowledge byte is wrong" },
{ STATE_ERR_NN_WRONG, "received size byte is wrong" },
{ STATE_ERR_RECV_RESP, "received response is invalid -> failed" },
{ STATE_ERR_RESP_CREA, "creating response failed" },
{ STATE_ERR_RESP_SEND, "sending response failed" },
{ STATE_ERR_BAD_TYPE, "received type does not allow an answer" } };

namespace ebus
{

struct Message : public Notify
{

	explicit Message(Telegram &tel) : Notify(), m_telegram(tel)
	{
	}

	Telegram &m_telegram;
	int m_state = 0;

};

enum class State
{
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

} // namespace ebus

class ebus::Ebus::EbusImpl : private Notify
{

public:
	EbusImpl(const std::byte address, const std::string &device);

	~EbusImpl();

	void open();
	void close();

	bool online();

	int transmit(const std::vector<std::byte> &message, std::vector<std::byte> &response);

	const std::string errorText(const int error) const;

	void register_logger(std::shared_ptr<ILogger> logger);

	void register_process(
		std::function<Reaction(const std::vector<std::byte> &message, std::vector<std::byte> &response)> process);

	void register_publish(
		std::function<void(const std::vector<std::byte> &message, const std::vector<std::byte> &response)> publish);

	void register_rawdata(std::function<void(const std::byte &byte)> rawdata);

	void set_access_timeout(const long &access_timeout);
	void set_lock_counter_max(const int &lock_counter_max);

	void set_open_counter_max(const int &open_counter_max);

	static const std::vector<std::byte> range(const std::vector<std::byte> &seq, const size_t index, const size_t len);
	static const std::vector<std::byte> to_vector(const std::string &str);
	static const std::string to_string(const std::vector<std::byte> &seq);

private:
	std::thread m_thread;

	bool m_running = true;
	bool m_online = false;
	bool m_close = false;

	const std::byte m_address;
	const std::byte m_slaveAddress;

	long m_access_timeout = 4400L;

	int m_lock_counter_max = 5;
	int m_lock_counter = 0;

	long m_open_counter_max = 10;
	long m_open_counter = 0;

	NQueue<std::shared_ptr<Message>> m_messageQueue;

	std::unique_ptr<Device> m_device = nullptr;

	std::shared_ptr<ILogger> m_logger = nullptr;

	std::function<Reaction(const std::vector<std::byte> &message, std::vector<std::byte> &response)> m_process;

	std::vector<std::function<void(const std::vector<std::byte> &message, const std::vector<std::byte> &response)>> m_publish;

	std::vector<std::function<void(const std::byte &byte)>> m_rawdata;

	Sequence m_sequence;
	std::shared_ptr<Message> m_activeMessage = nullptr;
	std::shared_ptr<Message> m_passiveMessage = nullptr;

	int transmit(Telegram &tel);

	void read(std::byte &byte, const long sec, const long nsec);
	void write(const std::byte &byte);
	void writeRead(const std::byte &byte, const long sec, const long nsec);

	void reset();

	const std::string stateMessage(const int state);
	const std::string telegramInfo(Telegram &tel);

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

	Reaction process(const std::vector<std::byte> &message, std::vector<std::byte> &response);

	void publish(const std::vector<std::byte> &message, const std::vector<std::byte> &response);

	void rawdata(const std::byte &byte);

	void logError(const std::string &message);
	void logWarn(const std::string &message);
	void logInfo(const std::string &message);
	void logDebug(const std::string &message);
	void logTrace(const std::string &message);

};

ebus::Ebus::Ebus(const std::byte address, const std::string &device) : impl
{ std::make_unique<EbusImpl>(address, device) }
{
}

// move functions
ebus::Ebus& ebus::Ebus::operator=(Ebus&&) = default;
ebus::Ebus::Ebus(Ebus&&) = default;

// copy functions
//ebus::Ebus& ebus::Ebus::Ebus::operator=(const Ebus &anotherEbus)
//{
//	*pImpl = *anotherEbus.pImpl;
//	return (*this);
//}

//ebus::Ebus::Ebus(const Ebus &anotherEbus) : pImpl(std::make_unique<Ebus::impl>(*anotherEbus.pImpl))
//{
//}

ebus::Ebus::~Ebus() = default;

void ebus::Ebus::open()
{
	this->impl->open();
}

void ebus::Ebus::close()
{
	this->impl->close();
}

bool ebus::Ebus::online()
{
	return (this->impl->online());
}

int ebus::Ebus::transmit(const std::vector<std::byte> &message, std::vector<std::byte> &response)
{
	return (this->impl->transmit(message, response));
}

const std::string ebus::Ebus::error_text(const int error) const
{
	return (this->impl->errorText(error));
}

void ebus::Ebus::register_logger(std::shared_ptr<ILogger> logger)
{
	this->impl->register_logger(logger);
}

void ebus::Ebus::register_process(
	std::function<Reaction(const std::vector<std::byte> &message, std::vector<std::byte> &response)> process)
{
	this->impl->register_process(process);
}

void ebus::Ebus::register_publish(
	std::function<void(const std::vector<std::byte> &message, const std::vector<std::byte> &response)> publish)
{
	this->impl->register_publish(publish);
}

void ebus::Ebus::register_rawdata(std::function<void(const std::byte &byte)> rawdata)
{
	this->impl->register_rawdata(rawdata);
}

void ebus::Ebus::set_access_timeout(const long &access_timeout)
{
	this->impl->set_access_timeout(access_timeout);
}

void ebus::Ebus::set_lock_counter_max(const int &lock_counter_max)
{
	this->impl->set_lock_counter_max(lock_counter_max);
}

void ebus::Ebus::set_open_counter_max(const int &open_counter_max)
{
	this->impl->set_open_counter_max(open_counter_max);
}

const std::vector<std::byte> ebus::Ebus::range(const std::vector<std::byte> &seq, const size_t index, const size_t len)
{
	return (EbusImpl::range(seq, index, len));
}

const std::vector<std::byte> ebus::Ebus::to_vector(const std::string &str)
{
	return (EbusImpl::to_vector(str));
}

const std::string ebus::Ebus::to_string(const std::vector<std::byte> &vec)
{
	return (EbusImpl::to_string(vec));
}

ebus::Ebus::EbusImpl::EbusImpl(const std::byte address, const std::string &device) : Notify(), m_address(address), m_slaveAddress(
	Telegram::slaveAddress(address)), m_device(std::make_unique<Device>(device))
{
	m_thread = std::thread(&EbusImpl::run, this);
}

ebus::Ebus::EbusImpl::~EbusImpl()
{
	close();

	struct timespec req =
	{ 0, 10000L };

	while (m_online)
		nanosleep(&req, (struct timespec*) NULL);

	m_running = false;
	nanosleep(&req, (struct timespec*) NULL);

	notify();
	m_thread.join();

	while (m_messageQueue.size() > 0)
		m_messageQueue.dequeue().reset();
}

void ebus::Ebus::EbusImpl::open()
{
	notify();
}

void ebus::Ebus::EbusImpl::close()
{
	m_close = true;
}

bool ebus::Ebus::EbusImpl::online()
{
	return (m_online);
}

int ebus::Ebus::EbusImpl::transmit(const std::vector<std::byte> &message, std::vector<std::byte> &response)
{
	Telegram tel;
	tel.createMaster(m_address, message);

	int result = transmit(tel);
	response = tel.getSlave().getSequence();

	return (result);
}

const std::string ebus::Ebus::EbusImpl::errorText(const int error) const
{
	return (EbusErrors[error]);
}

void ebus::Ebus::EbusImpl::register_logger(std::shared_ptr<ILogger> logger)
{
	m_logger = logger;
}

void ebus::Ebus::EbusImpl::register_process(
	std::function<Reaction(const std::vector<std::byte> &message, std::vector<std::byte> &response)> process)
{
	m_process = process;
}

void ebus::Ebus::EbusImpl::register_publish(
	std::function<void(const std::vector<std::byte> &message, const std::vector<std::byte> &response)> publish)
{
	m_publish.push_back(publish);
}

void ebus::Ebus::EbusImpl::register_rawdata(std::function<void(const std::byte &byte)> rawdata)
{
	m_rawdata.push_back(rawdata);
}

void ebus::Ebus::EbusImpl::set_access_timeout(const long &access_timeout)
{
	m_access_timeout = access_timeout;
}

void ebus::Ebus::EbusImpl::set_lock_counter_max(const int &lock_counter_max)
{
	m_lock_counter_max = lock_counter_max;
}

void ebus::Ebus::EbusImpl::set_open_counter_max(const int &open_counter_max)
{
	m_open_counter_max = open_counter_max;
}

const std::vector<std::byte> ebus::Ebus::EbusImpl::range(const std::vector<std::byte> &seq, const size_t index, const size_t len)
{
	return (Sequence::range(seq, index, len));
}

const std::vector<std::byte> ebus::Ebus::EbusImpl::to_vector(const std::string &str)
{
	std::vector<std::byte> result;

	for (size_t i = 0; i + 1 < str.size(); i += 2)
		result.push_back(std::byte(std::strtoul(str.substr(i, 2).c_str(), nullptr, 16)));

	return (result);
}

const std::string ebus::Ebus::EbusImpl::to_string(const std::vector<std::byte> &vec)
{
	std::ostringstream ostr;

	for (size_t i = 0; i < vec.size(); i++)
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(vec[i]);

	return (ostr.str());
}

int ebus::Ebus::EbusImpl::transmit(Telegram &tel)
{
	int result = SEQ_OK;

	if (tel.getMasterState() != SEQ_OK)
	{
		result = EBUS_ERR_SEQUENCE;
	}
	else if (!Telegram::isMaster(m_address))
	{
		result = EBUS_ERR_MASTER;
	}
	else if (!online())
	{
		result = EBUS_ERR_OFFLINE;
	}
	else
	{
		std::shared_ptr<Message> message = std::make_shared<Message>(tel);
		m_messageQueue.enqueue(message);
		message->waitNotify();
		result = message->m_state;
		message.reset();
	}

	return (result);
}

void ebus::Ebus::EbusImpl::read(std::byte &byte, const long sec, const long nsec)
{
	m_device->recv(byte, sec, nsec);

	rawdata(byte);

	std::ostringstream ostr;
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte) << std::nouppercase
		<< std::setw(0);
	logTrace("<" + ostr.str());
}

void ebus::Ebus::EbusImpl::write(const std::byte &byte)
{
	m_device->send(byte);

	std::ostringstream ostr;
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte) << std::nouppercase
		<< std::setw(0);
	logTrace(">" + ostr.str());
}

void ebus::Ebus::EbusImpl::writeRead(const std::byte &byte, const long sec, const long nsec)
{
	write(byte);

	std::byte readByte;
	read(readByte, sec, nsec);

	if (readByte != byte) logDebug(stateMessage(STATE_WRN_BYTE_DIF));
}

void ebus::Ebus::EbusImpl::reset()
{
	m_open_counter = 0;
	m_lock_counter = m_lock_counter_max;

	m_sequence.clear();

	if (m_activeMessage != nullptr)
	{
		publish(m_activeMessage->m_telegram.getMaster().getSequence(), m_activeMessage->m_telegram.getSlave().getSequence());

		m_activeMessage->notify();
		m_activeMessage = nullptr;
	}

	if (m_passiveMessage != nullptr)
	{
		std::shared_ptr<Message> message = m_passiveMessage;
		m_passiveMessage = nullptr;
	}
}

const std::string ebus::Ebus::EbusImpl::stateMessage(const int state)
{
	std::ostringstream ostr;

	ostr << StateMessages[state];

	return (ostr.str());
}

const std::string ebus::Ebus::EbusImpl::telegramInfo(Telegram &tel)
{
	std::ostringstream ostr;

	if (tel.getMasterState() == SEQ_OK)
	{
		if (tel.getType() == TEL_TYPE_BC)
			ostr << "BC ";
		else if (tel.getType() == TEL_TYPE_MM)
			ostr << "MM ";
		else
			ostr << "MS ";
	}

	ostr << tel.toString();

	return (ostr.str());
}

void ebus::Ebus::EbusImpl::run()
{
	logInfo("Ebus started");

	State state = State::OpenDevice;

	while (m_running)
	{
		try
		{
			switch (state)
			{
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
		} catch (const ebus::runtime_warning &ex)
		{
			state = handleDeviceError(false, ex.what());
		} catch (const std::runtime_error &ex)
		{
			state = handleDeviceError(true, ex.what());
		}

		if (m_close) state = State::IdleSystem;
	}

	logInfo("Ebus stopped");
}

ebus::State ebus::Ebus::EbusImpl::idleSystem()
{
	logDebug("idleSystem");

	if (m_device->isOpen())
	{
		m_device->close();

		if (!m_device->isOpen())
			logInfo(stateMessage(STATE_INF_DEV_CLOSE));
		else
			logWarn(stateMessage(STATE_ERR_CLOSE_FAIL));
	}

	reset();

	m_online = false;
	m_close = false;

	waitNotify();

	return (State::OpenDevice);
}

ebus::State ebus::Ebus::EbusImpl::openDevice()
{
	logDebug("openDevice");

	std::byte byte = seq_zero;

	if (!m_device->isOpen())
	{
		m_device->open();

		if (!m_device->isOpen())
		{
			logWarn(stateMessage(STATE_ERR_OPEN_FAIL));

			m_open_counter++;
			if (m_open_counter > m_open_counter_max) return (State::IdleSystem);

			sleep(1);
			return (State::OpenDevice);
		}
	}

	logInfo(stateMessage(STATE_INF_DEV_OPEN));

	do
	{
		read(byte, 1, 0);
	} while (byte != seq_syn);

	reset();

	m_online = true;

	logInfo(stateMessage(STATE_INF_DEV_FLUSH));

	return (State::MonitorBus);
}

ebus::State ebus::Ebus::EbusImpl::monitorBus()
{
	logDebug("monitorBus");

	std::byte byte = seq_zero;

	read(byte, 1, 0);

	if (byte == seq_syn)
	{
		if (m_lock_counter != 0)
		{
			m_lock_counter--;
			logDebug("m_lock_counter: " + std::to_string(m_lock_counter));
		}

		// decode Sequence
		if (m_sequence.size() != 0)
		{
			logDebug(m_sequence.toString());

			Telegram tel(m_sequence);
			logInfo(telegramInfo(tel));

			if (tel.isValid()) publish(tel.getMaster().getSequence(), tel.getSlave().getSequence());

			if (m_sequence.size() == 1 && m_lock_counter < 2) m_lock_counter = 2;

			tel.clear();
			m_sequence.clear();
		}

		// check for new Message
		if (m_activeMessage == nullptr && m_messageQueue.size() > 0) m_activeMessage = m_messageQueue.dequeue();

		// handle Message
		if (m_activeMessage != nullptr && m_lock_counter == 0) return (State::LockBus);
	}
	else
	{
		m_sequence.push_back(byte);

		// handle broadcast and at me addressed messages
		if (m_sequence.size() == 2
			&& (m_sequence[1] == seq_broad || m_sequence[1] == m_address || m_sequence[1] == m_slaveAddress))
			return (State::ReceiveMessage);

	}

	return (State::MonitorBus);
}

ebus::State ebus::Ebus::EbusImpl::receiveMessage()
{
	logDebug("receiveMessage");

	std::byte byte;

	// receive Header PBSBNN
	for (int i = 0; i < 3; i++)
	{
		byte = seq_zero;

		read(byte, 1, 0);

		m_sequence.push_back(byte);
	}

	// maximum data bytes
	if (std::to_integer<int>(m_sequence[4]) > seq_max_bytes)
	{
		logWarn(stateMessage(STATE_ERR_NN_WRONG));
		m_activeMessage->m_state = EBUS_ERR_TRANSMIT;

		reset();

		return (State::MonitorBus);
	}

	// bytes to receive
	int bytes = std::to_integer<int>(m_sequence[4]);

	// receive Data Dx
	for (int i = 0; i < bytes; i++)
	{
		byte = seq_zero;

		read(byte, 1, 0);

		m_sequence.push_back(byte);

		if (byte == seq_exp) bytes++;
	}

	// 1 for CRC
	bytes = 1;

	// receive CRC
	for (int i = 0; i < bytes; i++)
	{
		read(byte, 1, 0);

		m_sequence.push_back(byte);

		if (byte == seq_exp) bytes++;
	}

	logDebug(m_sequence.toString());

	Telegram tel;
	tel.createMaster(m_sequence);

	if (m_sequence[1] != seq_broad)
	{
		if (tel.getMasterState() == SEQ_OK)
		{
			byte = seq_ack;
		}
		else
		{
			byte = seq_nak;
			logInfo(stateMessage(STATE_WRN_RECV_MSG));
		}

		// send ACK
		writeRead(byte, 0, 0);

		tel.setSlaveACK(byte);
	}

	if (tel.getMasterState() == SEQ_OK)
	{
		if (tel.getType() != TEL_TYPE_MS)
		{
			logInfo(telegramInfo(tel));
			publish(tel.getMaster().getSequence(), tel.getSlave().getSequence());
		}

		return (State::ProcessMessage);
	}

	m_sequence.clear();

	return (State::MonitorBus);
}

ebus::State ebus::Ebus::EbusImpl::processMessage()
{
	logDebug("processMessage");

	Telegram tel;
	tel.createMaster(m_sequence);

	std::vector<std::byte> response;

	Reaction reaction = process(tel.getMaster().getSequence(), response);

	switch (reaction)
	{
	case Reaction::nofunction:
		logDebug(stateMessage(STATE_INF_NO_FUNC));
		break;
	case Reaction::undefined:
		logDebug(stateMessage(STATE_INF_NOT_DEF));
		break;
	case Reaction::ignore:
		logInfo(stateMessage(STATE_INF_MSG_INGORE));
		break;
	case Reaction::response:
		if (tel.getType() == TEL_TYPE_MS)
		{
			tel.createSlave(response);

			if (tel.getSlaveState() == SEQ_OK)
			{
				logInfo("response: " + tel.toStringSlave());
				m_passiveMessage = std::make_shared<Message>(tel);

				return (State::SendResponse);
			}
			else
			{
				logWarn(stateMessage(STATE_ERR_RESP_CREA));
			}
		}
		else
		{
			logWarn(stateMessage(STATE_ERR_BAD_TYPE));
		}

		break;
	default:
		break;
	}

	m_sequence.clear();

	return (State::MonitorBus);
}

ebus::State ebus::Ebus::EbusImpl::sendResponse()
{
	logDebug("sendResponse");

	Telegram &tel = m_passiveMessage->m_telegram;
	std::byte byte;

	for (int retry = 1; retry >= 0; retry--)
	{
		// send Message
		for (size_t i = retry; i < tel.getSlave().size(); i++)
			writeRead(tel.getSlave()[i], 0, 0);

		// send CRC
		writeRead(tel.getSlaveCRC(), 0, 0);

		// receive ACK
		read(byte, 0, 5000L);

		if (byte != seq_ack && byte != seq_nak)
		{
			logInfo(stateMessage(STATE_ERR_ACK_WRONG));
			break;
		}
		else if (byte == seq_ack)
		{
			break;
		}
		else
		{
			if (retry == 1)
			{
				logInfo(stateMessage(STATE_WRN_ACK_NEG));
			}
			else
			{
				logInfo(stateMessage(STATE_ERR_ACK_NEG));
				logInfo(stateMessage(STATE_ERR_RESP_SEND));
			}
		}
	}

	tel.setMasterACK(byte);

	logInfo(telegramInfo(tel));
	publish(tel.getMaster().getSequence(), tel.getSlave().getSequence());

	reset();

	return (State::MonitorBus);
}

ebus::State ebus::Ebus::EbusImpl::lockBus()
{
	logDebug("lockBus");

	Telegram &tel = m_activeMessage->m_telegram;
	std::byte byte = tel.getMasterQQ();

	write(byte);

	struct timespec req =
	{ 0, m_access_timeout * 1000L };
	nanosleep(&req, (struct timespec*) NULL);

	byte = seq_zero;

	read(byte, 0, 5000L);

	if (byte != tel.getMasterQQ())
	{
		logDebug(stateMessage(STATE_WRN_ARB_LOST));

		if ((byte & std::byte(0x0f)) != (tel.getMasterQQ() & std::byte(0x0f)))
		{
			m_lock_counter = m_lock_counter_max;
			logDebug(stateMessage(STATE_WRN_PRI_LOST));
		}
		else
		{
			m_lock_counter = 1;
			logDebug(stateMessage(STATE_WRN_PRI_FIT));
		}

		return (State::MonitorBus);
	}

	logDebug(stateMessage(STATE_INF_EBUS_LOCK));

	return (State::SendMessage);
}

ebus::State ebus::Ebus::EbusImpl::sendMessage()
{
	logDebug("sendMessage");

	Telegram &tel = m_activeMessage->m_telegram;

	for (int retry = 1; retry >= 0; retry--)
	{
		// send Message
		for (size_t i = retry; i < tel.getMaster().size(); i++)
			writeRead(tel.getMaster()[i], 0, 0);

		// send CRC
		writeRead(tel.getMasterCRC(), 0, 0);

		// Broadcast ends here
		if (tel.getType() == TEL_TYPE_BC)
		{
			logInfo(telegramInfo(tel) + " transmitted");
			return (State::FreeBus);
		}

		std::byte byte;

		// receive ACK
		read(byte, 0, 5000L);

		tel.setSlaveACK(byte);

		if (byte != seq_ack && byte != seq_nak)
		{
			logWarn(stateMessage(STATE_ERR_ACK_WRONG));
			m_activeMessage->m_state = EBUS_ERR_TRANSMIT;

			return (State::FreeBus);
		}
		else if (byte == seq_ack)
		{
			// Master Master ends here
			if (tel.getType() == TEL_TYPE_MM)
			{
				logInfo(telegramInfo(tel) + " transmitted");
				return (State::FreeBus);
			}
			else
			{
				return (State::ReceiveResponse);
			}
		}
		else
		{
			if (retry == 1)
			{
				logDebug(stateMessage(STATE_WRN_ACK_NEG));
			}
			else
			{
				logWarn(stateMessage(STATE_ERR_ACK_NEG));
				m_activeMessage->m_state = EBUS_ERR_TRANSMIT;
			}
		}
	}

	return (State::FreeBus);
}

ebus::State ebus::Ebus::EbusImpl::receiveResponse()
{
	logDebug("receiveResponse");

	Telegram &tel = m_activeMessage->m_telegram;
	std::byte byte;
	Sequence seq;

	for (int retry = 1; retry >= 0; retry--)
	{
		// receive NN
		read(byte, 1, 0);

		// maximum data bytes
		if (std::to_integer<int>(byte) > seq_max_bytes)
		{
			logWarn(stateMessage(STATE_ERR_NN_WRONG));
			m_activeMessage->m_state = EBUS_ERR_TRANSMIT;

			reset();

			return (State::MonitorBus);
		}

		seq.push_back(byte);

		// +1 for CRC
		size_t bytes = std::to_integer<size_t>(byte) + 1;

		for (size_t i = 0; i < bytes; i++)
		{
			read(byte, 1, 0);

			seq.push_back(byte);

			if (byte == seq_syn || byte == seq_exp) bytes++;
		}

		// create slave data
		tel.createSlave(seq);

		if (tel.getSlaveState() == SEQ_OK)
			byte = seq_ack;
		else
			byte = seq_nak;

		// send ACK
		writeRead(byte, 0, 0);

		tel.setMasterACK(byte);

		if (tel.getSlaveState() == SEQ_OK)
		{
			logInfo(telegramInfo(tel) + " transmitted");
			break;
		}

		if (retry == 1)
		{
			seq.clear();
			logDebug(stateMessage(STATE_WRN_RECV_RESP));
		}
		else
		{
			logWarn(stateMessage(STATE_ERR_RECV_RESP));
			m_activeMessage->m_state = EBUS_ERR_TRANSMIT;
		}
	}

	return (State::FreeBus);
}

ebus::State ebus::Ebus::EbusImpl::freeBus()
{
	logDebug("freeBus");

	std::byte byte = seq_syn;

	writeRead(byte, 0, 0);

	logDebug(stateMessage(STATE_INF_EBUS_FREE));

	reset();

	return (State::MonitorBus);
}

ebus::State ebus::Ebus::EbusImpl::handleDeviceError(bool error, const std::string &message)
{
	if (m_activeMessage != nullptr) m_activeMessage->m_state = EBUS_ERR_DEVICE;

	reset();

	if (error)
	{
		logError(message);

		m_device->close();

		if (!m_device->isOpen()) logInfo(stateMessage(STATE_INF_DEV_CLOSE));

		return (State::OpenDevice);
	}

	logWarn(message);
	return (State::MonitorBus);
}

ebus::Reaction ebus::Ebus::EbusImpl::process(const std::vector<std::byte> &message, std::vector<std::byte> &response)
{
	if (m_process != nullptr)
		return (m_process(message, response));
	else
		return (Reaction::nofunction);
}

void ebus::Ebus::EbusImpl::publish(const std::vector<std::byte> &message, const std::vector<std::byte> &response)
{
	if (!m_publish.empty())
	{
		for (const auto &publish : m_publish)
			publish(message, response);
	}
}

void ebus::Ebus::EbusImpl::rawdata(const std::byte &byte)
{
	if (!m_rawdata.empty())
	{
		for (const auto &rawdata : m_rawdata)
			rawdata(byte);
	}
}

void ebus::Ebus::EbusImpl::logError(const std::string &message)
{
	if (m_logger != nullptr) m_logger->error(message);
}

void ebus::Ebus::EbusImpl::logWarn(const std::string &message)
{
	if (m_logger != nullptr) m_logger->warn(message);
}

void ebus::Ebus::EbusImpl::logInfo(const std::string &message)
{
	if (m_logger != nullptr) m_logger->info(message);
}

void ebus::Ebus::EbusImpl::logDebug(const std::string &message)
{
	if (m_logger != nullptr) m_logger->debug(message);
}

void ebus::Ebus::EbusImpl::logTrace(const std::string &message)
{
	if (m_logger != nullptr) m_logger->trace(message);
}
