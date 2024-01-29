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
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico_telnetd.h"
#include "pico_telnetd/util.h"
#include "brickpico.h"


user_pwhash_entry_t users[] = {
	{ "admin", "$6$caRtcnraEpbI48d3$YizNnV2hIwqZ/Gu4jh9ebV/DXCRhCzvUM2E0yTF3BgGrMw1HrfYIJJ9CQ0rcVBbpScCfwBtKhynVpKSnW/5o.." },
	{ NULL, NULL }
};

void tcpserver_init()
{
	tcp_server_t *srv = telnet_server_init();

	if (!srv)
		return;

	srv->mode = TELNET_MODE;
	srv->log_cb = log_msg;
	srv->auth_cb = sha512crypt_auth_cb;
	srv->auth_cb_param = (void*)users;
	srv->banner = "\r\nBrickPico\r\n=========\r\n";

//	telnetd_log_level(LOG_INFO);

	telnet_server_start(true);

}

