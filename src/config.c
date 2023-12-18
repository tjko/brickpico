/* config.c
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
#include <malloc.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "cJSON.h"
#include "pico_hal.h"
#ifdef WIFI_SUPPORT
#include "lwip/ip_addr.h"
#endif

#include "brickpico.h"


#define DEFAULT_MQTT_STATUS_INTERVAL 60

struct brickpico_config brickpico_config;
const struct brickpico_config *cfg = &brickpico_config;
auto_init_mutex(config_mutex_inst);
mutex_t *config_mutex = &config_mutex_inst;



void clear_config(struct brickpico_config *cfg)
{
	int i;

	mutex_enter_blocking(config_mutex);

	memset(cfg, 0, sizeof(struct brickpico_config));

	for (i = 0; i < OUTPUT_MAX_COUNT; i++) {
		struct pwm_output *o = &cfg->outputs[i];
		snprintf(o->name, sizeof(o->name), "Output %d", i + 1);
		o->min_pwm = 0;
		o->max_pwm = 100;
		o->default_pwm = 100;
		o->default_state = 0;
		o->type = 0;
	}

	for (i = 0; i < MAX_EVENT_COUNT; i++) {
		struct timer_event *e = &cfg->events[i];
		e->name[0] = 0;
		e->minute = -1;
		e->hour = -1;
		e->wday = 0;
		e->action = ACTION_NONE;
		e->mask = 0;
	}
	cfg->event_count = 0;

	cfg->local_echo = false;
	cfg->spi_active = false;
	cfg->serial_active = true;
	cfg->led_mode = 0;
	cfg->pwm_freq = 1000;
	strncopy(cfg->name, "brickpico1", sizeof(cfg->name));
	strncopy(cfg->display_type, "default", sizeof(cfg->display_type));
	strncopy(cfg->display_theme, "default", sizeof(cfg->display_theme));
	strncopy(cfg->display_logo, "default", sizeof(cfg->display_logo));
	strncopy(cfg->display_layout_r, "", sizeof(cfg->display_layout_r));
	strncopy(cfg->timezone, "", sizeof(cfg->timezone));
#ifdef WIFI_SUPPORT
	cfg->wifi_ssid[0] = 0;
	cfg->wifi_passwd[0] = 0;
	cfg->wifi_mode = 0;
	cfg->hostname[0] = 0;
	strncopy(cfg->wifi_country, "XX", sizeof(cfg->wifi_country));
	ip_addr_set_any(0, &cfg->syslog_server);
	ip_addr_set_any(0, &cfg->ntp_server);
	ip_addr_set_any(0, &cfg->ip);
	ip_addr_set_any(0, &cfg->netmask);
	ip_addr_set_any(0, &cfg->gateway);
	cfg->mqtt_server[0] = 0;
	cfg->mqtt_port = 0;
	cfg->mqtt_tls = true;
	cfg->mqtt_user[0] = 0;
	cfg->mqtt_pass[0] = 0;
	cfg->mqtt_status_topic[0] = 0;
	cfg->mqtt_status_interval = DEFAULT_MQTT_STATUS_INTERVAL;
	cfg->mqtt_cmd_topic[0] = 0;
#endif

	mutex_exit(config_mutex);
}


cJSON *config_to_json(const struct brickpico_config *cfg)
{
	cJSON *config = cJSON_CreateObject();
	cJSON *outputs, *events, *o;
	int i;

	if (!config)
		return NULL;

	cJSON_AddItemToObject(config, "id", cJSON_CreateString("brickpico-config-v1"));
	cJSON_AddItemToObject(config, "debug", cJSON_CreateNumber(get_debug_level()));
	cJSON_AddItemToObject(config, "log_level", cJSON_CreateNumber(get_log_level()));
	cJSON_AddItemToObject(config, "syslog_level", cJSON_CreateNumber(get_syslog_level()));
	cJSON_AddItemToObject(config, "local_echo", cJSON_CreateBool(cfg->local_echo));
	cJSON_AddItemToObject(config, "led_mode", cJSON_CreateNumber(cfg->led_mode));
	cJSON_AddItemToObject(config, "spi_active", cJSON_CreateNumber(cfg->spi_active));
	cJSON_AddItemToObject(config, "serial_active", cJSON_CreateNumber(cfg->serial_active));
	cJSON_AddItemToObject(config, "pwm_freq", cJSON_CreateNumber(cfg->pwm_freq));
	if (strlen(cfg->display_type) > 0)
		cJSON_AddItemToObject(config, "display_type", cJSON_CreateString(cfg->display_type));
	if (strlen(cfg->display_theme) > 0)
		cJSON_AddItemToObject(config, "display_theme", cJSON_CreateString(cfg->display_theme));
	if (strlen(cfg->display_logo) > 0)
		cJSON_AddItemToObject(config, "display_logo", cJSON_CreateString(cfg->display_logo));
	if (strlen(cfg->display_layout_r) > 0)
		cJSON_AddItemToObject(config, "display_layout_r", cJSON_CreateString(cfg->display_layout_r));
	if (strlen(cfg->name) > 0)
		cJSON_AddItemToObject(config, "name", cJSON_CreateString(cfg->name));
	if (strlen(cfg->timezone) > 0)
		cJSON_AddItemToObject(config, "timezone", cJSON_CreateString(cfg->timezone));

#ifdef WIFI_SUPPORT
	if (strlen(cfg->hostname) > 0)
		cJSON_AddItemToObject(config, "hostname", cJSON_CreateString(cfg->hostname));
	if (strlen(cfg->wifi_country) > 0)
		cJSON_AddItemToObject(config, "wifi_country", cJSON_CreateString(cfg->wifi_country));
	if (strlen(cfg->wifi_ssid) > 0)
		cJSON_AddItemToObject(config, "wifi_ssid", cJSON_CreateString(cfg->wifi_ssid));
	if (strlen(cfg->wifi_passwd) > 0) {
		char *p = base64encode(cfg->wifi_passwd);
		if (p) {
			cJSON_AddItemToObject(config, "wifi_passwd", cJSON_CreateString(p));
			free(p);
		}
	}
	if (cfg->wifi_mode != 0) {
		cJSON_AddItemToObject(config, "wifi_mode", cJSON_CreateNumber(cfg->wifi_mode));
	}
	if (!ip_addr_isany(&cfg->syslog_server))
		cJSON_AddItemToObject(config, "syslog_server", cJSON_CreateString(ipaddr_ntoa(&cfg->syslog_server)));
	if (!ip_addr_isany(&cfg->ntp_server))
		cJSON_AddItemToObject(config, "ntp_server", cJSON_CreateString(ipaddr_ntoa(&cfg->ntp_server)));
	if (!ip_addr_isany(&cfg->ip))
		cJSON_AddItemToObject(config, "ip", cJSON_CreateString(ipaddr_ntoa(&cfg->ip)));
	if (!ip_addr_isany(&cfg->netmask))
		cJSON_AddItemToObject(config, "netmask", cJSON_CreateString(ipaddr_ntoa(&cfg->netmask)));
	if (!ip_addr_isany(&cfg->gateway))
		cJSON_AddItemToObject(config, "gateway", cJSON_CreateString(ipaddr_ntoa(&cfg->gateway)));
	if (strlen(cfg->mqtt_server) > 0)
		cJSON_AddItemToObject(config, "mqtt_server", cJSON_CreateString(cfg->mqtt_server));
	if (cfg->mqtt_port > 0)
		cJSON_AddItemToObject(config, "mqtt_port", cJSON_CreateNumber(cfg->mqtt_port));
	if (strlen(cfg->mqtt_user) > 0)
		cJSON_AddItemToObject(config, "mqtt_user", cJSON_CreateString(cfg->mqtt_user));
	if (strlen(cfg->mqtt_pass) > 0) {
		char *p = base64encode(cfg->mqtt_pass);
		if (p) {
			cJSON_AddItemToObject(config, "mqtt_pass", cJSON_CreateString(p));
			free(p);
		}
	}
	if (strlen(cfg->mqtt_status_topic) > 0)
		cJSON_AddItemToObject(config, "mqtt_status_topic", cJSON_CreateString(cfg->mqtt_status_topic));
	if (strlen(cfg->mqtt_cmd_topic) > 0)
		cJSON_AddItemToObject(config, "mqtt_cmd_topic", cJSON_CreateString(cfg->mqtt_cmd_topic));
	if (cfg->mqtt_tls != true)
		cJSON_AddItemToObject(config, "mqtt_tls", cJSON_CreateNumber(cfg->mqtt_tls));
	if (cfg->mqtt_status_interval != DEFAULT_MQTT_STATUS_INTERVAL)
		cJSON_AddItemToObject(config, "mqtt_status_interval", cJSON_CreateNumber(cfg->mqtt_status_interval));
#endif

	/* PWM Outputs */
	outputs = cJSON_CreateArray();
	if (!outputs)
		goto panic;
	for (i = 0; i < OUTPUT_COUNT; i++) {
		const struct pwm_output *f = &cfg->outputs[i];

		o = cJSON_CreateObject();
		if (!o)
			goto panic;
		cJSON_AddItemToObject(o, "id", cJSON_CreateNumber(i));
		cJSON_AddItemToObject(o, "name", cJSON_CreateString(f->name));
		cJSON_AddItemToObject(o, "min_pwm", cJSON_CreateNumber(f->min_pwm));
		cJSON_AddItemToObject(o, "max_pwm", cJSON_CreateNumber(f->max_pwm));
		cJSON_AddItemToObject(o, "default_pwm", cJSON_CreateNumber(f->default_pwm));
		cJSON_AddItemToObject(o, "default_state", cJSON_CreateNumber(f->default_state));
		cJSON_AddItemToObject(o, "type", cJSON_CreateNumber(f->type));
		cJSON_AddItemToArray(outputs, o);
	}
	cJSON_AddItemToObject(config, "outputs", outputs);

	/* Timers */
	if ((events = cJSON_CreateArray()) == NULL)
		goto panic;
	for (i = 0; i < cfg->event_count; i++) {
		const struct timer_event *e = &cfg->events[i];

		if ((o = cJSON_CreateObject()) == NULL)
			goto panic;
		cJSON_AddItemToObject(o, "name", cJSON_CreateString(e->name));
		cJSON_AddItemToObject(o, "minute", cJSON_CreateNumber(e->minute));
		cJSON_AddItemToObject(o, "hour", cJSON_CreateNumber(e->hour));
		cJSON_AddItemToObject(o, "wday", cJSON_CreateNumber(e->wday));
		cJSON_AddItemToObject(o, "action", cJSON_CreateNumber(e->action));
		cJSON_AddItemToObject(o, "mask", cJSON_CreateNumber(e->mask));
		cJSON_AddItemToArray(events, o);
	}
	cJSON_AddItemToObject(config, "timers", events);

	return config;

