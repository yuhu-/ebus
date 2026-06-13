/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/sequence.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <string>

namespace ebus::detail {

/**
 * Based on the eBUS specification, the Telegram class can parse, create, and
 * evaluate sequences of bytes that represent Master-Slave, Master-Master, and
 * Broadcast telegrams. It provides methods to extract the source and target
 * addresses, primary and secondary commands, data bytes, and CRC. The class
 * also checks the validity of the telegrams according to the specification and
 * can convert them to string representations.
 *
 * Master-Slave Telegram:
 * Sender   (Master): QQ ZZ PB SB NN DBx CRC                ACK SYN
 * Receiver ( Slave):                        ACK NN DBx CRC
 *
 * Master-Master Telegram:
 * Sender   (Master): QQ ZZ PB SB NN DBx CRC     SYN
 * Receiver (Master):                        ACK
 *
 * Broadcast Telegram:
 * Sender   (Master): QQ ZZ PB SB NN DBx CRC SYN
 * Receiver    (All):
 *
 * QQ...Source address (25 possible addresses)
 * ZZ...Target address (254 possible addresses)
 * PB...Primary command
 * SB...Secondary command
 * NN...Number of data bytes (0 <= NN <= 16)
 * DBx..Data bytes (payload)
 * CRC..8-Bit CRC byte
 * ACK..Acknowledgement byte (0x00 OK, 0xFF NOK)
 * SYN..Synchronisation byte (0xAA)
 */
template <size_t kInlineCapacity = SequenceLimits::default_capacity>
class TelegramImpl {
 public:
  // --- Public Types & Constants ---
  static constexpr size_t master_header_size = 5;  // QQ ZZ PB SB NN
  static constexpr size_t slave_header_size = 1;   // NN

  static constexpr size_t crc_size = 1;
  static constexpr size_t ack_size = 1;

  static constexpr size_t master_source_idx = 0;
  static constexpr size_t master_target_idx = 1;
  static constexpr size_t master_pb_idx = 2;
  static constexpr size_t master_sb_idx = 3;
  static constexpr size_t master_nn_idx = 4;

  static constexpr size_t slave_nn_idx = 0;

  TelegramImpl() = default;

  template <size_t C>
  explicit TelegramImpl(ebus::SequenceImpl<C>& sequence) {
    parse(sequence);
  }

  template <size_t C>
  void parse(ebus::SequenceImpl<C>& sequence) {
    clear();
    sequence.reduce();  // Normalise to logical bytes
    size_t cursor = 0;

    // 1. Try to parse Master Part.
    // If NAK was received, skip the failed attempt and try to parse the next
    // chunk as the master part (retry).
    if (!parseSequencePart(sequence, cursor, true)) return;

    if (telegram_type_ != TelegramType::broadcast &&
        master_ack_ == ebus::Symbols::nak) {
      if (!parseSequencePart(sequence, cursor, true)) return;
      // If second attempt also results in NAK, it's a protocol failure.
      if (master_ack_ == ebus::Symbols::nak)
        master_state_ = SequenceState::err_ack_negative;
    }

    if (master_state_ != SequenceState::seq_ok) return;

    // 2. Parse Slave Part for MS telegrams.
    if (telegram_type_ == TelegramType::master_slave) {
      if (!parseSequencePart(sequence, cursor, false)) return;
      if (slave_ack_ == Symbols::nak) {
        if (!parseSequencePart(sequence, cursor, false)) return;
        if (slave_ack_ == Symbols::nak)
          slave_state_ = SequenceState::err_ack_negative;
      }
    }
  }

  void createMaster(uint8_t source_address, ByteView data) {
    SequenceImpl<kInlineCapacity> sequence;
    sequence.push_back(source_address, false);
    for (uint8_t b : data) sequence.push_back(b, false);
    createMaster(sequence);
  }

