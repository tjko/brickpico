/* tcpserver.h
   Copyright (C) 2024 Timo Kokkonen <tjko@iki.fi>

   SPDX-License-Identifier: GPL-3.0-or-later

   This file is part of BrickPico.

   BrickPico is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   BrickPico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with BrickPico. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef TCPSERVER_H
#define TCPSERVER_H 1

#include "pico/stdio.h"
#include "ringbuffer.h"


typedef struct tcp_server_t {
	struct tcp_pcb *listen;
	struct tcp_pcb *client;
	simple_ringbuffer_t rb_in;
	simple_ringbuffer_t rb_out;
	uint8_t in_buf[512];
	uint8_t out_buf[2048];
	bool connected;
} tcp_server_t;


extern stdio_driver_t stdio_tcp_driver;

#endif /* TCPSERVER_H */