panic:
	cJSON_Delete(config);
	return NULL;
}


int json_to_config(cJSON *config, struct brickpico_config *cfg)
{
	cJSON *ref, *item;
	int id;
	const char *name, *val;

	if (!config || !cfg)
		return -1;

	mutex_enter_blocking(config_mutex);

	/* Parse JSON configuration */

	if ((ref = cJSON_GetObjectItem(config, "id")))
		log_msg(LOG_INFO, "Config version: %s", ref->valuestring);
	if ((ref = cJSON_GetObjectItem(config, "debug")))
		set_debug_level(cJSON_GetNumberValue(ref));
	if ((ref = cJSON_GetObjectItem(config, "log_level")))
		set_log_level(cJSON_GetNumberValue(ref));
	if ((ref = cJSON_GetObjectItem(config, "syslog_level")))
		set_syslog_level(cJSON_GetNumberValue(ref));
	if ((ref = cJSON_GetObjectItem(config, "local_echo")))
		cfg->local_echo = (cJSON_IsTrue(ref) ? true : false);
	if ((ref = cJSON_GetObjectItem(config, "led_mode")))
		cfg->led_mode = cJSON_GetNumberValue(ref);
	if ((ref = cJSON_GetObjectItem(config, "spi_active")))
		cfg->spi_active = cJSON_GetNumberValue(ref);
	if ((ref = cJSON_GetObjectItem(config, "serial_active")))
		cfg->serial_active = cJSON_GetNumberValue(ref);
	if ((ref = cJSON_GetObjectItem(config, "pwm_freq")))
		cfg->pwm_freq = cJSON_GetNumberValue(ref);
	if ((ref = cJSON_GetObjectItem(config, "display_type"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->display_type, val, sizeof(cfg->display_type));
	}
	if ((ref = cJSON_GetObjectItem(config, "display_theme"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->display_theme, val, sizeof(cfg->display_theme));
	}
	if ((ref = cJSON_GetObjectItem(config, "display_logo"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->display_logo, val, sizeof(cfg->display_logo));
	}
	if ((ref = cJSON_GetObjectItem(config, "display_layout_r"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->display_layout_r, val, sizeof(cfg->display_layout_r));
	}
	if ((ref = cJSON_GetObjectItem(config, "name"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->name, val, sizeof(cfg->name));
	}
	if ((ref = cJSON_GetObjectItem(config, "timezone"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->timezone, val, sizeof(cfg->timezone));
	}

#ifdef WIFI_SUPPORT
	if ((ref = cJSON_GetObjectItem(config, "hostname"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->hostname, val, sizeof(cfg->hostname));
	}
	if ((ref = cJSON_GetObjectItem(config, "wifi_country"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->wifi_country, val, sizeof(cfg->wifi_country));
	}
	if ((ref = cJSON_GetObjectItem(config, "wifi_ssid"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->wifi_ssid, val, sizeof(cfg->wifi_ssid));
	}
	if ((ref = cJSON_GetObjectItem(config, "wifi_passwd"))) {
		if ((val = cJSON_GetStringValue(ref))) {
			char *p = base64decode(val);
			if (p) {
				strncopy(cfg->wifi_passwd, p, sizeof(cfg->wifi_passwd));
				free(p);
			}
		}
	}
	if ((ref = cJSON_GetObjectItem(config, "wifi_mode"))) {
		cfg->wifi_mode = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "syslog_server"))) {
		if ((val = cJSON_GetStringValue(ref)))
			ipaddr_aton(val, &cfg->syslog_server);
	}
	if ((ref = cJSON_GetObjectItem(config, "ntp_server"))) {
		if ((val = cJSON_GetStringValue(ref)))
			ipaddr_aton(val, &cfg->ntp_server);
	}
	if ((ref = cJSON_GetObjectItem(config, "ip"))) {
		if ((val = cJSON_GetStringValue(ref)))
			ipaddr_aton(val, &cfg->ip);
	}
	if ((ref = cJSON_GetObjectItem(config, "netmask"))) {
		if ((val = cJSON_GetStringValue(ref)))
			ipaddr_aton(val, &cfg->netmask);
	}
	if ((ref = cJSON_GetObjectItem(config, "gateway"))) {
		if ((val = cJSON_GetStringValue(ref)))
			ipaddr_aton(val, &cfg->gateway);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_server"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->mqtt_server, val, sizeof(cfg->mqtt_server));
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_port"))) {
		cfg->mqtt_port = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_tls"))) {
		cfg->mqtt_tls = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_user"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->mqtt_user, val, sizeof(cfg->mqtt_user));
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_pass"))) {
		if ((val = cJSON_GetStringValue(ref))) {
			char *p = base64decode(val);
			if (p) {
				strncopy(cfg->mqtt_pass, p, sizeof(cfg->mqtt_pass));
				free(p);
			}
		}
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_status_topic"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->mqtt_status_topic, val, sizeof(cfg->mqtt_status_topic));
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_cmd_topic"))) {
		if ((val = cJSON_GetStringValue(ref)))
			strncopy(cfg->mqtt_cmd_topic, val, sizeof(cfg->mqtt_cmd_topic));
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_status_interval"))) {
		cfg->mqtt_status_interval = cJSON_GetNumberValue(ref);
	}
#endif

	/* PWM output configurations */
	ref = cJSON_GetObjectItem(config, "outputs");
	cJSON_ArrayForEach(item, ref) {
		id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "id"));
		if (id >= 0 && id < OUTPUT_COUNT) {
			struct pwm_output *f = &cfg->outputs[id];

			name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
			if (name) strncopy(f->name, name ,sizeof(f->name));

			if ((ref = cJSON_GetObjectItem(item, "min_pwm"))) {
				f->min_pwm = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "max_pwm"))) {
				f->max_pwm = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "default_pwm"))) {
				f->default_pwm = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "default_state"))) {
				f->default_state = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "type"))) {
				f->type = cJSON_GetNumberValue(ref);
			}
		}
	}

	/* Timers */
	cfg->event_count = 0;
	ref = cJSON_GetObjectItem(config, "timers");
	cJSON_ArrayForEach(item, ref) {
		if (cfg->event_count  < MAX_EVENT_COUNT) {
			struct timer_event *e = &cfg->events[cfg->event_count++];

			e->name[0] = 0;
			name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
			if (name) strncopy(e->name, name ,sizeof(e->name));

			if ((ref = cJSON_GetObjectItem(item, "minute"))) {
				e->minute = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "hour"))) {
				e->hour = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "wday"))) {
				e->wday = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "action"))) {
				e->action = cJSON_GetNumberValue(ref);
			}
			if ((ref = cJSON_GetObjectItem(item, "mask"))) {
				e->mask = cJSON_GetNumberValue(ref);
			}
		}
	}


	mutex_exit(config_mutex);
	return 0;
}


