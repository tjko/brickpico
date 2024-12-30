/* mqtt.c
   Copyright (C) 2023-2024 Timo Kokkonen <tjko@iki.fi>

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

#define MQTT_CMD_MAX_LEN 100


enum mqtt_topic_types {
	UNKNOWN_TOPIC = 0,
	CMD_TOPIC = 1,
	ERR_TOPIC = 2,
	WARN_TOPIC = 3,
	HA_TOPIC = 4,
	HA_CMD_TOPIC = 5,
};

struct mqtt_topic_name {
	enum mqtt_topic_types type;
	size_t offset;
	const char* str;
};


mqtt_client_t *mqtt_client = NULL;
ip_addr_t mqtt_server_ip = IPADDR4_INIT_BYTES(0, 0, 0, 0);
u16_t mqtt_server_port = 0;
enum mqtt_topic_types incoming_topic = UNKNOWN_TOPIC;
int incoming_topic_idx = 0;
int mqtt_qos = 1;
char mqtt_ha_birth_topic[64 + 1];
char mqtt_ha_base_topic[64 + 1];
char mqtt_ha_cmd_base_topic[64 + 10 + 1];
char mqtt_scpi_cmd[MQTT_CMD_MAX_LEN + 1];
bool mqtt_scpi_cmd_queued = false;
absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(t_mqtt_disconnect, 0);
absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(t_mqtt_ha_discovery, 0);
u16_t mqtt_reconnect = 0;
u16_t mqtt_ha_discovery = 0;


struct mqtt_topic_name topics[] = {
	{ ERR_TOPIC, offsetof(struct brickpico_config, mqtt_err_topic), NULL },
	{ WARN_TOPIC, offsetof(struct brickpico_config, mqtt_warn_topic), NULL },
	{ CMD_TOPIC, offsetof(struct brickpico_config, mqtt_cmd_topic), NULL },
	{ HA_TOPIC, 0, mqtt_ha_birth_topic },
	{ HA_CMD_TOPIC, 0, mqtt_ha_cmd_base_topic },
	{ 0, 0, NULL }
};


static void mqtt_connect(mqtt_client_t *client);



static void mqtt_pub_request_cb(void *arg, err_t result)
{
	const char *topic = (const char*)arg;

	if (!topic)
		topic = "NULL";

	if (result == ERR_OK) {
		log_msg(LOG_DEBUG, "MQTT publish successful (%s)", topic);
	}
	else if (result == ERR_TIMEOUT) {
		log_msg(LOG_NOTICE, "MQTT publish failed: timeout (%s)", topic);
	}
	else {
		log_msg(LOG_NOTICE, "MQTT publish failed: %d (%s)", result, topic);
	}
}

static int mqtt_publish_message(const char *topic, const char *buf, u16_t buf_len,
			u8_t qos, u8_t retain, const char *arg)
{
	if (!topic || !buf || buf_len == 0)
		return -1;
	if (!mqtt_client)
		return -2;

	/* Check that MQTT Client is connected */
	cyw43_arch_lwip_begin();
	u8_t connected = mqtt_client_is_connected(mqtt_client);
	cyw43_arch_lwip_end();
	if (!connected)
		return -3;

	log_msg(LOG_DEBUG, "MQTT publish to %s: %u bytes.", topic, buf_len);

	/* Publish message to a MQTT topic */
	cyw43_arch_lwip_begin();
	err_t err = mqtt_publish(mqtt_client, topic, buf, buf_len,
				qos, retain, mqtt_pub_request_cb, (void*)arg);
	cyw43_arch_lwip_end();
	if (err != ERR_OK) {
		log_msg(LOG_NOTICE, "mqtt_publish_message(): failed %d (topic=%s, buf_len=%u)",
			err, topic, buf_len);
	}
	return err;
}

static char* json_response_message(const char *cmd, int result, const char *msg)
{
	char *buf;
	cJSON *json;

	if (!(json = cJSON_CreateObject()))
		goto panic;

	cJSON_AddItemToObject(json, "command", cJSON_CreateString(cmd));
	cJSON_AddItemToObject(json, "result", cJSON_CreateString(result == 0 ? "OK" : "ERROR"));
	cJSON_AddItemToObject(json, "message", cJSON_CreateString(msg));

	if (!(buf = cJSON_Print(json)))
		goto panic;
	cJSON_Delete(json);
	return buf;

panic:
	if (json)
		cJSON_Delete(json);
	return NULL;
}

