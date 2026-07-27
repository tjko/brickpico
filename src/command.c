/* command.c
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
#include "pico/aon_timer.h"
#include "pico/rand.h"
#include "hardware/watchdog.h"
#include "cJSON.h"
#include "pico_sensor_lib.h"

#include "brickpico.h"
#ifdef WIFI_SUPPORT
#include "lwip/ip_addr.h"
#include "lwip/stats.h"
#include "pico_telnetd/util.h"
#include "util_net.h"
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

int acl_list_change(const char *cmd, const char *args, int query, char *prev_cmd, const char *name, acl_entry_t *acl, uint32_t list_len)
{
	ip_addr_t tmpip;
	char buf[255], tmp[32];
	char *t, *t2, *arg, *saveptr, *saveptr2;
	int prefix;
	int idx = 0;


	if (query) {
		for (int i = 0; i < list_len; i++) {
			if (i > 0) {
				if (acl[i].prefix == 0)
					break;
				printf(", ");
			}
			printf("%s/%u", ipaddr_ntoa(&acl[i].ip), acl[i].prefix);
		}
		printf("\n");
		return 0;
	}

	if (!(arg = strdup(args)))
		return 2;
	t = strtok_r(arg, ",", &saveptr);
	while (t && idx < list_len) {
		t = trim_str(t);
		t2 = strtok_r(t, "/", &saveptr2);
		if (t2 && ipaddr_aton(t2, &tmpip)) {
			t2 = strtok_r(NULL, "/", &saveptr2);
			if (t2 && str_to_int(t2, &prefix, 10)) {
				ip_addr_copy(acl[idx].ip, tmpip);
				acl[idx].prefix = clamp_int(prefix,
							0, IP_IS_V6(tmpip) ? 128 : 32);
				idx++;
				if (acl[idx - 1].prefix == 0)
					break;
			}
		}
		t = strtok_r(NULL, ",", &saveptr);
	}
	free(arg);
	if (idx == 0)
		return 1;

	buf[0] = 0;
	for (int i = 0; i < list_len; i++) {
		if (i >= idx) {
			ip_addr_set_zero(&acl[i].ip);
			acl[i].prefix = 0;
		} else {
			snprintf(tmp, sizeof(tmp), "%s%s/%u", (i > 0 ? ", " : ""),
				ipaddr_ntoa(&acl[i].ip), acl[i].prefix);
			strncatenate(buf, tmp, sizeof(buf));
		}
	}
	log_msg(LOG_NOTICE, "%s changed to '%s'", name, buf);

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
	printf(",%s%s\n", BRICKPICO_VERSION, BRICKPICO_BUILD_TAG);

	return 0;
}

int cmd_exit(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

#if WIFI_SUPPORT
//	telnetserver_disconnect();
	sshserver_disconnect();
#endif
	return 0;
}

int cmd_who(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

#if WIFI_SUPPORT
//	telnetserver_who();
	sshserver_who();
#endif
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

	printf("BrickPico-%s v%s%s (%s; %s; SDK v%s; %s)\n\n",
		BRICKPICO_MODEL,
		BRICKPICO_VERSION,
		BRICKPICO_BUILD_TAG,
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
		len = u8_ringbuffer_peek(log_rb, o, buf, MEM_LOG_BUF_SIZE, &next, &prev);
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

int cmd_gamma(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->gamma, sizeof(conf->gamma), "Gamma Correction", NULL);
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

int cmd_out_effect(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int out;
	int ret = 0;
	char *tok, *saveptr, *param;
	struct pwm_output *o;
	enum light_effect_types new_effect;
	void *new_ctx;


	out = atoi(&prev_cmd[6]) - 1;
	if (out < 0 || out >= OUTPUT_COUNT)
		return 1;

	o = &conf->outputs[out];
	if (query) {
		printf("%s", effect2str(o->effect));
		tok = effect_print_args(o->effect, o->effect_ctx);
		if (tok) {
			printf(",%s\n", tok);
			free(tok);
		} else {
			printf(",\n");
		}
	} else {
		if (!(param = strdup(args)))
			return 2;
		if ((tok = strtok_r(param, ",", &saveptr)) != NULL) {
			new_effect = str2effect(tok);
			tok = strtok_r(NULL, "\n", &saveptr);
			new_ctx = effect_parse_args(new_effect, tok ? tok : "");
			if (new_effect == EFFECT_NONE || new_ctx != NULL) {
				o->effect = new_effect;
				if (o->effect_ctx)
					free(o->effect_ctx);
				o->effect_ctx = new_ctx;
			} else {
				ret = 1;
			}
		}
		free(param);
	}

	return ret;
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

int cmd_wifi_auth_mode(const char *cmd, const char *args, int query, char *prev_cmd)
{
	uint32_t type;

	if (query) {
		printf("%s\n", conf->wifi_auth_mode);
	} else {
		if (!wifi_get_auth_type(args, &type))
			return 1;
		if (strncmp(conf->wifi_auth_mode, args, sizeof(conf->wifi_auth_mode))) {
			log_msg(LOG_NOTICE, "WiFi Auth Type change %s --> %s",
				conf->wifi_auth_mode, args);
			strncopy(conf->wifi_auth_mode, args, sizeof(conf->wifi_auth_mode));
		}
	}

	return 0;
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

int cmd_mqtt_ha_discovery(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->mqtt_ha_discovery_prefix,
			sizeof(conf->mqtt_ha_discovery_prefix), "MQTT Home Assistant Discovery Prefix", NULL);
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
	int incount = 1;
	int res = 0;

	if (query) {
		res = flash_read_file(&buf, &file_size, "key.pem");
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
		res = flash_delete_file("key.pem");
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
		int v;
		if (str_to_int(args, &v, 10)) {
			if (v >= 1 && v <= 3)
				incount = v;
		}
		printf("Paste private key in PEM format:\n");
		for(int i = 0; i < incount; i++) {
			if (read_pem_file(buf, buf_len, 5000, true) != 1) {
				printf("Invalid private key!\n");
				res = 2;
				break;
			}
		}
		if (res == 0) {
			res = flash_write_file(buf, strlen(buf) + 1, "key.pem");
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
		res = flash_read_file(&buf, &file_size, "cert.pem");
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
		res = flash_delete_file("cert.pem");
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

		if (read_pem_file(buf, buf_len, 5000, false) != 1) {
			printf("Invalid private key!\n");
			res = 2;
		} else {
			res = flash_write_file(buf, strlen(buf) + 1, "cert.pem");
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

int cmd_ssh_acls(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return acl_list_change(cmd, args, query, prev_cmd, "SSH Server ACLs", conf->ssh_acls,
		SSH_MAX_ACL_ENTRIES);
}

int cmd_ssh_auth(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->ssh_auth, "SSH Server Authentication");
}

int cmd_ssh_server(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->ssh_active, "SSH Server");
}

int cmd_ssh_port(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->ssh_port, 0, 65535, "SSH Port");
}

int cmd_ssh_user(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->ssh_user, sizeof(conf->ssh_user),
			"SSH Username", NULL);
}

int cmd_ssh_pass(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", cfg->ssh_pwhash);
		return 0;
	}

	if (strlen(args) > 0) {
		strncopy(conf->ssh_pwhash, generate_sha512crypt_pwhash(args),
			sizeof(conf->ssh_pwhash));
	} else {
		conf->ssh_pwhash[0] = 0;
		log_msg(LOG_NOTICE, "SSH password removed.");
	}
	return 0;
}

int cmd_ssh_pkey(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	ssh_list_pkeys();
	return 0;
}

int cmd_ssh_pkey_create(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

	return ssh_create_pkey(args);
}

int cmd_ssh_pkey_del(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

	return ssh_delete_pkey(args);
}

int cmd_ssh_pubkey(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int count = 0;
	char tmp[255];

	if (!query)
		return 1;

	for (int i = 0; i < SSH_MAX_PUB_KEYS; i++) {
		struct ssh_public_key *k = &conf->ssh_pub_keys[i];

		if (k->pubkey_size == 0 || strlen(k->username) < 1)
			continue;
		printf("%d: %s, %s\n", ++count, k->username,
			ssh_pubkey_to_str(k, tmp, sizeof(tmp)));
	}
	if (count < 1) {
		printf("No SSH (authentication) public keys found.\n");
	}

	return 0;
}

int cmd_ssh_pubkey_add(const char *cmd, const char *args, int query, char *prev_cmd)
{
	struct ssh_public_key pubkey;
	char *s, *username, *pkey, *saveptr;
	int res = 0;

	if (query)
		return 1;

	if (!(s = strdup(args)))
		return 2;

	if ((username = strtok_r(s, " ", &saveptr))) {
		username = trim_str(username);
		if (strlen(username) < 1)
			username = NULL;
	}

	if ((pkey = strtok_r(NULL, ",", &saveptr))) {
		pkey = trim_str(pkey);
		if (str_to_ssh_pubkey(pkey, &pubkey))
			pkey = NULL;
	}

	if (username && pkey) {
		int idx = -1;

		/* Check for first available slot */
		for(int i = 0; i < SSH_MAX_PUB_KEYS; i++) {
			if (conf->ssh_pub_keys[i].pubkey_size == 0) {
				idx = i;
				break;
			}
		}

		if (idx < 0) {
			printf("Maximum number of public keys already added.\n");
			res = 2;
		} else {
			strncopy(pubkey.username, username, sizeof(pubkey.username));
			conf->ssh_pub_keys[idx] = pubkey;
			log_msg(LOG_INFO, "SSH Public key added: slot %d: %s (%s)\n", idx + 1,
				pubkey.type, pubkey.username);
		}
	}

	free(s);

	return res;
}

