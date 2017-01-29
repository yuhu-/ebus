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

#include "Options.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>

using std::cerr;
using std::endl;
using std::hex;
using std::setw;
using std::setfill;
using std::left;
using std::dec;

size_t libutils::Options::m_maxNameLength = 0;

libutils::Options& libutils::Options::getOption(const string& command, const string& argument)
{
	static Options option(command, argument);
	return (option);
}

libutils::Options::~Options()
{
	m_options.clear();
	m_bools.clear();
	m_ints.clear();
	m_longs.clear();
	m_floats.clear();
	m_strings.clear();
}

void libutils::Options::setVersion(const string& version)
{
	m_version = version;
}

void libutils::Options::addDescription(const string& description, const int line)
{
	m_description += description;
	for (int i = 0; i < line + 1; i++)
		m_description += '\n';
}

void libutils::Options::addText(const string& text, const int line)
{
	Option option;

	option.name = "__text_only__";
	option.shortname = "";
	option.type = Type::t_text;
	option.description = text;
	for (int i = 0; i < line + 1; i++)
		option.description += '\n';

	m_options.push_back(option);

	if (option.name.length() > m_maxNameLength) m_maxNameLength = option.name.length();
}

void libutils::Options::addBool(const string& name, const string& shortname, const bool value, const string& description,
	const int line)
{
	if (name.size() != 0)
	{
		m_bools[name] = value;
		add(name, shortname, description, line, Type::t_bool);
	}

}

void libutils::Options::addHex(const string& name, const string& shortname, const int value, const string& description, const int line)
{
	if (name.size() != 0)
	{
		m_ints[name] = value;
		add(name, shortname, description, line, Type::t_hex);
	}

}

void libutils::Options::addInt(const string& name, const string& shortname, const int value, const string& description, const int line)
{
	if (name.size() != 0)
	{
		m_ints[name] = value;
		add(name, shortname, description, line, Type::t_int);
	}

}

void libutils::Options::addLong(const string& name, const string& shortname, const long value, const string& description,
	const int line)
{
	if (name.size() != 0)
	{
		m_longs[name] = value;
		add(name, shortname, description, line, Type::t_long);
	}

}

void libutils::Options::addFloat(const string& name, const string& shortname, const float value, const string& description,
	const int line)
{
	if (name.size() != 0)
	{
		m_floats[name] = value;
		add(name, shortname, description, line, Type::t_float);
	}

}

void libutils::Options::addString(const string& name, const string& shortname, const string& value, const string& description,
	const int line)
{
	if (name.size() != 0)
	{
		m_strings[name] = value;
		add(name, shortname, description, line, Type::t_string);
	}

}

