/* effects.c
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


static const effect_entry_t effects[] = {
	{ "none", NULL, NULL, NULL }, /* EFFECT_NONE */
	{ "fade", effect_fade_parse_args, effect_fade_print_args, effect_fade }, /* EFFECT_FADE */
	{ "blink", effect_blink_parse_args, effect_blink_print_args, effect_blink }, /* EFFECT_BLINK */
	{ "pulse", effect_fade_parse_args, effect_fade_print_args, effect_fade }, /* EFFECT_PULSE */
	{ NULL, NULL, NULL, NULL }
};


int str2effect(const char *s)
{
	int ret = EFFECT_NONE;

	for(int i = 0; effects[i].name; i++) {
		if (!strcasecmp(s, effects[i].name)) {
			ret = i;
			break;
		}
	}

	return ret;
}


const char* effect2str(enum light_effect_types effect)
{
	if (effect <= EFFECT_ENUM_MAX) {
		return effects[effect].name;
	}

	return "none";
}


void* effect_parse_args(enum light_effect_types effect, const char *args)
{
	void *ret = NULL;

	if (effect <= EFFECT_ENUM_MAX) {
		if (effects[effect].parse_args_func)
			ret = effects[effect].parse_args_func(args);
	}

	return ret;
}


char* effect_print_args(enum light_effect_types effect, void *ctx)
{
	char *ret = NULL;

	if (effect <= EFFECT_ENUM_MAX && ctx) {
		if (effects[effect].print_args_func)
			ret = effects[effect].print_args_func(ctx);
	}

	return ret;
}


uint8_t light_effect(enum light_effect_types effect, void *ctx, uint8_t pwm, uint8_t pwr)
{
	uint8_t ret = 0;

	if (effect <= EFFECT_ENUM_MAX) {
		if (effects[effect].effect_func)
			ret = effects[effect].effect_func(ctx, pwm, pwr);
		else
			ret = (pwr ? pwm : 0);
	}

	return ret;
}


/* eof :-) */
