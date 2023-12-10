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
	struct cmd_t *subcmds;
	int (*func)(const char *cmd, const char *args, int query, char *prev_cmd);
};

struct error_t {
	const char    *error;
	int            error_num;
};

struct error_t error_codes[] = {
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


int valid_wifi_country(const char *country)
{
	if (!country)
		return 0;

	if (strlen(country) < 2)
		return 0;

	if (!(country[0] >= 'A' && country[0] <= 'Z'))
		return 0;
	if (!(country[1] >= 'A' && country[1] <= 'Z'))
		return 0;

	if (strlen(country) == 2)
		return 1;

	if (country[2] >= '1' && country[2] <= '9')
		return 1;

	return 0;
}

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

	snprintf(buf, sizeof(buf), " fanpico-%s-%s", BRICKPICO_MODEL, PICO_BOARD);
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
	int mode;

	if (query) {
		printf("%d\n", conf->led_mode);
	} else if (str_to_int(args, &mode, 10)) {
		if (mode >= 0 && mode <= 2) {
			log_msg(LOG_NOTICE, "Set system LED mode: %d -> %d", conf->led_mode, mode);
			conf->led_mode = mode;
		}
	}
	return 0;
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
	int val;

	if (query) {
		printf("%u\n", conf->local_echo);
	} else if (str_to_int(args, &val, 10)) {
		conf->local_echo = (val > 0 ? true : false);
	}
	return 0;
}


int cmd_display_type(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", conf->display_type);
	} else {
		strncopy(conf->display_type, args, sizeof(conf->display_type));
	}
	return 0;
}

int cmd_display_theme(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", conf->display_theme);
	} else {
		strncopy(conf->display_theme, args, sizeof(conf->display_theme));
	}
	return 0;
}

int cmd_display_logo(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", conf->display_logo);
	} else {
		strncopy(conf->display_theme, args, sizeof(conf->display_logo));
	}
	return 0;
}