bool libutils::Options::parse(int argc, char* argv[])
{
	vector<string> _argv(argv, argv + argc);
	m_argv = _argv;
	int index;
	int saveIndexLong = -1;
	int saveIndexShort = -1;

	// walk through all arguments
	for (int i = 1; i < argc; i++)
	{
		// wrong prefix
		if (_argv[i].rfind("---") != string::npos)
		{
			cerr << endl << " Error: '" << _argv[i] << "' Only single '-' or double '--' prefix are allowed." << endl
				<< endl;
			return (toStringOptions());
		}

		// missing option
		else if ((_argv[i].rfind("--") == 0 && _argv[i].size() == 2) || (_argv[i].rfind("-") == 0 && _argv[i].size() == 1))
		{
			cerr << endl << " Error: '" << _argv[i] << "' without an option is not allowed." << endl << endl;
			return (toStringOptions());
		}

		// parse long format '--'
		else if (_argv[i].rfind("--") == 0 && _argv[i].size() > 2)
		{
			index = find(_argv[i].substr(2), false);
			if (index >= 0)
			{
				if (m_options[index].type == Type::t_bool)
					save(index, "");
				else
					saveIndexLong = index;
			}
			else
			{
				if (_argv[i].substr(2).compare("values") == 0) return (toStringValues());

				if (_argv[i].substr(2).compare("version") == 0) return (toStringVersion());

				if (_argv[i].substr(2).compare("help") == 0) return (toStringHelp());

				cerr << endl << " Error: Option '" << _argv[i] << "' was not found." << endl << endl;
				return (toStringOptions());
			}
		}

		// parse short format '-'
		else if (_argv[i].rfind("-") == 0 && _argv[i].size() > 1)
		{
			// walk through all characters
			for (size_t j = 1; j < _argv[i].size(); j++)
			{
				index = find(_argv[i].substr(j, 1), true);
				if (index >= 0)
				{
					if (m_options[index].type == Type::t_bool)
					{
						save(index, "");
					}
					else
					{
						if (_argv[i].size() == 2)
						{
							saveIndexShort = index;
						}
						else
						{
							cerr << endl << " Error: '-" << _argv[i].substr(j, 1) << "' in '" << _argv[i]
								<< "' is an option value and need to be in a single statement." << endl
								<< endl;
							return (toStringOptions());
						}
					}
				}
				else
				{
					if (strcmp(_argv[i].substr(j, 1).c_str(), "h") == 0) return (toStringHelp());
					cerr << endl << " Error: Option '-" << _argv[i].substr(j, 1) << "' was not found." << endl
						<< endl;
					return (toStringOptions());
				}
			}
		}

		// parse values, command and arguments
		else
		{
			if (saveIndexLong >= 0)
			{
				save(saveIndexLong, _argv[i]);
				saveIndexLong = -1;

			}
			else if (saveIndexShort >= 0)
			{
				save(saveIndexShort, _argv[i]);
				saveIndexShort = -1;
			}
			else if (m_withCommand.empty() == true)
			{
				cerr << endl << " Error: The given string '" << _argv[i] << "' is not needed." << endl << endl;
				return (toStringUsage());
			}
			else
			{
				m_command = _argv[i++];

				while (i < argc)
					m_arguments.push_back(_argv[i++]);
			}
		}
	}

	if (saveIndexLong >= 0)
	{
		cerr << endl << " Error: Option '--" << m_options[saveIndexLong].name << "' needs a value." << endl << endl;
		return (toStringOptions());
	}
	else if (saveIndexShort >= 0)
	{
		cerr << endl << " Error: Option '-" << m_options[saveIndexShort].shortname << "' needs a value." << endl << endl;
		return (toStringOptions());
	}

	if (m_withCommand.empty() == false && m_command.empty() == true)
	{
		cerr << endl << " Error: " << m_withCommand << " is missing." << endl;
		return (toStringUsage());
	}

	return (true);
}

int libutils::Options::numArgs() const
{
	return (m_arguments.size());
}

string libutils::Options::getArg(const int num) const
{
	return (m_arguments[num]);
}

string libutils::Options::getCommand() const
{
	return (m_command);
}

bool libutils::Options::missingCommand() const
{
	return (m_command.empty());
}

bool libutils::Options::getBool(const string& name)
{
	return (m_bools.find(name)->second);
}

int libutils::Options::getInt(const string& name)
{
	return (m_ints.find(name)->second);
}

long libutils::Options::getLong(const string& name)
{
	return (m_longs.find(name)->second);
}

float libutils::Options::getFloat(const string& name)
{
	return (m_floats.find(name)->second);
}

string libutils::Options::getString(const string& name)
{
	return (m_strings.find(name)->second);
}

bool libutils::Options::toStringVersion() const
{
	cerr << endl << m_version << endl << endl;

	return (false);
}

bool libutils::Options::toStringHelp()
{
	toStringUsage();
	if (m_description.empty() == false) toStringDescription();
	toStringOptions();

	return (false);
}

bool libutils::Options::toStringUsage()
{
	cerr << endl << "Usage:" << endl << " " << m_argv[0].substr(m_argv[0].find_last_of("/\\") + 1) << " [Options]";

	if (m_withCommand.size() != 0)
	{
		if (m_withArguments.size() != 0)
			cerr << " " << m_withCommand << " " << m_withArguments;
		else
			cerr << " " << m_withCommand;
	}

	cerr << endl << endl;

	return (false);
}

