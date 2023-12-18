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
#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"
#if TLS_SUPPORT
#include "lwip/altcp_tls.h"
#endif
#endif

#include "brickpico.h"

#ifdef WIFI_SUPPORT

#define MQTTS_PORT 8883
#define MQTT_BUF_SIZE 2048

mqtt_client_t *mqtt_client = NULL;
ip_addr_t mqtt_server_ip;
int incoming_topic = 0;

void mqtt_connect(mqtt_client_t *client);


static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	log_msg(LOG_DEBUG, "MQTT incoming publish at topic %s with total length %u",
		topic, (unsigned int)tot_len);
	if (!strncmp(topic, cfg->mqtt_cmd_topic, strlen(cfg->mqtt_cmd_topic) + 1)) {
		incoming_topic = 1;
	} else {
		incoming_topic = 0;
	}
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	log_msg(LOG_DEBUG, "MQTT incoming publish payload with length %d, flags %u\n",
		len, (unsigned int)flags);

	if (incoming_topic != 1)
		return;

	struct brickpico_state *st = brickpico_state;
	u8_t *p = (u8_t*)memmem(data, len, "CMD:", 4);
	if (p) {
		char buf[32];
		p += 4;
		u16_t l = len - (p - data);
		if (l > 0) {
			char *saveptr;
			int output = 0;
			int pwr = -1;
			int pwm = -1;

			memcpy(buf, p, (l < sizeof(buf) ? l : sizeof(buf) - 1));
			buf[l] = 0;
			log_msg(LOG_NOTICE, "MQTT command topic data received: '%s'", buf);

			char *tok = strtok_r(buf, ":", &saveptr);
			if (tok) {
				if (!strncasecmp(tok, "ALL", 4)) {
					output = -1;
				} else {
					int tmp;
					if (str_to_int(tok, &tmp, 10)) {
						if (tmp >= 1 && tmp <= OUTPUT_COUNT)
							output = tmp;
					}
				}
				if (output != 0) {
					if ((tok = strtok_r(NULL, ":", &saveptr))) {
						if (!strncasecmp(tok, "ON", 3)) {
							pwr = 1;
						}
						else if (!strncasecmp(tok, "OFF", 4)) {
							pwr = 0;
						}
						else if (!strncasecmp(tok, "PWM", 4)) {
							if ((tok = strtok_r(NULL, ":", &saveptr))) {
								int tmp;
								if (str_to_int(tok, &tmp, 10)) {
									if (tmp >= 0 && tmp <= 100)
										pwm = tmp;
								}
							}
						}
					}
					for (int i = 0; i < OUTPUT_COUNT; i++) {
						if (output == i + 1 || output == -1) {
							if (pwr >= 0)
								st->pwr[i] = pwr;
							if (pwm >= 0)
								st->pwm[i] = pwm;
						}
					}
				}
			}
		}
	}
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
	if (result != ERR_OK) {
		log_msg(LOG_WARNING, "MQTT failed to subscribe command topic: %d", result);
	}
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
	if (status == MQTT_CONNECT_ACCEPTED) {
		log_msg(LOG_INFO, "MQTT connected to %s", ipaddr_ntoa(&mqtt_server_ip));
		mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb ,arg);
		if (cfg->mqtt_cmd_topic) {
			log_msg(LOG_INFO, "MQTT subscribe command topic: %s", cfg->mqtt_cmd_topic);
			err_t err = mqtt_subscribe(client, cfg->mqtt_cmd_topic, 1, mqtt_sub_request_cb, arg);
			if (err != ERR_OK) {
				log_msg(LOG_WARNING, "mqtt_subscribe(): failed %d", err);
			}
		}
	} else {
		log_msg(LOG_WARNING, "mqtt_connection_cb: disconnected %d", status);
		mqtt_connect(client);
	}
}

static void mqtt_dns_resolve_cb(const char *name, const ip_addr_t *ipaddr, void *arg)
{
	if (ipaddr && ipaddr->addr) {
		log_msg(LOG_INFO, "DNS resolved: %s -> %s\n", name, ipaddr_ntoa(ipaddr));
		ip_addr_set(&mqtt_server_ip, ipaddr);
		mqtt_connect(mqtt_client);
	} else {
		log_msg(LOG_WARNING, "Failed to resolve MQTT server name: %s", name);
	}
}