static void send_mqtt_command_response(const char *cmd, int result, const char *msg)
{
	char *buf = NULL;

	if (!cmd || !msg || !mqtt_client || strlen(cfg->mqtt_resp_topic) < 1)
		return;

	/* Generate status message */
	if (!(buf = json_response_message(cmd, result, msg))) {
		log_msg(LOG_WARNING,"json_response_message(): failed");
		return;
	}
	mqtt_publish_message(cfg->mqtt_resp_topic, buf, strlen(buf), mqtt_qos, 0,
			cfg->mqtt_resp_topic);
	free(buf);
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	int i;
	const char *t;
	int len = 0;

	log_msg(LOG_DEBUG, "MQTT incoming publish at topic %s with total length %u",
		topic, (unsigned int)tot_len);


	/* Check topic name for match of expected topics...*/
	incoming_topic = UNKNOWN_TOPIC;
	for (i = 0; topics[i].type; i++) {
		t = topics[i].str;
		if (!t)
			t = (const char *)cfg + topics[i].offset;
		if (*t == 0)
			continue;
		len = strlen(t);
		if (!strncmp(topic, t, len)) {
			incoming_topic = topics[i].type;
			break;
		}
	}

	if (incoming_topic == HA_CMD_TOPIC) {
		t = topic + len;
		if (str_to_int(t, &i, 10)) {
			if (i > 0 && i <= OUTPUT_COUNT)
				incoming_topic_idx = i;
			else
				incoming_topic = UNKNOWN_TOPIC;
		} else {
			incoming_topic = UNKNOWN_TOPIC;
		}
	}

	if (incoming_topic == UNKNOWN_TOPIC) {
		log_msg(LOG_NOTICE, "Incoming publish for unkown topic '%s': %lu bytes",
			topic, tot_len);
	}
}

static void incoming_ha_status(const u8_t *data, u16_t len)
{
	/* Handle incoming Home Assistant status messages (online/offline) */

	log_msg(LOG_INFO, "Home Assistant status message received: %d", len);

	if (memmem(data, len, "online", 6)) {
		log_msg(LOG_INFO, "Schedule resending HA MQTT Discovery messages");
		mqtt_ha_discovery = 1;
		t_mqtt_ha_discovery = get_absolute_time();
	}
}

static void incoming_ha_cmd(const u8_t *data, u16_t len)
{
	char *buf;
	/* Handle incoming Home Assistant commands */

	log_msg(LOG_INFO, "Home Assistant command message received: %d (output=%d)", len,
		incoming_topic_idx);

	buf = calloc(1, len + 1);
	if (buf) {
		memcpy(buf, data, len);
		log_msg(LOG_INFO, "HA cmd: '%s'", buf);
		free(buf);
	}
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	char cmd[MQTT_CMD_MAX_LEN + 1];
	const u8_t *end, *start;
	int l;


	log_msg(LOG_DEBUG, "MQTT incoming publish payload with length %d, flags %u\n",
		len, (unsigned int)flags);

	if (len < 1 || incoming_topic == UNKNOWN_TOPIC)
		return;

	if (incoming_topic == ERR_TOPIC) {
		strncopy(cmd, (const char*)data, sizeof(cmd));
		log_msg(LOG_ERR, "MQTT Error received: %s", cmd);
		return;
	}
	else if (incoming_topic == WARN_TOPIC) {
		strncopy(cmd, (const char*)data, sizeof(cmd));
		log_msg(LOG_WARNING, "MQTT Warning received: %s", cmd);
		return;
	}
	else if (incoming_topic == HA_TOPIC) {
		incoming_ha_status(data, len);
		return;
	}
	else if (incoming_topic == HA_CMD_TOPIC) {
		incoming_ha_cmd(data, len);
		return;
	}

	if (incoming_topic != CMD_TOPIC)
		return;

	/* Check for command prefix, if found skip past prefix */
	if ((start = memmem(data, len, "CMD:", 4))) {
		start += 4;
		l = len - (start - data);
		if (l < 1)
			return;
	} else {
		start = data;
		l = len;
	}
	/* Check for command suffix, if found ignore anything past it */
	if (!(end = memchr(start, ';', l)))
		end = start + l;
	if ((l = end - start) < 1)
		return;
	if (l >= sizeof(cmd))
		l = sizeof(cmd) - 1;
	memcpy(cmd, start, l);
	cmd[l] = 0;


	/* Check if should be command allowed */
	if (!cfg->mqtt_allow_scpi) {
		if (strncasecmp(cmd, "WRITE:", 6)) {
			log_msg(LOG_NOTICE, "MQTT SCPI commands not allowed: '%s'", cmd);
			return;
		}
	}

	if (mqtt_scpi_cmd_queued) {
		log_msg(LOG_NOTICE, "MQTT SCPI command queue full: '%s'", cmd);
		send_mqtt_command_response(cmd, 1, "SCPI command queue full");
	} else {
		log_msg(LOG_NOTICE, "MQTT SCPI command queued: '%s'", cmd);
		strncopy(mqtt_scpi_cmd, cmd, sizeof(mqtt_scpi_cmd));
		mqtt_scpi_cmd_queued = true;
	}

}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
	if (result != ERR_OK) {
		log_msg(LOG_WARNING, "MQTT failed to subscribe command topic: %d", result);
	}
}

