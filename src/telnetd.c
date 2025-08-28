/* telnetd.c
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico_telnetd.h"
#include "pico_telnetd/util.h"
#include "brickpico.h"
#include "util_net.h"

static const char *telnet_banner = "\r\n"
	"______      _      _   ______ _\r\n"
	"| ___ \\    (_)    | |  | ___ (_)\r\n"
	"| |_/ /_ __ _  ___| | _| |_/ /_  ___ ___\r\n"
	"| ___ \\ '__| |/ __| |/ /  __/| |/ __/ _ \\\r\n"
	"| |_/ / |  | | (__|   <| |   | | (_| (_) |\r\n"
	"\\____/|_|  |_|\\___|_|\\_\\_|   |_|\\___\\___/\r\n";


static user_pwhash_entry_t users[] = {
	{ NULL, NULL },
	{ NULL, NULL }
};


static int telnet_allow_connection_cb(ip_addr_t *src_ip)
{
	int ret = 0;
	int aclcount = 0;

	if (!src_ip)
		return -1;

	for (int i = 0; i < TELNET_MAX_ACL_ENTRIES; i++) {
		const acl_entry_t *acl = &cfg->telnet_acls[i];

		if (acl->prefix > 0) {
			ip_addr_t netmask;

			aclcount++;
			make_netmask(&netmask, acl->prefix);
			if (ip_addr_netcmp(&acl->ip, src_ip, &netmask)) {
				ret = 1;
				break;
			}
		}
	}

	return (aclcount > 0 ? ret: 1);
}


void tcpserver_init()
{
	tcp_server_t *srv = telnet_server_init(4096, 10240);

	if (!srv)
		return;

	users[0].login = cfg->telnet_user;
	users[0].hash = cfg->telnet_pwhash;
	srv->mode = (cfg->telnet_raw_mode ? RAW_MODE : TELNET_MODE);
	if (cfg->telnet_port > 0)
		srv->port = cfg->telnet_port;
	if (cfg->telnet_auth) {
		srv->auth_cb = sha512crypt_auth_cb;
		srv->auth_cb_param = (void*)users;
	}
	srv->log_cb = log_msg;
	srv->banner = telnet_banner;
	srv->allow_connect_cb = telnet_allow_connection_cb;

	telnet_server_start(srv, true);
}

