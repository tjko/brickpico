/* config.c
   Copyright (C) 2023-2025 Timo Kokkonen <tjko@iki.fi>

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
#include <string.h>
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "cJSON.h"
#include "pico_sensor_lib.h"
#ifdef WIFI_SUPPORT
#include "lwip/ip_addr.h"
#endif

#include "brickpico.h"


struct brickpico_config brickpico_config;
const struct brickpico_config *cfg = &brickpico_config;
auto_init_mutex(config_mutex_inst);
mutex_t *config_mutex = &config_mutex_inst;



enum vsensor_modes str2vsmode(const char *s)
{
	int ret = VSMODE_MANUAL;

	if (s) {
		if (!strncasecmp(s, "max", 3))
			ret = VSMODE_MAX;
		else if (!strncasecmp(s, "min", 3))
			ret = VSMODE_MIN;
		else if (!strncasecmp(s, "avg", 3))
			ret = VSMODE_AVG;
		else if (!strncasecmp(s, "delta", 5))
			ret = VSMODE_DELTA;
		else if (!strncasecmp(s, "i2c", 3))
			ret = VSMODE_I2C;
	}

	return ret;
}

const char* vsmode2str(enum vsensor_modes mode)
{
	if (mode == VSMODE_MAX)
		return "max";
	else if (mode == VSMODE_MIN)
		return "min";
	else if (mode == VSMODE_AVG)
		return "avg";
	else if (mode == VSMODE_DELTA)
		return "delta";
	else if (mode == VSMODE_I2C)
		return "i2c";

	return "manual";
}



void json2effect(cJSON *item, enum light_effect_types *effect, void **effect_ctx)
{
	cJSON *args;
	const char *a = "";

	*effect = str2effect(cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")));
	if ((args = cJSON_GetObjectItem(item, "args")))
		a = cJSON_GetStringValue(args);
	*effect_ctx = effect_parse_args(*effect, a);
	if (!*effect_ctx)
		*effect = EFFECT_NONE;
}


cJSON* effect2json(enum light_effect_types effect, void *effect_ctx)
{
	cJSON *o;
	char *s;

	if ((o = cJSON_CreateObject()) == NULL)
		return NULL;

	cJSON_AddItemToObject(o, "name", cJSON_CreateString(effect2str(effect)));
	s = effect_print_args(effect, effect_ctx);
	cJSON_AddItemToObject(o, "args", cJSON_CreateString(s ? s : ""));
	if (s)
		free(s);

	return o;
}


void json2vsensors(cJSON *item, uint8_t *s)
{
	cJSON *o;
	int i,val;
	int count = 0;

	for (i = 0; i < VSENSOR_SOURCE_MAX_COUNT; i++)
		s[i] = 0;

	cJSON_ArrayForEach(o, item) {
		val = cJSON_GetNumberValue(o);
		if (count < VSENSOR_SOURCE_MAX_COUNT && val >= 1 && val <= VSENSOR_COUNT) {
			s[count++] = val;
		}
	}
}


cJSON* vsensors2json(const uint8_t *s)
{
	int i;
	cJSON *o;

	if ((o = cJSON_CreateArray()) == NULL)
		return NULL;

	for (i = 0; i < VSENSOR_SOURCE_MAX_COUNT; i++) {
		if (s[i]) {
			cJSON_AddItemToArray(o, cJSON_CreateNumber(s[i]));
		}
	}
	return o;
}



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
		o->effect = EFFECT_NONE;
		o->effect_ctx = NULL;
	}

	for (i = 0; i < VSENSOR_MAX_COUNT; i++) {
		struct vsensor_input *vs = &cfg->vsensors[i];

		vs->name[0] = 0;
		vs->mode = VSMODE_MANUAL;
		vs->default_temp = 0.0;
		vs->timeout = 60;
		for (int j = 0; j < VSENSOR_SOURCE_MAX_COUNT; j++)
			vs->sensors[j] = 0;
		vs->i2c_type = 0;
		vs->i2c_addr = 0;

		cfg->i2c_context[i] = NULL;
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
	cfg->i2c_speed = I2C_DEFAULT_SPEED;
	cfg->led_mode = 0;
	cfg->pwm_freq = 1000;
	cfg->adc_ref_voltage = 3.3;
	cfg->temp_offset = 0.0;
	cfg->temp_coefficient = 1.0;
	strncopy(cfg->name, "brickpico1", sizeof(cfg->name));
	strncopy(cfg->display_type, "default", sizeof(cfg->display_type));
	strncopy(cfg->display_theme, "default", sizeof(cfg->display_theme));
	strncopy(cfg->display_logo, "default", sizeof(cfg->display_logo));
	strncopy(cfg->display_layout_r, "", sizeof(cfg->display_layout_r));
	strncopy(cfg->gamma, "", sizeof(cfg->gamma));
	strncopy(cfg->timezone, "", sizeof(cfg->timezone));
#ifdef WIFI_SUPPORT
	cfg->wifi_ssid[0] = 0;
	cfg->wifi_passwd[0] = 0;
	strncopy(cfg->wifi_auth_mode, "default", sizeof(cfg->wifi_auth_mode));
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
	cfg->mqtt_allow_scpi = false;
	cfg->mqtt_user[0] = 0;
	cfg->mqtt_pass[0] = 0;
	cfg->mqtt_status_topic[0] = 0;
	cfg->mqtt_cmd_topic[0] = 0;
	cfg->mqtt_resp_topic[0] = 0;
	cfg->mqtt_err_topic[0] = 0;
	cfg->mqtt_warn_topic[0] = 0;
	cfg->mqtt_pwm_topic[0] = 0;
	cfg->mqtt_temp_topic[0] = 0;
	cfg->mqtt_status_interval = DEFAULT_MQTT_STATUS_INTERVAL;
	cfg->mqtt_temp_interval = DEFAULT_MQTT_TEMP_INTERVAL;
	cfg->mqtt_pwm_interval = DEFAULT_MQTT_PWM_INTERVAL;
	cfg->mqtt_pwm_mask = 0;
	cfg->mqtt_ha_discovery_prefix[0] = 0;
	cfg->telnet_active = false;
	cfg->telnet_auth = true;
	cfg->telnet_raw_mode = false;
	cfg->telnet_port = 0;
	cfg->telnet_user[0] = 0;
	cfg->telnet_pwhash[0] = 0;
#endif

	mutex_exit(config_mutex);
}


#define STRING_TO_JSON(name, var) {					\
	if (strlen(var) > 0)						\
		cJSON_AddItemToObject(config, name, cJSON_CreateString(var)); \
}

cJSON *config_to_json(const struct brickpico_config *cfg)
{
	cJSON *config = cJSON_CreateObject();
	cJSON *outputs, *events, *vsensors, *o;
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
	cJSON_AddItemToObject(config, "i2c_speed", cJSON_CreateNumber(cfg->i2c_speed));
	cJSON_AddItemToObject(config, "pwm_freq", cJSON_CreateNumber(cfg->pwm_freq));
	STRING_TO_JSON("display_type", cfg->display_type);
	STRING_TO_JSON("display_theme", cfg->display_theme);
	STRING_TO_JSON("display_logo", cfg->display_logo);
	STRING_TO_JSON("display_layout_r", cfg->display_layout_r);
	STRING_TO_JSON("gamma", cfg->gamma);
	STRING_TO_JSON("name", cfg->name);
	STRING_TO_JSON("timezone", cfg->timezone);

#ifdef WIFI_SUPPORT
	STRING_TO_JSON("hostname", cfg->hostname);
	STRING_TO_JSON("wifi_country", cfg->wifi_country);
	STRING_TO_JSON("wifi_ssid", cfg->wifi_ssid);
	if (strlen(cfg->wifi_passwd) > 0) {
		char *p = base64encode(cfg->wifi_passwd);
		if (p) {
			cJSON_AddItemToObject(config, "wifi_passwd", cJSON_CreateString(p));
			free(p);
		}
	}
	STRING_TO_JSON("wifi_auth_mode", cfg->wifi_auth_mode);
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
	STRING_TO_JSON("mqtt_server", cfg->mqtt_server);
	if (cfg->mqtt_port > 0)
		cJSON_AddItemToObject(config, "mqtt_port", cJSON_CreateNumber(cfg->mqtt_port));
	STRING_TO_JSON("mqtt_user", cfg->mqtt_user);
	if (strlen(cfg->mqtt_pass) > 0) {
		char *p = base64encode(cfg->mqtt_pass);
		if (p) {
			cJSON_AddItemToObject(config, "mqtt_pass", cJSON_CreateString(p));
			free(p);
		}
	}
	STRING_TO_JSON("mqtt_status_topic", cfg->mqtt_status_topic);
	STRING_TO_JSON("mqtt_cmd_topic", cfg->mqtt_cmd_topic);
	STRING_TO_JSON("mqtt_resp_topic", cfg->mqtt_resp_topic);
	STRING_TO_JSON("mqtt_err_topic", cfg->mqtt_err_topic);
	STRING_TO_JSON("mqtt_warn_topic", cfg->mqtt_warn_topic);
	STRING_TO_JSON("mqtt_pwm_topic", cfg->mqtt_pwm_topic);
	STRING_TO_JSON("mqtt_temp_topic", cfg->mqtt_temp_topic);
	if (cfg->mqtt_tls != true)
		cJSON_AddItemToObject(config, "mqtt_tls", cJSON_CreateNumber(cfg->mqtt_tls));
	if (cfg->mqtt_allow_scpi == true)
		cJSON_AddItemToObject(config, "mqtt_allow_scpi",
				cJSON_CreateNumber(cfg->mqtt_allow_scpi));
	if (cfg->mqtt_status_interval != DEFAULT_MQTT_STATUS_INTERVAL)
		cJSON_AddItemToObject(config, "mqtt_status_interval",
				cJSON_CreateNumber(cfg->mqtt_status_interval));
	if (cfg->mqtt_temp_interval != DEFAULT_MQTT_TEMP_INTERVAL)
		cJSON_AddItemToObject(config, "mqtt_temp_interval",
				cJSON_CreateNumber(cfg->mqtt_temp_interval));
	if (cfg->mqtt_pwm_interval != DEFAULT_MQTT_PWM_INTERVAL)
		cJSON_AddItemToObject(config, "mqtt_pwm_interval",
				cJSON_CreateNumber(cfg->mqtt_pwm_interval));
	if (cfg->mqtt_pwm_mask)
		cJSON_AddItemToObject(config, "mqtt_pwm_mask",
				cJSON_CreateString(
					bitmask_to_str(cfg->mqtt_pwm_mask, OUTPUT_COUNT,
						1, true)));
	STRING_TO_JSON("mqtt_ha_discovery_prefix", cfg->mqtt_ha_discovery_prefix);
	if (cfg->telnet_active)
		cJSON_AddItemToObject(config, "telnet_active", cJSON_CreateNumber(cfg->telnet_active));
	if (cfg->telnet_auth != true)
		cJSON_AddItemToObject(config, "telnet_auth", cJSON_CreateNumber(cfg->telnet_auth));
	if (cfg->telnet_raw_mode)
		cJSON_AddItemToObject(config, "telnet_raw_mode", cJSON_CreateNumber(cfg->telnet_raw_mode));
	if (cfg->telnet_port > 0)
		cJSON_AddItemToObject(config, "telnet_port", cJSON_CreateNumber(cfg->telnet_port));
	STRING_TO_JSON("telnet_user", cfg->telnet_user);
	STRING_TO_JSON("telnet_pwhash", cfg->telnet_pwhash);
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
		cJSON_AddItemToObject(o, "effect", effect2json(f->effect, f->effect_ctx));
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

	/* Virtual Sensors */
	vsensors = cJSON_CreateArray();
	if (!vsensors)
		goto panic;
	for (i = 0; i < VSENSOR_COUNT; i++) {
		const struct vsensor_input *s = &cfg->vsensors[i];

		o = cJSON_CreateObject();
		if (!o)
			goto panic;
		cJSON_AddItemToObject(o, "id", cJSON_CreateNumber(i));
		cJSON_AddItemToObject(o, "name", cJSON_CreateString(s->name));
		cJSON_AddItemToObject(o, "mode", cJSON_CreateString(vsmode2str(s->mode)));
		if (s->mode == VSMODE_MANUAL) {
			cJSON_AddItemToObject(o, "default_temp", cJSON_CreateNumber(s->default_temp));
			cJSON_AddItemToObject(o, "timeout", cJSON_CreateNumber(s->timeout));
		} else if (s->mode == VSMODE_I2C) {
			cJSON_AddItemToObject(o, "i2c_type",
					cJSON_CreateString(i2c_sensor_type_str(s->i2c_type)));
			cJSON_AddItemToObject(o, "i2c_addr",
					cJSON_CreateNumber(s->i2c_addr));
		} else {
			cJSON_AddItemToObject(o, "sensors", vsensors2json(s->sensors));
		}
		cJSON_AddItemToArray(vsensors, o);
	}
	cJSON_AddItemToObject(config, "vsensors", vsensors);

        return config;