static void mqtt_ha_sub_request_cb(void *arg, err_t result)
{
	if (result != ERR_OK) {
		log_msg(LOG_WARNING, "MQTT failed to subscribe HA Birth topic: %d", result);
	}
}

static void mqtt_ha_cmd_sub_request_cb(void *arg, err_t result)
{
	if (result != ERR_OK) {
		log_msg(LOG_WARNING, "MQTT failed to subscribe HA command topics: %d", result);
	}
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
	err_t err;
	char topics[100];

	t_mqtt_disconnect = get_absolute_time();

	if (status == MQTT_CONNECT_ACCEPTED) {
		log_msg(LOG_INFO, "MQTT connected to %s:%u", ipaddr_ntoa(&mqtt_server_ip),
			mqtt_server_port);
		mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
		if (strlen(cfg->mqtt_cmd_topic) > 0) {
			log_msg(LOG_INFO, "MQTT subscribe to command topic: %s", cfg->mqtt_cmd_topic);
			err = mqtt_subscribe(client, cfg->mqtt_cmd_topic, 1, mqtt_sub_request_cb, arg);
			if (err != ERR_OK)
				log_msg(LOG_WARNING, "MQTT subscribe to command topic failed: %d", err);
		}
		if (strlen(cfg->mqtt_err_topic) > 0) {
			log_msg(LOG_INFO, "MQTT subscribe to error topic: %s", cfg->mqtt_err_topic);
			err = mqtt_subscribe(client, cfg->mqtt_err_topic, 1, mqtt_sub_request_cb, arg);
			if (err != ERR_OK)
				log_msg(LOG_WARNING, "MQTT subscribe to error topic failed: %d", err);
		}
		if (strlen(cfg->mqtt_warn_topic) > 0) {
			log_msg(LOG_INFO, "MQTT subscribe to warning topic: %s", cfg->mqtt_warn_topic);
			err = mqtt_subscribe(client, cfg->mqtt_warn_topic, 1, mqtt_sub_request_cb, arg);
			if (err != ERR_OK)
				log_msg(LOG_WARNING, "MQTT subscribe to warning topic failed: %d", err);
		}
		if (strlen(cfg->mqtt_ha_discovery_prefix) > 0) {
			/* Subscribe to Home Assistant Birth topic */
			snprintf(mqtt_ha_birth_topic, sizeof(mqtt_ha_birth_topic), "%s/status",
				cfg->mqtt_ha_discovery_prefix);
			log_msg(LOG_INFO, "MQTT subscribe to HA Birth topic: %s", mqtt_ha_birth_topic);
			err = mqtt_subscribe(client, mqtt_ha_birth_topic, 1, mqtt_ha_sub_request_cb, arg);
			if (err != ERR_OK) {
				log_msg(LOG_WARNING, "MQTT subscribe failed: %d", err);
			}

			snprintf(mqtt_ha_base_topic, sizeof(mqtt_ha_base_topic), "%s/device/brickpico_%s",
				cfg->mqtt_ha_discovery_prefix, pico_serial_str());
			snprintf(mqtt_ha_cmd_base_topic, sizeof(mqtt_ha_cmd_base_topic), "%s/set/",
				mqtt_ha_base_topic);

			/* Subscribe to Home Assistant command topics */
			snprintf(topics, sizeof(topics), "%s+", mqtt_ha_cmd_base_topic);
			log_msg(LOG_INFO, "MQTT subscribe to HA command topics: %s", topics);
			err = mqtt_subscribe(client, topics, 1, mqtt_ha_cmd_sub_request_cb, arg);
			if (err != ERR_OK) {
				log_msg(LOG_WARNING, "MQTT subscribe failed: %d", err);
			}

			mqtt_ha_discovery = 5;
			t_mqtt_ha_discovery = get_absolute_time();
		}
	}
	else if (status == MQTT_CONNECT_DISCONNECTED) {
		log_msg(LOG_WARNING, "MQTT disconnected from %s", ipaddr_ntoa(&mqtt_server_ip));
		mqtt_reconnect = 15;
	}
	else if (status == MQTT_CONNECT_TIMEOUT) {
		log_msg(LOG_WARNING, "MQTT connect: timeout (%s)", ipaddr_ntoa(&mqtt_server_ip));
		mqtt_reconnect = 30;
	}
	else if (status == MQTT_CONNECT_REFUSED_USERNAME_PASS) {
		log_msg(LOG_WARNING, "MQTT connect: login failure (%s)", ipaddr_ntoa(&mqtt_server_ip));
	}
	else if (status == MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_) {
		log_msg(LOG_WARNING, "MQTT connect: not authorized (%s)", ipaddr_ntoa(&mqtt_server_ip));
		mqtt_reconnect = 180;
	}
	else {
		log_msg(LOG_WARNING, "MQTT connect: refused (status=%d) (%s)", status, ipaddr_ntoa(&mqtt_server_ip));
		mqtt_reconnect = 90;
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
		t_mqtt_disconnect = get_absolute_time();
		mqtt_reconnect = 30;
	}
}

