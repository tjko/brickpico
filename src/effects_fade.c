/* effects_fade.c
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
#include <math.h>
#include <assert.h>
#include "pico/stdlib.h"

#include "brickpico.h"
#include "effects.h"


typedef struct fade_context {
	float fade_in;
	float fade_out;
	int64_t in_l;
	int64_t out_l;
	uint8_t last_state;
	uint8_t mode;
	absolute_time_t start_t;
} fade_context_t;


void* effect_fade_parse_args(const char *args)
{
	fade_context_t *c;
	char *tok, *saveptr, s[64];
	float fade_in, fade_out;

	if (!args)
		return NULL;

	strncopy(s, args, sizeof(s));

	/* fade in parameter (seconds) */
	if (!(tok = strtok_r(s, ",", &saveptr)))
		return NULL;
	if (!str_to_float(tok, &fade_in))
		return NULL;
	if (fade_in < 0.0)
		return NULL;

	/* fade out parameter (seconds) */
	if (!(tok = strtok_r(NULL, ",", &saveptr)))
		return NULL;
	if (!str_to_float(tok, &fade_out))
		return NULL;
	if (fade_out < 0.0)
		return NULL;


	if (!(c = malloc(sizeof(fade_context_t))))
		return NULL;

	c->fade_in = fade_in;
	c->fade_out = fade_out;
	c->in_l = fade_in * 1000000;
	c->out_l = fade_out * 1000000;
	c->last_state = 0;
	c->mode = 0;
	c->start_t = 0;

	return c;
}

char* effect_fade_print_args(void *ctx)
{
	fade_context_t *c = (fade_context_t*)ctx;
	char buf[128];

	snprintf(buf, sizeof(buf), "%f,%f", c->fade_in, c->fade_out);

	return strdup(buf);
}

uint8_t effect_fade(void *ctx, uint8_t pwm, uint8_t pwr)
{
	fade_context_t *c = (fade_context_t*)ctx;
	absolute_time_t t_now = get_absolute_time();
	int64_t t_d;
	uint8_t ret = 0;

	if (c->last_state != pwr) {
		c->start_t = t_now;
		if (pwr) {
			c->mode = 1;
			ret = 0;
		} else {
			c->mode = 0;
			ret = pwm;
		}
	}
	else {
		t_d = absolute_time_diff_us(c->start_t, t_now);

		if (c->mode == 1) {
			if (t_d < c->in_l) {
				ret = pwm * t_d / c->in_l;
			} else {
				ret = pwm;
			}
		}
		else {
			if (t_d < c->out_l) {
				ret = pwm - (pwm * t_d / c->out_l);
			} else {
				ret = 0;
			}
		}
	}

	c->last_state = pwr;

	return ret;
}


/* eof :-) */
