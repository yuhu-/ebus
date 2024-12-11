/*
 * Copyright (C) Roland Jax 2012-2024 <roland.jax@liwest.at>
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

#ifndef INCLUDE_EBUS_EBUS_H_
#define INCLUDE_EBUS_EBUS_H_

#include <cstddef>
#include <experimental/propagate_const>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ebus {

/**
 * logger interface
 */
class ILogger {
 public:
  virtual ~ILogger() = default;

  virtual void error(const std::string &message) = 0;
  virtual void warn(const std::string &message) = 0;
  virtual void info(const std::string &message) = 0;
  virtual void debug(const std::string &message) = 0;
  virtual void trace(const std::string &message) = 0;
};

/**
 * reaction type for active ebus participants only
 */
enum class Reaction {
  nofunction,  // no function
  undefined,   // message undefined
  ignore,      // message ignored
  response     // send response
};

/**
 * ebus communication class
 */
class Ebus {
 public:
  /**
   * create an ebus object
   *
   * @param address - own address byte
   * @param device - serial device string
   */
  Ebus(const uint8_t address, const std::string &device);

  /**
   * move functions
   */
  Ebus &operator=(Ebus &&);
  Ebus(Ebus &&);

  /**
   * copy functions
   */
  Ebus &operator=(const Ebus &) = delete;
  Ebus(const Ebus &) = delete;

  /**
   * destructor
   */
  ~Ebus();

  /**
   * open the ebus device
   */
  void open();

  /**
   * close the ebus device
   */
  void close();

  /**
   * ebus device status
   *
   * @return true, when the ebus device is open
   */
  bool online();

  /**
   * transmit an ebus message
   *
   * @param message to transmit
   * @param response to transmitted message
   *
   * @return error number if an error occurred
   */
  int transmit(const std::vector<uint8_t> &message,
               std::vector<uint8_t> &response);

  /**
   * error description
   *
   * @return string of given error number
   */
  const std::string error_text(const int error) const;

  /**
   * register a logger object for internal logging facilities
   *
   * @param logger object
   */
  void register_logger(std::shared_ptr<ILogger> logger);

  /**
   * register a 'process' reference which is necessary for active ebus
   * participants only
   *
   * @param process callback function
   */
  void register_process(
      std::function<Reaction(const std::vector<uint8_t> &message,
                             std::vector<uint8_t> &response)>
          process);

  /**
   * register a 'publish' reference which is triggered after each successful
   * ebus telegram
   *
   * @param publish callback function
   */
  void register_publish(
      std::function<void(const std::vector<uint8_t> &message,
                         const std::vector<uint8_t> &response)>
          publish);

  /**
   * register a 'rawdata' reference which is triggered after each received byte
   *
   * @param rawdata callback function
   */
  void register_rawdata(std::function<void(const uint8_t &byte)> rawdata);

  /**
   * timeout for bus access
   *
   * @param access_timeout [default: 4400 us]
   */
  void set_access_timeout(const uint16_t &access_timeout);

  /**
   * number of skipped characters after a successful ebus access
   *
   * @param lock_counter_max [default: 5 max: 25]
   */
  void set_lock_counter_max(const uint8_t &lock_counter_max);

  /**
   * number of attempts to open the ebus device (one second pause between two
   * attempts)
   *
   * @param open_counter_max [default: 10]
   */
  void set_open_counter_max(const uint8_t &open_counter_max);

  /**
   * returns a part of given byte vector
   *
   * @param vec - byte vector
   * @param index - start position
   * @param len - length
   *
   * @return byte vector
   */
  static const std::vector<uint8_t> range(const std::vector<uint8_t> &vec,
                                          const size_t index, const size_t len);

  /**
   * convert a given string into a byte vector
   *
   * @param str - string
   *
   * @return byte vector
   */
  static const std::vector<uint8_t> to_vector(const std::string &str);

  /**
   * convert a given byte vector into a string
   *
   * @param vec - byte vector
   *
   * @return string
   */
  static const std::string to_string(const std::vector<uint8_t> &vec);

 private:
  class EbusImpl;
  std::experimental::propagate_const<std::unique_ptr<EbusImpl>> impl;
};

}  // namespace ebus

#endif  // INCLUDE_EBUS_EBUS_H_