static void mqtt_connect(mqtt_client_t *client)
{
#if TLS_SUPPORT
	static struct altcp_tls_config *tlsconfig = NULL;
#endif
	struct mqtt_connect_client_info_t ci;
	char client_id[32];
	uint16_t port = MQTT_PORT;
	err_t err;

	if (!client)
		return;

	mqtt_reconnect = 0;
	mqtt_ha_discovery = 0;

	/* Resolve domain name */
	cyw43_arch_lwip_begin();
	err = dns_gethostbyname(cfg->mqtt_server, &mqtt_server_ip, mqtt_dns_resolve_cb, NULL);
	cyw43_arch_lwip_end();
	if (err != ERR_OK) {
		if (err == ERR_INPROGRESS)
			log_msg(LOG_INFO, "Resolving DNS name: %s\n", cfg->mqtt_server);
		else
			log_msg(LOG_WARNING, "Failed to resolve MQTT server name: %s (%d)" ,
				cfg->mqtt_server, err);

		return;
	}

	/* Setup client connect info */
	memset(&ci, 0, sizeof(ci));
	snprintf(client_id, sizeof(client_id), "brickpico-%s_%s",
		BRICKPICO_BOARD, pico_serial_str());
	ci.client_id = client_id;
	ci.client_user = cfg->mqtt_user;
	ci.client_pass = cfg->mqtt_pass;
	ci.keep_alive = 60;
	ci.will_topic = NULL;
	ci.will_msg = NULL;
	ci.will_retain = 0;
	ci.will_qos = 0;

#if TLS_SUPPORT
	if (cfg->mqtt_tls) {
		if (!tlsconfig) {
			cyw43_arch_lwip_begin();
			tlsconfig = altcp_tls_create_config_client(NULL, 0);
			cyw43_arch_lwip_end();
			if (!tlsconfig)
				log_msg(LOG_WARNING, "altcp_tls_create_config_client(): failed");
		}
		ci.tls_config = tlsconfig;
		port = MQTT_TLS_PORT;;
	}
#endif
	if (cfg->mqtt_port > 0)
		port = cfg->mqtt_port;
	mqtt_server_port = port;

	log_msg(LOG_INFO, "MQTT Connecting to %s:%u%s",	cfg->mqtt_server, mqtt_server_port,
		cfg->mqtt_tls ? " (TLS)" : "");
	cyw43_arch_lwip_begin();
	err = mqtt_client_connect(mqtt_client, &mqtt_server_ip, mqtt_server_port,
				mqtt_connection_cb, 0, &ci);
	cyw43_arch_lwip_end();
	if (err != ERR_OK) {
		log_msg(LOG_WARNING,"mqtt_client_connect(): failed %d", err);
	}
}

