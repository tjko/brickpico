/* timer.c
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
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

#include "brickpico.h"


/* Event string syntax:
    <minute> <hour> <day of week> <action> <fans> <comments> ...

    30 18 * on 1,2,3,4 Turn 1-4 on at 18:30
    45 18 * on 5,6,7,8 Turn 5-8 on at 18:45
    0 23 1-5 off * Turn all off at 23:00 during the week
    0 0 0,6 off * Turn all off at 00:00 on weekends
*/


const char* timer_action_type_str(enum timer_action_types type)
{
	switch (type) {
	case ACTION_ON:
		return "ON";
	case ACTION_OFF:
		return "OFF";
	default:
		break;
	}
	return "NONE";
}

enum timer_action_types str_to_timer_action_type(const char *str)
{
	if (!str)
		return ACTION_NONE;

	if (!strncasecmp(str, "ON", 2))
		return ACTION_ON;
	else if (!strncasecmp(str, "OFF", 3))
		return ACTION_OFF;

	return ACTION_NONE;
}

int parse_timer_event_str(const char *str, struct timer_event *event)
{
	char *tok, *saveptr, *s;
	int count = 0;
	int res = 10;

	if (!str || !event)
		return -1;
	if ((s = strdup(str)) == NULL)
		return -2;

	event->name[0] = 0;

	tok = strtok_r(s, " ", &saveptr);
	while (tok) {
		int i;
		int as = (tok[0] == '*' && tok[1] == 0) ? 1 : 0;
		if (count == 0) { /* minute */
			if (as) {
				event->minute = -1;
			} else if (str_to_int(tok, &i, 10)) {
				if (i >= 0 && i <= 59) {
					event->minute = i;
				} else {
					res = 1;
					goto abort;
				}
			} else {
				res = 1;
				goto abort;
			}
		} else if (count == 1) { /* hour */
			if (as) {
				event->hour = -1;
			} else if (str_to_int(tok, &i, 10)) {
				if (i >= 0 && i <= 23) {
					event->hour = i;
				} else {
					res = 2;
					goto abort;
				}
			} else {
				res = 2;
				goto abort;
			}
		} else if (count == 2) { /* day of week */
			uint32_t m;
			if (str_to_bitmask(tok, 7, &m, 0) == 0) {
				event->wday = m;
			} else {
				res = 3;
				goto abort;
			}
		} else if (count == 3) { /* action */
			enum timer_action_types a = str_to_timer_action_type(tok);
			if (a != ACTION_NONE) {
				event->action = a;
			} else {
				res = 4;
				goto abort;
			}
		} else if (count == 4) { /* output mask */
			uint32_t m;
			if (str_to_bitmask(tok, OUTPUT_COUNT, &m, 1) == 0) {
				event->mask = m;
			} else {
				res = 5;
				goto abort;
			}
			res = 0;
		} else { /* name */
			if (strlen(event->name) > 0)
				strncatenate(event->name, " ", sizeof(event->name));
			strncatenate(event->name, tok, sizeof(event->name));
		}

		tok = strtok_r(NULL, " ", &saveptr);
		count++;
	}

abort:
	free(s);
	return res;
}


const char* timer_event_str(const struct timer_event *e)
{
	static char buf[128];
	char tmp[16];

	if (!e)
		return NULL;

	buf[0] = 0;

	/* minute */
	if (e->minute >= 0)
		snprintf(tmp, sizeof(tmp), "%02d ", e->minute);
	else
		strncopy(tmp, "* ", sizeof(tmp));
	strncatenate(buf, tmp, sizeof(buf));

	/* hour */
	if (e->hour >= 0)
		snprintf(tmp, sizeof(tmp), "%02d ", e->hour);
	else
		strncopy(tmp, "* ", sizeof(tmp));
	strncatenate(buf, tmp, sizeof(buf));

	/* day of week */
	if (e->wday > 0) {
		strncatenate(buf, bitmask_to_str(e->wday, 7, 0, true), sizeof(buf));
		strncatenate(buf, " ", sizeof(buf));
	} else {
		strncatenate(buf, "* ", sizeof(buf));
	}

	/* action */
	strncatenate(buf, timer_action_type_str(e->action), sizeof(buf));
	strncatenate(buf, " ", sizeof(buf));

	/* mask */
	if (e->mask > 0) {
		strncatenate(buf, bitmask_to_str(e->mask, OUTPUT_COUNT, 1, true), sizeof(buf));
	} else {
		strncatenate(buf, "*", sizeof(buf));
	}

	/* name */
	if (strlen(e->name) > 0) {
		strncatenate(buf, " ", sizeof(buf));
		strncatenate(buf, e->name, sizeof(buf));
	}

	return buf;
}


int check_timer_event(const struct timer_event *e, struct tm *t)
{
	if (!e || !t)
		return 0;

	if (e->minute >= 0 && e->minute != t->tm_min)
		return 0;
	if (e->hour >= 0 && e->hour != t->tm_hour)
		return 0;
	if (e->wday > 0 && !(e->wday & ((1 << t->tm_wday))))
			return 0;

	return 1;
}

int handle_timer_events(const struct brickpico_config *conf, struct brickpico_state *state)
{
	static time_t lastevent[MAX_EVENT_COUNT];
	static uint8_t initialized = 0;
	char tmp[32];
	struct tm t;
	time_t t_now;
	int res = 0;
	int i, o;


	if (!initialized) {
		for (i = 0; i < MAX_EVENT_COUNT; i++) {
			lastevent[i] = 0;
		}
		initialized = 1;
	}

	/* Get current time from RTC */
	if (!rtc_get_time(&t_now))
		return -1;
	localtime_r(&t_now, &t);

	/* Check if any of the timer events match current RTC time... */
	for (i = 0; i < conf->event_count; i++) {
		const struct timer_event *e = &conf->events[i];

		if (t_now < lastevent[i] + 60)
			continue;
		if (!check_timer_event(e, &t))
			continue;

		log_msg(LOG_NOTICE,"timer_event[%d]: outputs=%s, action=%s, time=%s",
			i + 1,
			bitmask_to_str(e->mask, OUTPUT_COUNT, 1, true),
			timer_action_type_str(e->action),
			time_t_to_str(tmp, sizeof(tmp), t_now));

		for (o = 0; o < OUTPUT_COUNT; o++) {
			if (e->mask & (1 << o)) {
				state->pwr[o] = (e->action == ACTION_ON ? 1 : 0);
			}
		}

		lastevent[i] = t_now;
		res++;
	}

	return res;
}

/* eof :-) */
