/* effects_pulse.c
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


#define ARG_COUNT 4

typedef struct pulse_context {
	float args[ARG_COUNT];
	int64_t end[ARG_COUNT];
} pulse_context_t;


void* effect_pulse_parse_args(const char *args)
{
	pulse_context_t *c;
	char *tok, *saveptr, *s;

	if (!args)
		return NULL;
	if (!(c = calloc(1, sizeof(pulse_context_t))))
		return NULL;


	/* Defaults (in seconds) */
	c->args[0] = 2.0; /* Fade In */
	c->args[1] = 0.5; /* ON Time */
	c->args[2] = 2.0; /* Fade Out */
	c->args[3] = 0.5; /* OFF Time */


	/* Parse arguments */
	if ((s = strdup(args))) {
		for(int i = 0; i < ARG_COUNT; i ++) {
			float arg;

			if (!(tok = strtok_r((i == 0 ? s : NULL), ",", &saveptr)))
				break;
			if (str_to_float(tok, &arg)) {
				if (arg >= 0.0) {
					c->args[i] = arg;
				}
			}
		}
		free(s);
	}

	for(int i = 0; i < ARG_COUNT; i++) {
		c->end[i] = c->args[i] * 1000000;
		if (i > 0)
			c->end[i] += c->end[i - 1];
		printf("pulse_arg[%d]: %f, %lld\n", i, c->args[i], c->end[i]);
	}

	return c;
}


char* effect_pulse_print_args(void *ctx)
{
	pulse_context_t *c = (pulse_context_t*)ctx;
	char buf[128];

	snprintf(buf, sizeof(buf), "%f,%f,%f,%f", c->args[0], c->args[1], c->args[2], c->args[3]);

	return strdup(buf);
}


uint8_t effect_pulse(void *ctx, uint64_t t_now, uint8_t pwm, uint8_t pwr)
{
	pulse_context_t *c = (pulse_context_t*)ctx;
	int64_t t;
	uint8_t ret = 0;

	if (pwr) {
		t = t_now % c->end[3];

		if (t < c->end[0]) { /* Fade In */
			ret = pwm * t / c->end[0];
		}
		else if (t < c->end[1]) { /* ON */
			ret = pwm;
		}
		else if (t < c->end[2]) { /* Fade Out */
			ret = pwm - (pwm * (t - c->end[1]) / (c->end[2] - c->end[1]));
		}
		else { /* OFF */
			ret = 0;
		}
	}

	return ret;
}


/* eof :-) */
