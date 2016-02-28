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

#include "Options.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>

using std::cerr;
using std::endl;
using std::hex;
using std::setw;
using std::setfill;
using std::dec;

Options& Options::getOption(const string& command, const string& argument)
{
	static Options option(command, argument);
	return (option);
}

Options::~Options()
{
	m_options.clear();
	m_bools.clear();
	m_ints.clear();
	m_longs.clear();
	m_floats.clear();
	m_strings.clear();
}

void Options::setVersion(const string& version)
{
	m_version = version;
}

void Options::addDescription(const string& description)
{
	m_description = description;
}

void Options::addText(const string& text)
{
	Option option;

	option.name = "__text_only__";
	option.shortname = "";
	option.type = t_text;
	option.description = text;

	m_options.push_back(option);
}

void Options::addBool(const string& name, const string& shortname, const bool& value, const string& description)
{
	if (name.size() != 0)
	{
		m_bools[name] = value;
		add(name, shortname, description, t_bool);
	}

}

void Options::addHex(const string& name, const string& shortname, const int& value, const string& description)
{
	if (name.size() != 0)
	{
		m_ints[name] = value;
		add(name, shortname, description, t_hex);
	}

}

void Options::addInt(const string& name, const string& shortname, const int& value, const string& description)
{
	if (name.size() != 0)
	{
		m_ints[name] = value;
		add(name, shortname, description, t_int);
	}

}

void Options::addLong(const string& name, const string& shortname, const long& value, const string& description)
{
	if (name.size() != 0)
	{
		m_longs[name] = value;
		add(name, shortname, description, t_long);
	}

}

void Options::addFloat(const string& name, const string& shortname, const float& value, const string& description)
{
	if (name.size() != 0)
	{
		m_floats[name] = value;
		add(name, shortname, description, t_float);
	}

}

void Options::addString(const string& name, const string& shortname, const string& value, const string& description)
{
	if (name.size() != 0)
	{
		m_strings[name] = value;
		add(name, shortname, description, t_string);
	}

}

bool Options::parse(int argc, char* argv[])
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
			cerr << endl << " Error: '" << _argv[i]
				<< "' Only single '-' or double '--' prefix are allowed." << endl << endl;
			return (toStringOptions());
		}

		// missing option
		else if ((_argv[i].rfind("--") == 0 && _argv[i].size() == 2)
			|| (_argv[i].rfind("-") == 0 && _argv[i].size() == 1))
		{
			cerr << endl << " Error: '" << _argv[i] << "' without an option is not allowed." << endl
				<< endl;
			return (toStringOptions());
		}

		// parse long format '--'
		else if (_argv[i].rfind("--") == 0 && _argv[i].size() > 2)
		{
			index = find(_argv[i].substr(2), false);
			if (index >= 0)
			{
				if (m_options[index].type == t_bool)
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
					if (m_options[index].type == t_bool)
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
							cerr << endl << " Error: '-" << _argv[i].substr(j, 1)
								<< "' in '" << _argv[i]
								<< "' is an option value and need to be in a single statement."
								<< endl << endl;
							return (toStringOptions());
						}
					}
				}
				else
				{
					if (strcmp(_argv[i].substr(j, 1).c_str(), "h") == 0) return (toStringHelp());
					cerr << endl << " Error: Option '-" << _argv[i].substr(j, 1)
						<< "' was not found." << endl << endl;
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
			else if (m_withCommand.empty() == false && m_command.empty() == true)
			{
				m_command = _argv[i];
			}
			else
			{
				m_arguments.push_back(_argv[i]);
			}
		}
	}

	if (saveIndexLong >= 0)
	{
		cerr << endl << " Error: Option '--" << m_options[saveIndexLong].name << "' needs a value." << endl
			<< endl;
		return (toStringOptions());
	}
	else if (saveIndexShort >= 0)
	{
		cerr << endl << " Error: Option '-" << m_options[saveIndexShort].shortname << "' needs a value." << endl
			<< endl;
		return (toStringOptions());
	}

	if (m_withCommand.empty() == false && m_command.empty() == true)
	{
		cerr << endl << " Error: " << m_withCommand << " is missing." << endl;
		return (toStringUsage());
	}

	return (true);
}

