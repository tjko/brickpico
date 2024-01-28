/* tcpserver.c
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
#include "tcpserver.h"
#include "brickpico.h"

#ifdef WIFI_SUPPORT
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define TELNET_DEFAULT_PORT 23
#define TCP_SERVER_MAX_CONN 1
#define TCP_CLIENT_POLL_TIME 1

tcp_server_t *tcpserv = NULL;

static void (*chars_available_callback)(void*) = NULL;
static void *chars_available_param = NULL;


const char* telnet_login_prompt = "\r\nlogin: ";
const char* telnet_passwd_prompt = "\r\npassword: ";
const char* telnet_login_failed = "\r\nLogin failed.\r\n";
const char* telnet_login_success = "\r\nLogin successful.\r\n";

const uint8_t telnet_default_options[] = {
	IAC, TELNET_DO, TO_SUP_GA,
	IAC, TELNET_WILL, TO_ECHO,
	IAC, TELNET_WONT, TO_LINEMODE,
};

static tcp_server_t* tcp_server_init(void) {
	tcp_server_t *st = calloc(1, sizeof(tcp_server_t));

	if (!st)
		log_msg(LOG_WARNING, "Failed to allocate tcp server state");

	simple_ringbuffer_init(&st->rb_in, st->in_buf, sizeof(st->in_buf));
	simple_ringbuffer_init(&st->rb_out, st->out_buf, sizeof(st->out_buf));
	st->mode = RAW_MODE;
	st->cstate = CS_NONE;
	st->auth_cb = NULL;
	st->port = TELNET_DEFAULT_PORT;

	return st;
}


static err_t close_client_connection(struct tcp_pcb *pcb)
{
	err_t err = ERR_OK;

	tcp_arg(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_err(pcb, NULL);
	tcp_poll(pcb, NULL, 0);

	if ((err = tcp_close(pcb)) != ERR_OK) {
		tcp_abort(pcb);
		err = ERR_ABRT;
	}

	return err;
}


static err_t tcp_server_close(void *arg) {
	tcp_server_t *st = (tcp_server_t*)arg;
	err_t err = ERR_OK;

	st->cstate = CS_NONE;
	if (st->client) {
		err = close_client_connection(st->client);
		st->client = NULL;
	}

	if (st->listen) {
		tcp_arg(st->listen, NULL);
		tcp_accept(st->listen, NULL);
		if ((err = tcp_close(st->listen)) != ERR_OK) {
			log_msg(LOG_NOTICE, "tcp_server_close: failed to close listen pcb: %d", err);
			tcp_abort(st->listen);
			err = ERR_ABRT;
		}
		st->listen = NULL;
	}

	return err;
}


static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	//log_msg(LOG_DEBUG, "tcp_server_sent: %u", len);
	return ERR_OK;
}


static void process_telnet_cmd(tcp_server_t *st)
{
	int resp = -1;
//	printf("process_telnet_cmd(%d,%d)\n", st->telnet_cmd, st->telnet_opt);


	switch(st->telnet_cmd) {
	case TELNET_DO:
		switch (st->telnet_opt) {
		case TO_ECHO:
			/* do nothing since we sent WILL for these...*/
			break;

		case TO_BINARY:
		case TO_SUP_GA:
			resp = TELNET_WILL;
			break;

		default:
			resp = TELNET_WONT;
			break;
		}
		break;

	case TELNET_WILL:
		switch (st->telnet_opt) {
		case TO_SUP_GA:
			/* do nothing since we sent DO for these... */
			break;

		case TO_NAWS:
		case TO_TSPEED:
		case TO_RFLOWCTRL:
		case TO_LINEMODE:
		case TO_XDISPLOC:
		case TO_ENV:
		case TO_AUTH:
		case TO_ENCRYPT:
		case TO_NEWENV:
			resp = TELNET_DONT;
			break;

		default:
			resp = TELNET_DO;
		}
		break;

	case TELNET_DONT:
	case TELNET_WONT:
		/* ignore these */
		break;

	default:
		//printf("Unknown command: %d\n", st->telnet_cmd);
		break;
	}


	if (resp >= 0) {
		uint8_t buf[3] = { IAC, resp, st->telnet_opt };
		tcp_write(st->client, buf, 3, TCP_WRITE_FLAG_COPY);
	}

	st->telnet_cmd_count++;
}