panic:
	cJSON_Delete(config);
	return NULL;
}



#define JSON_TO_STRING(name, var, len) { \
	if ((ref = cJSON_GetObjectItem(config, name))) {	\
		if ((val = cJSON_GetStringValue(ref)))		\
			strncopy(var, val, len);		\
	}							\
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
	if ((ref = cJSON_GetObjectItem(config, "i2c_speed")))
		cfg->i2c_speed = cJSON_GetNumberValue(ref);
	if ((ref = cJSON_GetObjectItem(config, "pwm_freq")))
		cfg->pwm_freq = cJSON_GetNumberValue(ref);
	JSON_TO_STRING("display_type", cfg->display_type, sizeof(cfg->display_type));
	JSON_TO_STRING("display_theme", cfg->display_theme, sizeof(cfg->display_theme));
	JSON_TO_STRING("display_logo", cfg->display_logo, sizeof(cfg->display_logo));
	JSON_TO_STRING("display_layout_r", cfg->display_layout_r, sizeof(cfg->display_layout_r));
	JSON_TO_STRING("gamma", cfg->gamma, sizeof(cfg->gamma));
	JSON_TO_STRING("name", cfg->name, sizeof(cfg->name));
	JSON_TO_STRING("timezone", cfg->timezone, sizeof(cfg->timezone));