void read_config(bool multicore)
{
	cJSON *config = NULL;
	int res;
	uint32_t file_size;
	char  *buf = NULL;


	log_msg(LOG_INFO, "Reading configuration...");

	if (multicore)
		multicore_lockout_start_blocking();
	res = flash_read_file(&buf, &file_size, "brickpico.cfg", true);
	if (multicore)
		multicore_lockout_end_blocking();
	if (res == 0 && buf != NULL) {
		/* parse saved config... */
		config = cJSON_Parse(buf);
		if (!config) {
			const char *error_str = cJSON_GetErrorPtr();
			log_msg(LOG_ERR, "Failed to parse saved config: %s",
				(error_str ? error_str : "") );
		}
		free(buf);
	}

	clear_config(&brickpico_config);

	if (!config) {
		log_msg(LOG_NOTICE, "Using default configuration...");
		return;
	}

        /* Parse JSON configuration */
	if (json_to_config(config, &brickpico_config) < 0) {
		log_msg(LOG_ERR, "Error parsing JSON configuration");
	}

	cJSON_Delete(config);
}


void save_config(bool multicore)
{
	cJSON *config;
	char *str;

	log_msg(LOG_NOTICE, "Saving configuration...");

	config = config_to_json(cfg);
	if (!config) {
		log_msg(LOG_ALERT, "Out of memory!");
		return;
	}

	if ((str = cJSON_Print(config)) == NULL) {
		log_msg(LOG_ERR, "Failed to generate JSON output");
	} else {
		uint32_t config_size = strlen(str) + 1;
		if (multicore)
			multicore_lockout_start_blocking();
		flash_write_file(str, config_size, "brickpico.cfg");
		if (multicore)
			multicore_lockout_end_blocking();
		free(str);
	}

	cJSON_Delete(config);
}


void print_config()
{
	cJSON *config;
	char *str;

	config = config_to_json(cfg);
	if (!config) {
		log_msg(LOG_ALERT, "Out of memory");
		return;
	}

	if ((str = cJSON_Print(config)) == NULL) {
		log_msg(LOG_ERR, "Failed to generate JSON output");
	} else {
		printf("Current Configuration:\n%s\n---\n", str);
		free(str);
	}

	cJSON_Delete(config);
}


void delete_config(bool multicore)
{
	int res;

	if (multicore)
		multicore_lockout_start_blocking();
	res = flash_delete_file("fanpico.cfg");
	if (multicore)
		multicore_lockout_end_blocking();

	if (res) {
		log_msg(LOG_ERR, "Failed to delete configuration.");
	}
}
