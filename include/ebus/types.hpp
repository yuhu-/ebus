/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include <esp_timer.h>  // For esp_timer_get_time()
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ebus/address.hpp"
#include "ebus/detail/protocol_limits.hpp"

namespace ebus {

/**
 * @brief Custom monotonic clock for ESP-IDF that uses esp_timer_get_time().
 * This avoids potential issues with std::chrono::steady_clock's underlying
 * implementation on ESP-IDF, especially when converting to absolute times
 * for condition variables, which can sometimes lead to EINVAL if the system
 * clock is not stable or has rollover issues.
 *
 * It meets the requirements of a C++17 `std::chrono::Clock`.
 */
struct EbusSteadyClock {
  using rep = long long;
  using period = std::micro;
  using duration = std::chrono::duration<rep, period>;
  using time_point = std::chrono::time_point<EbusSteadyClock>;
  static constexpr bool is_steady = true;

  static time_point now() noexcept {
#if defined(ESP_PLATFORM)
    return time_point(duration(esp_timer_get_time()));
#else
    return time_point(std::chrono::duration_cast<duration>(
        std::chrono::steady_clock::now().time_since_epoch()));
#endif
  }
};

using Clock = EbusSteadyClock;

/**
 * @brief Callback for streaming JSON chunks.
 */
using JsonChunkVisitor = std::function<void(std::string_view)>;

namespace detail {
class JsonWriter;  // Forward declaration

/**
 * SFINAE helper to detect if a type has a toJson method.
 */
template <typename T, typename = void>
struct has_to_json : std::false_type {};

template <typename T>
struct has_to_json<T, std::void_t<decltype(std::declval<T>().toJson(
                          std::declval<JsonWriter&>()))>> : std::true_type {};

/**
 * SFINAE helper to detect a contiguous byte range (vector, array, ByteView).
 */
template <typename T, typename = void>
struct is_byte_range : std::false_type {};

template <typename T>
struct is_byte_range<T, std::void_t<decltype(std::data(std::declval<T&>())),
                                    decltype(std::size(std::declval<T&>()))>>
    : std::is_same<std::decay_t<decltype(*std::data(std::declval<T&>()))>,
                   uint8_t> {};

/**
 * SFINAE helper to detect an indexable byte container (e.g. CircularBuffer).
 * Unlike is_byte_range, this does not require contiguous storage (data()).
 */
template <typename T, typename = void>
struct is_byte_indexable : std::false_type {};

template <typename T>
struct is_byte_indexable<T,
                         std::void_t<decltype(std::declval<T>()[0]),
                                     decltype(std::size(std::declval<T&>()))>>
    : std::is_same<std::decay_t<decltype(std::declval<T>()[0])>, uint8_t> {};
}  // namespace detail

// --- Protocol Enums ---

enum class LogLevel : uint8_t { none, error, info, debug };

enum class MessageType : uint8_t { undefined, active, passive, reactive };

enum class SequenceState : uint8_t {
  seq_empty,
  seq_ok,
  err_seq_too_short,
  err_seq_too_long,
  err_source_address,
  err_target_address,
  err_data_byte,
  err_crc_invalid,
  err_ack_invalid,
  err_ack_missing,
  err_ack_negative
};

enum class HandlerState : uint8_t {
  passive_receive_master,
  passive_receive_master_acknowledge,
  passive_receive_slave,
  passive_receive_slave_acknowledge,
  reactive_send_master_positive_acknowledge,
  reactive_send_master_negative_acknowledge,
  reactive_send_slave,
  reactive_receive_slave_acknowledge,
  request_bus,
  active_send_master,
  active_receive_master_acknowledge,
  active_receive_slave,
  active_send_slave_positive_acknowledge,
  active_send_slave_negative_acknowledge,
  release_bus
};

enum class RequestState : uint8_t { observe, first, retry, second };

enum class RequestResult : uint8_t {
  observe_syn,
  observe_data,
  first_syn,
  first_won,
  first_retry,
  first_lost,
  first_error,
  retry_syn,
  retry_error,
  second_won,
  second_lost,
  second_error
};

enum class ProtocolError : uint8_t {
  none,
  error_passive_master,
  error_passive_master_ack,
  error_passive_slave,
  error_passive_slave_ack,
  error_reactive_master,
  error_reactive_master_ack,
  error_reactive_slave,
  error_reactive_slave_ack,
  error_active_master_echo,
  error_active_master,
  error_active_master_ack,
  error_active_slave,
  error_active_slave_ack,
  check_passive_buffers,
  check_active_buffers,
  illegal_fsm_transition,
  handler_busy,
  invalid_message,
  fsm_timeout,
  arbitration_lost,
  total_transfer_timeout,
};

/**
 * Available client types for the network bridge.
 */
enum class ClientType : uint8_t { read_only, regular, enhanced };

enum class SessionState : uint8_t {
  idle,      // Waiting for a client to have data
  request,   // Bus request pending, waiting for our slot to send
  response,  // Waiting for arbitration result from eBUS
  transmit   // Arbitration won, sending telegram body
};

// --- String Conversion ---

const char* toString(LogLevel level) noexcept;
const char* toString(MessageType type) noexcept;
const char* toString(SequenceState state) noexcept;
const char* toString(HandlerState state) noexcept;
const char* toString(RequestState state) noexcept;
const char* toString(RequestResult state) noexcept;
const char* toString(ProtocolError error) noexcept;
const char* toString(ClientType type) noexcept;
const char* toString(SessionState state) noexcept;

// --- Struct ---

/**
 * A lightweight, non-owning view of a byte sequence.
 * Similar to std::string_view but for uint8_t.
 */
struct ByteView {
  constexpr ByteView() = default;
  constexpr ByteView(const uint8_t* data, size_t size)
      : data_(data), size_(size) {}

