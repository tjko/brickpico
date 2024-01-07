/* command.c
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
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"
#include "pico/util/datetime.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/rtc.h"
#include "cJSON.h"
#include "lfs.h"

#include "brickpico.h"
#ifdef WIFI_SUPPORT
#include "lwip/ip_addr.h"
#include "lwip/stats.h"
#endif



struct cmd_t {
	const char   *cmd;
	uint8_t       min_match;
	const struct cmd_t *subcmds;
	int (*func)(const char *cmd, const char *args, int query, char *prev_cmd);
};

struct error_t {
	const char    *error;
	int            error_num;
};

const struct error_t error_codes[] = {
	{ "No Error", 0 },
	{ "Command Error", -100 },
	{ "Syntax Error", -102 },
	{ "Undefined Header", -113 },
	{ NULL, 0 }
};

int last_error_num = 0;

struct brickpico_state *st = NULL;
struct brickpico_config *conf = NULL;

/* credits.s */
extern const char brickpico_credits_text[];



/* Helper functions for commands */

typedef int (*validate_str_func_t)(const char *args);

int string_setting(const char *cmd, const char *args, int query, char *prev_cmd,
		char *var, size_t var_len, const char *name, validate_str_func_t validate_func)
{
	if (query) {
		printf("%s\n", var);
	} else {
		if (validate_func) {
			if (!validate_func(args)) {
				log_msg(LOG_WARNING, "%s invalid argument: '%s'", name, args);
				return 2;
			}
		}
		if (strcmp(var, args)) {
			log_msg(LOG_NOTICE, "%s change '%s' --> '%s'", name, var, args);
			strncopy(var, args, var_len);
		}
	}
	return 0;
}

int bitmask16_setting(const char *cmd, const char *args, int query, char *prev_cmd,
		uint16_t *mask, uint16_t len, uint8_t base, const char *name)
{
	uint32_t old = *mask;
	uint32_t new;

	if (query) {
		printf("%s\n", bitmask_to_str(old, len, base, true));
		return 0;
	}

	if (!str_to_bitmask(args, len, &new, base)) {
		if (old != new) {
			log_msg(LOG_NOTICE, "%s change 0x%lx --> 0x%lx", name, old, new);
			*mask = new;
		}
		return 0;
	}
	return 1;
}

int uint32_setting(const char *cmd, const char *args, int query, char *prev_cmd,
		uint32_t *var, uint32_t min_val, uint32_t max_val, const char *name)
{
	uint32_t val;
	int v;

	if (query) {
		printf("%lu\n", *var);
		return 0;
	}

	if (str_to_int(args, &v, 10)) {
		val = v;
		if (val >= min_val && val <= max_val) {
			if (*var != val) {
				log_msg(LOG_NOTICE, "%s change %lu --> %lu", name, *var, val);
				*var = val;
			}
		} else {
			log_msg(LOG_WARNING, "Invalid %s value: %s", name, args);
			return 2;
		}
		return 0;
	}
	return 1;
}

int uint8_setting(const char *cmd, const char *args, int query, char *prev_cmd,
		uint8_t *var, uint8_t min_val, uint8_t max_val, const char *name)
{
	uint8_t val;
	int v;

	if (query) {
		printf("%u\n", *var);
		return 0;
	}

	if (str_to_int(args, &v, 10)) {
		val = v;
		if (val >= min_val && val <= max_val) {
			if (*var != val) {
				log_msg(LOG_NOTICE, "%s change %u --> %u", name, *var, val);
				*var = val;
			}
		} else {
			log_msg(LOG_WARNING, "Invalid %s value: %s", name, args);
			return 2;
		}
		return 0;
	}
	return 1;
}

int bool_setting(const char *cmd, const char *args, int query, char *prev_cmd,
		bool *var, const char *name)
{
	bool val;

	if (query) {
		printf("%s\n", (*var ? "ON" : "OFF"));
		return 0;
	}

	if ((args[0] == '1' && args[1] == 0) || !strncasecmp(args, "true", 5)
		|| !strncasecmp(args, "on", 3)) {
		val = true;
	}
	else if ((args[0] == '0' && args[1] == 0) || !strncasecmp(args, "false", 6)
		|| !strncasecmp(args, "off", 4)) {
		val =  false;
	} else {
		log_msg(LOG_WARNING, "Invalid %s value: %s", name, args);
		return 2;
	}

	if (*var != val) {
		log_msg(LOG_NOTICE, "%s change %u --> %u", name, *var, val);
		*var = val;
	}
	return 0;
}

#ifdef WIFI_SUPPORT
int ip_change(const char *cmd, const char *args, int query, char *prev_cmd, const char *name, ip_addr_t *ip)
{
	ip_addr_t tmpip;

	if (query) {
		printf("%s\n", ipaddr_ntoa(ip));
	} else {
		if (!ipaddr_aton(args, &tmpip))
			return 2;
		log_msg(LOG_NOTICE, "%s change '%s' --> %s'", name, ipaddr_ntoa(ip), args);
		ip_addr_copy(*ip, tmpip);
	}
	return 0;
}
#endif



/* Command functions */

int cmd_idn(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int i;
	pico_unique_board_id_t board_id;

	if (!query)
		return 1;

	printf("TJKO Industries,BRICKPICO-%s,", BRICKPICO_MODEL);
	pico_get_unique_board_id(&board_id);
	for (i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++)
		printf("%02x", board_id.id[i]);
	printf(",%s\n", BRICKPICO_VERSION);

	return 0;
}