int cmd_display_layout_r(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", conf->display_layout_r);
	} else {
		strncopy(conf->display_layout_r, args, sizeof(conf->display_layout_r));
	}
	return 0;
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
		printf("output%d,\"%s\",%d\n", i + 1,
			conf->outputs[i].name,
			st->pwm[i]);
	}

	return 0;
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
				log_msg(LOG_NOTICE, "output%d: change default PWM %d%% --> %d%%",
					out + 1, conf->outputs[out].default_pwm, val);
				conf->outputs[out].default_pwm = val;
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
			printf("%d\n", conf->outputs[out].default_state);
		} else if (str_to_int(args, &val, 10)) {
			if (val >= 0 && val <= 1) {
				log_msg(LOG_NOTICE, "output%d: change default state %d --> %d",
					out + 1, conf->outputs[out].default_state, val);
				conf->outputs[out].default_state = val;
			} else {
				log_msg(LOG_WARNING, "output%d: invalid new value for state: %d", out + 1,
					val);
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

int cmd_out_write(const char *cmd, const char *args, int query, char *prev_cmd)
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
		if (str_to_int(args, &val, 10)) {
			if (val >= 0 && val <= 100) {
				log_msg(LOG_INFO, "output%d: change PWM %d%% --> %d%%", out + 1,
					st->pwm[out], val);
				st->pwm[out] = val;
				set_pwm_duty_cycle(out, val);
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
	if (query) {
		printf("%s\n", conf->wifi_ssid);
	} else {
		log_msg(LOG_NOTICE, "Wi-Fi SSID change '%s' --> '%s'",
			conf->wifi_ssid, args);
		strncopy(conf->wifi_ssid, args, sizeof(conf->wifi_ssid));
	}
	return 0;
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
	if (query) {
		printf("%s\n", conf->wifi_country);
	} else {
		if (valid_wifi_country(args)) {
			log_msg(LOG_NOTICE, "Wi-Fi Country change '%s' --> '%s'",
				conf->wifi_country, args);
			strncopy(conf->wifi_country, args, sizeof(conf->wifi_country));
		} else {
			log_msg(LOG_WARNING, "Invalid Wi-Fi country: %s", args);
			return 2;
		}
	}
	return 0;
}

int cmd_wifi_password(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", conf->wifi_passwd);
	} else {
		log_msg(LOG_NOTICE, "Wi-Fi Password change '%s' --> '%s'",
			conf->wifi_passwd, args);
		strncopy(conf->wifi_passwd, args, sizeof(conf->wifi_passwd));
	}
	return 0;
}

int cmd_wifi_hostname(const char *cmd, const char *args, int query, char *prev_cmd)
{
	if (query) {
		printf("%s\n", conf->hostname);
	} else {
		for (int i = 0; i < strlen(args); i++) {
			if (!(isalpha((int)args[i]) || args[i] == '-')) {
				return 1;
			}
		}
		log_msg(LOG_NOTICE, "System hostname change '%s' --> '%s'", conf->hostname, args);
		strncopy(conf->hostname, args, sizeof(conf->hostname));
	}
	return 0;
}

int cmd_wifi_mode(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int val;

	if (query) {
		printf("%u\n", conf->wifi_mode);
		return 0;
	}

	if (str_to_int(args, &val, 10)) {
		if (val >= 0 && val <= 1) {
			log_msg(LOG_NOTICE, "WiFi mode change %d --> %d", cfg->wifi_mode, val);
			conf->wifi_mode = val;
		} else {
			log_msg(LOG_WARNING, "Invalid WiFi mode: %s", args);
			return 2;
		}
		return 0;
	}
	return 1;
}

#if TLS_SUPPORT
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

	if (query) {
		if (rtc_get_datetime(&t)) {
			printf("%04d-%02d-%02d %02d:%02d:%02d\n",
				t.year, t.month, t.day,	t.hour, t.min, t.sec);
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
	if (query) {
		printf("%s\n", conf->timezone);
		return 0;
	}

	const char *tz = args;
	while (iswspace(*tz))
		tz++;
	strncopy(conf->timezone, tz, sizeof(conf->timezone));
	if (strlen(tz) > 0) {
		log_msg(LOG_NOTICE, "Set timezone: %s", conf->timezone);
	} else {
		log_msg(LOG_NOTICE, "Clear timezone setting.");
	}
	return 0;
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
	if (query) {
		printf("%s\n", conf->name);
	} else {
		log_msg(LOG_NOTICE, "System name change '%s' --> '%s'", conf->name, args);
		strncopy(conf->name, args, sizeof(conf->name));
	}
	return 0;
}

int cmd_memory(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int blocksize;

	if (query) {
		print_mallinfo();
		return 0;
	}
	if (str_to_int(args, &blocksize, 10)) {
		if (blocksize >= 512) {
			void *buf = NULL;
			size_t bufsize = blocksize;
			do {
				if (buf) {
					free(buf);
					bufsize += blocksize;
				}
				buf = malloc(bufsize);
			} while (buf);
			printf("Maximum available memory: %u bytes\n", bufsize - blocksize);
		}
		return 0;
	}
	return 1;
}

int cmd_serial(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int val;

	if (query) {
		printf("%d\n", conf->serial_active);
		return 0;
	}
	if (str_to_int(args, &val, 10)) {
		if (val >= 0 && val <= 1) {
			log_msg(LOG_NOTICE, "Serial console active: %d -> %d", conf->serial_active, val);
			conf->serial_active = val;
			return 0;
		}
	}
	return 1;
}


int cmd_spi(const char *cmd, const char *args, int query, char *prev_cmd)
{
	int val;

	if (query) {
		printf("%d\n", conf->spi_active);
		return 0;
	}
	if (str_to_int(args, &val, 10)) {
		if (val >= 0 && val <= 1) {
			log_msg(LOG_NOTICE, "SPI (LCD Display) active: %d -> %d", conf->spi_active, val);
			conf->spi_active = val;
			return 0;
		}
	}
	return 1;
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
	}

	return 1;
}


struct cmd_t display_commands[] = {
	{ "LAYOUTR",   7, NULL,              cmd_display_layout_r },
	{ "LOGO",      4, NULL,              cmd_display_logo },
	{ "THEMe",     4, NULL,              cmd_display_theme },
	{ 0, 0, 0, 0 }
};
struct cmd_t wifi_commands[] = {
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

struct cmd_t tls_commands[] = {
#ifdef WIFI_SUPPORT
#if TLS_SUPPORT
	{ "CERT",      4, NULL,              cmd_tls_cert },
	{ "PKEY",      4, NULL,              cmd_tls_pkey },
#endif
#endif
	{ 0, 0, 0, 0 }
};

struct cmd_t system_commands[] = {
	{ "DEBUG",     5, NULL,              cmd_debug }, /* Obsolete ? */
	{ "DISPlay",   4, display_commands,  cmd_display_type },
	{ "ECHO",      4, NULL,              cmd_echo },
	{ "ERRor",     3, NULL,              cmd_err },
	{ "OUTputs",   3, NULL,              cmd_outputs },
	{ "LED",       3, NULL,              cmd_led },
	{ "LOG",       3, NULL,              cmd_log_level },
	{ "MEMory",    3, NULL,              cmd_memory },
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

struct cmd_t output_c_commands[] = {
	{ "MAXpwm",    3, NULL,              cmd_out_max_pwm },
	{ "MINpwm",    3, NULL,              cmd_out_min_pwm },
	{ "NAME",      4, NULL,              cmd_out_name },
	{ "PWM",       3, NULL,              cmd_out_default_pwm },
	{ "STAte",     3, NULL,              cmd_out_default_state },
	{ 0, 0, 0, 0 }
};

struct cmd_t config_commands[] = {
	{ "DELete",    3, NULL,              cmd_delete_config },
	{ "OUTPUT",    6, output_c_commands, NULL },
	{ "Read",      1, NULL,              cmd_print_config },
	{ "SAVe",      3, NULL,              cmd_save_config },
	{ 0, 0, 0, 0 }
};

struct cmd_t output_commands[] = {
	{ "PWM",       3, NULL,              cmd_out_read },
	{ "Read",      1, NULL,              cmd_out_read },
	{ 0, 0, 0, 0 }
};

struct cmd_t measure_commands[] = {
	{ "OUTPUT",    6, output_commands,   cmd_out_read },
	{ "Read",      1, NULL,              cmd_read },
	{ 0, 0, 0, 0 }
};

struct cmd_t write_commands[] = {
	{ "OUTPUT",    6, NULL,              cmd_out_write },
	{ 0, 0, 0, 0 }
};

struct cmd_t commands[] = {
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



struct cmd_t* run_cmd(char *cmd, struct cmd_t *cmd_level, char **prev_subcmd)
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
	struct cmd_t *cmd_level = commands;

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