static err_t process_received_data(void *arg, uint8_t *buf, size_t len)
{
	tcp_server_t *st = (tcp_server_t*)arg;

	if (!arg || !buf)
		return ERR_VAL;

	if (len < 1)
		return ERR_OK;

	simple_ringbuffer_t *rb = &st->rb_in;

	//printf("data: ");
	for(int i = 0; i < len; i++) {
		uint8_t c = buf[i];
		//printf("%c(%d) ", (isprint(c) ? c : '?'), c);

		if (st->mode == TELNET_MODE) {
		telnet_state_macine:
			if (st->telnet_state == 0) { /* normal (pass-through) mode */
				if (c == IAC) {
					st->telnet_state = 1;
					continue;
				}
			}
			else if (st->telnet_state == 1) { /* IAC seen */
				if (c == IAC) { /* escaped 0xff */
					st->telnet_state = 0;
				}
				else { /* Telnet command */
					st->telnet_cmd = c;
					st->telnet_opt = 0;
					if (c == TELNET_WILL || c == TELNET_WONT
						|| c == TELNET_DO || c == TELNET_DONT
						|| c== TELNET_SB) {
						st->telnet_state = 2;
					} else {
						process_telnet_cmd(st);
						st->telnet_state = 0;
					}
					continue;
				}
			}
			else if (st->telnet_state == 2) { /* Telnet option */
				//printf("\n");
				st->telnet_opt = c;
				if (st->telnet_cmd == TELNET_SB) {
					st->telnet_state = 3;
				} else {
					process_telnet_cmd(st);
					st->telnet_state = 0;
				}
				continue;
			}
			else if (st->telnet_state == 3) { /* Subnegotiation data */
				if (c == IAC)
					st->telnet_state = 4;
				continue;
			}
			else if (st->telnet_state == 4) { /* subnegotiation end? */
				if (c == IAC)
					st->telnet_state = 3;
				else {
					st->telnet_state = 1;
					goto telnet_state_macine;
				}
			}
			else {
				st->telnet_state = 0;
			}
		}

		if (simple_ringbuffer_add_char(rb, c, false) < 0)
			return ERR_MEM;
	}
	//printf("\n");

	return ERR_OK;
}


static err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	tcp_server_t *st = (tcp_server_t*)arg;
	struct pbuf *buf;
	size_t len;

	if (!p) {
		/* Connection closed by client */
		log_msg(LOG_INFO, "Client closed connection: %d (%x)", err, pcb);
		close_client_connection(pcb);
		st->cstate = CS_NONE;
		st->client = NULL;
		return ERR_OK;
	}
	if (err != ERR_OK) {
		/* unknown error... */
		log_msg(LOG_WARNING, "tcp_server_recv: error received: %d", err);
		if (p)
			pbuf_free(p);
		return err;
	}


	log_msg(LOG_DEBUG, "tcp_server_recv: data received (pcb=%x): tot_len=%d, len=%d, err=%d",
		pcb, p->tot_len, p->len, err);