int cmd_ssh_pubkey_del(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int idx;

	if (query)
		return 1;

	if (!str_to_int(args, &idx, 10))
		return 2;
	if (idx < 1 || idx > SSH_MAX_PUB_KEYS)
		return 2;

	idx--;
	if (conf->ssh_pub_keys[idx].pubkey_size > 0) {
		log_msg(LOG_INFO, "SSH Public key deleted: slot %d: %s:%s (%s)\n", idx + 1,
			conf->ssh_pub_keys[idx].username,
			conf->ssh_pub_keys[idx].type,
			conf->ssh_pub_keys[idx].name);
		memset(&conf->ssh_pub_keys[idx], 0, sizeof(struct ssh_public_key));
	}

	return 0;
}

int cmd_telnet_acls(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return acl_list_change(cmd, args, query, prev_cmd, "Telnet Server ACLs",
			conf->telnet_acls, TELNET_MAX_ACL_ENTRIES);
}

int cmd_telnet_auth(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->telnet_auth, "Telnet Server Authentication");
}

int cmd_telnet_rawmode(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->telnet_raw_mode, "Telnet Server Raw Mode");
}

int cmd_telnet_server(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return bool_setting(cmd, args, query, prev_cmd,
			&conf->telnet_active, "Telnet Server");
}

