/* effects.h
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

#ifndef BRICKPICO_EFFECTS_H
#define BRICKPICO_EFFECTS_H 1

enum light_effect_types {
	EFFECT_NONE          = 0, /* No effects */
	EFFECT_FADE          = 1, /* Fade in/out at defined rates */
	EFFECT_BLINK         = 2, /* Blink at defined rate */
	EFFECT_PULSE         = 3, /* Pulse at defined rate */
};
#define EFFECT_ENUM_MAX 3


typedef void* (effect_parse_args_func_t)(const char *args);
typedef char* (effect_print_args_func_t)(void *ctx);
typedef uint8_t (effect_func_t)(void *ctx, uint64_t t_now, uint8_t pwm, uint8_t pwr);

typedef struct effect_entry {
	const char* name;
	effect_parse_args_func_t *parse_args_func;
	effect_print_args_func_t *print_args_func;
	effect_func_t *effect_func;
} effect_entry_t;





#endif /* BRICKPICO_EFFECTS_H */