//	if (st->cstate == CS_ACCEPT)
//		st->cstate = CS_CONNECT;

	buf = p;
	while (buf != NULL) {
		err = process_received_data(st, buf->payload, buf->len);
		if  (err != ERR_OK)
			break;
		buf = buf->next;
	}

	if ((len = simple_ringbuffer_size(&st->rb_in)) > 0) {
		if (st->cstate == CS_AUTH_LOGIN || st->cstate == CS_AUTH_PASSWD) {
			int i;
			int l = -1;
			for (i = 0; i < len; i++) {
				int c = simple_ringbuffer_peek_char(&st->rb_in, i);
				if (c == 10 || c == 13) {
					l = i;
					break;
				}
			}
			if (l >= 0) {
				if (st->cstate == CS_AUTH_LOGIN) {
					if (l >= sizeof(st->login))
						l = sizeof(st->login) - 1;
					simple_ringbuffer_read(&st->rb_in, st->login, l+1);
					st->login[l] = 0;
					st->cstate = CS_AUTH_PASSWD;
					tcp_write(pcb, telnet_passwd_prompt, strlen(telnet_passwd_prompt), 0);
					tcp_output(pcb);
				} else {
					if (l >= sizeof(st->passwd))
						l = sizeof(st->passwd) - 1;
					simple_ringbuffer_read(&st->rb_in, st->passwd, l+1);
					st->passwd[l] = 0;
					if (st->auth_cb((const char*)st->login, (const char*)st->passwd) == 0) {
						st->cstate = CS_CONNECT;
						tcp_write(pcb, telnet_login_success,
							strlen(telnet_login_success), 0);
					} else {
						st->cstate = CS_ACCEPT;
						tcp_write(pcb, telnet_login_failed,
							strlen(telnet_login_failed), 0);
					}
					tcp_output(pcb);
				}
				simple_ringbuffer_flush(&st->rb_in);

			}
		}
		else if (st->cstate == CS_CONNECT) {
			if (chars_available_callback && len > 0) {
				chars_available_callback(chars_available_param);
			}
		}
	}

	//printf("buffer len: %u\n", simple_ringbuffer_size(&st->rb_in));

	tcp_recved(pcb, p->tot_len);
	pbuf_free(p);

	return ERR_OK;
}





static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb)
{
	tcp_server_t *st = (tcp_server_t*)arg;
	int wcount = 0;
	int waiting;
	uint8_t *rbuf;
	err_t err;


	//log_msg(LOG_DEBUG, "tcp_server_poll: poll %x", pcb);

	if (st->cstate == CS_ACCEPT) {
		if ((st->mode == TELNET_MODE && st->telnet_cmd_count > 0) ||
			st->mode == RAW_MODE)
			st->cstate = (st->auth_cb ? CS_AUTH_LOGIN : CS_CONNECT);

		if (st->cstate == CS_AUTH_LOGIN) {
			tcp_write(pcb, telnet_login_prompt, strlen(telnet_login_prompt), 0);
			wcount++;
		}
	}

	while ((waiting = simple_ringbuffer_size(&st->rb_out)) > 0) {
		size_t len = simple_ringbuffer_peek(&st->rb_out, &rbuf, waiting);
		if (len > 0) {
			//printf("tcp_write: %u\n", len);
			err = tcp_write(pcb, rbuf, len, TCP_WRITE_FLAG_COPY);
			if (err != ERR_OK)
				break;
			simple_ringbuffer_read(&st->rb_out, NULL, len);
			wcount++;
		}
	}

	if (wcount > 0)
		tcp_output(pcb);

	return ERR_OK;
}


static void tcp_server_err(void *arg, err_t err)
{
	if (err == ERR_ABRT)
		return;

	log_msg(LOG_ERR,"tcp_server_err: client connection error: %d", err);
}