int cmd_usb_boot(const char *cmd, const char *args, int query, char *prev_cmd)
{
	char buf[64];
	const char *msg[] = {
		"FIRMWARE UPGRADE MODE",
		"=====================",
		"Use file (.uf2):",
		buf,
		"",
		"Copy file to: RPI-RP2",
		"",
		"Press RESET to abort.",
	};

	if (query)
		return 1;

	snprintf(buf, sizeof(buf), " brickpico-%s-%s", BRICKPICO_MODEL, PICO_BOARD);
	display_message(8, msg);

	reset_usb_boot(0, 0);
	return 0; /* should never get this far... */
}

int cmd_version(const char *cmd, const char *args, int query, char *prev_cmd)
{
	const char* credits = brickpico_credits_text;

	if (cmd && !query)
		return 1;

	printf("BrickPico-%s v%s (%s; %s; SDK v%s; %s)\n\n",
		BRICKPICO_MODEL,
		BRICKPICO_VERSION,
		__DATE__,
		PICO_CMAKE_BUILD_TYPE,
		PICO_SDK_VERSION_STRING,
		PICO_BOARD);

	if (query)
		printf("%s\n", credits);

	return 0;
}

int cmd_outputs(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	printf("%d\n", OUTPUT_COUNT);
	return 0;
}

int cmd_led(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint8_setting(cmd, args, query, prev_cmd,
			&conf->led_mode, 0, 2, "System LED Mode");
}

int cmd_null(const char *cmd, const char *args, int query, char *prev_cmd)
{
	log_msg(LOG_INFO, "null command: %s %s (query=%d)", cmd, args, query);
	return 0;
}

int cmd_debug(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int level;

	if (query) {
		printf("%d\n", get_debug_level());
	} else if (str_to_int(args, &level, 10)) {
		set_debug_level((level < 0 ? 0 : level));
	}
	return 0;
}

int cmd_log_level(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int level = get_log_level();
	int new_level;
	const char *name, *new_name;

	name = log_priority2str(level);

	if (query) {
		if (name) {
			printf("%s\n", name);
		} else {
			printf("%d\n", level);
		}
	} else {
		if ((new_level = str2log_priority(args)) < 0)
			return 1;
		new_name = log_priority2str(new_level);

		log_msg(LOG_NOTICE, "Change log level: %s (%d) -> %s (%d)",
			(name ? name : ""), level, new_name, new_level);
		set_log_level(new_level);
	}
	return 0;
}

#define MEM_LOG_BUF_SIZE 256

int cmd_mem_log(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int c, o, len, next, prev;
	uint8_t *buf;

	if (!query)
		return 1;
	if (!(buf = malloc(MEM_LOG_BUF_SIZE)))
		return 2;

	printf("logbuffer: items=%u, size=%u, free=%u\n",
		log_rb->items, log_rb->size, log_rb->free);

	c = 0;
	o = log_rb->head;
	do {
		c++;
		len = ringbuffer_peek(log_rb, o, buf, MEM_LOG_BUF_SIZE, &next, &prev);
		if (len > 0)
			printf(">%s\n", buf);
		o = next;
	} while (len > 0 && o >= 0);

	free(buf);
	return 0;
}

int cmd_syslog_level(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int level = get_syslog_level();
	int new_level;
	const char *name, *new_name;

	name = log_priority2str(level);

	if (query) {
		if (name) {
			printf("%s\n", name);
		} else {
			printf("%d\n", level);
		}
	} else {
		if ((new_level = str2log_priority(args)) < 0)
			return 1;
		new_name = log_priority2str(new_level);
		log_msg(LOG_NOTICE, "Change syslog level: %s (%d) -> %s (%d)",
			(name ? name : "N/A"), level, new_name, new_level);
		set_syslog_level(new_level);
	}
	return 0;
}


int cmd_echo(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->local_echo, "Command Echo");
}


int cmd_display_type(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->display_type, sizeof(conf->display_type), "Display Type", NULL);
}

int cmd_display_theme(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->display_theme, sizeof(conf->display_theme), "Display Theme", NULL);
}

int cmd_display_logo(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->display_logo, sizeof(conf->display_logo), "Display Logo", NULL);
}

int cmd_display_layout_r(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->display_layout_r, sizeof(conf->display_layout_r),
			"Display Layout (Right)", NULL);
}

int cmd_reset(const char *cmd, const char *args, int query, char *prev_cmd)
{
	const char *msg[] = {
		"    Rebooting...",
	};

	if (query)
		return 1;

	log_msg(LOG_ALERT, "Initiating reboot...");
	display_message(1, msg);
	update_persistent_memory();

	watchdog_disable();
	sleep_ms(500);
	watchdog_reboot(0, SRAM_END, 1);
	while (1);

	/* Should never get this far... */
	return 0;
}

int cmd_save_config(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;
	save_config(true);
	return 0;
}

int cmd_print_config(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;
	print_config();
	return 0;
}

int cmd_delete_config(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;
	delete_config(true);
	return 0;
}

int cmd_one(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		printf("1\n");
	return 0;
}

int cmd_zero(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		printf("0\n");
	return 0;
}