void brickpico_setup_mqtt_client()
{
	if (mqtt_client)
		return;

	ip_addr_set_zero(&mqtt_server_ip);
	mqtt_reconnect = 0;
	mqtt_ha_discovery = 0;
	mqtt_ha_birth_topic[0] = 0;
	mqtt_ha_base_topic[0] = 0;
	mqtt_ha_cmd_base_topic[0] = 0;
	mqtt_scpi_cmd[0] = 0;

	cyw43_arch_lwip_begin();
	mqtt_client = mqtt_client_new();
	cyw43_arch_lwip_end();
	if (!mqtt_client) {
		log_msg(LOG_WARNING,"mqtt_client_new(): failed");
		return;
	}

	mqtt_connect(mqtt_client);
}

int brickpico_mqtt_client_active()
{
	return (mqtt_client == NULL ? 0 : 1);
}

void brickpico_mqtt_reconnect()
{
	if (mqtt_reconnect == 0)
		return;

	if (time_passed(&t_mqtt_disconnect, mqtt_reconnect * 1000)) {
		log_msg(LOG_INFO, "MQTT attempt reconnecting to server");
		cyw43_arch_lwip_begin();
		u8_t connected = mqtt_client_is_connected(mqtt_client);
		cyw43_arch_lwip_end();
		if (connected) {
			log_msg(LOG_INFO, "MQTT attempt disconnecting existing connection");
			cyw43_arch_lwip_begin();
			mqtt_disconnect(mqtt_client);
			cyw43_arch_lwip_end();
		}
		mqtt_connect(mqtt_client);
	}
}


static cJSON* brickpico_ha_component(const char *type, int idx)
{
	cJSON *o;
	char tmp[100];

	if (!(o = cJSON_CreateObject()))
		return NULL;

	if (!strncmp(type, "temp", 5)) {
		cJSON_AddItemToObject(o, "p", cJSON_CreateString("sensor"));
		cJSON_AddItemToObject(o, "name", cJSON_CreateString("Temperature (Pico)"));
		cJSON_AddItemToObject(o, "dev_cla", cJSON_CreateString("temperature"));
		cJSON_AddItemToObject(o, "unit_of_meas", cJSON_CreateString("°C"));
		cJSON_AddItemToObject(o, "val_tpl", cJSON_CreateString("{{ value_json.temp }}"));
		snprintf(tmp, sizeof(tmp), "%s_temp_int", pico_serial_str());
		cJSON_AddItemToObject(o, "uniq_id", cJSON_CreateString(tmp));
	}
	else if (!strncmp(type, "out", 4)) {
		cJSON_AddItemToObject(o, "p", cJSON_CreateString("light"));
		cJSON_AddItemToObject(o, "schema", cJSON_CreateString("template"));
		snprintf(tmp, sizeof(tmp), "%s", cfg->outputs[idx - 1].name);
		cJSON_AddItemToObject(o, "name", cJSON_CreateString(tmp));
		snprintf(tmp, sizeof(tmp), "{{ value_json.state%02d|lower }}", idx);
		cJSON_AddItemToObject(o, "stat_tpl", cJSON_CreateString(tmp));
		snprintf(tmp, sizeof(tmp), "{{ value_json.bri%02d|d }}", idx);
		cJSON_AddItemToObject(o, "bri_tpl", cJSON_CreateString(tmp));
		cJSON_AddItemToObject(o, "bri_scl", cJSON_CreateNumber(100));
//		cJSON_AddItemToObject(o, "brightness", cJSON_CreateTrue());
		cJSON_AddItemToObject(o, "cmd_on_tpl", cJSON_CreateString("ON,{{ brightness|d }}"));
		cJSON_AddItemToObject(o, "cmd_off_tpl", cJSON_CreateString("OFF"));
		snprintf(tmp, sizeof(tmp), "%s%02d", mqtt_ha_cmd_base_topic, idx);
		cJSON_AddItemToObject(o, "cmd_t", cJSON_CreateString(tmp));
//		cJSON_AddItemToObject(o, "payload_on", cJSON_CreateString("on"));
//		cJSON_AddItemToObject(o, "payload_off", cJSON_CreateString("off"));
		snprintf(tmp, sizeof(tmp), "%s_output_%02d", pico_serial_str(), idx);
		cJSON_AddItemToObject(o, "uniq_id", cJSON_CreateString(tmp));
	}

	return o;
}

