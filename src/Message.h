/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#ifndef EBUS_MESSAGE_H
#define EBUS_MESSAGE_H

#include <Notify.h>

namespace ebus
{

class Telegram;

class Message : public Notify
{

public:
	explicit Message(Telegram &tel);

	Telegram& getTelegram();

	void setState(int state);
	int getState() const;

private:
	Telegram &m_telegram;
	int m_state = 0;

};

} // namespace ebus

#endif // EBUS_MESSAGE_H