void mqtt_connect(mqtt_client_t *client)
{
	uint16_t mqtt_port = MQTT_PORT;
	err_t err;

	if (!client)
		return;

	err = dns_gethostbyname(cfg->mqtt_server, &mqtt_server_ip, mqtt_dns_resolve_cb, NULL);
	if (err != ERR_OK) {
		if (err == ERR_INPROGRESS)
			log_msg(LOG_INFO, "Resolving hostname: %s...\n", cfg->mqtt_server);
		else
			log_msg(LOG_WARNING, "Failed to resolve MQTT server name: %s (%d)" ,
				cfg->mqtt_server, err);

		return;
	}

	struct mqtt_connect_client_info_t ci;
	memset(&ci, 0, sizeof(ci));

	char client_id[32];
	snprintf(client_id, sizeof(client_id), "brickpico-%s_%s", BRICKPICO_BOARD, pico_serial_str());
	/* printf("client_id='%s', len=%u\n", client_id, strlen(client_id)); */
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

	log_msg(LOG_INFO, "MQTT Connecting to %s:%u%s",
		cfg->mqtt_server,
		mqtt_port,
		cfg->mqtt_tls ? " (TLS)" : "");

	err = mqtt_client_connect(mqtt_client, &mqtt_server_ip, mqtt_port, mqtt_connection_cb, 0, &ci);
	if (err != ERR_OK) {
		log_msg(LOG_WARNING,"mqtt_client_connect(): failed %d", err);
	}
}

static void mqtt_pub_request_cb(void *arg, err_t result)
{
	if (result != ERR_OK) {
		log_msg(LOG_NOTICE, "MQTT failed to publish to topic: %d", result);
	}
}

void brickpico_setup_mqtt_client()
{
	ip_addr_set_zero(&mqtt_server_ip);

	cyw43_arch_lwip_begin();
	if ((mqtt_client = mqtt_client_new())) {
		mqtt_connect(mqtt_client);
	}
	cyw43_arch_lwip_end();

	if (!mqtt_client) {
		log_msg(LOG_WARNING,"mqtt_client_new(): failed");
	}
}

char* json_status_message()
{
	const struct brickpico_state *st = brickpico_state;
	char *buf;
	cJSON *json, *outputs, *o;
	int i;

	if (!(json = cJSON_CreateObject()))
		goto panic;

	cJSON_AddItemToObject(json, "name", cJSON_CreateString(cfg->name));
	cJSON_AddItemToObject(json, "hostname", cJSON_CreateString(network_hostname()));
	if (network_ip())
		cJSON_AddItemToObject(json, "ip", cJSON_CreateString(network_ip()));
	if (!(outputs = cJSON_CreateArray()))
		goto panic;
	cJSON_AddItemToObject(json, "outputs", outputs);
	for (i = 0; i < OUTPUT_COUNT; i++) {
		if (!(o = cJSON_CreateObject()))
			goto panic;
		cJSON_AddItemToObject(o, "id", cJSON_CreateNumber(i + 1));
		//cJSON_AddItemToObject(o, "name", cJSON_CreateString(cfg->outputs[i].name));
		cJSON_AddItemToObject(o, "pwm", cJSON_CreateNumber(round_decimal(st->pwm[i], 1)));
		cJSON_AddItemToObject(o, "state", cJSON_CreateString(st->pwr[i] ? "ON" : "OFF"));
		cJSON_AddItemToArray(outputs, o);
	}

	if (!(buf = cJSON_Print(json)))
		goto panic;
	cJSON_Delete(json);
	return buf;

panic:
	if (json)
		cJSON_Delete(json);
	return NULL;
}

void brickpico_mqtt_publish()
{
	char *buf = NULL;

	if (!mqtt_client || strlen(cfg->mqtt_status_topic) < 1)
		return;

	/* Check that MQTT Client is connected */
	cyw43_arch_lwip_begin();
	u8_t connected = mqtt_client_is_connected(mqtt_client);
	cyw43_arch_lwip_end();
	if (!connected)
		return;

	/* Generate status message */
	if (!(buf = json_status_message())) {
		log_msg(LOG_WARNING,"json_status_message(): failed");
		return;
	}
	printf("DATA:\n%s\n", buf);
	log_msg(LOG_INFO, "MQTT publish to %s: %u bytes.", cfg->mqtt_status_topic, strlen(buf));

	u8_t qos = 2;
	u8_t retain = 0;

	/* Publish status message to a MQTT topic */
	cyw43_arch_lwip_begin();
	err_t err = mqtt_publish(mqtt_client, cfg->mqtt_status_topic, buf, strlen(buf),
				qos, retain, mqtt_pub_request_cb, NULL);
	cyw43_arch_lwip_end();
	if (err != ERR_OK) {
		log_msg(LOG_WARNING,"mqtt_publish(): failed %d", err);
	}
	free(buf);
}


#endif /* WIFI_SUPPORT */
