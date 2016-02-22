/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
 *
 * This file is part of ebusgate.
 *
 * ebusgate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusgate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusgate. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBUTILS_OPTION_H
#define LIBUTILS_OPTION_H

#include <string>
#include <cstring>
#include <map>
#include <vector>

using std::string;
using std::vector;
using std::map;

enum DataType
{
	dt_none,    // default for __text_only__
	dt_bool,    // boolean
	dt_hex,     // hex integer
	dt_int,     // dec integer
	dt_long,    // long
	dt_float,   // float
	dt_string   // string
};

enum OptionType
{
	ot_none, ot_optional, ot_mandatory
};

typedef struct
{
	string name;
	string shortname;
	string description;
	DataType datatype;
	OptionType optiontype;
} opt_t;

union OptVal
{
	bool b;
	int i;
	long l;
	float f;
	const char* c;

	OptVal()
	{
		memset(this, 0, sizeof(OptVal));
	}

	OptVal(bool _b)
		: b(_b)
	{
	}
	OptVal(int _i)
		: i(_i)
	{
	}
	OptVal(long _l)
		: l(_l)
	{
	}
	OptVal(float _f)
		: f(_f)
	{
	}
	OptVal(const char* _c)
		: c(_c)
	{
	}
};

class Option
{

public:
	static Option& getOption(const string& command = "", const string& argument = "");

	~Option();

	void setVersion(const string& version);

	void addText(const string& text);

	void addOption(const string& name, const string& shortname, const OptVal& optval, const DataType& datatype,
		const OptionType& optiontype, const string& description);

	template<typename T>
	T getOptVal(const string& name)
	{
		ov_it = m_optvals.find(name);
		return (reinterpret_cast<T&>(ov_it->second));
	}

	bool parseArgs(int argc, char* argv[]);

	int numArgs() const;

	string getArg(const int& num) const;

	string getCommand() const;

	bool missingCommand() const;

private:
	Option(const string& command, const string& argument);
	Option(const Option&);
	Option& operator=(const Option&);

	vector<opt_t> m_opts;

	vector<opt_t>::const_iterator o_it;

	map<const string, OptVal> m_optvals;

	map<const string, OptVal>::iterator ov_it;

	vector<string> m_argv;

	string m_version;
	string m_withCommand;
	string m_withArgument;
	string m_command;

	vector<string> m_arguments;

	bool checkOption(const string& option, const string& value);
	void setOptVal(const string& option, const string value, DataType datatype);

	bool toStringVersion() const;

	bool toStringHelp();

	bool toStringOptions();
};

#endif // LIBUTILS_OPTION_H