int cmd_read(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int i;

	if (!query)
		return 1;

	for (i = 0; i < OUTPUT_COUNT; i++) {
		printf("output%d,\"%s\",%d,%s\n", i + 1,
			conf->outputs[i].name,
			st->pwm[i],
			st->pwr[i] ? "ON": "OFF");
	}

	return 0;
}

int cmd_defaults_pwm(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

	for (int i = 0; i < OUTPUT_COUNT; i++) {
		conf->outputs[i].default_pwm = st->pwm[i];
	}
	return 0;
}

int cmd_defaults_state(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

	for (int i = 0; i < OUTPUT_COUNT; i++) {
		conf->outputs[i].default_state = st->pwr[i];
	}
	return 1;
}

int cmd_out_name(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out = atoi(&prev_cmd[6]) - 1;

	if (out >= 0 && out < OUTPUT_COUNT) {
		if (query) {
			printf("%s\n", conf->outputs[out].name);
		} else {
			log_msg(LOG_NOTICE, "output%d: change name '%s' --> '%s'", out + 1,
				conf->outputs[out].name, args);
			strncopy(conf->outputs[out].name, args, sizeof(conf->outputs[out].name));
		}
		return 0;
	}
	return 1;
}

int cmd_out_min_pwm(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out, val;

	out = atoi(&prev_cmd[6]) - 1;
	if (out >= 0 && out < OUTPUT_COUNT) {
		if (query) {
			printf("%d\n", conf->outputs[out].min_pwm);
		} else if (str_to_int(args, &val, 10)) {
			if (val >= 0 && val <= 100) {
				log_msg(LOG_NOTICE, "output%d: change min PWM %d%% --> %d%%", out + 1,
					conf->outputs[out].min_pwm, val);
				conf->outputs[out].min_pwm = val;
			} else {
				log_msg(LOG_WARNING, "output%d: invalid new value for min PWM: %d", out + 1,
					val);
				return 2;
			}
		}
		return 0;
	}
	return 1;
}

int cmd_out_max_pwm(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out, val;

	out = atoi(&prev_cmd[6]) - 1;
	if (out >= 0 && out < OUTPUT_COUNT) {
		if (query) {
			printf("%d\n", conf->outputs[out].max_pwm);
		} else if (str_to_int(args, &val, 10)) {
			if (val >= 0 && val <= 100) {
				log_msg(LOG_NOTICE, "output%d: change max PWM %d%% --> %d%%", out + 1,
					conf->outputs[out].max_pwm, val);
				conf->outputs[out].max_pwm = val;
			} else {
				log_msg(LOG_WARNING, "output%d: invalid new value for max PWM: %d", out + 1,
					val);
				return 2;
			}
		}
		return 0;
	}
	return 1;
}

int cmd_out_default_pwm(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out, val;

	out = atoi(&prev_cmd[6]) - 1;
	if (out >= 0 && out < OUTPUT_COUNT) {
		if (query) {
			printf("%d\n", conf->outputs[out].default_pwm);
		} else if (str_to_int(args, &val, 10)) {
			if (val >= 0 && val <= 100) {
				if (conf->outputs[out].default_pwm != val) {
					log_msg(LOG_NOTICE, "output%d: change default PWM %d%% --> %d%%",
						out + 1, conf->outputs[out].default_pwm, val);
					conf->outputs[out].default_pwm = val;
				}
			} else {
				log_msg(LOG_WARNING, "output%d: invalid new value for max PWM: %d", out + 1,
					val);
				return 2;
			}
		}
		return 0;
	}
	return 1;
}

int cmd_out_default_state(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out, val;

	out = atoi(&prev_cmd[6]) - 1;
	if (out >= 0 && out < OUTPUT_COUNT) {
		if (query) {
			printf("%s\n", (conf->outputs[out].default_state ? "ON" : "OFF"));
		} else {
			if (!strncasecmp(args, "on", 3))
				val = 1;
			else if (!strncasecmp(args, "off", 4))
				val = 0;
			else
				val = 1;
			if (val >= 0) {
				if (conf->outputs[out].default_state != val) {
					log_msg(LOG_NOTICE, "output%d: change default state %s",
						out + 1, (val ? "ON" : "OFF"));
					conf->outputs[out].default_state = val;
				}
			} else {
				log_msg(LOG_WARNING, "output%d: invalid new value for state: %s",
					out + 1, args);
				return 2;
			}
		}
		return 0;
	}
	return 1;
}

int cmd_out_read(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out;
	int d;

	if (!query)
		return 1;

	if (!strncasecmp(prev_cmd, "output", 6)) {
		out = atoi(&prev_cmd[6]) - 1;
	} else {
		out = atoi(&cmd[6]) - 1;
	}

	if (out >= 0 && out < OUTPUT_COUNT) {
		d = st->pwm[out];
		log_msg(LOG_DEBUG, "output%d duty = %d%%", out + 1, d);
		printf("%d\n", d);
		return 0;
	}

	return 1;
}

