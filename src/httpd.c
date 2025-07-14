/* httpd.c
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
#include <string.h>
#include <time.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "cJSON.h"
#ifdef LIB_PICO_CYW43_ARCH
#include "lwip/apps/httpd.h"
#endif

#include "brickpico.h"

#ifdef WIFI_SUPPORT

#define INDEX_URL "/brickpico-" BRICKPICO_BOARD ".shtml"
#define BUF_LEN 1024


u16_t timer_table(char *insert, int insertlen, u16_t current_tag_part, u16_t *next_tag_part)
{
	static char *buf = NULL;
	static char *p;
	static u16_t part;
	static size_t buf_left;
	char row[128];
	int i;
	size_t printed, count;

	/* printf("timer_table(%p,%d,%u,%u)\n", (void*)insert, insertlen,
	          current_tag_part, *next_tag_part); */

	if (current_tag_part == 0) {
		/* Generate 'output' into a buffer that then will be fed in chunks to LwIP... */
		if (!(buf = malloc(BUF_LEN)))
			return 0;
		buf[0] = 0;

		for (i = 0; i < cfg->event_count; i++) {
			const struct timer_event *e = &cfg->events[i];
			char tmp[64];

			snprintf(row, sizeof(row), "<tr><td>%d<td>", i + 1);
			strncatenate(row, (e->wday > 0 ? bitmask_to_str(e->wday, 7, 0, true) : "*"),
				sizeof(row));
			strncatenate(row, "<td>", sizeof(row));
			if (e->hour >= 0) {
				snprintf(tmp, sizeof(tmp), "%02d:", e->hour);
			} else {
				strncopy(tmp, "**:", sizeof(tmp));
			}
			strncatenate(row, tmp, sizeof(row));
			if (e->minute >= 0) {
				snprintf(tmp, sizeof(tmp), "%02d<td>", e->minute);
			} else {
				strncopy(tmp, "**<td>", sizeof(tmp));
			}
			strncatenate(row, tmp, sizeof(row));
			strncatenate(row, timer_action_type_str(e->action), sizeof(row));
			strncatenate(row, "<td>", sizeof(row));
			if (e->mask > 0) {
				strncatenate(row, bitmask_to_str(e->mask, OUTPUT_COUNT, 1, true),
					sizeof(row));
			} else {
				strncatenate(row, "*", sizeof(row));
			}
			strncatenate(row, "<td>", sizeof(row));
			strncatenate(row, e->name, sizeof(row));
			strncatenate(row, "</tr>\n", sizeof(row));

			strncatenate(buf, row, BUF_LEN);
		}

		if (cfg->event_count == 0) {
			snprintf(buf, BUF_LEN, "<tr><td colspan=\"6\">No Timers Defined.</tr>\n");
		}

		p = buf;
		buf_left = strlen(buf);
		part = 1;
	}

	/* Copy a part of the multi-part response into LwIP buffer ...*/
	count = (buf_left < insertlen - 1 ? buf_left : insertlen - 1);
	memcpy(insert, p, count);

	p += count;
	printed = count;
	buf_left -= count;

	if (buf_left > 0) {
		*next_tag_part = part++;
	} else {
		free(buf);
		buf = p = NULL;
	}

	return printed;
}


u16_t csv_stats(char *insert, int insertlen, u16_t current_tag_part, u16_t *next_tag_part)
{
	const struct brickpico_state *st = brickpico_state;
	static char *buf = NULL;
	static char *p;
	static u16_t part;
	static size_t buf_left;
	char row[128];
	int i;
	size_t printed, count;

	if (current_tag_part == 0) {
		/* Generate 'output' into a buffer that then will be fed in chunks to LwIP... */
		if (!(buf = malloc(BUF_LEN)))
			return 0;
		buf[0] = 0;

		for (i = 0; i < OUTPUT_COUNT; i++) {
			snprintf(row, sizeof(row), "output%d,\"%s\",%u,%s\n",
				i+1,
				cfg->outputs[i].name,
				st->pwm[i],
				st->pwr[i] ? "ON" : "OFF");
			strncatenate(buf, row, BUF_LEN);
		}

		p = buf;
		buf_left = strlen(buf);
		part = 1;
	}

	/* Copy a part of the multi-part response into LwIP buffer ...*/
	count = (buf_left < insertlen - 1 ? buf_left : insertlen - 1);
	memcpy(insert, p, count);

	p += count;
	printed = count;
	buf_left -= count;

	if (buf_left > 0) {
		*next_tag_part = part++;
	} else {
		free(buf);
		buf = p = NULL;
	}

	return printed;
}


