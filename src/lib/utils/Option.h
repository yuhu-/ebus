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

enum OptionType
{
	ot_none, ot_optional, ot_mandatory
};

class Option
{

private:
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

	typedef struct
	{
		string name;
		string shortname;
		string description;
		DataType datatype;
		OptionType optiontype;
	} option_t;

public:
	static Option& getOption(const string& command = "", const string& argument = "");

	~Option();

	void setVersion(const string& version);

	void addText(const string& text);

	void addBool(const string& name, const string& shortname, const bool& data, const OptionType& optiontype,
		const string& description);

	void addHex(const string& name, const string& shortname, const int& data, const OptionType& optiontype,
		const string& description);

	void addInt(const string& name, const string& shortname, const int& data, const OptionType& optiontype,
		const string& description);

	void addLong(const string& name, const string& shortname, const long& data, const OptionType& optiontype,
		const string& description);

	void addFloat(const string& name, const string& shortname, const float& data, const OptionType& optiontype,
		const string& description);

	void addString(const string& name, const string& shortname, const string& data, const OptionType& optiontype,
		const string& description);

	bool parseArgs(int argc, char* argv[]);

	int numArgs() const;

	string getArg(const int& num) const;

	string getCommand() const;

	bool missingCommand() const;

	bool getBool(const string& name);

	int getInt(const string& name);

	long getLong(const string& name);

	float getFloat(const string& name);

	string getString(const string& name);

private:
	Option(const string& command, const string& argument);
	Option(const Option&);
	Option& operator=(const Option&);

	vector<option_t> m_options;

	map<const string, bool> m_bools;
	map<const string, int> m_ints;
	map<const string, long> m_longs;
	map<const string, float> m_floats;
	map<const string, string> m_strings;

	vector<string> m_argv;

	string m_version;
	string m_withCommand;
	string m_withArgument;
	string m_command;

	vector<string> m_arguments;

	void addOption(const string& name, const string& shortname, const DataType& datatype,
		const OptionType& optiontype, const string& description);

	void setOptVal(const string& option, const string value, DataType datatype);

	bool checkOption(const string& option, const string& value);

	bool toStringVersion() const;

	bool toStringHelp();

	bool toStringOptions();
};

#endif // LIBUTILS_OPTION_H
