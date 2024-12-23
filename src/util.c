/* util.c
   Copyright (C) 2023 Timo Kokkonen <tjko@iki.fi>

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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <wctype.h>
#include <assert.h>
#include <malloc.h>
#include <time.h>
//#include "pico/stdlib.h"
#include "pico/aon_timer.h"
#include "b64/cencode.h"
#include "b64/cdecode.h"

#include "brickpico.h"


void print_mallinfo()
{
	struct mallinfo mi = mallinfo();

	printf("Total non-mmapped bytes (arena):       %d\n", mi.arena);
	printf("# of free chunks (ordblks):            %d\n", mi.ordblks);
	printf("# of free fastbin blocks (smblks):     %d\n", mi.smblks);
	printf("# of mapped regions (hblks):           %d\n", mi.hblks);
	printf("Bytes in mapped regions (hblkhd):      %d\n", mi.hblkhd);
	printf("Max. total allocated space (usmblks):  %d\n", mi.usmblks);
	printf("Free bytes held in fastbins (fsmblks): %d\n", mi.fsmblks);
	printf("Total allocated space (uordblks):      %d\n", mi.uordblks);
	printf("Total free space (fordblks):           %d\n", mi.fordblks);
	printf("Topmost releasable block (keepcost):   %d\n", mi.keepcost);
}

char *trim_str(char *s)
{
	char *e;

	if (!s) return NULL;

	while (iswspace(*s))
		s++;
	e = s + strlen(s) - 1;
	while (e >= s && iswspace(*e))
		*e-- = 0;

	return s;
}


int str_to_int(const char *str, int *val, int base)
{
	char *endptr;

	if (!str || !val)
		return 0;

	*val = strtol(str, &endptr, base);

	return (str == endptr ? 0 : 1);
}


int str_to_float(const char *str, float *val)
{
	char *endptr;

	if (!str || !val)
		return 0;

	*val = strtof(str, &endptr);

	return (str == endptr ? 0 : 1);
}


time_t timespec_to_time_t(const struct timespec *ts)
{
	if (!ts)
		return 0;
	return ts->tv_sec;
}


struct timespec* time_t_to_timespec(time_t t, struct timespec *ts)
{
	if (!ts)
		return NULL;

	ts->tv_sec = t;
	ts->tv_nsec = 0;

	return ts;
}


char* time_t_to_str(char *buf, size_t size, const time_t t)
{
	struct tm tm;

	if (!buf)
		return NULL;

	localtime_r(&t, &tm);
	snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return buf;
}


bool str_to_time_t(const char *str, time_t *t)
{
	struct tm tm;

	if (!str || !t)
		return false;

	if (!strptime(str, "%Y-%m-%d %H:%M:%S", &tm))
		return false;

	*t = mktime(&tm);
	return true;
}


bool rtc_get_tm(struct tm *tm)
{
	struct timespec ts;
	time_t t;

	if (!tm || !aon_timer_is_running())
		return false;

	aon_timer_get_time(&ts);
	t = timespec_to_time_t(&ts);
	localtime_r(&t, tm);

	return true;
}


bool rtc_get_time(time_t *t)
{
	struct timespec ts;

	if (!t || !aon_timer_is_running())
		return false;

	aon_timer_get_time(&ts);
	*t = timespec_to_time_t(&ts);

	return true;
}


const char *mac_address_str_r(const uint8_t *mac, char *buf, size_t buf_len)
{
	if (!buf || buf_len < 1)
		return NULL;

	snprintf(buf, buf_len, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return buf;
}

const char *mac_address_str(const uint8_t *mac)
{
	static char buf[18];

	return mac_address_str_r(mac, buf, sizeof(buf));
}

int valid_wifi_country(const char *country)
{
	if (!country)
		return 0;

	if (strlen(country) < 2)
		return 0;

	if (!(country[0] >= 'A' && country[0] <= 'Z'))
		return 0;
	if (!(country[1] >= 'A' && country[1] <= 'Z'))
		return 0;

	if (strlen(country) == 2)
		return 1;

	if (country[2] >= '1' && country[2] <= '9')
		return 1;

	return 0;
}

int valid_hostname(const char *name)
{
	if (!name)
		return 0;

	for (int i = 0; i < strlen(name); i++) {
		if (!(isalnum((int)name[i]) || name[i] == '-')) {
			return 0;
		}
	}

	return 1;
}

int check_for_change(double oldval, double newval, double threshold)
{
	double delta = fabs(oldval - newval);

	if (delta >= threshold)
		return 1;

	return 0;
}


#define SQRT_INT64_MAX 3037000499  // sqrt(INT64_MAX)

int64_t pow_i64(int64_t x, uint8_t y)
{
	int64_t res;

	/* Handle edge cases. */
	if (x == 1 || y == 0)
		return 1;
	if (x == 0)
		return 0;

	/* Calculate x^y by squaring method. */
	res = (y & 1 ? x : 1);
	y >>= 1;
	while (y) {
		/* Check for overflow. */
		if (x > SQRT_INT64_MAX)
			return 0;
		x *= x;
		if (y & 1)
			res *= x; /* This may still overflow... */
		y >>= 1;
	}

	return res;
}

double round_decimal(double val, unsigned int decimal)
{
	double f = pow_i64(10, decimal);
	return round(val * f) / f;
}