int cmd_write_state(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out, val;

	if (query)
		return 1;

	if (!strncasecmp(prev_cmd, "output", 6)) {
		out = atoi(&prev_cmd[6]) - 1;
	} else {
		out = atoi(&cmd[6]) - 1;
	}

	if (out >= 0 && out < OUTPUT_COUNT) {
		if (!strncasecmp(args, "on", 3))
			val = 1;
		else if (!strncasecmp(args, "off", 4))
			val = 0;
		else
			val = -1;

		if (val >= 0) {
			if (st->pwr[out] != val) {
				log_msg(LOG_INFO, "output%d: change power %s", out + 1,
					(val ? "ON" : "OFF"));
				st->pwr[out] = val;
			}
			return 0;
		} else {
			log_msg(LOG_WARNING, "output%d: invalid new value for power: %s", out + 1,
				args);
			return 2;
		}
	}

	return 1;
}

int cmd_write_pwm(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out, val;

	if (query)
		return 1;

	out = atoi(&prev_cmd[6]) - 1;

	if (out >= 0 && out < OUTPUT_COUNT) {
		if (str_to_int(args, &val, 10)) {
			if (val >= 0 && val <= 100) {
				if (st->pwm[out] != val) {
					log_msg(LOG_INFO, "output%d: change PWM %d%% --> %d%%", out + 1,
						st->pwm[out], val);
					st->pwm[out] = val;
				}
				return 0;
			} else {
				log_msg(LOG_WARNING, "output%d: invalid new value for PWM: %d", out + 1,
					val);
				return 2;
			}
		}
	}

	return 1;
}

int cmd_wifi(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
#ifdef WIFI_SUPPORT
		printf("1\n");
#else
		printf("0\n");
#endif
		return 0;
	}
	return 1;
}


#ifdef WIFI_SUPPORT

int cmd_wifi_ip(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return ip_change(cmd, args, query, prev_cmd, "IP", &conf->ip);
}

int cmd_wifi_netmask(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return ip_change(cmd, args, query, prev_cmd, "Netmask", &conf->netmask);
}

int cmd_wifi_gateway(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return ip_change(cmd, args, query, prev_cmd, "Default Gateway", &conf->gateway);
}

int cmd_wifi_syslog(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return ip_change(cmd, args, query, prev_cmd, "Syslog Server", &conf->syslog_server);
}

int cmd_wifi_ntp(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return ip_change(cmd, args, query, prev_cmd, "NTP Server", &conf->ntp_server);
}

int cmd_wifi_mac(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		network_mac();
		return 0;
	}
	return 1;
}

int cmd_wifi_ssid(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->wifi_ssid, sizeof(conf->wifi_ssid), "WiFi SSID", NULL);
}

int cmd_wifi_status(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		network_status();
		return 0;
	}
	return 1;
}

int cmd_wifi_stats(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		stats_display();
		return 0;
	}
	return 1;
}

int cmd_wifi_country(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->wifi_country, sizeof(conf->wifi_country),
			"WiFi Country", valid_wifi_country);
}

int cmd_wifi_password(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->wifi_passwd, sizeof(conf->wifi_passwd),
			"WiFi Password", NULL);
}

int cmd_wifi_hostname(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->hostname, sizeof(conf->hostname),
			"WiFi Hostname", valid_hostname);
}

int cmd_wifi_mode(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint8_setting(cmd, args, query, prev_cmd,
			&conf->wifi_mode, 0, 1, "WiFi Mode");
}

int cmd_mqtt_server(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_server, sizeof(conf->mqtt_server), "MQTT Server", NULL);
}

int cmd_mqtt_port(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->mqtt_port, 0, 65535, "MQTT Port");
}

int cmd_mqtt_user(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_user, sizeof(conf->mqtt_user), "MQTT User", NULL);
}

int cmd_mqtt_pass(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_pass, sizeof(conf->mqtt_pass), "MQTT Password", NULL);
}

int cmd_mqtt_status_interval(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->mqtt_status_interval, 0, (86400 * 30), "MQTT Publish Status Interval");
}

int cmd_mqtt_temp_interval(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->mqtt_temp_interval, 0, (86400 * 30), "MQTT Publish Temp Interval");
}

int cmd_mqtt_pwm_interval(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->mqtt_pwm_interval, 0, (86400 * 30), "MQTT Publish PWM Interval");
}

int cmd_mqtt_allow_scpi(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->mqtt_allow_scpi, "MQTT Allow SCPI Commands");
}

int cmd_mqtt_status_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_status_topic, sizeof(conf->mqtt_status_topic),
			"MQTT Status Topic", NULL);
}

int cmd_mqtt_temp_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_temp_topic, sizeof(conf->mqtt_temp_topic),
			"MQTT Temp Topic", NULL);
}

int cmd_mqtt_cmd_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_cmd_topic, sizeof(conf->mqtt_cmd_topic),
			"MQTT Command Topic", NULL);
}

int cmd_mqtt_resp_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_resp_topic, sizeof(conf->mqtt_resp_topic),
			"MQTT Response Topic", NULL);
}

int cmd_mqtt_err_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_err_topic, sizeof(conf->mqtt_err_topic),
			"MQTT Error Topic", NULL);
}

int cmd_mqtt_warn_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_warn_topic, sizeof(conf->mqtt_warn_topic),
			"MQTT Warning Topic", NULL);
}

int cmd_mqtt_pwm_topic(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_pwm_topic, sizeof(conf->mqtt_pwm_topic),
			"MQTT PWM Topic", NULL);
}

int cmd_mqtt_mask_pwm(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bitmask16_setting(cmd, args, query, prev_cmd,
				&conf->mqtt_pwm_mask, OUTPUT_COUNT,
				1, "MQTT PWM Mask");
}


