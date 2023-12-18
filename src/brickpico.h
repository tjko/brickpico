/* brickpico.h
   Copyright (C) 2021-2023 Timo Kokkonen <tjko@iki.fi>

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

#ifndef BRICKPICO_H
#define BRICKPICO_H 1

#include "config.h"
#include "log.h"
#include <time.h>
#include "pico/mutex.h"
#ifdef LIB_PICO_CYW43_ARCH
#define WIFI_SUPPORT 1
#include "lwip/ip_addr.h"
#endif

#ifndef BRICKPICO_MODEL
#error unknown board model
#endif

#define OUTPUT_MAX_COUNT     16   /* Max number of PWM outputs on the board */

#define MAX_NAME_LEN   64
#define MAX_MAP_POINTS 32
#define MAX_GPIO_PINS  32

#define WIFI_SSID_MAX_LEN    32
#define WIFI_PASSWD_MAX_LEN  64
#define WIFI_COUNTRY_MAX_LEN 3

#ifdef NDEBUG
#define WATCHDOG_ENABLED      1
#define WATCHDOG_REBOOT_DELAY 5000
#endif

#define MAX_EVENT_NAME_LEN  30
#define MAX_EVENT_COUNT     20

enum timer_action_types {
	ACTION_NONE = 0,
	ACTION_ON = 1,
	ACTION_OFF = 2,
};

struct timer_event {
	char name[MAX_EVENT_NAME_LEN];
	int8_t minute;          /* 0-59 */
	int8_t hour;            /* 0-23 */
	uint8_t wday;            /* bitmask for weekdays */
	enum timer_action_types action;
	uint16_t mask;          /* bitmask of outputs this applies to */
};

struct pwm_output {
	char name[MAX_NAME_LEN];

	/* output PWM signal settings */
	uint8_t min_pwm;
	uint8_t max_pwm;
	uint8_t default_pwm;   /* 0..100 (PWM duty cycle) */
	uint8_t default_state; /* 0 = off, 1 = on */
	uint8_t type; /* 0 = Dimmer, 1 = Toggle (on/off) */
};

struct brickpico_config {
	struct pwm_output outputs[OUTPUT_MAX_COUNT];
	bool local_echo;
	uint8_t led_mode;
	char display_type[64];
	char display_theme[16];
	char display_logo[16];
	char display_layout_r[64];
	char name[32];
	char timezone[64];
	bool spi_active;
	bool serial_active;
	uint pwm_freq;
	struct timer_event events[MAX_EVENT_COUNT];
	uint8_t event_count;
#ifdef WIFI_SUPPORT
	char wifi_ssid[WIFI_SSID_MAX_LEN + 1];
	char wifi_passwd[WIFI_PASSWD_MAX_LEN + 1];
	char wifi_country[WIFI_COUNTRY_MAX_LEN + 1];
	uint8_t wifi_mode;
	char hostname[32];
	ip_addr_t syslog_server;
	ip_addr_t ntp_server;
	ip_addr_t ip;
	ip_addr_t netmask;
	ip_addr_t gateway;
	char mqtt_server[32];
	uint32_t mqtt_port;
	bool mqtt_tls;
	char mqtt_user[32];
	char mqtt_pass[64];
	char mqtt_status_topic[32];
	uint32_t mqtt_status_interval;
	char mqtt_cmd_topic[32];
#endif
};

struct brickpico_state {
	/* outputs */
	uint8_t pwm[OUTPUT_MAX_COUNT];
	uint8_t pwr[OUTPUT_MAX_COUNT];
};


/* brickpico.c */
extern struct brickpico_state *brickpico_state;
extern bool rebooted_by_watchdog;
extern mutex_t *state_mutex;
void update_display_state();

/* bi_decl.c */
void set_binary_info();

/* command.c */
void process_command(struct brickpico_state *state, struct brickpico_config *config, char *command);
int cmd_version(const char *cmd, const char *args, int query, char *prev_cmd);

/* config.c */
extern mutex_t *config_mutex;
extern const struct brickpico_config *cfg;
void read_config(bool multicore);
void save_config(bool multicore);
void delete_config(bool multicore);
void print_config();

/* display.c */
void display_init();
void clear_display();
void display_message(int rows, const char **text_lines);
void display_status(const struct brickpico_state *state, const struct brickpico_config *config);

/* display_oled.c */
void oled_display_init();
void oled_clear_display();
void oled_display_status(const struct brickpico_state *state, const struct brickpico_config *conf);
void oled_display_message(int rows, const char **text_lines);

/* flash.h */
int flash_read_file(char **bufptr, uint32_t *sizeptr, const char *filename, int init_flash);
int flash_write_file(const char *buf, uint32_t size, const char *filename);
int flash_delete_file(const char *filename);

/* network.c */
void network_init();
void network_mac();
void network_poll();
void network_status();
void set_pico_system_time(long unsigned int sec);
const char *network_ip();
const char *network_hostname();

/* httpd.c */
#if WIFI_SUPPORT
void brickpico_setup_http_handlers();
#endif

/* mqtt.c */
#if WIFI_SUPPORT
void brickpico_setup_mqtt_client();
void brickpico_mqtt_publish();
#endif

/* tls.c */
int read_pem_file(char *buf, uint32_t size, uint32_t timeout);
#ifdef WIFI_SUPPORT
struct altcp_tls_config* tls_server_config();
#endif

/* pwm.c */
void setup_pwm_inputs();
void setup_pwm_outputs();
void set_pwm_duty_cycle(uint fan, float duty);
float get_pwm_duty_cycle(uint fan);
void get_pwm_duty_cycles(const struct brickpico_config *config);


/* log.c */
int str2log_priority(const char *pri);
const char* log_priority2str(int pri);
int str2log_facility(const char *facility);
const char* log_facility2str(int facility);
void log_msg(int priority, const char *format, ...);
int get_debug_level();
void set_debug_level(int level);
int get_log_level();
void set_log_level(int level);
int get_syslog_level();
void set_syslog_level(int level);
void debug(int debug_level, const char *fmt, ...);

/* timer.c */
int parse_timer_event_str(const char *str, struct timer_event *event);
const char* timer_event_str(const struct timer_event *event);
int handle_timer_events(const struct brickpico_config *conf, struct brickpico_state *state);

/* util.c */
void print_mallinfo();
void print_irqinfo();
char *trim_str(char *s);
int str_to_int(const char *str, int *val, int base);
int str_to_float(const char *str, float *val);
int str_to_datetime(const char *str, datetime_t *t);
char* datetime_str(char *buf, size_t size, const datetime_t *t);
const char *rp2040_model_str();
const char *pico_serial_str();
const char *mac_address_str(const uint8_t *mac);
int check_for_change(double oldval, double newval, double threshold);
int time_passed(absolute_time_t *t, uint32_t us);
int64_t pow_i64(int64_t x, uint8_t y);
double round_decimal(double val, unsigned int decimal);
char* base64encode(const char *input);
char* base64decode(const char *input);
char *strncopy(char *dst, const char *src, size_t size);
char *strncatenate(char *dst, const char *src, size_t size);
datetime_t *tm_to_datetime(const struct tm *tm, datetime_t *t);
struct tm *datetime_to_tm(const datetime_t *t, struct tm *tm);
time_t datetime_to_time(const datetime_t *datetime);
void watchdog_disable();
int getstring_timeout_ms(char *str, uint32_t maxlen, uint32_t timeout);
int clamp_int(int val, int min, int max);
void* memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);


#endif /* BRICKPICO_H */
