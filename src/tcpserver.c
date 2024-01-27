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

#define TCP_SERVER_MAX_CON 1
#define TCP_CLIENT_POLL_TIME 1

tcp_server_t *tcpserv = NULL;

static void (*chars_available_callback)(void*) = NULL;
static void *chars_available_param = NULL;


static tcp_server_t* tcp_server_init(void) {
	tcp_server_t *state = calloc(1, sizeof(tcp_server_t));

	if (!state)
		log_msg(LOG_WARNING, "Failed to allocate tcp server state");

	simple_ringbuffer_init(&state->rb_in, state->in_buf, sizeof(state->in_buf));
	simple_ringbuffer_init(&state->rb_out, state->out_buf, sizeof(state->out_buf));
	return state;
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
	tcp_server_t *state = (tcp_server_t*)arg;
	err_t err = ERR_OK;

	state->connected = false;
	if (state->client) {
		err = close_client_connection(state->client);
		state->client = NULL;
	}

	if (state->listen) {
		tcp_arg(state->listen, NULL);
		tcp_accept(state->listen, NULL);
		if ((err = tcp_close(state->listen)) != ERR_OK) {
			log_msg(LOG_NOTICE, "tcp_server_close: failed to close listen pcb: %d", err);
			tcp_abort(state->listen);
			err = ERR_ABRT;
		}
		state->listen = NULL;
	}

	return err;
}


static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	//log_msg(LOG_DEBUG, "tcp_server_sent: %u", len);
	return ERR_OK;
}


static err_t process_received_data(simple_ringbuffer_t *rb, uint8_t *buf, size_t len)
{
	int state = 0;

	if (!rb || !buf)
		return ERR_VAL;

	if (len < 1)
		return ERR_OK;

	//printf("data: ");
	for(int i = 0; i < len; i++) {
		uint8_t c = buf[i];
		//printf("%c(%d) ", (isprint(c) ? c : '?'), c);

		/* "filter out" telnet protocol... */
		if (state == 0) {
			if (c == 255) {
				state = 1;
				continue;
			}
		}
		else if (state == 1) {
			if (c == 255) {
				state = 0;
			}
			else {
				state = 2;
				continue;
			}
		}
		else {
			state = 0;
			continue;
		}

		if (simple_ringbuffer_add_char(rb, c, false) < 0)
			return ERR_MEM;
	}
	//printf("\n");

	return ERR_OK;
}


static err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	tcp_server_t *state = (tcp_server_t*)arg;
	struct pbuf *buf;

	if (!p) {
		/* Connection closed by client */
		log_msg(LOG_INFO, "Client closed connection: %d (%x)", err, pcb);
		state->connected = false;
		close_client_connection(pcb);
		state->client = NULL;
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

	buf = p;
	while (buf != NULL) {
		err = process_received_data(&state->rb_in, buf->payload, buf->len);
		if  (err != ERR_OK)
			break;
		buf = buf->next;
	}

	if (chars_available_callback && (simple_ringbuffer_size(&state->rb_in) > 0)) {
		chars_available_callback(chars_available_param);
	}

	//printf("buffer len: %u\n", simple_ringbuffer_size(&state->rb_in));

	tcp_recved(pcb, p->tot_len);
	pbuf_free(p);

	return ERR_OK;
}


static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb)
{
	tcp_server_t *state = (tcp_server_t*)arg;
	int count;
	uint8_t *rbuf;
	err_t err;

	//log_msg(LOG_DEBUG, "tcp_server_poll: poll %x", pcb);

	while ((count = simple_ringbuffer_size(&state->rb_out)) > 0) {
		size_t len = simple_ringbuffer_peek(&state->rb_out, &rbuf, count);
		if (len > 0) {
			//printf("tcp_write: %u\n", len);
			err = tcp_write(pcb, rbuf, len, TCP_WRITE_FLAG_COPY);
			if (err != ERR_OK)
				break;
			simple_ringbuffer_read(&state->rb_out, NULL, len);
		}
	}

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
	tcp_server_t *state = (tcp_server_t*)arg;

	if (!pcb || err != ERR_OK) {
		log_msg(LOG_ERR, "tcp_server_accept: failure: %d", err);
		return ERR_VAL;
	}

	log_msg(LOG_INFO, "Client connected: %x", pcb);

	if (state->connected) {
		log_msg(LOG_ERR, "tcp_server_accept: reject connection");
		return ERR_MEM;
	}

	state->client = pcb;
	tcp_arg(pcb, state);
	tcp_sent(pcb, tcp_server_sent);
	tcp_recv(pcb, tcp_server_recv);
	tcp_poll(pcb, tcp_server_poll, TCP_CLIENT_POLL_TIME);
	tcp_err(pcb, tcp_server_err);

	state->connected = true;
	simple_ringbuffer_flush(&state->rb_in);
	simple_ringbuffer_flush(&state->rb_out);

	return ERR_OK;
}


static bool tcp_server_open(void *arg, u16_t port) {
	tcp_server_t *state = (tcp_server_t*)arg;
	struct tcp_pcb *pcb;
	err_t err;

	if (!arg)
		return false;

	if (!(pcb = tcp_new_ip_type(IPADDR_TYPE_ANY))) {
		log_msg(LOG_ERR, "tcp_server_open: failed to create pcb");
		return false;
	}

	if ((err = tcp_bind(pcb, NULL, port)) != ERR_OK) {
		log_msg(LOG_ERR, "tcp_server_open: cannot bind to port %d: %d", port, err);
		tcp_abort(pcb);
		return false;
	}

	if (!(state->listen = tcp_listen_with_backlog(pcb, TCP_SERVER_MAX_CON))) {
		log_msg(LOG_ERR, "tcp_server_open: failed to listen on port %d", port);
		tcp_abort(pcb);
		return false;
	}

	tcp_arg(state->listen, state);
	tcp_accept(state->listen, tcp_server_accept);

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
	if (!tcpserv->connected)
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
	if (!tcpserv->connected)
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



void tcpserver_init()
{
	tcpserv = tcp_server_init();

	if (!tcpserv)
		return;

	cyw43_arch_lwip_begin();
	if (!tcp_server_open(tcpserv, 23)) {
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