#ifdef WIFI_SUPPORT
	JSON_TO_STRING("hostname", cfg->hostname, sizeof(cfg->hostname));
	JSON_TO_STRING("wifi_country", cfg->wifi_country, sizeof(cfg->wifi_country));
	JSON_TO_STRING("wifi_ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
	if ((ref = cJSON_GetObjectItem(config, "wifi_passwd"))) {
		if ((val = cJSON_GetStringValue(ref))) {
			char *p = base64decode(val);
			if (p) {
				strncopy(cfg->wifi_passwd, p, sizeof(cfg->wifi_passwd));
				free(p);
			}
		}
	}
	JSON_TO_STRING("wifi_auth_mode", cfg->wifi_auth_mode, sizeof(cfg->wifi_auth_mode));
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
	JSON_TO_STRING("mqtt_server", cfg->mqtt_server, sizeof(cfg->mqtt_server));
	if ((ref = cJSON_GetObjectItem(config, "mqtt_port"))) {
		cfg->mqtt_port = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_tls"))) {
		cfg->mqtt_tls = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_allow_scpi"))) {
		cfg->mqtt_allow_scpi = cJSON_GetNumberValue(ref);
	}
	JSON_TO_STRING("mqtt_user", cfg->mqtt_user, sizeof(cfg->mqtt_user));
	if ((ref = cJSON_GetObjectItem(config, "mqtt_pass"))) {
		if ((val = cJSON_GetStringValue(ref))) {
			char *p = base64decode(val);
			if (p) {
				strncopy(cfg->mqtt_pass, p, sizeof(cfg->mqtt_pass));
				free(p);
			}
		}
	}
	JSON_TO_STRING("mqtt_status_topic", cfg->mqtt_status_topic, sizeof(cfg->mqtt_status_topic));
	JSON_TO_STRING("mqtt_cmd_topic", cfg->mqtt_cmd_topic, sizeof(cfg->mqtt_cmd_topic));
	JSON_TO_STRING("mqtt_resp_topic", cfg->mqtt_resp_topic, sizeof(cfg->mqtt_resp_topic));
	JSON_TO_STRING("mqtt_err_topic", cfg->mqtt_err_topic, sizeof(cfg->mqtt_err_topic));
	JSON_TO_STRING("mqtt_warn_topic", cfg->mqtt_warn_topic, sizeof(cfg->mqtt_warn_topic));
	JSON_TO_STRING("mqtt_pwm_topic", cfg->mqtt_pwm_topic, sizeof(cfg->mqtt_pwm_topic));
	JSON_TO_STRING("mqtt_temp_topic", cfg->mqtt_temp_topic, sizeof(cfg->mqtt_temp_topic));
	if ((ref = cJSON_GetObjectItem(config, "mqtt_status_interval"))) {
		cfg->mqtt_status_interval = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_temp_interval"))) {
		cfg->mqtt_temp_interval = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_pwm_interval"))) {
		cfg->mqtt_pwm_interval = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "mqtt_pwm_mask"))) {
		uint32_t m;
		if (!str_to_bitmask(cJSON_GetStringValue(ref), OUTPUT_COUNT, &m, 1))
			cfg->mqtt_pwm_mask = m;
	}
	JSON_TO_STRING("mqtt_ha_discovery_prefix", cfg->mqtt_ha_discovery_prefix, sizeof(cfg->mqtt_ha_discovery_prefix));
	if ((ref = cJSON_GetObjectItem(config, "telnet_active"))) {
		cfg->telnet_active = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "telnet_auth"))) {
		cfg->telnet_auth = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "telnet_raw_mode"))) {
		cfg->telnet_raw_mode = cJSON_GetNumberValue(ref);
	}
	if ((ref = cJSON_GetObjectItem(config, "telnet_port"))) {
		cfg->telnet_port = cJSON_GetNumberValue(ref);
	}
	JSON_TO_STRING("telnet_user", cfg->telnet_user, sizeof(cfg->telnet_user));
	JSON_TO_STRING("telnet_pwhash", cfg->telnet_pwhash, sizeof(cfg->telnet_pwhash));
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
			if ((ref = cJSON_GetObjectItem(item, "effect"))) {
				json2effect(ref, &f->effect, &f->effect_ctx);
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

	/* Virtual Sensor configurations */
	ref = cJSON_GetObjectItem(config, "vsensors");
	cJSON_ArrayForEach(item, ref) {
		cJSON *r;
		id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "id"));
		if (id >= 0 && id < VSENSOR_COUNT) {
			struct vsensor_input *s = &cfg->vsensors[id];

			name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
			if (name) strncopy(s->name, name ,sizeof(s->name));

			s->mode = str2vsmode(cJSON_GetStringValue(cJSON_GetObjectItem(item, "mode")));
			if (s->mode == VSMODE_MANUAL) {
				if ((r = cJSON_GetObjectItem(item, "default_temp")))
					s->default_temp = cJSON_GetNumberValue(r);
				if ((r = cJSON_GetObjectItem(item, "timeout")))
					s->timeout = cJSON_GetNumberValue(r);
			} else if (s->mode == VSMODE_I2C) {
				if ((r = cJSON_GetObjectItem(item, "i2c_type")))
					s->i2c_type = get_i2c_sensor_type(cJSON_GetStringValue(r));
				if ((r = cJSON_GetObjectItem(item, "i2c_addr")))
					s->i2c_addr = cJSON_GetNumberValue(r);
			} else {
				if ((r = cJSON_GetObjectItem(item, "sensors")))
					json2vsensors(r, s->sensors);
			}
		}
	}

	mutex_exit(config_mutex);
	return 0;
}


void read_config()
{
	cJSON *config = NULL;
	int res;
	uint32_t file_size;
	char  *buf = NULL;


	log_msg(LOG_INFO, "Reading configuration...");

	res = flash_read_file(&buf, &file_size, "brickpico.cfg");
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


void save_config()
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
		flash_write_file(str, config_size, "brickpico.cfg");
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


void delete_config()
{
	int res;

	res = flash_delete_file("brickpico.cfg");
	if (res) {
		log_msg(LOG_ERR, "Failed to delete configuration.");
	}
}