int Options::numArgs() const
{
	return (m_arguments.size());
}

string Options::getArg(const int& num) const
{
	return (m_arguments[num]);
}

string Options::getCommand() const
{
	return (m_command);
}

bool Options::missingCommand() const
{
	return (m_command.empty());
}

bool Options::getBool(const string& name)
{
	return (m_bools.find(name)->second);
}

int Options::getInt(const string& name)
{
	return (m_ints.find(name)->second);
}

long Options::getLong(const string& name)
{
	return (m_longs.find(name)->second);
}

float Options::getFloat(const string& name)
{
	return (m_floats.find(name)->second);
}

string Options::getString(const string& name)
{
	return (m_strings.find(name)->second);
}

bool Options::toStringVersion() const
{
	cerr << endl << m_version << endl << endl;

	return (false);
}

bool Options::toStringHelp()
{
	toStringUsage();
	if (m_description.empty() == false) toStringDescription();
	toStringOptions();

	return (false);
}

bool Options::toStringUsage()
{
	cerr << endl << "Usage:" << endl << " " << m_argv[0].substr(m_argv[0].find_last_of("/\\") + 1)
		<< " [Options...]";

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

bool Options::toStringDescription()
{
	cerr << "Description:" << endl << m_description << endl << endl;

	return (false);
}

bool Options::toStringOptions()
{
	cerr << "Options:" << endl;

	if (m_options.empty() == false) cerr << endl;

	for (Option option : m_options)
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

	cerr << endl << "   | --values";
	cerr << endl << "   | --version";
	cerr << endl << "-h | --help" << endl << endl;

	return (false);
}

bool Options::toStringValues()
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

		if (option.type == t_bool)
		{
			if (getBool(option.name) == true)
				cerr << "yes" << endl;
			else
				cerr << "no" << endl;
		}
		else if (option.type == t_hex)
		{
			cerr << "0x" << hex << setw(2) << setfill('0') << getInt(option.name) << dec << endl;
		}
		else if (option.type == t_int)
		{
			cerr << getInt(option.name) << endl;
		}
		else if (option.type == t_long)
		{
			cerr << getLong(option.name) << endl;
		}
		else if (option.type == t_float)
		{
			cerr << getFloat(option.name) << endl;
		}
		else if (option.type == t_string)
		{
			cerr << getString(option.name) << endl;
		}
	}

	cerr << endl;

	return (false);
}

Options::Options(const string& command, const string& argument)
	: m_withCommand(command), m_withArguments(argument)
{
}

void Options::add(const string& name, const string& shortname, const string& description, const Type& type)
{
	Option option;

	option.name = name;
	option.shortname = shortname;
	option.description = description;
	option.type = type;

	m_options.push_back(option);
}

int Options::find(const string& name, const bool& shortname)
{
	for (size_t i = 0; i < m_options.size(); i++)
	{
		if (m_options[i].type != t_text)
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

void Options::save(const int& index, const string& value)
{
	switch (m_options[index].type)
	{
	case t_bool:
		m_bools[m_options[index].name] = (m_bools[m_options[index].name] == true ? false : true);
		break;
	case t_hex:
		m_ints[m_options[index].name] = strtol(value.c_str(), nullptr, 16);
		break;
	case t_int:
		m_ints[m_options[index].name] = strtol(value.c_str(), nullptr, 10);
		break;
	case t_long:
		m_longs[m_options[index].name] = strtol(value.c_str(), nullptr, 10);
		break;
	case t_float:
		m_floats[m_options[index].name] = static_cast<float>(strtod(value.c_str(), nullptr));
		break;
	case t_string:
		m_strings[m_options[index].name] = value;
		break;
	default:
		break;
	}
}