int cmd_telnet_port(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->telnet_port, 0, 65535, "Telnet Port");
}

int cmd_telnet_user(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return string_setting(cmd, args, query, prev_cmd,
			conf->telnet_user, sizeof(conf->telnet_user),
			"Telnet Username", NULL);
}

int cmd_telnet_pass(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", cfg->telnet_pwhash);
		return 0;
	}

	if (strlen(args) > 0) {
		strncopy(conf->telnet_pwhash, generate_sha512crypt_pwhash(args),
			sizeof(conf->telnet_pwhash));
	} else {
		conf->telnet_pwhash[0] = 0;
		log_msg(LOG_NOTICE, "Telnet password removed.");
	}
	return 0;
}


#endif /* WIFI_SUPPOERT */

int cmd_time(const char *cmd, const char *args, int query, char *prev_cmd)
{
	struct timespec ts;
	time_t t;
	char buf[32];

	if (query) {
		if (aon_timer_is_running()) {
			aon_timer_get_time(&ts);
			time_t_to_str(buf, sizeof(buf), timespec_to_time_t(&ts));
			printf("%s\n", buf);
		}
		return 0;
	}

	if (str_to_time_t(args, &t)) {
		time_t_to_timespec(t, &ts);
		if (aon_timer_is_running()) {
			aon_timer_set_time(&ts);
		} else {
			aon_timer_start(&ts);
		}
		time_t_to_str(buf, sizeof(buf), t);
		log_msg(LOG_NOTICE, "Set system clock: %s", buf);
		return 0;
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

int cmd_lfs(const char *cmd, const char *args, int query, char *prev_cmd)
{
	size_t size, free, used, files, dirs;

	if (!query)
		return 1;
	if (flash_get_fs_info(&size, &free, &files, &dirs, NULL) < 0)
		return 2;

	used = size - free;
	printf("Filesystem size:                       %u\n", size);
	printf("Filesystem used:                       %u\n", used);
	printf("Filesystem free:                       %u\n", free);
	printf("Number of files:                       %u\n", files);
	printf("Number of subdirectories:              %u\n", dirs);

	return 0;
}

int cmd_lfs_del(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

	if (strlen(args) < 1)
		return 2;

	if (flash_delete_file(args))
		return 2;

	return 0;
}

int cmd_lfs_dir(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	flash_list_directory("/", true);

	return 0;
}

int cmd_lfs_format(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query)
		return 1;

	printf("Formatting flash filesystem...\n");
	if (flash_format(true))
		return 2;
	printf("Filesystem successfully formatted.\n");

	return 0;
}

int cmd_lfs_ren(const char *cmd, const char *args, int query, char *prev_cmd)
{
	char *saveptr, *oldname, *newname, *arg;
	int res = 2;

	if (query)
		return 1;

	if (!(arg = strdup(args)))
		return 2;

	oldname = strtok_r(arg, " \t", &saveptr);
	if (oldname && strlen(oldname) > 0) {
		newname = strtok_r(NULL, " \t", &saveptr);
		if (newname && strlen(newname) > 0) {
			if (!flash_rename_file(oldname, newname)) {
				res = 0;
			}
		}
	}
	free(arg);

	return res;
}

int cmd_lfs_copy(const char *cmd, const char *args, int query, char *prev_cmd)
{
	char *saveptr, *srcname, *dstname, *arg;
	int res = 2;

	if (query)
		return 1;

	if (!(arg = strdup(args)))
		return 2;

	srcname = strtok_r(arg, " \t", &saveptr);
	if (srcname && strlen(srcname) > 0) {
		dstname = strtok_r(NULL, " \t", &saveptr);
		if (dstname && strlen(dstname) > 0) {
			if (strcmp(srcname, dstname)) {
				if (!flash_copy_file(srcname, dstname, false)) {
					res = 0;
				}
			}
		}
	}
	free(arg);

	return res;
}

int cmd_flash(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	print_rp2040_flashinfo();
	return 0;
}


#define TEST_MEM_SIZE (1024*1024)

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
	if (buf)
		free(buf);

	/* Test how much memory available in 'blocksize' blocks... */
	int i = 0;
	int max = TEST_MEM_SIZE / blocksize + 1;
	size_t refbufsize = max * sizeof(void*);
	void **refbuf = malloc(refbufsize);
	if (refbuf) {
		memset(refbuf, 0, refbufsize);
		while (i < max) {
			if (!(refbuf[i] = malloc(blocksize)))
				break;
			i++;
		}
	}
	printf("Total available memory:                %u bytes\n",
		i * blocksize + refbufsize);
	if (refbuf) {
		i = 0;
		while (i < max && refbuf[i]) {
			free(refbuf[i++]);
		}
		free(refbuf);
	}
	return 0;
}