char* base64encode(const char *input)
{
	base64_encodestate ctx;
	size_t buf_len, input_len, count;
	char *buf, *c;

	if (!input)
		return NULL;

	base64_init_encodestate(&ctx);

	input_len = strlen(input);
	buf_len = base64_encode_length(input_len, &ctx);
	if (!(buf = malloc(buf_len + 1)))
		return NULL;

	c = buf;
	count = base64_encode_block(input, input_len, c, &ctx);
	c += count;
	count = base64_encode_blockend(c, &ctx);
	c += count;
	*c =  0;

	return buf;
}


char* base64decode(const char *input)
{
	base64_decodestate ctx;
	size_t buf_len, input_len, count;
	char *buf, *c;

	if (!input)
		return NULL;

	base64_init_decodestate(&ctx);

	input_len = strlen(input);
	buf_len = base64_decode_maxlength(input_len);
	if (!(buf = malloc(buf_len + 1)))
		return NULL;

	c = buf;
	count = base64_decode_block(input, input_len, c, &ctx);
	c += count;
	*c = 0;

	return buf;
}


char *strncopy(char *dst, const char *src, size_t size)
{
	if (!dst || !src || size < 1)
		return dst;

	if (size > 1)
		strncpy(dst, src, size - 1);
	dst[size - 1] = 0;

	return dst;
}


char *strncatenate(char *dst, const char *src, size_t size)
{
	int used, free;

	if (!dst || !src || size < 1)
		return dst;

	/* Check if dst string is already "full" ... */
	used = strnlen(dst, size);
	if ((free = size - used) <= 1)
		return dst;

	return strncat(dst + used, src, free - 1);
}


int clamp_int(int val, int min, int max)
{
	int res = val;

	if (res < min)
		res = min;
	if (res > max)
		res = max;

	return res;
}


void* memmem(const void *haystack, size_t haystacklen,
	const void *needle, size_t needlelen)
{
	const uint8_t *h = (const uint8_t *) haystack;
	const uint8_t *n = (const uint8_t *) needle;

	if (haystacklen == 0 || needlelen == 0 || haystacklen < needlelen)
		return NULL;

	if (needlelen == 1)
		return memchr(haystack, (int)n[0], haystacklen);

	const uint8_t *search_end = h + haystacklen - needlelen;

	for (const uint8_t *p = h; p <= search_end; p++) {
		if (*p == *n) {
			if (!memcmp(p, n, needlelen)) {
				return (void *)p;
			}
		}
	}

	return NULL;
}

char *bitmask_to_str_r(uint32_t mask, uint8_t len, uint8_t base, bool range, char *buf, size_t buf_len)
{
	int state = 0;
	int prev = -2;
	int last = 0;
	char *s = buf;
	size_t buf_free;
	int i;

	if (!buf || buf_len < 1)
		return NULL;

	*s = 0;
	buf_free = buf_len;

	if (len < 1 || len > 32 || buf_len < 2)
		return buf;

	/* Handle special case of all bits set... */
	if (!range && mask == (1 << len) - 1) {
		*s++ = '*';
		*s = 0;
		return buf;
	}

	for (i = 0; i < len; i++) {
		if (mask & (1 << i)) {
			int consecutive = (i - prev == 1 ? 1 : 0);
			int w = 0;

			if (state == 0) {
				w = snprintf(s, buf_free, "%d", i + base);
				state = 1;
			}
			else if (state == 1) {
				if (range && consecutive) {
					last = i;
					state = 2;
				} else {
					w = snprintf(s, buf_free, ",%d", i + base);
				}
			}
			else if (state == 2) {
				if (consecutive) {
					last = i;
				} else {
					w = snprintf(s, buf_free, "-%d,%d", last + base, i + base);
					state = 1;
				}
			}

			if (w > 0) {
				if (w >= buf_free)
					return buf;
				s += w;
				buf_free -= w;
			}
			prev = i;
		}
	}
	if (range && state == 2) {
		snprintf(s , buf_free, "-%d", last + base);
	}

	return buf;
}


char *bitmask_to_str(uint32_t mask, uint8_t len, uint8_t base, bool range)
{
	static char buf[96];

	return bitmask_to_str_r(mask, len, base, range, buf, sizeof(buf));
}


int str_to_bitmask(const char *str, uint8_t len, uint32_t *mask, uint8_t base)
{
	char *s, *tok, *saveptr, *saveptr2;

	if (!str || !mask)
		return -1;
	*mask = 0;

	if (len < 1 || len > 32)
		return -2;

	if (!strcmp(str, "*")) {
		*mask = (1 << len) - 1;
		return 0;
	}

	if ((s = strdup(str)) == NULL)
		return -3;

	tok = strtok_r(s, ",", &saveptr);
	while (tok) {
		int a, b;
		tok = strtok_r(tok, "-", &saveptr2);
		if (str_to_int(tok, &a, 10)) {
			a -= base;
			if (a >= 0 && a < len) {
				*mask |= (1 << a);
			}
			tok = strtok_r(NULL, "-", &saveptr2);
			if (str_to_int(tok, &b, 10)) {
				b -= base;
				if (b > a && b < len) {
					while (++a <= b) {
						*mask |= (1 << a);
					}
				}
			}
		}
		tok = strtok_r(NULL, ",", &saveptr);
	}

	free(s);
	return 0;
}

/* eof */