  template <size_t C>
  void createMaster(SequenceImpl<C>& sequence) {
    master_state_ = SequenceState::seq_ok;
    sequence.reduce();
    if (sequence.size() < master_header_size) {
      master_state_ = SequenceState::err_seq_too_short;
      return;
    }
    if (!isMaster(sequence[master_source_idx])) {
      master_state_ = SequenceState::err_source_address;
      return;
    }
    if (!isTarget(sequence[master_target_idx])) {
      master_state_ = SequenceState::err_target_address;
      return;
    }
    if (uint8_t(sequence[master_nn_idx]) > SequenceLimits::max_data_bytes) {
      master_state_ = SequenceState::err_data_byte;
      return;
    }

    /* Spec 5.4 & 5.5: Primary and Secondary commands cannot be 0xAA or 0xA9 */
    if (sequence[master_pb_idx] == Symbols::syn ||
        sequence[master_pb_idx] == Symbols::ext ||
        sequence[master_sb_idx] == Symbols::syn ||
        sequence[master_sb_idx] == Symbols::ext) {
      master_state_ = SequenceState::err_data_byte;
      return;
    }

    if (sequence.size() <
        static_cast<size_t>(master_header_size +
                            uint8_t(sequence[master_nn_idx]))) {
      master_state_ = SequenceState::err_seq_too_short;
      return;
    }

    telegram_type_ = typeOf(sequence[master_target_idx]);
    master_nn_ = static_cast<size_t>(uint8_t(sequence[master_nn_idx]));

    if (sequence.size() ==
        static_cast<size_t>(master_header_size + master_nn_)) {
      master_.assignSlice(sequence, 0);
      master_crc_ = sequence.crc();
    } else if (sequence.size() == static_cast<size_t>(master_header_size +
                                                      master_nn_ + crc_size)) {
      master_.assignSlice(sequence, 0, master_header_size + master_nn_);
      master_crc_ = sequence[master_header_size + master_nn_];
      if (master_.crc() != master_crc_)
        master_state_ = SequenceState::err_crc_invalid;
    } else {
      master_state_ = SequenceState::err_seq_too_long;
      return;
    }
  }

  void createSlave(ByteView data) {
    SequenceImpl<kInlineCapacity> sequence;
    for (uint8_t b : data) sequence.push_back(b, false);
    createSlave(sequence);
  }

  template <size_t C>
  void createSlave(SequenceImpl<C>& sequence) {
    slave_state_ = SequenceState::seq_ok;
    sequence.reduce();
    if (sequence.size() < 2) {
      slave_state_ = SequenceState::err_seq_too_short;
      return;
    }
    if (uint8_t(sequence[slave_nn_idx]) > SequenceLimits::max_data_bytes) {
      slave_state_ = SequenceState::err_data_byte;
      return;
    }

    if (sequence.size() <
        static_cast<size_t>(slave_header_size +
                            uint8_t(sequence[slave_nn_idx]))) {
      slave_state_ = SequenceState::err_seq_too_short;
      return;
    }

    slave_nn_ = static_cast<size_t>(uint8_t(sequence[slave_nn_idx]));
    if (sequence.size() == (1 + slave_nn_)) {
      slave_.assignSlice(sequence, 0);
      slave_crc_ = sequence.crc();
    } else if (sequence.size() == (1 + slave_nn_ + 1)) {
      slave_.assignSlice(sequence, 0, 1 + slave_nn_);
      slave_crc_ = sequence[1 + slave_nn_];
      if (slave_.crc() != slave_crc_)
        slave_state_ = SequenceState::err_crc_invalid;
    } else {
      slave_state_ = SequenceState::err_seq_too_long;
      return;
    }
  }

  void clear() {
    telegram_type_ = TelegramType::undefined;
    master_.clear();
    master_nn_ = 0;
    master_crc_ = Symbols::zero;
    master_ack_ = Symbols::zero;
    master_state_ = SequenceState::seq_empty;
    slave_.clear();
    slave_nn_ = 0;
    slave_crc_ = Symbols::zero;
    slave_ack_ = Symbols::zero;
    slave_state_ = SequenceState::seq_empty;
  }

  // returns the master sequence [QQ ZZ PB SB NN DBx] without CRC byte
  const SequenceImpl<kInlineCapacity>& getMaster() const { return master_; }

  uint8_t getSourceAddress() const { return master_[master_source_idx]; }
  uint8_t getTargetAddress() const { return master_[master_target_idx]; }

  uint8_t getPrimaryCommand() const { return master_[master_pb_idx]; }
  uint8_t getSecondaryCommand() const { return master_[master_sb_idx]; }

  uint8_t getMasterNumberBytes() const { return master_[master_nn_idx]; }
  ByteView getMasterDataBytes() const {
    return master_.range(master_header_size,
                         master_.size() - master_header_size);
  }

  uint8_t getMasterCRC() const { return master_crc_; }
  ebus::SequenceState getMasterState() const { return master_state_; }

  void setMasterACK(uint8_t ack_byte) { master_ack_ = ack_byte; }
  uint8_t getMasterACK() const { return master_ack_; }

  // returns the slave sequence [NN DBx] without CRC byte
  const SequenceImpl<kInlineCapacity>& getSlave() const { return slave_; }

  uint8_t getSlaveNumberBytes() const { return slave_[slave_nn_idx]; }
  ByteView getSlaveDataBytes() const {
    return slave_.range(slave_header_size, slave_.size() - slave_header_size);
  }

