/* ringbuffer.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ringbuffer.h"

#define PREFIX_LEN 1
#define SUFFIX_LEN 1



int simple_ringbuffer_init(simple_ringbuffer_t *rb, uint8_t *buf, size_t size)
{
	if (!rb)
		return -1;

	if (!buf) {
		if (!(rb->buf = calloc(1, size)))
			return -2;
		rb->free_buf = true;
	} else {
		rb->buf = buf;
		rb->free_buf = false;
	}

	rb->size = size;
	rb->free = size;
	rb->head = 0;
	rb->tail = 0;

	return 0;
}


int simple_ringbuffer_free(simple_ringbuffer_t *rb)
{
	if (!rb)
		return -1;

	if (rb->free_buf && rb->buf)
		free(rb->buf);

	rb->buf = NULL;
	rb->size = 0;
	rb->free = 0;
	rb->head = 0;
	rb->tail = 0;

	return 0;
}


int simple_ringbuffer_flush(simple_ringbuffer_t *rb)
{
	if (!rb)
		return -1;

	rb->head = 0;
	rb->tail = 0;
	rb->free = rb->size;

	return 0;
}

inline size_t simple_ringbuffer_size(simple_ringbuffer_t *rb)
{
	return (rb ? rb->size - rb->free : 0);
}


static inline size_t simple_ringbuffer_offset(simple_ringbuffer_t *rb, size_t offset, size_t delta, int direction)
{
	size_t o = offset % rb->size;
	size_t d = delta % rb->size;

	if (direction >= 0) {
		o = (o + d) % rb->size;
	} else {
		if (o < d) {
			o = rb->size - (d - o);
		} else {
			o -= d;
		}
	}

	return o;
}

inline int simple_ringbuffer_add_char(simple_ringbuffer_t *rb, uint8_t ch, bool overwrite)
{
	if (!rb)
		return -1;

	if (overwrite && rb->free < 1) {
		rb->head = simple_ringbuffer_offset(rb, rb->head, 1, 1);
		rb->free += 1;
	}
	if (rb->free < 1)
		return -2;

	rb->buf[rb->tail] = ch;
	rb->tail = simple_ringbuffer_offset(rb, rb->tail, 1, 1);
	rb->free--;

	return 0;
}

int simple_ringbuffer_add(simple_ringbuffer_t *rb, uint8_t *data, size_t len, bool overwrite)
{
	if (!rb || !data)
		return -1;

	if (len == 0)
		return 0;

	if (len > rb->size)
		return -2;

	if (overwrite && rb->free < len) {
		size_t needed = len - rb->free;
		rb->head = simple_ringbuffer_offset(rb, rb->head, needed, 1);
		rb->free += needed;
	}
	if (rb->free < len)
		return -3;

	size_t new_tail = simple_ringbuffer_offset(rb, rb->tail, len, 1);

	if (new_tail > rb->tail) {
		memcpy(rb->buf + rb->tail, data, len);
	} else {
		size_t part1 = rb->size - rb->tail;
		size_t part2 = len - part1;
		memcpy(rb->buf + rb->tail, data, part1);
		memcpy(rb->buf, data + part1, part2);
	}

	rb->tail = new_tail;
	rb->free -= len;

	return 0;
}

inline int simple_ringbuffer_read_char(simple_ringbuffer_t *rb)
{
	if (!rb)
		return -1;
	if (rb->head == rb->tail)
		return -2;

	int val = rb->buf[rb->head];
	rb->head = simple_ringbuffer_offset(rb, rb->head, 1, 1);
	rb->free++;

	return val;
}

int simple_ringbuffer_read(simple_ringbuffer_t *rb, uint8_t *ptr, size_t size)
{
	if (!rb || size < 1)
		return -1;

	size_t used = rb->size - rb->free;
	if (used < size)
		return -2;

	size_t new_head = simple_ringbuffer_offset(rb, rb->head, size, 1);

	if (ptr) {
		if (new_head > rb->head) {
			memcpy(ptr, rb->buf + rb->head, size);
		} else {
			size_t part1 = rb->size - rb->head;
			size_t part2 = size - part1;
			memcpy(ptr, rb->buf + rb->head, part1);
			memcpy(ptr + part1, rb->buf, part2);
		}
	}

	rb->head = new_head;
	rb->free += size;

	return 0;
}

size_t simple_ringbuffer_peek(simple_ringbuffer_t *rb, uint8_t **ptr, size_t size)
{
	if (!rb || !ptr || size < 1)
		return 0;

	*ptr = NULL;
	size_t used = rb->size - rb->free;
	size_t toread = (size < used ? size : used);
	size_t len = rb->size - rb->head;

	if (used < 1)
		return 0;

	*ptr = rb->buf + rb->head;

	return (len < toread ? len : toread);
}

int u8_ringbuffer_init(u8_ringbuffer_t *rb, uint8_t *buf, size_t size)
{
	if (!rb)
		return -1;

	if (!buf) {
		if (!(rb->buf = malloc(size)))
			return -2;
		rb->free_buf = true;
	} else {
		rb->buf = buf;
		rb->free_buf = false;
	}

	rb->size = size;
	rb->free = size;
	rb->head = 0;
	rb->tail = 0;
	rb->items = 0;

	return 0;
}


int u8_ringbuffer_free(u8_ringbuffer_t *rb)
{
	if (!rb)
		return -1;

	if (rb->free_buf && rb->buf)
		free(rb->buf);

	rb->buf = NULL;
	rb->size = 0;
	rb->free = 0;
	rb->head = 0;
	rb->tail = 0;
	rb->items = 0;

	return 0;
}


static size_t u8_next_item_offset(u8_ringbuffer_t *rb, size_t offset)
{
	size_t o = (offset % rb->size) + PREFIX_LEN + rb->buf[offset] + SUFFIX_LEN;

	if (o >= rb->size)
		o = o % rb->size;

	return o;
}


static size_t u8_previous_item_offset(u8_ringbuffer_t *rb, size_t offset)
{
	size_t o = offset % rb->size;

	if (o < SUFFIX_LEN)
		o = rb->size - (SUFFIX_LEN - o);
	else
		o -= SUFFIX_LEN;

	uint8_t prev = rb->buf[o] + PREFIX_LEN;
	if (o < prev)
		o = rb->size - (prev - o);
	else
		o -= prev;

	return o;
}


int u8_ringbuffer_remove_first_item(u8_ringbuffer_t *rb)
{
	if (!rb)
		return -1;
	if (rb->items < 1)
		return -2;
	if (rb->head >= rb->size)
		return -3;

	size_t item_len = PREFIX_LEN + rb->buf[rb->head] + SUFFIX_LEN;
	rb->head = u8_next_item_offset(rb, rb->head);
	rb->free += item_len;
	rb->items--;

	return 0;
}


int u8_ringbuffer_remove_last_item(u8_ringbuffer_t *rb)
{
	if (!rb)
		return -1;
	if (rb->items < 1)
		return -2;
	if (rb->tail >= rb->size)
		return -3;

	size_t item_len = PREFIX_LEN + rb->buf[rb->tail] + SUFFIX_LEN;
	rb->tail = u8_previous_item_offset(rb, rb->tail);
	rb->free += item_len;
	rb->items--;

	return 0;
}


int u8_ringbuffer_add(u8_ringbuffer_t *rb, uint8_t *data, uint8_t len, bool overwrite)
{
	size_t item_len = len + PREFIX_LEN + SUFFIX_LEN;

	if (!rb || !data || len < 1)
		return -1;
	if (rb->tail >= rb->size || rb->free > rb->size)
		return -2;

	if (overwrite && rb->free < item_len) {
		while (!u8_ringbuffer_remove_first_item(rb)) {
			if (rb->free >= item_len)
				break;
		}
	}
	if (rb->free < item_len)
		return -2;

	uint8_t *src = data;
	if (rb->items > 0)
		rb->tail = u8_next_item_offset(rb, rb->tail);
	uint8_t *dst = rb->buf + rb->tail;
	*dst++ = len;
	int i = 0;
	while (i++ < len) {
		if (dst >= rb->buf + rb->size)
			dst = rb->buf;
		*dst++ = *src++;
	}
	*dst++ = len;

	rb->free -= len + PREFIX_LEN + SUFFIX_LEN;
	rb->items++;

	return 0;
}


int u8_ringbuffer_peek(u8_ringbuffer_t *rb, size_t offset, uint8_t *ptr, size_t size, int *next, int *prev)
{
	if (!rb || !ptr)
		return -1;

	if (prev)
		*prev = -1;
	if (next)
		*next = -1;

	if (offset >= rb->size)
		return -2;

	if (rb->items < 1)
		return -3;

	uint8_t len = rb->buf[offset];
	if (len > size)
		return -4;

	uint8_t *src = rb->buf + offset + PREFIX_LEN;
	uint8_t *dst = ptr;
	int i = 0;
	while (i++ < len) {
		if (src >= rb->buf + rb->size)
			src = rb->buf;
		*dst++ = *src++;
	}

	if (next && offset != rb->tail)
		*next = u8_next_item_offset(rb, offset);
	if (prev && offset != rb->head)
		*prev = u8_previous_item_offset(rb, offset);

	return len;
}


int u8_ringbuffer_remove_first(u8_ringbuffer_t *rb, uint8_t *ptr, size_t size)
{
	int len = u8_ringbuffer_peek(rb, rb->head, ptr, size, NULL, NULL);
	if (len > 0) {
		u8_ringbuffer_remove_first_item(rb);
	}
	return len;
}


int u8_ringbuffer_remove_last(u8_ringbuffer_t *rb, uint8_t *ptr, size_t size)
{
	int len = u8_ringbuffer_peek(rb, rb->tail, ptr, size, NULL, NULL);
	if (len > 0) {
		u8_ringbuffer_remove_last_item(rb);
	}
	return len;
}