#if TLS_SUPPORT
int cmd_mqtt_tls(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->mqtt_tls, "MQTT TLS Mode");
}

int cmd_tls_pkey(const char *cmd, const char *args, int query, char *prev_cmd)
{
	char *buf;
	uint32_t buf_len = 4096;
	uint32_t file_size;
	int res = 0;

	if (query) {
		multicore_lockout_start_blocking();
		res = flash_read_file(&buf, &file_size, "key.pem", false);
		multicore_lockout_end_blocking();
		if (res == 0 && buf != NULL) {
			printf("%s\n", buf);
			free(buf);
			return 0;
		} else {
			printf("No private key present.\n");
		}
		return 2;
	}

	if ((buf = malloc(buf_len)) == NULL) {
		log_msg(LOG_ERR,"cmd_tls_pkey(): not enough memory");
		return 2;
	}
	buf[0] = 0;

#if WATCHDOG_ENABLED
	watchdog_disable();
#endif
	if (!strncasecmp(args, "DELETE", 7)) {
		multicore_lockout_start_blocking();
		res = flash_delete_file("key.pem");
		multicore_lockout_end_blocking();
		if (res == -2) {
			printf("No private key present.\n");
			res = 0;
		}
		else if (res) {
			printf("Failed to delete private key: %d\n", res);
			res = 2;
		} else {
			printf("Private key succesfully deleted.\n");
		}
	}
	else {
		printf("Paste private key in PEM format:\n");

		if (read_pem_file(buf, buf_len, 5000) != 1) {
			printf("Invalid private key!\n");
			res = 2;
		} else {
			multicore_lockout_start_blocking();
			res = flash_write_file(buf, strlen(buf) + 1, "key.pem");
			multicore_lockout_end_blocking();
			if (res) {
				printf("Failed to save private key.\n");
				res = 2;
			} else {
				printf("Private key succesfully saved. (length=%u)\n",
					strlen(buf));
				res = 0;
			}
		}
	}
#if WATCHDOG_ENABLED
	watchdog_enable(WATCHDOG_REBOOT_DELAY, 1);
#endif

	free(buf);
	return res;
}

int cmd_tls_cert(const char *cmd, const char *args, int query, char *prev_cmd)
{
	char *buf;
	uint32_t buf_len = 8192;
	uint32_t file_size;
	int res = 0;

	if (query) {
		multicore_lockout_start_blocking();
		res = flash_read_file(&buf, &file_size, "cert.pem", false);
		multicore_lockout_end_blocking();
		if (res == 0 && buf != NULL) {
			printf("%s\n", buf);
			free(buf);
			return 0;
		} else {
			printf("No certificate present.\n");
		}
		return 2;
	}

	if ((buf = malloc(buf_len)) == NULL) {
		log_msg(LOG_ERR,"cmd_tls_cert(): not enough memory");
		return 2;
	}
	buf[0] = 0;

#if WATCHDOG_ENABLED
	watchdog_disable();
#endif
	if (!strncasecmp(args, "DELETE", 7)) {
		multicore_lockout_start_blocking();
		res = flash_delete_file("cert.pem");
		multicore_lockout_end_blocking();
		if (res == -2) {
			printf("No certificate present.\n");
			res = 0;
		}
		else if (res) {
			printf("Failed to delete certificate: %d\n", res);
			res = 2;
		} else {
			printf("Certificate succesfully deleted.\n");
		}
	}
	else {
		printf("Paste certificate in PEM format:\n");

		if (read_pem_file(buf, buf_len, 5000) != 1) {
			printf("Invalid private key!\n");
			res = 2;
		} else {
			multicore_lockout_start_blocking();
			res = flash_write_file(buf, strlen(buf) + 1, "cert.pem");
			multicore_lockout_end_blocking();
			if (res) {
				printf("Failed to save certificate.\n");
				res = 2;
			} else {
				printf("Certificate succesfully saved. (length=%u)\n",
					strlen(buf));
				res = 0;
			}
		}
	}
#if WATCHDOG_ENABLED
	watchdog_enable(WATCHDOG_REBOOT_DELAY, 1);
#endif

	free(buf);
	return res;
}
#endif /* TLS_SUPPORT */
#endif /* WIFI_SUPPOERT */

int cmd_time(const char *cmd, const char *args, int query, char *prev_cmd)
{
	datetime_t t;
	char buf[32];

	if (query) {
		if (rtc_get_datetime(&t)) {
			printf("%s\n", datetime_str(buf, sizeof(buf), &t));
		}
		return 0;
	}

	if (str_to_datetime(args, &t)) {
		if (rtc_set_datetime(&t)) {
			log_msg(LOG_NOTICE, "Set system clock: %04d-%02d-%02d %02d:%02d:%02d",
				t.year, t.month, t.day, t.hour, t.min, t.sec);
			return 0;
		}
	}
	return 2;
}

int cmd_timezone(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->timezone, sizeof(conf->timezone), "Timezone", NULL);
}

int cmd_uptime(const char *cmd, const char *args, int query, char *prev_cmd)
{
	uint32_t secs = to_us_since_boot(get_absolute_time()) / 1000000;
	uint32_t mins =  secs / 60;
	uint32_t hours = mins / 60;
	uint32_t days = hours / 24;

	if (!query)
		return 1;

	printf("up %lu days, %lu hours, %lu minutes%s\n", days, hours % 24, mins % 60,
		(rebooted_by_watchdog ? " [rebooted by watchdog]" : ""));

	return 0;
}