  uint8_t getSlaveCRC() const { return slave_crc_; }
  ebus::SequenceState getSlaveState() const { return slave_state_; }

  void setSlaveACK(uint8_t ack_byte) { slave_ack_ = ack_byte; }
  uint8_t getSlaveACK() const { return slave_ack_; }

  ebus::TelegramType getType() const { return telegram_type_; }

  bool isValid() const {
    if (telegram_type_ != TelegramType::master_slave)
      return master_state_ == SequenceState::seq_ok;

    return (master_state_ == SequenceState::seq_ok &&
            slave_state_ == SequenceState::seq_ok);
  }

  void toJson(detail::JsonWriter& writer) const {
    detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::object);
    writer.writeField("type", ebus::toString(telegram_type_));
    writer.writeField("valid", isValid());

    {
      auto masterScope = writer.objectScope("master");
      writer.writeHexField("source", ByteView(&master_[master_source_idx], 1));
      writer.writeHexField("target", ByteView(&master_[master_target_idx], 1));
      writer.writeHexField("pb", ByteView(&master_[master_pb_idx], 1));
      writer.writeHexField("sb", ByteView(&master_[master_sb_idx], 1));
      writer.writeField("nn", static_cast<uint32_t>(master_[master_nn_idx]));
      writer.writeHexField("data", getMasterDataBytes());
      writer.writeHexField("crc", ByteView(&master_crc_, 1));
      writer.writeHexField("ack", ByteView(&master_ack_, 1));
      writer.writeField("state", ebus::toString(master_state_));
    }

    if (telegram_type_ == TelegramType::master_slave) {
      auto slaveScope = writer.objectScope("slave");
      writer.writeField("nn", static_cast<uint32_t>(slave_[slave_nn_idx]));
      writer.writeHexField("data", getSlaveDataBytes());
      writer.writeHexField("crc", ByteView(&slave_crc_, 1));
      writer.writeHexField("ack", ByteView(&slave_ack_, 1));
      writer.writeField("state", ebus::toString(slave_state_));
    }
  }

  /**
   * @brief Appends a human-readable string representation of the telegram
   * to an existing std::string. Heap-free if 'out' has sufficient capacity.
   */
  void toString(std::string& out) const {
    toStringMaster(out);

    if (master_state_ == SequenceState::seq_ok &&
        telegram_type_ == TelegramType::master_slave) {
      out += ' ';
      toStringSlave(out);
    }
  }

  /**
   * @brief Returns a human-readable string representation of the entire
   * telegram. Allocates a new std::string.
   */
  std::string toString() const {
    std::string res;
    res.reserve(128);
    toString(res);
    return res;
  }

  /**
   * @brief Appends a human-readable string representation of the master part
   * to an existing std::string. Heap-free if 'out' has sufficient capacity.
   */
  void toStringMaster(std::string& out) const {
    if (master_state_ != SequenceState::seq_ok) {
      toStringMasterState(out);
      return;
    }
    master_.toHexString(out);
  }

  std::string toStringMaster() const {
    std::string res;
    res.reserve(master_.size() * 2 + 32);  // Estimate
    toStringMaster(res);
    return res;
  }

  /**
   * @brief Appends a human-readable string representation of the slave part
   * to an existing std::string. Heap-free if 'out' has sufficient capacity.
   */
  void toStringSlave(std::string& out) const {
    if (slave_state_ != SequenceState::seq_ok &&
        telegram_type_ != TelegramType::broadcast) {
      toStringSlaveState(out);
      return;
    }
    slave_.toHexString(out);
  }

  std::string toStringSlave() const {
    std::string res;
    res.reserve(slave_.size() * 2 + 32);  // Estimate
    toStringSlave(res);
    return res;
  }

  void toStringMasterState(std::string& out) const {
    if (master_.size() > 0) {
      out += '\'';
      master_.toHexString(out);
      out += "' ";
    }
    out += "master ";
    out += ebus::toString(master_state_);
  }

  void toStringSlaveState(std::string& out) const {
    if (slave_.size() > 0) {
      out += '\'';
      slave_.toHexString(out);
      out += "' ";
    }
    out += "slave ";
    out += ebus::toString(slave_state_);
  }
  static TelegramType typeOf(uint8_t byte) {
    if (byte == Symbols::broad)
      return TelegramType::broadcast;
    else if (isMaster(byte))
      return TelegramType::master_master;
    else
      return TelegramType::master_slave;
  }

