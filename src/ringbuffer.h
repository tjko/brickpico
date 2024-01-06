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

typedef struct ringbuffer {
	uint8_t *buf;
	bool free_buf;
	size_t size;
	size_t free;
	size_t head;
	size_t tail;
	size_t items;
} ringbuffer_t;



int ringbuffer_init(ringbuffer_t *rb, uint8_t *buf, size_t size);
int ringbuffer_free(ringbuffer_t *rb);
int ringbuffer_remove_first_item(ringbuffer_t *rb);
int ringbuffer_remove_last_item(ringbuffer_t *rb);
int ringbuffer_add(ringbuffer_t *rb, uint8_t *data, uint8_t len);
int ringbuffer_peek(ringbuffer_t *rb, size_t offset, uint8_t *ptr, size_t size, int *next, int *prev);
int ringbuffer_remove(ringbuffer_t *rb, uint8_t *ptr, size_t size);


#endif /* BRICKPICO_RINGBUFFER_H */
