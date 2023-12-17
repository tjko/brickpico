/* mqtt.c
   Copyright (C) 2023 Timo Kokkonen <tjko@iki.fi>

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
#include "cJSON.h"
#ifdef LIB_PICO_CYW43_ARCH
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#if TLS_SUPPORT
#include "lwip/altcp_tls.h"
#endif
#endif

#include "brickpico.h"

#ifdef WIFI_SUPPORT

#define MQTTS_PORT 8883

mqtt_client_t *mqtt_client = NULL;
ip_addr_t mqtt_server_ip = IPADDR4_INIT_BYTES(52,54,110,50);
int incoming_topic = 0;

void mqtt_connect(mqtt_client_t *client);


static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	printf("Incoming publish at topic %s with total length %u\n", topic, (unsigned int)tot_len);
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	char buf[64];
	printf("Incoming publish payload with length %d, flags %u\n", len, (unsigned int)flags);

	u16_t l = (len < sizeof(buf) ? len : len - 1);
	memcpy(buf, data, l);
	buf[l] = 0;
	printf("DATA: '%s'\n", buf);
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
  printf("Subscribe result: %d\n", result);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
	if (status == MQTT_CONNECT_ACCEPTED) {
		log_msg(LOG_NOTICE, "MQTT connected to %s", ipaddr_ntoa(&mqtt_server_ip));
		mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb ,arg);

		err_t err = mqtt_subscribe(client, "tjkofi/feeds/command", 1, mqtt_sub_request_cb, arg);
		if (err != ERR_OK) {
			log_msg(LOG_WARNING, "mqtt_subscribe(): failed %d", err);
		}
	} else {
		log_msg(LOG_WARNING, "mqtt_connection_cb: disconnected %d", status);
		mqtt_connect(client);
	}
}

void mqtt_connect(mqtt_client_t *client)
{
	uint16_t mqtt_port = MQTT_PORT;

	if (!client)
		return;

	struct mqtt_connect_client_info_t ci;
	memset(&ci, 0, sizeof(ci));

	char client_id[64];
	snprintf(client_id, sizeof(client_id), "brickpico-%s_%s", BRICKPICO_BOARD, pico_serial_str());
	printf("client_id='%s'\n", client_id);

	ci.client_id = client_id;
	ci.client_user = cfg->mqtt_user;
	ci.client_pass = cfg->mqtt_pass;
	ci.keep_alive = 0;
	ci.will_topic = NULL;
	ci.will_msg = NULL;
	ci.will_retain = 0;
	ci.will_qos = 0;

#if TLS_SUPPORT
	if (cfg->mqtt_tls) {
		ci.tls_config = altcp_tls_create_config_client(NULL, 0);
		mqtt_port = MQTTS_PORT;;
	}
#endif
	if (cfg->mqtt_port > 0)
		mqtt_port = cfg->mqtt_port;

	log_msg(LOG_NOTICE,"MQTT Connecting to %s:%u%s",
		cfg->mqtt_server,
		mqtt_port,
		cfg->mqtt_tls ? " (TLS)" : "");

	err_t err = mqtt_client_connect(mqtt_client, &mqtt_server_ip, mqtt_port, mqtt_connection_cb, 0, &ci);
	if (err != ERR_OK) {
		log_msg(LOG_WARNING,"mqtt_client_connect(): failed %d", err);
	}
}

static void mqtt_pub_request_cb(void *arg, err_t result)
{
	printf("Publish result: %d\n", result);
}

void brickpico_setup_mqtt_client()
{
	cyw43_arch_lwip_begin();
	mqtt_client = mqtt_client_new();
	if (mqtt_client) {
		mqtt_connect(mqtt_client);
	}
	cyw43_arch_lwip_end();

	if (!mqtt_client) {
		log_msg(LOG_WARNING,"mqtt_client_new(): failed");
	}
}

void brickpico_mqtt_publish()
{
	char buf[32];
	uint64_t t = to_us_since_boot(get_absolute_time());

	if (!mqtt_client)
		return;

	snprintf(buf, sizeof(buf), "%llu", t);
	printf("brickpico_mqtt_publish(): '%s'\n", buf);

	u8_t qos = 2;
	u8_t retain = 0;

	cyw43_arch_lwip_begin();
	err_t err = mqtt_publish(mqtt_client, "tjkofi/feeds/test", buf, strlen(buf), qos, retain, mqtt_pub_request_cb, NULL);
	cyw43_arch_lwip_end();
	if (err != ERR_OK) {
		log_msg(LOG_WARNING,"mqtt_publish(): failed %d", err);
	}
}


#endif /* WIFI_SUPPORT */
