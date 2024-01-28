/* ringbuffer.h
   Copyright (C) 2024 Timo Kokkonen <tjko@iki.fi>

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

#ifndef BRICKPICO_RINGBUFFER_H
#define BRICKPICO_RINGBUFFER_H 1

#include <stdint.h>
#include <stdbool.h>

typedef struct u8_ringbuffer {
	uint8_t *buf;
	bool free_buf;
	size_t size;
	size_t free;
	size_t head;
	size_t tail;
	size_t items;
} u8_ringbuffer_t;


typedef struct simple_ringbuffer {
	uint8_t *buf;
	bool free_buf;
	size_t size;
	size_t free;
	size_t head;
	size_t tail;
} simple_ringbuffer_t;


int simple_ringbuffer_init(simple_ringbuffer_t *rb, uint8_t *buf, size_t size);
int simple_ringbuffer_free(simple_ringbuffer_t *rb);
int simple_ringbuffer_flush(simple_ringbuffer_t *rb);
size_t simple_ringbuffer_size(simple_ringbuffer_t *rb);
int simple_ringbuffer_add_char(simple_ringbuffer_t *rb, uint8_t ch, bool overwrite);
int simple_ringbuffer_add(simple_ringbuffer_t *rb, uint8_t *data, size_t len, bool overwrite);
int simple_ringbuffer_read_char(simple_ringbuffer_t *rb);
int simple_ringbuffer_read(simple_ringbuffer_t *rb, uint8_t *ptr, size_t size);
size_t simple_ringbuffer_peek(simple_ringbuffer_t *rb, uint8_t **ptr, size_t size);
int simple_ringbuffer_peek_char(simple_ringbuffer_t *rb, size_t offset);


int u8_ringbuffer_init(u8_ringbuffer_t *rb, uint8_t *buf, size_t size);
int u8_ringbuffer_free(u8_ringbuffer_t *rb);
int u8_ringbuffer_remove_first_item(u8_ringbuffer_t *rb);
int u8_ringbuffer_remove_last_item(u8_ringbuffer_t *rb);
int u8_ringbuffer_add(u8_ringbuffer_t *rb, uint8_t *data, uint8_t len, bool overwrite);
int u8_ringbuffer_peek(u8_ringbuffer_t *rb, size_t offset, uint8_t *ptr, size_t size, int *next, int *prev);
int u8_ringbuffer_remove_first(u8_ringbuffer_t *rb, uint8_t *ptr, size_t size);
int u8_ringbuffer_remove_last(u8_ringbuffer_t *rb, uint8_t *ptr, size_t size);


#endif /* BRICKPICO_RINGBUFFER_H */
