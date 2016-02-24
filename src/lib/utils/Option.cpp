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

#include "Option.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>

using std::cerr;
using std::endl;
using std::hex;
using std::setw;
using std::setfill;
using std::dec;

Option& Option::getOption(const string& command, const string& argument)
{
	static Option option(command, argument);
	return (option);
}

Option::~Option()
{
	m_options.clear();
	m_bools.clear();
	m_ints.clear();
	m_longs.clear();
	m_floats.clear();
	m_strings.clear();
}

void Option::setVersion(const string& version)
{
	m_version = version;
}

void Option::addText(const string& text)
{
	option_t opt;
	opt.name = "__text_only__";
	opt.shortname = "";
	opt.datatype = dt_none;
	opt.optiontype = ot_none;
	opt.description = text;
	m_options.push_back(opt);
}

void Option::addBool(const string& name, const string& shortname, const bool& data, const OptionType& optiontype,
	const string& description)
{
	if (name.size() != 0)
	{
		m_bools[name] = data;
		addOption(name, shortname, dt_bool, optiontype, description);
	}

}

void Option::addHex(const string& name, const string& shortname, const int& data, const OptionType& optiontype,
	const string& description)
{
	if (name.size() != 0)
	{
		m_ints[name] = data;
		addOption(name, shortname, dt_hex, optiontype, description);
	}

}

void Option::addInt(const string& name, const string& shortname, const int& data, const OptionType& optiontype,
	const string& description)
{
	if (name.size() != 0)
	{
		m_ints[name] = data;
		addOption(name, shortname, dt_int, optiontype, description);
	}

}

void Option::addLong(const string& name, const string& shortname, const long& data, const OptionType& optiontype,
	const string& description)
{
	if (name.size() != 0)
	{
		m_longs[name] = data;
		addOption(name, shortname, dt_long, optiontype, description);
	}

}

void Option::addFloat(const string& name, const string& shortname, const float& data, const OptionType& optiontype,
	const string& description)
{
	if (name.size() != 0)
	{
		m_floats[name] = data;
		addOption(name, shortname, dt_float, optiontype, description);
	}

}

void Option::addString(const string& name, const string& shortname, const string& data, const OptionType& optiontype,
	const string& description)
{
	if (name.size() != 0)
	{
		m_strings[name] = data;
		addOption(name, shortname, dt_string, optiontype, description);
	}

}

bool Option::parseArgs(int argc, char* argv[])
{
	vector<string> _argv(argv, argv + argc);
	m_argv = _argv;
	int i;
	bool lastOption = false;

	// walk through all arguments
	for (i = 1; i < argc; i++)
	{
		// find option with long format '--'
		if (_argv[i].rfind("--") == 0 && _argv[i].size() > 2)
		{
			// is next item an added argument?
			if (i + 1 < argc && _argv[i + 1].rfind("-", 0) == string::npos)
			{
				if (checkOption(_argv[i].substr(2), _argv[i + 1]) == false) return (false);
			}
			else
			{
				if (checkOption(_argv[i].substr(2), "") == false) return (false);
			}

			lastOption = true;
		}
		// find option with short format '-'
		else if (_argv[i].rfind("-") == 0 && _argv[i].size() > 1)
		{
			// walk through all characters
			for (size_t j = 1; j < _argv[i].size(); j++)
			{
				// only last charater could have an argument
				if (i + 1 < argc && _argv[i + 1].rfind("-", 0) == string::npos
					&& j + 1 == _argv[i].size())
				{
					if (checkOption(_argv[i].substr(j, 1), _argv[i + 1]) == false) return (false);
				}
				else
				{
					if (checkOption(_argv[i].substr(j, 1), "") == false) return (false);
				}
			}

			lastOption = true;
		}
		else
		{
			// break loop with command
			if (lastOption == false && m_withCommand.size() != 0)
				break;
			else
				lastOption = false;

		}
	}

	if (i < argc && m_withCommand.size() != 0)
	{
		// save command
		m_command = _argv[i];

		if (m_withArgument.size() != 0)
		{
			// save args of command
			for (++i; i < argc; i++)
				m_arguments.push_back(_argv[i]);
		}
	}

	return (true);
}

int Option::numArgs() const
{
	return (m_arguments.size());
}

string Option::getArg(const int& num) const
{
	return (m_arguments[num]);
}

string Option::getCommand() const
{
	return (m_command);
}

bool Option::missingCommand() const
{
	return (m_command.size() == 0 ? true : false);
}

bool Option::getBool(const string& name)
{
	return (m_bools.find(name)->second);
}

int Option::getInt(const string& name)
{
	return (m_ints.find(name)->second);
}

