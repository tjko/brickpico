/* timer.c
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
#include <string.h>
#include "pico/stdlib.h"

#include "brickpico.h"


/* Event string syntax:
    <minute> <hour> <day of week> <action> <fans> <name>

    30 18 * on 1,2,3,4 "Turn 1-4 on at 18:30"
    45 18 * on 5,6,7,8 "Turn 5-8 on at 18:45"
    0 23 1,2,3,4,5 off * "Turn all off at 23:00 during the week"
    0 0  0,6 off * "Turn all off at 00:00 on weekends"
*/


char *bitmask_to_str(uint32_t mask, uint16_t len)
{
	static char buf[96 + 1];
	int count = 0;
	char *s = buf;

	buf[0] = 0;

	if (len > 0 && len <= 32) {
		for (int i = 0; i < len; i++) {
			if (mask & (1 << i)) {
				if (count > 0)
					*s++ = ',';
				int w = snprintf(s, 3, "%d", i);
				s += (w > 2 ? 2 : w);
				count++;
			}
		}
	}

	return buf;
}


int str_to_bitmask(const char *str, uint16_t len, uint32_t *mask)
{
	char *s, *tok, *saveptr;

	if (!str || !mask)
		return -1;
	*mask = 0;

	if (len < 1 || len > 32)
		return -2;
	if ((s = strdup(str)) == NULL)
		return -3;

	tok = strtok_r(s, ",", &saveptr);
	while (tok) {
		int i;
		if (str_to_int(tok, &i, 10)) {
			if (i >= 0 && i < len) {
				*mask |= (1 << i);
			}
		}
		tok = strtok_r(NULL, ",", &saveptr);
	}

	free(s);
	return 0;
}

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

	if (!strncmp(str, "ON", 2))
		return ACTION_ON;
	else if (!strncmp(str, "OFF", 3))
		return ACTION_OFF;

	return ACTION_NONE;
}

int parse_timer_event_str(const char *str, struct timer_event *event)
{
	char *tok, *saveptr, *s;
	int count = 0;
	int res = 0;

	if (!str || !event)
		return -1;
	if ((s = strdup(str)) == NULL)
		return -2;

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
			if (str_to_bitmask(tok, 7, &m) == 0) {
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
			if (str_to_bitmask(tok, OUTPUT_COUNT, &m) == 0) {
				event->mask = m;
			} else {
				res = 5;
				goto abort;
			}
			event->name[0] = 0;
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
	static char buf[64];
	char tmp[16];

	if (!e)
		return NULL;

	buf[0] = 0;

	/* minute */
	if (e->minute >= 0)
		snprintf(tmp, sizeof(tmp), "%-2d ", e->minute);
	else
		strncopy(tmp, "* ", sizeof(tmp));
	strncatenate(buf, tmp, sizeof(buf));

	/* hour */
	if (e->hour >= 0)
		snprintf(tmp, sizeof(tmp), "%-2d ", e->hour);
	else
		strncopy(tmp, "* ", sizeof(tmp));
	strncatenate(buf, tmp, sizeof(buf));

	/* day of week */
	if (e->wday > 0) {
		strncatenate(buf, bitmask_to_str(e->wday, 7), sizeof(buf));
		strncatenate(buf, " ", sizeof(buf));
	} else {
		strncatenate(buf, "* ", sizeof(buf));
	}

	/* action */
	strncatenate(buf, timer_action_type_str(e->action), sizeof(buf));

	/* mask */
	if (e->mask > 0) {
		strncatenate(buf, bitmask_to_str(e->mask, OUTPUT_COUNT), sizeof(buf));
		strncatenate(buf, " ", sizeof(buf));
	} else {
		strncatenate(buf, "* ", sizeof(buf));
	}

	/* name */
	strncatenate(buf, e->name, sizeof(buf));

	return buf;
}



/* eof :-) */
