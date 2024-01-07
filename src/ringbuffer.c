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



int ringbuffer_init(ringbuffer_t *rb, uint8_t *buf, size_t size)
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


int ringbuffer_free(ringbuffer_t *rb)
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


size_t next_item_offset(ringbuffer_t *rb, size_t offset)
{
	size_t o = (offset % rb->size) + PREFIX_LEN + rb->buf[offset] + SUFFIX_LEN;

	if (o >= rb->size)
		o = o % rb->size;

	return o;
}


size_t previous_item_offset(ringbuffer_t *rb, size_t offset)
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


int ringbuffer_remove_first_item(ringbuffer_t *rb)
{
	if (!rb)
		return -1;
	if (rb->items < 1)
		return -2;
	if (rb->head >= rb->size)
		return -3;

	size_t item_len = PREFIX_LEN + rb->buf[rb->head] + SUFFIX_LEN;
	rb->head = next_item_offset(rb, rb->head);
	rb->free += item_len;
	rb->items--;

	return 0;
}


int ringbuffer_remove_last_item(ringbuffer_t *rb)
{
	if (!rb)
		return -1;
	if (rb->items < 1)
		return -2;
	if (rb->tail >= rb->size)
		return -3;

	size_t item_len = PREFIX_LEN + rb->buf[rb->tail] + SUFFIX_LEN;
	rb->tail = previous_item_offset(rb, rb->tail);
	rb->free += item_len;
	rb->items--;

	return 0;
}


int ringbuffer_add(ringbuffer_t *rb, uint8_t *data, uint8_t len)
{
	size_t item_len = len + PREFIX_LEN + SUFFIX_LEN;

	if (!rb || !data || len < 1)
		return -1;
	if (rb->tail >= rb->size || rb->free > rb->size)
		return -2;

	if (rb->free < item_len) {
		while (!ringbuffer_remove_first_item(rb)) {
			if (rb->free >= item_len)
				break;
		}
		if (rb->free < item_len)
			return -2;
	}

	uint8_t *src = data;
	if (rb->items > 0)
		rb->tail = next_item_offset(rb, rb->tail);
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


int ringbuffer_peek(ringbuffer_t *rb, size_t offset, uint8_t *ptr, size_t size, int *next, int *prev)
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
		*next = next_item_offset(rb, offset);
	if (prev && offset != rb->head)
		*prev = previous_item_offset(rb, offset);

	return len;
}


int ringbuffer_remove(ringbuffer_t *rb, uint8_t *ptr, size_t size)
{
	int len = ringbuffer_peek(rb, rb->head, ptr, size, NULL, NULL);
	if (len > 0) {
		ringbuffer_remove_first_item(rb);
	}
	return len;
}