bool libutils::Options::toStringDescription() const
{
	cerr << "Description:" << endl << m_description << endl << endl;

	return (false);
}

bool libutils::Options::toStringOptions()
{
	cerr << "Options:" << endl;

	if (m_options.empty() == false) cerr << endl;

	for (Option option : m_options)
	{
		if (strcmp(option.name.c_str(), "__text_only__") != 0)
		{
			const string c = (option.shortname.size() == 1) ? option.shortname.c_str() : " ";
			cerr << ((c == " ") ? " " : "-") << c << " | --" << setw(m_maxNameLength) << setfill(' ') << left
				<< option.name << "\t" << setw(0);
		}

		cerr << option.description;
	}

	cerr << endl << "   | --values";
	cerr << endl << "   | --version";
	cerr << endl << "-h | --help" << endl << endl;

	return (false);
}

bool libutils::Options::toStringValues()
{
	if (m_withCommand.empty() == false)
	{
		cerr << endl << "Command:   " << m_command;

		if (m_withArguments.empty() == false)
		{
			cerr << endl << "Arguments: ";

			for (string argument : m_arguments)
				cerr << argument << " ";
		}

		cerr << endl;
	}

	cerr << endl << "Options:" << endl << endl;

	for (Option option : m_options)
	{
		if (strcmp(option.name.c_str(), "__text_only__") == 0) continue;

		const string c = (option.shortname.size() == 1) ? option.shortname.c_str() : " ";
		cerr << ((c == " ") ? " " : "-") << c << " | --" << option.name << " = ";

		if (option.type == Type::t_bool)
		{
			if (getBool(option.name) == true)
				cerr << "yes" << endl;
			else
				cerr << "no" << endl;
		}
		else if (option.type == Type::t_hex)
		{
			cerr << "0x" << hex << setw(2) << setfill('0') << getInt(option.name) << dec << endl;
		}
		else if (option.type == Type::t_int)
		{
			cerr << getInt(option.name) << endl;
		}
		else if (option.type == Type::t_long)
		{
			cerr << getLong(option.name) << endl;
		}
		else if (option.type == Type::t_float)
		{
			cerr << getFloat(option.name) << endl;
		}
		else if (option.type == Type::t_string)
		{
			cerr << getString(option.name) << endl;
		}
	}

	cerr << endl;

	return (false);
}

libutils::Options::Options(const string& command, const string& argument)
	: m_withCommand(command), m_withArguments(argument)
{
}

void libutils::Options::add(const string& name, const string& shortname, const string& description, const int line, const Type type)
{
	Option option;

	option.name = name;
	option.shortname = shortname;
	option.description = description;
	for (int i = 0; i < line + 1; i++)
		option.description += '\n';
	option.type = type;

	m_options.push_back(option);

	if (option.name.length() > m_maxNameLength) m_maxNameLength = option.name.length();
}

int libutils::Options::find(const string& name, const bool shortname)
{
	for (size_t i = 0; i < m_options.size(); i++)
	{
		if (m_options[i].type != Type::t_text)
		{
			if (shortname == true)
			{
				if (m_options[i].shortname == name) return (i);
			}
			else
			{
				if (m_options[i].name == name) return (i);
			}
		}
	}

	return (-1);
}

void libutils::Options::save(const int index, const string& value)
{
	switch (m_options[index].type)
	{
	case Type::t_bool:
		m_bools[m_options[index].name] = (m_bools[m_options[index].name] == true ? false : true);
		break;
	case Type::t_hex:
		m_ints[m_options[index].name] = strtol(value.c_str(), nullptr, 16);
		break;
	case Type::t_int:
		m_ints[m_options[index].name] = strtol(value.c_str(), nullptr, 10);
		break;
	case Type::t_long:
		m_longs[m_options[index].name] = strtol(value.c_str(), nullptr, 10);
		break;
	case Type::t_float:
		m_floats[m_options[index].name] = static_cast<float>(strtod(value.c_str(), nullptr));
		break;
	case Type::t_string:
		m_strings[m_options[index].name] = value;
		break;
	default:
		break;
	}
}