int cmd_err(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	for (int i = 0; error_codes[i].error != NULL; i++) {
		if (error_codes[i].error_num == last_error_num) {
			printf("%d,\"%s\"\n", last_error_num, error_codes[i].error);
			last_error_num = 0;
			return 0;
		}
	}
	printf("-1,\"Internal Error\"\n");
	return 0;
}

int cmd_name(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->name, sizeof(conf->name), "System Name", NULL);
}


#define TEST_MEM_SIZE (264*1024)

int cmd_memory(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int blocksize;

	if (query) {
		print_rp2040_meminfo();
		printf("mallinfo:\n");
		print_mallinfo();
		return 0;
	}

	if (str_to_int(args, &blocksize, 10)) {
		if (blocksize < 512)
			blocksize = 512;
		if (blocksize > 8192)
			blocksize= 8192;
	} else {
		blocksize = 1024;
	}

	/* Test for largest available memory block... */
	void *buf = NULL;
	size_t bufsize = blocksize;
	do {
		if (buf) {
			free(buf);
			bufsize += blocksize;
		}
		buf = malloc(bufsize);
	} while (buf && bufsize < TEST_MEM_SIZE);
	printf("Largest available memory block:        %u bytes\n",
		bufsize - blocksize);

	/* Test how much memory available in 'blocksize' blocks... */
	int i = 0;
	int max = TEST_MEM_SIZE / blocksize + 1;
	void **refbuf = malloc(max * sizeof(void*));
	if (refbuf) {
		memset(refbuf, 0, max * sizeof(void*));
		while (i < max) {
			if (!(refbuf[i] = malloc(blocksize)))
				break;
			i++;
		}
	}
	printf("Total available memory:                %u bytes (%d x %dbytes)\n",
		i * blocksize, i, blocksize);
	if (refbuf) {
		i = 0;
		while (i < max && refbuf[i]) {
			free(refbuf[i++]);
		}
		free(refbuf);
	}
	return 0;
}

int cmd_serial(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->serial_active, "Serial Console status");
}

int cmd_spi(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->spi_active, "SPI (LCD Display) status");
}

int cmd_pwm_freq(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int val;

	if (query) {
		printf("%u\n", conf->pwm_freq);
		return 0;
	}

	if (str_to_int(args, &val, 10)) {
		if (val >= 10 && val <= 100000) {
			log_msg(LOG_NOTICE, "change PWM frequency %uHz --> %uHz",
				conf->pwm_freq, val);
			conf->pwm_freq = val;
		} else {
			log_msg(LOG_WARNING, "invalid new value for PWM frequency: %d",	val);
			return 2;
		}
		return 0;
	}
	return 1;
}

int cmd_timer(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int i;

	if (!query)
		return 1;

	for (i = 0; i < conf->event_count; i++) {
		printf("%d: %s\n", i + 1, timer_event_str(&conf->events[i]));
	}

	return 0;
}

int cmd_timer_add(const char *cmd, const char *args, int query, char *prev_cmd)
{
	struct timer_event e;
	int res;

	if (query)
		return 1;

	res = parse_timer_event_str(args, &e);
	if (res != 0) {
		log_msg(LOG_WARNING, "Invalid timer event: %s", args);
		return 2;
	}
	if (conf->event_count >= MAX_EVENT_COUNT) {
		log_msg(LOG_WARNING, "Timer event table full: %u", conf->event_count);
		return 2;
	}
	memcpy(&conf->events[conf->event_count], &e, sizeof(struct timer_event));
	conf->event_count++;
	log_msg(LOG_NOTICE, "Added new timer entry: %s", timer_event_str(&e));

	return 0;
}

int cmd_timer_del(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int idx = -1;
	int res = 2;

	if (query)
		return 1;

	if (str_to_int(args, &idx, 10)) {
		if (idx >= 1 && idx <= conf->event_count) {
			log_msg(LOG_NOTICE, "Remove timer event: %d", idx);
			for (int i = idx - 1; i < conf->event_count - 1; i++) {
				memcpy(&conf->events[i], &conf->events[i + 1], sizeof(struct timer_event));
			}
			conf->event_count--;
			res = 0;
		} else {
			log_msg(LOG_WARNING, "Timer event does not exist: %d", idx);
		}
	}

	return res;
}



const struct cmd_t display_commands[] = {
	{ "LAYOUTR",   7, NULL,              cmd_display_layout_r },
	{ "LOGO",      4, NULL,              cmd_display_logo },
	{ "THEMe",     4, NULL,              cmd_display_theme },
	{ 0, 0, 0, 0 }
};

const struct cmd_t wifi_commands[] = {
#ifdef WIFI_SUPPORT
	{ "COUntry",   3, NULL,              cmd_wifi_country },
	{ "GATEway",   4, NULL,              cmd_wifi_gateway },
	{ "HOSTname",  4, NULL,              cmd_wifi_hostname },
	{ "IPaddress", 2, NULL,              cmd_wifi_ip },
	{ "MAC",       3, NULL,              cmd_wifi_mac },
	{ "NETMask",   4, NULL,              cmd_wifi_netmask },
	{ "NTP",       3, NULL,              cmd_wifi_ntp },
	{ "MODE",      4, NULL,              cmd_wifi_mode },
	{ "PASSword",  4, NULL,              cmd_wifi_password },
	{ "SSID",      4, NULL,              cmd_wifi_ssid },
	{ "STATS",     5, NULL,              cmd_wifi_stats },
	{ "STATus",    4, NULL,              cmd_wifi_status },
	{ "SYSLOG",    6, NULL,              cmd_wifi_syslog },
#endif
	{ 0, 0, 0, 0 }
};