  template <size_t C>
  static ebus::SequenceState checkMasterSequence(
      const SequenceImpl<C>& sequence, size_t offset = 0) {
    // sequence is too short
    if (sequence.size() < offset + master_header_size)
      return SequenceState::err_seq_too_short;

    // source address is invalid
    if (!isMaster(sequence[offset + master_source_idx]))
      return SequenceState::err_source_address;

    // target address is invalid
    if (!isTarget(sequence[offset + master_target_idx]))
      return SequenceState::err_target_address;

    // data byte is invalid
    if (uint8_t(sequence[offset + master_nn_idx]) >
        SequenceLimits::max_data_bytes)
      return SequenceState::err_data_byte;

    // sequence is too short (incl. CRC)
    if (sequence.size() <
        static_cast<size_t>(master_header_size +
                            uint8_t(sequence[offset + master_nn_idx]) +
                            crc_size))
      return SequenceState::err_seq_too_short;

    return SequenceState::seq_ok;
  }

  template <size_t C>
  static ebus::SequenceState checkSlaveSequence(const SequenceImpl<C>& sequence,
                                                size_t offset = 0) {
    // sequence is too short
    if (sequence.size() < offset + slave_header_size)
      return SequenceState::err_seq_too_short;

    // data byte is invalid
    if (uint8_t(sequence[offset + slave_nn_idx]) >
        SequenceLimits::max_data_bytes)
      return SequenceState::err_data_byte;

    // sequence is too short (incl. CRC)
    if (sequence.size() <
        static_cast<size_t>(offset + slave_header_size +
                            uint8_t(sequence[offset + slave_nn_idx]) +
                            crc_size))
      return SequenceState::err_seq_too_short;

    return SequenceState::seq_ok;
  }

 private:
  TelegramType telegram_type_ = TelegramType::undefined;

  SequenceImpl<kInlineCapacity> master_;
  size_t master_nn_ = 0;
  uint8_t master_crc_ = Symbols::zero;
  uint8_t master_ack_ = Symbols::zero;
  SequenceState master_state_ = SequenceState::seq_empty;

  SequenceImpl<kInlineCapacity> slave_;
  size_t slave_nn_ = 0;
  uint8_t slave_crc_ = Symbols::zero;
  uint8_t slave_ack_ = Symbols::zero;
  SequenceState slave_state_ = SequenceState::seq_empty;

  template <size_t C>
  bool parseSequencePart(const SequenceImpl<C>& sequence, size_t& offset,
                         bool is_master) {
    if (is_master) {
      master_state_ = checkMasterSequence(sequence, offset);
      if (master_state_ != SequenceState::seq_ok) return false;

      size_t len = master_header_size +
                   static_cast<size_t>(sequence[offset + master_nn_idx]) +
                   crc_size;
      master_.assignSlice(sequence, offset, len);
      createMaster(master_);
      offset += len;

      if (master_state_ != SequenceState::seq_ok)
        return false;  // Master part invalid
      if (telegram_type_ != TelegramType::broadcast) {
        if (sequence.size() <= offset) {
          master_state_ = SequenceState::err_ack_missing;
          return false;
        }
        master_ack_ = sequence[offset++];
        if (master_ack_ != Symbols::ack && master_ack_ != Symbols::nak) {
          master_state_ = SequenceState::err_ack_invalid;
          return false;
        }
      }
    } else {
      slave_state_ = checkSlaveSequence(sequence, offset);
      if (slave_state_ != SequenceState::seq_ok) return false;

      size_t len = slave_header_size +
                   static_cast<size_t>(sequence[offset + slave_nn_idx]) +
                   crc_size;
      slave_.assignSlice(sequence, offset, len);
      createSlave(slave_);
      offset += len;

      if (slave_state_ != SequenceState::seq_ok)
        return false;  // Slave part invalid
      if (sequence.size() <= offset) {
        slave_state_ = SequenceState::err_ack_missing;
        return false;
      }
      slave_ack_ = sequence[offset++];
      if (slave_ack_ != Symbols::ack && slave_ack_ != Symbols::nak) {
        slave_state_ = SequenceState::err_ack_invalid;
        return false;
      }
    }
    return true;
  }
};

/**
 * Default eBUS telegram with 64-byte SBO buffers.
 */
using Telegram = TelegramImpl<SequenceLimits::default_capacity>;

/**
 * Factory function to create and parse a telegram from a raw ByteView.
 */
template <size_t N = SequenceLimits::default_capacity>
TelegramImpl<N> makeTelegram(ByteView data) {
  SequenceImpl<N> seq;
  seq.assign(data, true);  // Assume extended wire format
  TelegramImpl<N> tel;
  tel.parse(seq);
  return tel;
}

}  // namespace ebus::detail