  // cppcheck-suppress noExplicitConstructor
  ByteView(const std::vector<uint8_t>& v) : data_(v.data()), size_(v.size()) {}

  constexpr const uint8_t* begin() const noexcept { return data_; }
  constexpr const uint8_t* end() const noexcept { return data_ + size_; }

  constexpr const uint8_t* data() const noexcept { return data_; }

  constexpr size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr uint8_t operator[](size_t i) const { return data_[i]; }

  bool operator==(ByteView other) const {
    if (this == &other) return true;
    if (size_ != other.size_) return false;
    return size_ == 0 || std::memcmp(data_, other.data_, size_) == 0;
  }
  bool operator!=(ByteView other) const { return !(*this == other); }

 private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
};

// Ensure definitions.hpp remains lean and free of heavy template logic
static_assert(std::is_standard_layout_v<ByteView>,
              "ByteView must maintain standard layout for ABI compatibility.");

static_assert(std::is_trivially_copyable_v<ByteView>,
              "ByteView must be trivially copyable to remain heap-free in the "
              "hot path.");

/**
 * A trivially copyable, fixed-capacity string.
 * Prevents heap allocations during status updates and orchestration.
 */
template <size_t Cap>
struct FixedString {
  char buffer[Cap]{};
  uint8_t size_bytes = 0;

  FixedString() = default;
  explicit FixedString(std::string_view s) { assign(s); }

  void assign(std::string_view s) {
    size_bytes = static_cast<uint8_t>((s.size() < Cap) ? s.size() : Cap - 1);
    if (size_bytes > 0) std::memcpy(buffer, s.data(), size_bytes);
    buffer[size_bytes] = '\0';
  }

  const char* c_str() const noexcept { return buffer; }
  size_t size() const noexcept { return size_bytes; }
  bool empty() const noexcept { return size_bytes == 0; }

  FixedString& operator=(std::string_view s) {
    assign(s);
    return *this;
  }

  operator std::string_view() const noexcept {
    return std::string_view(buffer, size_bytes);
  }
};

/**
 * A trivially copyable, owning byte sequence for use in bitwise-copy queues.
 */
template <size_t Cap>
struct StaticSequence {
  uint8_t buffer[Cap]{};
  uint8_t size_bytes = 0;

  void assign(const uint8_t* data, size_t len) {
    size_bytes = static_cast<uint8_t>((len < Cap) ? len : Cap);
    if (size_bytes > 0) std::memcpy(buffer, data, size_bytes);
  }

  uint8_t* begin() noexcept { return buffer; }
  const uint8_t* begin() const noexcept { return buffer; }
  uint8_t* end() noexcept { return buffer + size_bytes; }
  const uint8_t* end() const noexcept { return buffer + size_bytes; }

  uint8_t* data() noexcept { return buffer; }
  const uint8_t* data() const noexcept { return buffer; }

  size_t size() const noexcept { return size_bytes; }
  static constexpr size_t capacity() noexcept { return Cap; }
  void clear() noexcept { size_bytes = 0; }
  bool empty() const { return size_bytes == 0; }

  uint8_t& operator[](size_t i) { return buffer[i]; }
  const uint8_t& operator[](size_t i) const { return buffer[i]; }

  template <size_t N>
  StaticSequence& operator=(const char (&str)[N]) {
    assign(reinterpret_cast<const uint8_t*>(str),
           N > 0 && str[N - 1] == '\0' ? N - 1 : N);
    return *this;
  }

  /**
   * Implicit conversion to ByteView for zero-copy interoperability.
   */
  operator ByteView() const { return ByteView(buffer, size_bytes); }
};

/**
 * Internal carrier for protocol results and decoupled public callbacks.
 */
struct ProtocolEvent {
  enum class Type : uint8_t { won, lost, telegram, error } type;

  // Shared metadata
  uint32_t session_id;
  uint32_t poll_id;
  uint32_t retry_count;
  HandlerState handler_state;
  RequestState request_state;

  union {
    struct {
      MessageType message_type;
      TelegramType telegram_type;
    } tel;
    struct {
      ProtocolError protocol_error;
      RequestResult result;
      SequenceState sequence_state;
      LogLevel level;
    } err;
  } data;