#ifdef WIFI_SUPPORT
const struct cmd_t mqtt_mask_commands[] = {
	{ "PWM",       3, NULL,              cmd_mqtt_mask_pwm },
	{ 0, 0, 0, 0 }
};

const struct cmd_t mqtt_interval_commands[] = {
	{ "STATus",    4, NULL,              cmd_mqtt_status_interval },
	{ "TEMP",      4, NULL,              cmd_mqtt_temp_interval },
	{ "PWM",       3, NULL,              cmd_mqtt_pwm_interval },
	{ 0, 0, 0, 0 }
};

const struct cmd_t mqtt_topic_commands[] = {
	{ "STATus",    4, NULL,              cmd_mqtt_status_topic },
	{ "TEMP",      4, NULL,              cmd_mqtt_temp_topic },
	{ "COMMand",   4, NULL,              cmd_mqtt_cmd_topic },
	{ "RESPonse",  4, NULL,              cmd_mqtt_resp_topic },
	{ "PWM",       3, NULL,              cmd_mqtt_pwm_topic },
	{ "ERRor",     3, NULL,              cmd_mqtt_err_topic },
	{ "WARNing",   4, NULL,              cmd_mqtt_warn_topic },
	{ 0, 0, 0, 0 }
};
#endif

const struct cmd_t mqtt_commands[] = {
#ifdef WIFI_SUPPORT
	{ "SERVer",    4, NULL,              cmd_mqtt_server },
	{ "PORT",      4, NULL,              cmd_mqtt_port },
	{ "USER",      4, NULL,              cmd_mqtt_user },
	{ "PASSword",  4, NULL,              cmd_mqtt_pass },
	{ "SCPI",      4, NULL,              cmd_mqtt_allow_scpi },
#if TLS_SUPPORT
	{ "TLS",       3, NULL,              cmd_mqtt_tls },
#endif
	{ "INTerval",  3, mqtt_interval_commands, NULL },
	{ "MASK",      4, mqtt_mask_commands, NULL },
	{ "TOPIC",     5, mqtt_topic_commands, NULL },
#endif
	{ 0, 0, 0, 0 }
};

const struct cmd_t tls_commands[] = {
#ifdef WIFI_SUPPORT
#if TLS_SUPPORT
	{ "CERT",      4, NULL,              cmd_tls_cert },
	{ "PKEY",      4, NULL,              cmd_tls_pkey },
#endif
#endif
	{ 0, 0, 0, 0 }
};

const struct cmd_t system_commands[] = {
	{ "DEBUG",     5, NULL,              cmd_debug }, /* Obsolete ? */
	{ "DISPlay",   4, display_commands,  cmd_display_type },
	{ "ECHO",      4, NULL,              cmd_echo },
	{ "ERRor",     3, NULL,              cmd_err },
	{ "OUTputs",   3, NULL,              cmd_outputs },
	{ "LED",       3, NULL,              cmd_led },
	{ "LOG",       3, NULL,              cmd_log_level },
	{ "MEMLOG",    6, NULL,              cmd_mem_log },
	{ "MEMory",    3, NULL,              cmd_memory },
	{ "MQTT",      4, mqtt_commands,     NULL },
	{ "NAME",      4, NULL,              cmd_name },
	{ "PWMfreq",   3, NULL,              cmd_pwm_freq },
	{ "SERIAL",    6, NULL,              cmd_serial },
	{ "SPI",       3, NULL,              cmd_spi },
	{ "SYSLOG",    6, NULL,              cmd_syslog_level },
	{ "TIMEZONE",  8, NULL,              cmd_timezone },
	{ "TIME",      4, NULL,              cmd_time },
	{ "TLS",       3, tls_commands,      NULL },
	{ "UPGRADE",   7, NULL,              cmd_usb_boot },
	{ "UPTIme",    4, NULL,              cmd_uptime },
	{ "VERsion",   3, NULL,              cmd_version },
	{ "WIFI",      4, wifi_commands,     cmd_wifi },
	{ 0, 0, 0, 0 }
};

const struct cmd_t defaults_c_commands[] = {
	{ "PWM",       3, NULL,              cmd_defaults_pwm },
	{ "STAte",     3, NULL,              cmd_defaults_state },
	{ 0, 0, 0, 0 }
};

const struct cmd_t output_c_commands[] = {
	{ "MAXpwm",    3, NULL,              cmd_out_max_pwm },
	{ "MINpwm",    3, NULL,              cmd_out_min_pwm },
	{ "NAME",      4, NULL,              cmd_out_name },
	{ "PWM",       3, NULL,              cmd_out_default_pwm },
	{ "STAte",     3, NULL,              cmd_out_default_state },
	{ 0, 0, 0, 0 }
};

