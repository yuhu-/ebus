/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebuscpp.
 *
 * ebuscpp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebuscpp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebuscpp. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBLOGGER_LOGGER_H
#define LIBLOGGER_LOGGER_H

#include "LogHandler.h"

namespace liblogger
{

class Logger
{

public:
	explicit Logger(const string& function);

	void start(const string& level, const string& file = "");

	void setLevel(const string& level);

	template<typename Data, typename ... Args>
	void error(Data data, Args ... args)
	{
		log(Level::error, data, args...);
	}

	template<typename Data, typename ... Args>
	void warn(Data data, Args ... args)
	{
		log(Level::warn, data, args...);
	}

	template<typename Data, typename ... Args>
	void info(Data data, Args ... args)
	{
		log(Level::info, data, args...);
	}

	template<typename Data, typename ... Args>
	void debug(Data data, Args ... args)
	{
		log(Level::debug, data, args...);
	}

	template<typename Data, typename ... Args>
	void trace(Data data, Args ... args)
	{
		log(Level::trace, data, args...);
	}

private:
	LogHandler& m_logHandler;
	const string m_function;

	void addConsole();
	void addFile(const string& file);

	void log(const Level level, const string data, ...);

};

} // namespace liblogger

inline const string getClassMethod(const string& prettyFunction)
{
	size_t end = prettyFunction.find("(");
	size_t begin = prettyFunction.substr(0, end).rfind(" ") + 1;

	return (prettyFunction.substr(begin, end - begin));
}

#define __CLASS_METHOD__ getClassMethod(__PRETTY_FUNCTION__)

#define LIBLOGGER_CONSOLE(level) liblogger::Logger(__CLASS_METHOD__).start(level)
#define LIBLOGGER_FILE(level, file) liblogger::Logger(__CLASS_METHOD__).start(level, file)

#define LIBLOGGER_LEVEL(level) liblogger::Logger(__CLASS_METHOD__).setLevel(level)

#define LIBLOGGER_ERROR(data, ...) liblogger::Logger(__CLASS_METHOD__).error(data, ##__VA_ARGS__)
#define LIBLOGGER_WARN(data, ...) liblogger::Logger(__CLASS_METHOD__).warn(data, ##__VA_ARGS__)
#define LIBLOGGER_INFO(data, ...) liblogger::Logger(__CLASS_METHOD__).info(data, ##__VA_ARGS__)
#define LIBLOGGER_DEBUG(data, ...) liblogger::Logger(__CLASS_METHOD__).debug(data, ##__VA_ARGS__)
#define LIBLOGGER_TRACE(data, ...) liblogger::Logger(__CLASS_METHOD__).trace(data, ##__VA_ARGS__)

#endif // LIBLOGGER_LOGGER_H