  // Optimization for ESP32-C3: Reduced buffer size for internal event
  // passing. Logical eBUS telegrams are max 21 bytes (master) / 17 bytes
  // (slave).
  StaticSequence<detail::SequenceLimits::model_capacity> master;
  StaticSequence<detail::SequenceLimits::model_capacity> slave;
};

static_assert(std::is_trivially_copyable_v<ProtocolEvent>,
              "ProtocolEvent must be trivially copyable for Reactor Queue.");
static_assert(
    sizeof(ProtocolEvent) <= 192,
    "ProtocolEvent exceeds the memory threshold for constrained targets. "
    "Verify enum packing and buffer sizes.");

/**
 * Internal event types for the Unified Reactor loop.
 */
enum class OrchestrationEventType : uint8_t {
  bus_byte,         // Byte received from the physical bus (ISR/Low-level task)
  network_byte,     // Byte received from an external client (ebusd bridge)
  user_request,     // New message enqueued by the application
  protocol_result,  // Signal from Handler (Won/Lost/Telegram/Error)
  timer_wakeup,     // Triggered when a Poll or Scheduler retry is due
  callback_ready,   // Signal that a user callback is pending dispatch
  client_io_ready,  // A client has I/O readiness (read or write)

  shutdown  // Signal to terminate the worker thread
};

/**
 * Trivially copyable carrier for all orchestration signals.
 * Sized to fit comfortably in a FreeRTOS queue.
 */
struct OrchestrationEvent {
  OrchestrationEvent()
      : type(OrchestrationEventType::shutdown),
        timestamp(Clock::now()),
        session_id(0) {
    std::memset(static_cast<void*>(&data), 0, sizeof(data));
  }

  OrchestrationEventType type;

  // Shared metadata
  Clock::time_point timestamp;
  uint32_t session_id = 0;

  union Data {
    /**
     * Explicitly define default constructor to do nothing.
     * Required because request_data contains StaticSequence, which has
     * default member initializers making it non-trivial for unions.
     */
    Data() {}

    struct {
      uint8_t val;
      bool bus_request;
      bool start_bit;
      Clock::time_point timestamp;
    } byte_data;
    struct {
      uint8_t priority;
      uint32_t poll_id;
      StaticSequence<detail::SequenceLimits::model_capacity> payload;
    } request_data;
    struct {
      int client_fd;
      uint16_t events;
    } client_io_data;

    ProtocolEvent protocol_data;
  } data;
};

/**
 * Records a single state transition in the protocol handler.
 */
struct HandlerTransition {
  HandlerTransition() = default;
  HandlerTransition(HandlerState from_state, HandlerState to_state, uint64_t ts)
      : from(from_state), to(to_state), timestamp(ts) {}

  HandlerState from = HandlerState::passive_receive_master;
  HandlerState to = HandlerState::passive_receive_master;
  uint64_t timestamp = 0;  // ms since epoch

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Records a single state transition in the arbitration engine.
 */
struct RequestTransition {
  RequestTransition() = default;
  RequestTransition(RequestState from_state, RequestState to_state, uint64_t ts)
      : from(from_state), to(to_state), timestamp(ts) {}

  RequestState from = RequestState::observe;
  RequestState to = RequestState::observe;
  uint64_t timestamp = 0;  // ms since epoch

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Persistent entry for the diagnostic error log.
 * Uses fixed-size buffers to ensure zero heap allocation during logging.
 */
struct ErrorEntry {
  ErrorEntry() = default;
  ErrorEntry(uint32_t s_id, uint32_t p_id, LogLevel lvl, ProtocolError pe,
             RequestResult res, SequenceState ss, HandlerState hs,
             RequestState rs, uint32_t retries, ByteView m_view,
             ByteView s_view, uint64_t ts)
      : session_id(s_id),
        poll_id(p_id),
        level(lvl),
        protocol_error(pe),
        result(res),
        sequence_state(ss),
        handler_state(hs),
        request_state(rs),
        retry_count(retries),
        timestamp(ts) {
    master.assign(m_view.data(), m_view.size());
    slave.assign(s_view.data(), s_view.size());
  }

  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  LogLevel level = LogLevel::error;
  ProtocolError protocol_error = ProtocolError::none;
  RequestResult result = RequestResult::observe_data;
  SequenceState sequence_state = SequenceState::seq_empty;
  HandlerState handler_state = HandlerState::passive_receive_master;
  RequestState request_state = RequestState::observe;
  uint32_t retry_count = 0;
  StaticSequence<detail::SequenceLimits::model_capacity> master;
  StaticSequence<detail::SequenceLimits::model_capacity> slave;
  uint64_t timestamp = 0;  // ms since epoch

  void toJson(detail::JsonWriter& writer) const;

  /**
   * @brief Appends a human-readable representation to an existing string.
   */
  void toString(std::string& out) const;

  /**
   * @brief Returns a human-readable string representation.
   */
  inline std::string toString() const {
    std::string res;
    res.reserve(128);
    toString(res);
    return res;
  }

  void setMaster(const uint8_t* data, size_t len) { master.assign(data, len); }

  void setSlave(const uint8_t* data, size_t len) { slave.assign(data, len); }
};

static_assert(std::is_trivially_copyable_v<ErrorEntry>,
              "ErrorEntry must be trivially copyable for zero-allocation "
              "logging.");

}  // namespace ebus