const struct cmd_t timer_c_commands[] = {
	{ "ADD",       3, NULL,              cmd_timer_add },
	{ "DEL",       3, NULL,              cmd_timer_del },
	{ 0, 0, 0, 0 }
};

const struct cmd_t config_commands[] = {
	{ "DEFAULTS",  8, defaults_c_commands, NULL },
	{ "DELete",    3, NULL,              cmd_delete_config },
	{ "OUTPUT",    6, output_c_commands, NULL },
	{ "Read",      1, NULL,              cmd_print_config },
	{ "SAVe",      3, NULL,              cmd_save_config },
	{ "TIMER",     5, timer_c_commands,  cmd_timer },
	{ 0, 0, 0, 0 }
};

const struct cmd_t output_commands[] = {
	{ "PWM",       3, NULL,              cmd_out_read },
	{ "Read",      1, NULL,              cmd_out_read },
	{ 0, 0, 0, 0 }
};

const struct cmd_t measure_commands[] = {
	{ "OUTPUT",    6, output_commands,   cmd_out_read },
	{ "Read",      1, NULL,              cmd_read },
	{ 0, 0, 0, 0 }
};

const struct cmd_t write_o_commands[] = {
	{ "PWM",       3, NULL,              cmd_write_pwm },
	{ "STAte",     3, NULL,              cmd_write_state },
	{ 0, 0, 0, 0 }
};

const struct cmd_t write_commands[] = {
	{ "OUTPUT",    6, write_o_commands,  cmd_write_state },
	{ 0, 0, 0, 0 }
};

const struct cmd_t commands[] = {
	{ "*CLS",      4, NULL,              cmd_null },
	{ "*ESE",      4, NULL,              cmd_null },
	{ "*ESR",      4, NULL,              cmd_zero },
	{ "*IDN",      4, NULL,              cmd_idn },
	{ "*OPC",      4, NULL,              cmd_one },
	{ "*RST",      4, NULL,              cmd_reset },
	{ "*SRE",      4, NULL,              cmd_zero },
	{ "*STB",      4, NULL,              cmd_zero },
	{ "*TST",      4, NULL,              cmd_zero },
	{ "*WAI",      4, NULL,              cmd_null },
	{ "CONFigure", 4, config_commands,   cmd_print_config },
	{ "MEAsure",   3, measure_commands,  NULL },
	{ "SYStem",    3, system_commands,   NULL },
	{ "Read",      1, NULL,              cmd_read },
	{ "WRIte",     3, write_commands,    NULL },
	{ 0, 0, 0, 0 }
};



const struct cmd_t* run_cmd(char *cmd, const struct cmd_t *cmd_level, char **prev_subcmd)
{
	int i, query, cmd_len, total_len;
	char *saveptr1, *saveptr2, *t, *sub, *s, *arg;
	int res = -1;

	total_len = strlen(cmd);
	t = strtok_r(cmd, " \t", &saveptr1);
	if (t && strlen(t) > 0) {
		cmd_len = strlen(t);
		if (*t == ':' || *t == '*') {
			/* reset command level to 'root' */
			cmd_level = commands;
			*prev_subcmd = NULL;
		}
		/* Split command to subcommands and search from command tree ... */
		sub = strtok_r(t, ":", &saveptr2);
		while (sub && strlen(sub) > 0) {
			s = sub;
			sub = NULL;
			i = 0;
			while (cmd_level[i].cmd) {
				if (!strncasecmp(s, cmd_level[i].cmd, cmd_level[i].min_match)) {
					sub = strtok_r(NULL, ":", &saveptr2);
					if (cmd_level[i].subcmds && sub && strlen(sub) > 0) {
						/* Match for subcommand...*/
						*prev_subcmd = s;
						cmd_level = cmd_level[i].subcmds;
					} else if (cmd_level[i].func) {
						/* Match for command */
						query = (s[strlen(s)-1] == '?' ? 1 : 0);
						arg = t + cmd_len + 1;
						if (!query)
							mutex_enter_blocking(config_mutex);
						res = cmd_level[i].func(s,
								(total_len > cmd_len+1 ? arg : ""),
								query,
								(*prev_subcmd ? *prev_subcmd : ""));
						if (!query)
							mutex_exit(config_mutex);
					}
					break;
				}
				i++;
			}
		}
	}

	if (res < 0) {
		log_msg(LOG_INFO, "Unknown command.");
		last_error_num = -113;
	}
	else if (res == 0) {
		last_error_num = 0;
	}
	else if (res == 1) {
		last_error_num = -100;
	}
	else if (res == 2) {
		last_error_num = -102;
	} else {
		last_error_num = -1;
	}

	return cmd_level;
}


void process_command(struct brickpico_state *state, struct brickpico_config *config, char *command)
{
	char *saveptr, *cmd;
	char *prev_subcmd = NULL;
	const struct cmd_t *cmd_level = commands;

	if (!state || !config || !command)
		return;

	st = state;
	conf = config;

	cmd = strtok_r(command, ";", &saveptr);
	while (cmd) {
		cmd = trim_str(cmd);
		log_msg(LOG_DEBUG, "command: '%s'", cmd);
		if (cmd && strlen(cmd) > 0) {
			cmd_level = run_cmd(cmd, cmd_level, &prev_subcmd);
		}
		cmd = strtok_r(NULL, ";", &saveptr);
	}
}

int last_command_status()
{
	return last_error_num;
}
