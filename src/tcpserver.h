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



/* Telnet commands */
#define TELNET_SE     240
#define TELNET_NOP    241
#define TELNET_DM     242
#define TELNET_BRK    243
#define TELNET_IP     244
#define TELNET_AO     245
#define TELNET_AYT    246
#define TELNET_EC     247
#define TELNET_EL     248
#define TELNET_GA     249
#define TELNET_SB     250
#define TELNET_WILL   251
#define TELNET_WONT   252
#define TELNET_DO     253
#define TELNET_DONT   254
#define IAC           255

/* Telnet options */
#define TO_BINARY     0
#define TO_ECHO       1
#define TO_RECONNECT  2
#define TO_SUP_GA     3
#define TO_AMSN       4
#define TO_STATUS     5
#define TO_NAWS       31
#define TO_TSPEED     32
#define TO_RFLOWCTRL  33
#define TO_LINEMODE   34
#define TO_XDISPLOC   35
#define TO_ENV        36
#define TO_AUTH       37
#define TO_ENCRYPT    38
#define TO_NEWENV     39


typedef struct user_pwhash_entry {
	const char *login;
	const char *hash;
} user_pwhash_entry_t;


typedef enum tcp_server_mode {
	RAW_MODE = 0,
	TELNET_MODE
} tcp_server_mode_t;

typedef enum tcp_connection_state {
	CS_NONE = 0,
	CS_ACCEPT,
	CS_AUTH_LOGIN,
	CS_AUTH_PASSWD,
	CS_CONNECT,
	CS_CLOSE,
} tcp_connection_state_t;

typedef struct tcp_server_t {
	uint16_t port;
	struct tcp_pcb *listen;
	struct tcp_pcb *client;
	tcp_server_mode_t mode;
	tcp_connection_state_t cstate;
	simple_ringbuffer_t rb_in;
	simple_ringbuffer_t rb_out;
	uint8_t in_buf[2048];
	uint8_t out_buf[8192];
	int telnet_state;
	uint8_t telnet_cmd;
	uint8_t telnet_opt;
	int telnet_cmd_count;
	int (*auth_cb)(void* param, const char *login, const char *password);
	void *auth_cb_param;
	uint8_t login[32 + 1];
	uint8_t passwd[64 + 1];
	const char *banner;
} tcp_server_t;


extern stdio_driver_t stdio_tcp_driver;

#endif /* TCPSERVER_H */
