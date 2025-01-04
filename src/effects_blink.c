/* effects_blink.c
   Copyright (C) 2025 Timo Kokkonen <tjko@iki.fi>

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
#include "effects.h"


typedef struct blink_context {
	float on_time;
	float off_time;
	int64_t on_l;
	int64_t off_l;
	uint8_t last_state;
	uint8_t mode;
	uint64_t start_t;
} blink_context_t;


void* effect_blink_parse_args(const char *args)
{
	blink_context_t *c;
	char *tok, *saveptr, s[64];
	float arg;

	if (!args)
		return NULL;
	if (!(c = calloc(1, sizeof(blink_context_t))))
		return NULL;

	/* Defaults (seconds) */
	c->on_time = 0.5;
	c->off_time = 1.5;

	strncopy(s, args, sizeof(s));

	/* Parse parameters */
	if ((tok = strtok_r(s, ",", &saveptr))) {
		if (str_to_float(tok, &arg)) {
			if (arg >= 0.0)
				c->on_time = arg;
			if ((tok = strtok_r(NULL, ",", &saveptr))) {
				if (str_to_float(tok, &arg)) {
					if (arg >= 0.0)
						c->off_time = arg;
				}
			}
		}
	}

	c->on_l = c->on_time * 1000000;
	c->off_l = c->off_time * 1000000;
	c->last_state = 0;
	c->mode = 0;
	c->start_t = 0;

	return c;
}

char* effect_blink_print_args(void *ctx)
{
	blink_context_t *c = (blink_context_t*)ctx;
	char buf[64];

	snprintf(buf, sizeof(buf), "%f,%f", c->on_time, c->off_time);

	return strdup(buf);
}

uint8_t effect_blink(void *ctx, uint64_t t_now, uint8_t pwm, uint8_t pwr)
{
	blink_context_t *c = (blink_context_t*)ctx;
	int64_t t_d;
	uint8_t ret = 0;

	if (c->last_state != pwr) {
		c->start_t = t_now;
		if (pwr) {
			c->mode = 1;
			ret = pwm;
		} else {
			c->mode = 0;
			ret = 0;
		}
	}
	else {
		ret = 0;
		if (pwr) {
			t_d = t_now - c->start_t;

			if (c->mode == 1) {
				if (t_d < c->on_l) {
					ret = pwm;
				} else {
					c->start_t = t_now;
					c->mode = 0;
					ret = 0;
				}
			}
			else {
				if (t_d < c->off_l) {
					ret = 0;
				} else {
					c->start_t = t_now;
					c->mode = 1;
					ret = pwm;
				}
			}
		}
	}

	c->last_state = pwr;

	return ret;
}


/* eof :-) */