static char* json_ha_discovery_message()
{
	char *buf;
	cJSON *json, *o, *c, *a;
	char tmp[100];

	if (!(json = cJSON_CreateObject()))
		return NULL;

	/* Device Section */
	if (!(o = cJSON_CreateObject()))
		goto panic;
	if (!(a = cJSON_CreateArray()))
		goto panic;
	cJSON_AddItemToArray(a, cJSON_CreateString(pico_serial_str()));
	cJSON_AddItemToObject(o, "ids", a);
	cJSON_AddItemToObject(o, "name", cJSON_CreateString(cfg->name));
	cJSON_AddItemToObject(o, "mf", cJSON_CreateString("TJKO Industries"));
	cJSON_AddItemToObject(o, "mdl", cJSON_CreateString("BrickPico"));
	cJSON_AddItemToObject(o, "mdl_id", cJSON_CreateString(BRICKPICO_MODEL));
	cJSON_AddItemToObject(o, "sn", cJSON_CreateString(pico_serial_str()));
	cJSON_AddItemToObject(o, "sw", cJSON_CreateString(BRICKPICO_VERSION));
	cJSON_AddItemToObject(json, "dev", o);

	/* Origin Section */
	if (!(o = cJSON_CreateObject()))
		goto panic;
	cJSON_AddItemToObject(o, "name", cJSON_CreateString("BrickPico MQTT"));
	cJSON_AddItemToObject(o, "sw", cJSON_CreateString(BRICKPICO_VERSION));
	cJSON_AddItemToObject(o, "url", cJSON_CreateString("https://github.com/tjko/brickpico/wiki"));
	cJSON_AddItemToObject(json, "o", o);

	/* Components Section */
	if (!(o = cJSON_CreateObject()))
		goto panic;
	if (!(c = brickpico_ha_component("temp", 0)))
		goto panic;
	cJSON_AddItemToObject(o, "temp_int0", c);
	for (int i = 1; i <= OUTPUT_COUNT; i++) {
		if (cfg->mqtt_pwm_mask & (1 << (i - 1))) {
			if (!(c = brickpico_ha_component("out", i)))
				goto panic;
			snprintf(tmp, sizeof(tmp), "output_%d", i);
			cJSON_AddItemToObject(o, tmp, c);
		}
	}
	cJSON_AddItemToObject(json, "cmps", o);

	snprintf(tmp, sizeof(tmp), "%s/state", mqtt_ha_base_topic);
	cJSON_AddItemToObject(json, "state_topic", cJSON_CreateString(tmp));


	if (!(buf = cJSON_Print(json)))
		goto panic;
	cJSON_Delete(json);
	return buf;

panic:
	cJSON_Delete(json);
	return NULL;
}


static void brickpico_mqtt_ha_discovery()
{
	char *buf;
	char topic[100];

	if (mqtt_ha_discovery == 0)
		return;

	if (!time_passed(&t_mqtt_ha_discovery, mqtt_ha_discovery * 1000))
		return;

	mqtt_ha_discovery = 0;

	if (!(buf = json_ha_discovery_message())) {
		log_msg(LOG_WARNING, "json_ha_discovery_message(): failed");
		return;
	}

	snprintf(topic, sizeof(topic), "%s/config", mqtt_ha_base_topic);
	log_msg(LOG_INFO, "Publish HA Discovery Message: %s (%d)", topic, strlen(buf));
	printf("---\n%s\n---\n", buf);
	mqtt_publish_message(topic, buf, strlen(buf), mqtt_qos, 0, topic);
	free(buf);
}

