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

typedef void* (effect_parse_args_func_t)(const char *args);
typedef char* (effect_print_args_func_t)(void *ctx);
typedef uint8_t (effect_func_t)(void *ctx, uint8_t pwm, uint8_t pwr);

typedef struct effect_entry {
	const char* name;
	effect_parse_args_func_t *parse_args_func;
	effect_print_args_func_t *print_args_func;
	effect_func_t *effect_func;
} effect_entry_t;

/* effects_fade.c */
void* effect_fade_parse_args(const char *args);
char* effect_fade_print_args(void *ctx);
uint8_t effect_fade(void *ctx, uint8_t pwm, uint8_t pwr);

/* effects_blink.c */
void* effect_blink_parse_args(const char *args);
char* effect_blink_print_args(void *ctx);
uint8_t effect_blink(void *ctx, uint8_t pwm, uint8_t pwr);



#endif /* BRICKPICO_EFFECTS_H */
