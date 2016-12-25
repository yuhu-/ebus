/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#ifndef LIBUTILS_OPTION_H
#define LIBUTILS_OPTION_H

#include <string>
#include <cstring>
#include <map>
#include <vector>

using std::string;
using std::vector;
using std::map;

namespace libutils
{

class Options
{

	enum class Type
	{
		t_none,    // not defined
		t_text,    // default for __text_only__
		t_bool,    // boolean
		t_hex,     // hex integer
		t_int,     // dec integer
		t_long,    // long
		t_float,   // float
		t_string   // string
	};

	struct Option
	{
		string name;
		string shortname;
		string description;
		Type type;
	};

public:
	static Options& getOption(const string& command = "", const string& argument = "");

	~Options();

	void setVersion(const string& version);

	void addDescription(const string& description);

	void addText(const string& text);

	void addBool(const string& name, const string& shortname, const bool value, const string& description);

	void addHex(const string& name, const string& shortname, const int value, const string& description);

	void addInt(const string& name, const string& shortname, const int value, const string& description);

	void addLong(const string& name, const string& shortname, const long value, const string& description);

	void addFloat(const string& name, const string& shortname, const float value, const string& description);

	void addString(const string& name, const string& shortname, const string& value, const string& description);

	bool parse(int argc, char* argv[]);

	int numArgs() const;

	string getArg(const int num) const;

	string getCommand() const;

	bool missingCommand() const;

	bool getBool(const string& name);

	int getInt(const string& name);

	long getLong(const string& name);

	float getFloat(const string& name);

	string getString(const string& name);

	bool toStringVersion() const;

	bool toStringHelp();

	bool toStringUsage();

	bool toStringDescription() const;

	bool toStringOptions();

	bool toStringValues();

private:
	Options(const string& command, const string& argument);
	Options(const Options&);
	Options& operator=(const Options&);

	vector<Option> m_options;

	map<const string, bool> m_bools;
	map<const string, int> m_ints;
	map<const string, long> m_longs;
	map<const string, float> m_floats;
	map<const string, string> m_strings;

	vector<string> m_argv;

	string m_version;
	string m_description;
	string m_withCommand;
	string m_withArguments;
	string m_command;
	vector<string> m_arguments;

	void add(const string& name, const string& shortname, const string& description, const Type type);

	int find(const string& name, const bool shortname);

	void save(const int index, const string& value);
};

} // namespace libutils

#endif // LIBUTILS_OPTION_H