static err_t tcp_server_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	tcp_server_t *st = (tcp_server_t*)arg;


	if (!pcb || err != ERR_OK) {
		log_msg(LOG_ERR, "tcp_server_accept: failure: %d", err);
		return ERR_VAL;
	}

	log_msg(LOG_INFO, "Client connected: %x", pcb);

	if (st->cstate != CS_NONE) {
		log_msg(LOG_ERR, "tcp_server_accept: reject connection");
		return ERR_MEM;
	}

	st->client = pcb;
	tcp_arg(pcb, st);
	tcp_sent(pcb, tcp_server_sent);
	tcp_recv(pcb, tcp_server_recv);
	tcp_poll(pcb, tcp_server_poll, TCP_CLIENT_POLL_TIME);
	tcp_err(pcb, tcp_server_err);

	st->cstate = CS_ACCEPT;
	st->telnet_state = 0;
	st->telnet_cmd_count = 0;
	simple_ringbuffer_flush(&st->rb_in);
	simple_ringbuffer_flush(&st->rb_out);

	if (st->mode == TELNET_MODE) {
		tcp_write(pcb, telnet_default_options, sizeof(telnet_default_options), 0);
		tcp_output(pcb);
	} else {
		tcp_write(pcb, "BrickPico\r\n", 11, 0);
		tcp_output(pcb);
	}

	return ERR_OK;
}


static bool tcp_server_open(tcp_server_t *st) {
	struct tcp_pcb *pcb;
	err_t err;

	if (!st)
		return false;

	if (!(pcb = tcp_new_ip_type(IPADDR_TYPE_ANY))) {
		log_msg(LOG_ERR, "tcp_server_open: failed to create pcb");
		return false;
	}

	if ((err = tcp_bind(pcb, NULL, st->port)) != ERR_OK) {
		log_msg(LOG_ERR, "tcp_server_open: cannot bind to port %d: %d", st->port, err);
		tcp_abort(pcb);
		return false;
	}

	if (!(st->listen = tcp_listen_with_backlog(pcb, TCP_SERVER_MAX_CONN))) {
		log_msg(LOG_ERR, "tcp_server_open: failed to listen on port %d", st->port);
		tcp_abort(pcb);
		return false;
	}

	tcp_arg(st->listen, st);
	tcp_accept(st->listen, tcp_server_accept);

	return true;
}





void stdio_tcp_init()
{
	chars_available_callback = NULL;
	chars_available_param = NULL;
	stdio_set_driver_enabled(&stdio_tcp_driver, true);
}


static void stdio_tcp_out_chars(const char *buf, int length)
{
	if (!tcpserv)
		return;
	if (tcpserv->cstate != CS_CONNECT)
		return;

	for (int i = 0; i <length; i++) {
		if (simple_ringbuffer_add_char(&tcpserv->rb_out, buf[i], false) < 0)
			return;
	}
}

static int stdio_tcp_in_chars(char *buf, int length)
{
	int i = 0;
	int c;

	if (!tcpserv)
		return PICO_ERROR_NO_DATA;
	if (tcpserv->cstate != CS_CONNECT)
		return PICO_ERROR_NO_DATA;

	while (i < length && ((c = simple_ringbuffer_read_char(&tcpserv->rb_in)) >= 0)) {
		buf[i++] = c;
	}

	return i ? i : PICO_ERROR_NO_DATA;
}


static void stdio_tcp_set_chars_available_callback(void (*fn)(void*), void *param)
{
    chars_available_callback = fn;
    chars_available_param = param;
}


int dummy_auth_cb(const char *login, const char *password)
{
	printf("login='%s', password='%s'\n", login, password);
	return 0;
}

void tcpserver_init()
{
	tcpserv = tcp_server_init();

	if (!tcpserv)
		return;

	tcpserv->mode = TELNET_MODE;
	tcpserv->auth_cb = dummy_auth_cb;

	cyw43_arch_lwip_begin();
	if (!tcp_server_open(tcpserv)) {
		log_msg(LOG_ERR, "Failed to start TCP/Telnet server");
		tcp_server_close(tcpserv);
	} else {
		stdio_tcp_init();
	}
	cyw43_arch_lwip_end();

}




stdio_driver_t stdio_tcp_driver = {
	.out_chars = stdio_tcp_out_chars,
	.in_chars = stdio_tcp_in_chars,
	.set_chars_available_callback = stdio_tcp_set_chars_available_callback,
	.crlf_enabled = PICO_STDIO_DEFAULT_CRLF
};


#endif /* WIFI_SUPPORT */