u16_t json_stats(char *insert, int insertlen, u16_t current_tag_part, u16_t *next_tag_part)
{
	const struct brickpico_state *st = brickpico_state;
	cJSON *json = NULL;
	static char *buf = NULL;
	static char *p;
	static u16_t part;
	static size_t buf_left;
	int i;
	size_t printed, count;

	if (current_tag_part == 0) {
		/* Generate 'output' into a buffer that then will be fed in chunks to LwIP... */
		cJSON *array, *o;

		if (!(json = cJSON_CreateObject()))
			goto panic;

		/* Outputs */
		if (!(array = cJSON_CreateArray()))
			goto panic;
		for (i = 0; i < OUTPUT_COUNT; i++) {
			if (!(o = cJSON_CreateObject()))
				goto panic;

			cJSON_AddItemToObject(o, "output", cJSON_CreateNumber(i+1));
			cJSON_AddItemToObject(o, "name", cJSON_CreateString(cfg->outputs[i].name));
			cJSON_AddItemToObject(o, "duty_cycle", cJSON_CreateNumber(round_decimal(st->pwm[i], 1)));
			cJSON_AddItemToObject(o, "state", cJSON_CreateString(st->pwr[i] ? "ON" : "OFF"));
			cJSON_AddItemToArray(array, o);
		}
		cJSON_AddItemToObject(json, "outputs", array);

		if (!(buf = cJSON_Print(json)))
			goto panic;
		cJSON_Delete(json);
		json = NULL;

		p = buf;
		buf_left = strlen(buf);
		part = 1;
	}

	/* Copy a part of the multi-part response into LwIP buffer ...*/
	count = (buf_left < insertlen - 1 ? buf_left : insertlen - 1);
	memcpy(insert, p, count);

	p += count;
	printed = count;
	buf_left -= count;

	if (buf_left > 0) {
		*next_tag_part = part++;
	} else {
		free(buf);
		buf = p = NULL;
	}

	return printed;

panic:
	if (json)
		cJSON_Delete(json);
	return 0;
}



int extract_tag_index(const char *tag)
{
	if (!tag)
		return -1;

	uint32_t tag_len = strnlen(tag, 64);
	if (tag_len > 2) {
		char a = tag[tag_len - 2];
		char b = tag[tag_len - 1];

		if (isdigit(a) && isdigit(b)) {
			return (a - '0') * 10 + (b - '0') - 1;
		}
	}

	return -1;
}