int cmd_i2c(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	display_i2c_status();
	return 0;
}

int cmd_i2c_scan(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	scan_i2c_bus();
	return 0;
}

int cmd_i2c_speed(const char *cmd, const char *args, int query, char *prev_cmd)
{
	return uint32_setting(cmd, args, query, prev_cmd,
			&conf->i2c_speed, 10000, 3400000, "I2C Bus Speed (Hz)");
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

int cmd_vsensors(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	printf("%d\n", VSENSOR_COUNT);
	return 0;
}

int cmd_vsensors_sources(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (!query)
		return 1;

	for (int i = 0; i < VSENSOR_COUNT; i++) {
		const struct vsensor_input *v = &conf->vsensors[i];
		printf("vsensor%d,%s", i + 1, vsmode2str(v->mode));
		switch (v->mode) {
		case VSMODE_MANUAL:
			printf(",%0.2f,%ld", v->default_temp, v->timeout);
			break;
		case VSMODE_I2C:
			printf(",0x%02x,%s", v->i2c_addr, i2c_sensor_type_str(v->i2c_type));
			break;
		default:
			for (int j = 0; j < VSENSOR_SOURCE_MAX_COUNT; j++) {
				if (v->sensors[j])
					printf(",%d", v->sensors[j]);
			}
			break;
		}
		printf("\n");
	}

	return 0;
}

int cmd_vsensor_name(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int sensor;

	sensor = atoi(&prev_cmd[7]) - 1;
	if (sensor >= 0 && sensor < VSENSOR_COUNT) {
		if (query) {
			printf("%s\n", conf->vsensors[sensor].name);
		} else {
			log_msg(LOG_NOTICE, "vsensor%d: change name '%s' --> '%s'", sensor + 1,
				conf->vsensors[sensor].name, args);
			strncopy(conf->vsensors[sensor].name, args,
				sizeof(conf->vsensors[sensor].name));
		}
		return 0;
	}
	return 1;
}

int cmd_vsensor_source(const char *cmd, const char *args, int query, char *prev_cmd)
{
	struct vsensor_input *v;
	int sensor, val, i;
	uint8_t vsmode, selected[VSENSOR_SOURCE_MAX_COUNT];
	float default_temp;
	int timeout;
	char *tok, *saveptr, *param, temp_str[32], tmp[8];
	int ret = 0;
	int count = 0;

	sensor = atoi(&prev_cmd[7]) - 1;
	if (sensor < 0 || sensor >= VSENSOR_COUNT)
		return 1;
	v = &conf->vsensors[sensor];

	if (query) {
		printf("%s", vsmode2str(v->mode));
		if (v->mode == VSMODE_MANUAL) {
			printf(",%0.2f,%ld", v->default_temp, v->timeout);
		} else if (v->mode == VSMODE_I2C) {
			printf(",0x%02x,%s", v->i2c_addr, i2c_sensor_type_str(v->i2c_type));
		} else {
			for(i = 0; i < VSENSOR_SOURCE_MAX_COUNT; i++) {
				if (v->sensors[i])
					printf(",%d", v->sensors[i]);
			}
		}
		printf("\n");
	} else {
		ret = 2;
		if ((param = strdup(args)) == NULL)
			return 1;

		if ((tok = strtok_r(param, ",", &saveptr)) != NULL) {
			vsmode = str2vsmode(tok);
			if (vsmode == VSMODE_MANUAL) {
				tok = strtok_r(NULL, ",", &saveptr);
				if (str_to_float(tok, &default_temp)) {
					tok = strtok_r(NULL, ",", &saveptr);
					if (str_to_int(tok, &timeout, 10)) {
						if (timeout < 0)
							timeout = 0;
						log_msg(LOG_NOTICE, "vsensor%d: set source to %s,%0.2f,%d",
							sensor + 1,
							vsmode2str(vsmode),
							default_temp,
							timeout);
						v->mode = vsmode;
						v->default_temp = default_temp;
						v->timeout = timeout;
						ret = 0;
					}
				}
			} else if (vsmode == VSMODE_I2C) {
				tok = strtok_r(NULL, ",", &saveptr);
				if (str_to_int(tok, &val, 16)) {
					if (val > 0 && val < 128 && !i2c_reserved_address(val)) {
						tok = strtok_r(NULL, ",", &saveptr);
						uint type = get_i2c_sensor_type(tok);
						if (type > 0) {
							log_msg(LOG_NOTICE, "vsensor%d: set source to %s,0x%02x,%s",
								sensor + 1,
								vsmode2str(vsmode),
								val,
								i2c_sensor_type_str(type));
							v->mode = vsmode;
							v->i2c_type = type;
							v->i2c_addr = val;
							ret = 0;
						}
					}
				}
			} else if (vsmode == VSMODE_PICOTEMP) {
				log_msg(LOG_NOTICE, "vsensor%d: set source to Pico (MCU) temperature.",
					sensor + 1);
				v->mode = vsmode;
				for(i = 0; i < VSENSOR_COUNT; i++) {
					v->sensors[i] = 0;
				}
				ret = 0;
			} else {
				temp_str[0] = 0;
				for(i = 0; i < VSENSOR_SOURCE_MAX_COUNT; i++)
					selected[i] = 0;
				while((tok = strtok_r(NULL, ",", &saveptr)) != NULL) {
					if (count < VSENSOR_SOURCE_MAX_COUNT && str_to_int(tok, &val, 10)) {
						if ((val >= 1 && val <= VSENSOR_COUNT) || val == 101) {
							selected[count++] = val;
							snprintf(tmp, sizeof(tmp), ",%d", val);
							strncatenate(temp_str, tmp, sizeof(temp_str));
						}
					}
				}
				if (count >= 2) {
					log_msg(LOG_NOTICE, "vsensor%d: set source to %s%s",
						sensor + 1,
						vsmode2str(vsmode),
						temp_str);
					v->mode = vsmode;
					for(i = 0; i < VSENSOR_COUNT; i++) {
						v->sensors[i] = selected[i];
					}
					ret = 0;
				}
			}
		}
		free(param);
	}

	return ret;
}

int cmd_vsensor_temp(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int sensor;
	float d;

	if (!query)
		return 1;

	if (!strncasecmp(prev_cmd, "vsensor", 7)) {
		sensor = atoi(&prev_cmd[7]) - 1;
	} else {
		sensor = atoi(&cmd[7]) - 1;
	}

	if (sensor >= 0 && sensor < VSENSOR_COUNT) {
		d = st->vtemp[sensor];
		log_msg(LOG_DEBUG, "vsensor%d temperature = %fC", sensor + 1, d);
		printf("%.0f\n", d);
		return 0;
	}

	return 1;
}

int cmd_vsensor_humidity(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int sensor;
	float d;

	if (!query)
		return 1;

	sensor = atoi(&prev_cmd[7]) - 1;
	if (sensor >= 0 && sensor < VSENSOR_COUNT) {
		d = st->vhumidity[sensor];
		log_msg(LOG_DEBUG, "vsensor%d humidity = %f%%", sensor + 1, d);
		printf("%.0f\n", d);
		return 0;
	}

	return 1;
}

int cmd_vsensor_pressure(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int sensor;
	float d;

	if (!query)
		return 1;

	sensor = atoi(&prev_cmd[7]) - 1;
	if (sensor >= 0 && sensor < VSENSOR_COUNT) {
		d = st->vpressure[sensor];
		log_msg(LOG_DEBUG, "vsensor%d pressure = %fhPa", sensor + 1, d);
		printf("%.0f\n", d);
		return 0;
	}

	return 1;
}

int cmd_vsensors_read(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int i;

	if (!query)
		return 1;

	for (i = 0; i < VSENSOR_COUNT; i++) {
		printf("vsensor%d,\"%s\",%.1lf,%.0f,%0.0f\n", i+1,
			conf->vsensors[i].name,
			st->vtemp[i],
			st->vhumidity[i],
			st->vpressure[i]);
	}

	return 0;
}

int cmd_vsensor_write(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int sensor;
	float val;

	if (query)
		return 1;

	if (!strncasecmp(prev_cmd, "vsensor", 7)) {
		sensor = atoi(&prev_cmd[7]) - 1;
	} else {
		sensor = atoi(&cmd[7]) - 1;
	}

	if (sensor >= 0 && sensor < VSENSOR_COUNT) {
		if (conf->vsensors[sensor].mode == VSMODE_MANUAL) {
			if (str_to_float(args, &val)) {
				log_msg(LOG_INFO, "vsensor%d: write temperature = %fC", sensor + 1, val);
				st->vtemp[sensor] = val;
				st->vtemp_updated[sensor] = get_absolute_time();
				return 0;
			}
		} else {
			return 2;
		}
	}

	return 1;
}



const struct cmd_t display_commands[] = {
	{ "LAYOUTR",   7, NULL,              cmd_display_layout_r },
	{ "LOGO",      4, NULL,              cmd_display_logo },
	{ "THEMe",     4, NULL,              cmd_display_theme },
	{ 0, 0, 0, 0 }
};

const struct cmd_t lfs_commands[] = {
	{ "COPY",      4, NULL,              cmd_lfs_copy },
	{ "DELete",    3, NULL,              cmd_lfs_del },
	{ "DIRectory", 3, NULL,              cmd_lfs_dir },
	{ "FORMAT",    6, NULL,              cmd_lfs_format },
	{ "REName",    3, NULL,              cmd_lfs_ren },
	{ 0, 0, 0, 0 }
};

const struct cmd_t wifi_commands[] = {
#ifdef WIFI_SUPPORT
	{ "AUTHmode",  4, NULL,              cmd_wifi_auth_mode },
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

const struct cmd_t mqtt_ha_commands[] = {
	{ "DISCovery", 4, NULL,              cmd_mqtt_ha_discovery },
	{ 0, 0, 0, 0 }
};

const struct cmd_t mqtt_commands[] = {
	{ "SERVer",    4, NULL,              cmd_mqtt_server },
	{ "PORT",      4, NULL,              cmd_mqtt_port },
	{ "USER",      4, NULL,              cmd_mqtt_user },
	{ "PASSword",  4, NULL,              cmd_mqtt_pass },
	{ "SCPI",      4, NULL,              cmd_mqtt_allow_scpi },
#if TLS_SUPPORT
	{ "TLS",       3, NULL,              cmd_mqtt_tls },
#endif
	{ "HA",        2, mqtt_ha_commands, NULL },
	{ "INTerval",  3, mqtt_interval_commands, NULL },
	{ "MASK",      4, mqtt_mask_commands, NULL },
	{ "TOPIC",     5, mqtt_topic_commands, NULL },
	{ 0, 0, 0, 0 }
};

const struct cmd_t ssh_pkey_commands[] = {
	{ "CREate",    3, NULL,              cmd_ssh_pkey_create },
	{ "DELete",    3, NULL,              cmd_ssh_pkey_del },
	{ "LIST",      4, NULL,              cmd_ssh_pkey },
	{ 0, 0, 0, 0 }
};

const struct cmd_t ssh_pubkey_commands[] = {
	{ "ADD",       3, NULL,              cmd_ssh_pubkey_add },
	{ "DELete",    3, NULL,              cmd_ssh_pubkey_del },
	{ "LIST",      4, NULL,              cmd_ssh_pubkey },
	{ 0, 0, 0, 0 }
};

const struct cmd_t ssh_commands[] = {
	{ "ACLs",      3, NULL,              cmd_ssh_acls },
	{ "AUTH",      4, NULL,              cmd_ssh_auth },
	{ "PORT",      4, NULL,              cmd_ssh_port },
	{ "SERVer",    4, NULL,              cmd_ssh_server },
	{ "PASSword",  4, NULL,              cmd_ssh_pass },
	{ "USER",      4, NULL,              cmd_ssh_user },
	{ "KEY",       3, ssh_pkey_commands, cmd_ssh_pkey },
	{ "PUBKEY",    6, ssh_pubkey_commands, cmd_ssh_pubkey },
	{ 0, 0, 0, 0 }
};

const struct cmd_t telnet_commands[] = {
	{ "ACLs",      3, NULL,              cmd_telnet_acls },
	{ "AUTH",      4, NULL,              cmd_telnet_auth },
	{ "PORT",      4, NULL,              cmd_telnet_port },
	{ "RAWmode",   3, NULL,              cmd_telnet_rawmode },
	{ "SERVer",    4, NULL,              cmd_telnet_server },
	{ "PASSword",  4, NULL,              cmd_telnet_pass },
	{ "USER",      4, NULL,              cmd_telnet_user },
	{ 0, 0, 0, 0 }
};

const struct cmd_t tls_commands[] = {
#if TLS_SUPPORT
	{ "CERT",      4, NULL,              cmd_tls_cert },
	{ "PKEY",      4, NULL,              cmd_tls_pkey },
#endif
	{ 0, 0, 0, 0 }
};
#endif



const struct cmd_t i2c_commands[] = {
	{ "SCAN",      4, NULL,              cmd_i2c_scan },
	{ "SPEED",     5, NULL,              cmd_i2c_speed },
	{ 0, 0, 0, 0 }
};

const struct cmd_t system_commands[] = {
	{ "DEBUG",     5, NULL,              cmd_debug }, /* Obsolete ? */
	{ "DISPlay",   4, display_commands,  cmd_display_type },
	{ "ECHO",      4, NULL,              cmd_echo },
	{ "ERRor",     3, NULL,              cmd_err },
	{ "FLASH",     5, NULL,              cmd_flash },
	{ "I2C",       3, i2c_commands,      cmd_i2c },
	{ "GAMMA",     5, NULL,              cmd_gamma },
	{ "OUTputs",   3, NULL,              cmd_outputs },
	{ "LED",       3, NULL,              cmd_led },
	{ "LFS",       3, lfs_commands,      cmd_lfs },
	{ "LOG",       3, NULL,              cmd_log_level },
	{ "MEMLOG",    6, NULL,              cmd_mem_log },
	{ "MEMory",    3, NULL,              cmd_memory },
	{ "NAME",      4, NULL,              cmd_name },
	{ "PWMfreq",   3, NULL,              cmd_pwm_freq },
	{ "SERIAL",    6, NULL,              cmd_serial },
	{ "SPI",       3, NULL,              cmd_spi },
	{ "SYSLOG",    6, NULL,              cmd_syslog_level },
	{ "TIMEZONE",  8, NULL,              cmd_timezone },
	{ "TIME",      4, NULL,              cmd_time },
	{ "UPGRADE",   7, NULL,              cmd_usb_boot },
	{ "UPTIme",    4, NULL,              cmd_uptime },
	{ "VERsion",   3, NULL,              cmd_version },
	{ "VSENSORS",  8, NULL,              cmd_vsensors },
	{ "WIFI",      4, wifi_commands,     cmd_wifi },
#ifdef WIFI_SUPPORT
	{ "MQTT",      4, mqtt_commands,     NULL },
	{ "TELNET",    6, telnet_commands,   NULL },
	{ "SSH",       3, ssh_commands,      NULL },
	{ "TLS",       3, tls_commands,      NULL },
#endif
	{ 0, 0, 0, 0 }
};

const struct cmd_t defaults_c_commands[] = {
	{ "PWM",       3, NULL,              cmd_defaults_pwm },
	{ "STAte",     3, NULL,              cmd_defaults_state },
	{ 0, 0, 0, 0 }
};

const struct cmd_t output_c_commands[] = {
	{ "EFFect",    3, NULL,              cmd_out_effect },
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

const struct cmd_t vsensor_c_commands[] = {
	{ "NAME",        4, NULL,            cmd_vsensor_name },
	{ "SOUrce",      3, NULL,            cmd_vsensor_source },
	{ 0, 0, 0, 0 }
};

const struct cmd_t vsensors_c_commands[] = {
	{ "SOUrce",   3, NULL,               cmd_vsensors_sources },
	{ 0, 0, 0, 0 }
};

const struct cmd_t config_commands[] = {
	{ "DEFAULTS",  8, defaults_c_commands, NULL },
	{ "DELete",    3, NULL,              cmd_delete_config },
	{ "OUTPUT",    6, output_c_commands, NULL },
	{ "Read",      1, NULL,              cmd_print_config },
	{ "SAVe",      3, NULL,              cmd_save_config },
	{ "TIMER",     5, timer_c_commands,  cmd_timer },
	{ "VSENSORS",  8, vsensors_c_commands, cmd_vsensors_sources },
	{ "VSENSOR",   7, vsensor_c_commands, NULL },
	{ 0, 0, 0, 0 }
};

const struct cmd_t output_commands[] = {
	{ "PWM",       3, NULL,              cmd_out_read },
	{ "Read",      1, NULL,              cmd_out_read },
	{ 0, 0, 0, 0 }
};

const struct cmd_t vsensor_commands[] = {
	{ "HUMidity",  3, NULL,              cmd_vsensor_humidity },
	{ "PREssure",  3, NULL,              cmd_vsensor_pressure },
	{ "Read",      1, NULL,              cmd_vsensor_temp },
	{ "TEMP",      4, NULL,              cmd_vsensor_temp },
	{ 0, 0, 0, 0 }
};

const struct cmd_t measure_commands[] = {
	{ "OUTPUT",    6, output_commands,   cmd_out_read },
	{ "Read",      1, NULL,              cmd_read },
	{ "VSENSORS",  8, NULL,              cmd_vsensors_read },
	{ "VSENSOR",   7, vsensor_commands,  cmd_vsensor_temp },
	{ 0, 0, 0, 0 }
};

const struct cmd_t write_o_commands[] = {
	{ "PWM",       3, NULL,              cmd_write_pwm },
	{ "STAte",     3, NULL,              cmd_write_state },
	{ 0, 0, 0, 0 }
};

const struct cmd_t write_commands[] = {
	{ "OUTPUT",    6, write_o_commands,  cmd_write_state },
	{ "VSENSOR",   7, NULL,              cmd_vsensor_write },
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
	{ "EXIT",      4, NULL,              cmd_exit },
	{ "MEAsure",   3, measure_commands,  NULL },
	{ "SYStem",    3, system_commands,   NULL },
	{ "Read",      1, NULL,              cmd_read },
	{ "WHO",       3, NULL,              cmd_who },
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