long Option::getLong(const string& name)
{
	return (m_longs.find(name)->second);
}

float Option::getFloat(const string& name)
{
	return (m_floats.find(name)->second);
}

string Option::getString(const string& name)
{
	return (m_strings.find(name)->second);
}

Option::Option(const string& command, const string& argument)
	: m_withCommand(command), m_withArgument(argument)
{
}

void Option::addOption(const string& name, const string& shortname, const DataType& datatype,
	const OptionType& optiontype, const string& description)
{
	option_t opt;
	opt.name = name;
	opt.shortname = shortname;
	opt.datatype = datatype;
	opt.optiontype = optiontype;
	opt.description = description;
	m_options.push_back(opt);
}

void Option::setOptVal(const string& name, const string data, DataType datatype)
{
	switch (datatype)
	{
	case dt_bool:
		m_bools[name] = true;
		break;
	case dt_hex:
		m_ints[name] = strtol(data.c_str(), nullptr, 16);
		break;
	case dt_int:
		m_ints[name] = strtol(data.c_str(), nullptr, 10);
		break;
	case dt_long:
		m_longs[name] = strtol(data.c_str(), nullptr, 10);
		break;
	case dt_float:
		m_floats[name] = static_cast<float>(strtod(data.c_str(), nullptr));
		break;
	case dt_string:
		m_strings[name] = data;
		break;
	default:
		break;
	}
}

bool Option::checkOption(const string& name, const string& data)
{
	if (strcmp(name.c_str(), "options") == 0) return (toStringOptions());

	if (strcmp(name.c_str(), "version") == 0) return (toStringVersion());

	if (strcmp(name.c_str(), "h") == 0 || strcmp(name.c_str(), "help") == 0) return (toStringHelp());

	for (option_t option : m_options)
	{
		if (option.shortname == name || option.name == name)
		{
			// need this option and argument?
			if (option.optiontype == ot_mandatory && data.size() == 0)
			{
				cerr << endl << "option requires an argument '" << name << "'" << endl;
				return (toStringHelp());
			}

			// add given value to option
			if ((option.optiontype == ot_optional && data.size() != 0) || option.optiontype != ot_optional)
				setOptVal(option.name, data, option.datatype);

			return (true);
		}
	}

	cerr << endl << "unknown option '" << name << "'" << endl;
	return (toStringHelp());
}

bool Option::toStringVersion() const
{
	cerr << m_version << endl;

	return (false);
}

bool Option::toStringHelp()
{
	cerr << endl << "Usage:" << endl << "  " << m_argv[0].substr(m_argv[0].find_last_of("/\\") + 1)
		<< " [Options...]";

	if (m_withCommand.size() != 0)
	{
		if (m_withArgument.size() != 0)
			cerr << " " << m_withCommand << " " << m_withArgument << endl << endl;
		else
			cerr << " " << m_withCommand << endl << endl;
	}
	else
	{
		cerr << endl << endl;
	}

	for (option_t option : m_options)
	{
		if (strcmp(option.name.c_str(), "__text_only__") == 0)
		{
			cerr << option.description << endl;
		}
		else
		{
			const string c = (option.shortname.size() == 1) ? option.shortname.c_str() : " ";
			cerr << ((c == " ") ? " " : "-") << c << " | --" << option.name << "\t" << option.description
				<< endl;
		}
	}

	cerr << endl << "   | --options\n   | --version\n-h | --help" << endl << endl;

	return (false);
}

bool Option::toStringOptions()
{
	cerr << endl << "Options:" << endl << endl;

	for (option_t option : m_options)
	{
		if (strcmp(option.name.c_str(), "__text_only__") == 0) continue;

		const string c = (option.shortname.size() == 1) ? option.shortname.c_str() : " ";
		cerr << ((c == " ") ? " " : "-") << c << " | --" << option.name << " = ";

		if (option.datatype == dt_bool)
		{
			if (getBool(option.name) == true)
				cerr << "yes" << endl;
			else
				cerr << "no" << endl;
		}
		else if (option.datatype == dt_hex)
		{
			cerr << "0x" << hex << setw(2) << setfill('0') << getInt(option.name) << dec << endl;
		}
		else if (option.datatype == dt_int)
		{
			cerr << getInt(option.name) << endl;
		}
		else if (option.datatype == dt_long)
		{
			cerr << getLong(option.name) << endl;
		}
		else if (option.datatype == dt_float)
		{
			cerr << getFloat(option.name) << endl;
		}
		else if (option.datatype == dt_string)
		{
			cerr << getString(option.name) << endl;
		}
	}

	cerr << endl;

	return (false);
}