static char* json_ha_state_message()
{
	const struct brickpico_state *st = brickpico_state;
	cJSON *json;
	char *buf, name[32];

	if (!(json = cJSON_CreateObject()))
		return NULL;


	for (int i = 0; i < OUTPUT_COUNT; i++) {
		if (cfg->mqtt_pwm_mask & (1 << i)) {
			snprintf(name, sizeof(name), "state%02d", i + 1);
			cJSON_AddItemToObject(json, name, cJSON_CreateString(st->pwr[i] ? "ON" : "OFF"));
			snprintf(name, sizeof(name), "bri%02d", i + 1);
			cJSON_AddItemToObject(json, name, cJSON_CreateNumber(st->pwm[i]));
		}
	}

	cJSON_AddItemToObject(json, "temp", cJSON_CreateNumber(round_decimal(st->temp,1)));

	if (!(buf = cJSON_Print(json)))
		goto panic;
	cJSON_Delete(json);
	return buf;

panic:
	cJSON_Delete(json);
	return NULL;
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
		// cJSON_AddItemToObject(o, "name", cJSON_CreateString(cfg->outputs[i].name));
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
	char topic[64 + 10 + 1];
	char *buf = NULL;

	if (!mqtt_client)
		return;

	if (strlen(cfg->mqtt_status_topic) > 0) {
		/* Generate status message */
		if (!(buf = json_status_message())) {
			log_msg(LOG_WARNING,"json_status_message(): failed");
			return;
		}
		mqtt_publish_message(cfg->mqtt_status_topic, buf, strlen(buf), mqtt_qos, 0,
				cfg->mqtt_status_topic);
		free(buf);
	}
	else if (strlen(cfg->mqtt_ha_discovery_prefix) > 0) {
		if (mqtt_ha_discovery > 0)
			brickpico_mqtt_ha_discovery();
		/* Generate Home Assistant State message */
		if (!(buf = json_ha_state_message())) {
			log_msg(LOG_WARNING,"json_ha_state_message(): failed");
			return;
		}
		snprintf(topic, sizeof(topic), "%s/state", mqtt_ha_base_topic);
		printf("JSON start:\n%sJSONend\n", buf);
		mqtt_publish_message(topic, buf, strlen(buf), mqtt_qos, 0, topic);
		free(buf);
	}
}

void brickpico_mqtt_publish_duty()
{
	const struct brickpico_state *st = brickpico_state;
	char topic[MQTT_MAX_TOPIC_LEN + 8];
	char buf[64];

	if (!mqtt_client)
		return;

	if (strlen(cfg->mqtt_pwm_topic) > 0) {
		for (int i = 0; i < OUTPUT_COUNT; i++) {
			if (cfg->mqtt_pwm_mask & (1 << i)) {
				snprintf(topic, sizeof(topic), cfg->mqtt_pwm_topic, i + 1);
				snprintf(buf, sizeof(buf), "%u", (st->pwr[i] ? st->pwm[i] : 0));
				mqtt_publish_message(topic, buf, strlen(buf), mqtt_qos, 0,
						cfg->mqtt_pwm_topic);
			}
		}
	}
}

void brickpico_mqtt_publish_temp()
{
	const struct brickpico_state *st = brickpico_state;
	char buf[32];

	if (!mqtt_client || strlen(cfg->mqtt_temp_topic) < 1)
		return;

	snprintf(buf, sizeof(buf), "%.1lf", st->temp);
	mqtt_publish_message(cfg->mqtt_temp_topic, buf, strlen(buf), mqtt_qos, 0,
			cfg->mqtt_temp_topic);
}

void brickpico_mqtt_scpi_command()
{
	struct brickpico_state *st = brickpico_state;
	char cmd[MQTT_CMD_MAX_LEN + 1];
	int res;

	if (!mqtt_client || !mqtt_scpi_cmd_queued)
		return;

	strncopy(cmd, mqtt_scpi_cmd, sizeof(cmd));
	process_command(st, (struct brickpico_config *)cfg, cmd);
	if ((res = last_command_status()) == 0) {
		log_msg(LOG_INFO, "MQTT SCPI command successfull: '%s'", mqtt_scpi_cmd);
		send_mqtt_command_response(mqtt_scpi_cmd, res, "SCPI command successfull");
	} else {
		log_msg(LOG_NOTICE, "MQTT SCPI command failed: '%s' (%d)", mqtt_scpi_cmd, res);
		if (res == -113)
			send_mqtt_command_response(mqtt_scpi_cmd, res, "SCPI unknown command");
		else
			send_mqtt_command_response(mqtt_scpi_cmd, res, "SCPI command failed");
	}

	mqtt_scpi_cmd[0] = 0;
	mqtt_scpi_cmd_queued = false;
	update_core1_state();
}

#endif /* WIFI_SUPPORT */