u16_t brickpico_ssi_handler(const char *tag, char *insert, int insertlen,
			u16_t current_tag_part, u16_t *next_tag_part)
{
	const struct brickpico_state *st = brickpico_state;
	size_t printed = 0;
	uint32_t tag_len = strlen(tag);
	int idx = -1;


	/* printf("brickpico_ssi_handler('%s',%p,%d,%u,%p)\n",
	   tag, (void*)insert, insertlen, current_tag_part, (void*)next_tag_part); */

	if (tag_len > 2) {
		/* Check for 2 digit index in the end of tag... */
		char a = tag[tag_len - 2];
		char b = tag[tag_len - 1];

		if (isdigit(a) && isdigit(b)) {
			idx = (a - '0') * 10 + (b - '0') - 1;
		}
	}

	if (idx >= 0) {
		if (!strncmp(tag, "o_pwm", 5)) {
			if (idx < OUTPUT_COUNT)
				printed = snprintf(insert, insertlen, "%u", st->pwm[idx]);
		}
		else if (!strncmp(tag, "o_pwrr", 6)) {
			if (idx < OUTPUT_COUNT)
				printed = snprintf(insert, insertlen, "%s", (st->pwr[idx] ? "checked=\"true\"" : ""));
		}
		else if (!strncmp(tag, "o_pwr", 5)) {
			if (idx < OUTPUT_COUNT)
				printed = snprintf(insert, insertlen, "%s", (st->pwr[idx] ? "1" : "0"));
		}
		else if (!strncmp(tag, "o_name", 6)) {
			if (idx < OUTPUT_COUNT)
				printed = snprintf(insert, insertlen, "%s", cfg->outputs[idx].name);
		}
		else if (!strncmp(tag, "sensor", 6)) {
			char other[24], tmp[12];

			other[0] = 0;
			if (st->vhumidity[idx] >= 0.0) {
				snprintf(tmp, sizeof(tmp), "%0.0f %%rh", st->vhumidity[idx]);
				strncatenate(other, tmp, sizeof(other));
			}
			if (st->vpressure[idx] >= 0.0) {
				snprintf(tmp, sizeof(tmp), "%0.0f hPa", st->vpressure[idx]);
				if (strlen(other) > 0)
					strncatenate(other, ", ", sizeof(other));
				strncatenate(other, tmp, sizeof(other));
			}
			if (idx < VSENSOR_COUNT && cfg->vsensors[idx].name[0]) {
				printed = snprintf(insert, insertlen,
						"<tr><td>%s<td>%0.1f C<td>%s</tr>",
						cfg->vsensors[idx].name,
						st->vtemp[idx],
						other);
			}
		}
	}
	else if (!strncmp(tag, "datetime", 8)) {
		time_t t;
		if (rtc_get_time(&t)) {
			time_t_to_str(insert, insertlen, t);
			printed = strnlen(insert, insertlen);
		}
	}
	else if (!strncmp(tag, "uptime", 6)) {
		uint32_t secs = to_us_since_boot(get_absolute_time()) / 1000000;
		uint32_t mins =  secs / 60;
		uint32_t hours = mins / 60;
		uint32_t days = hours / 24;

		printed = snprintf(insert, insertlen, "%lu days %02lu:%02lu:%02lu",
				days,
				hours % 24,
				mins % 60,
				secs % 60);
	}
	else if (!strncmp(tag, "watchdog", 8)) {
		printed = snprintf(insert, insertlen, "%s",
				(rebooted_by_watchdog ? "&#x2639;" : "&#x263a;"));
	}
	else if (!strncmp(tag, "model", 5)) {
		printed = snprintf(insert, insertlen, "%s", BRICKPICO_MODEL);
	}
	else if (!strncmp(tag, "version", 5)) {
		printed = snprintf(insert, insertlen, "%s", BRICKPICO_VERSION);
	}
	else if (!strncmp(tag, "name", 4)) {
		printed = snprintf(insert, insertlen, "%s", cfg->name);
	}
	else if (!strncmp(tag, "csvstat", 7)) {
		printed = csv_stats(insert, insertlen, current_tag_part, next_tag_part);
	}
	else if (!strncmp(tag, "jsonstat", 8)) {
		printed = json_stats(insert, insertlen, current_tag_part, next_tag_part);
	}
	else if (!strncmp(tag, "timertbl", 9)) {
		printed = timer_table(insert, insertlen, current_tag_part, next_tag_part);
	}
	else if (!strncmp(tag, "refresh", 8)) {
		/* generate "random" refresh time for a page, to help spread out the load... */
		printed = snprintf(insert, insertlen, "%u", (uint)(30 + ((double)rand() / RAND_MAX) * 30));
	}


	/* Check if snprintf() output was truncated... */
	printed = (printed >= insertlen ? insertlen - 1 : printed);
	/* printf("printed=%u\n", printed); */

	return printed;
}


static const char* brickpico_cgi_handler(int index, int numparams, char *param[], char *value[])
{
	struct brickpico_state *st = brickpico_state;

	/* log_msg(LOG_INFO,"cgi: index=%d, numparams=%d", index, numparams); */

	if (numparams > 0) {
		for (int i = 0; i < numparams; i++) {
			char *p = param[i];
			char *v = value[i];
			int idx = extract_tag_index(p);

			log_msg(LOG_DEBUG, "cgi param[%d]: param='%s' value='%s' idx=%d", i, p, v, idx);

			if (idx >= 0) {
				if (!strncmp(p, "pwr", 3)) {
					int pwr;
					if (str_to_int(v, &pwr, 10)) {
						st->pwr[idx] = pwr ? 1 : 0;
					}
				}
				else if (!strncmp(p, "pwm", 3)) {
					int pwm;
					if (str_to_int(v, &pwm, 10)) {
						if (pwm >= 0 && pwm <= 100) {
							st->pwm[idx] = pwm;
						}
					}
				}
			} else {
				if (!strncmp(p, "pwrall", 6)) {
					int mode;
					if (str_to_int(v, &mode, 10)) {
						mode = (mode ? 1 : 0);
						for (int j = 0; j < OUTPUT_COUNT; j++) {
							st->pwr[j] = mode;
						}
					}
				}
			}
		}
	}

	/* Redirect back to "/" ... */
	return "/303.main";
}


static const char* index_handler(int index, int numparams, char *param[], char *value[])
{
	/* log_msg(LOG_INFO, "index_handler: index=%d, numparams=%d (%s)", index, numparams, INDEX_URL); */

	return INDEX_URL;
}

static const tCGI cgi_handlers[] = {
	{ "/", index_handler },
	{ "/cgi", brickpico_cgi_handler },
	{ "/index.shtml", index_handler },
};


void brickpico_setup_http_handlers()
{
	http_set_ssi_handler(brickpico_ssi_handler, NULL, 0);
	http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
}


#endif /* WIFI_SUPPORT */
