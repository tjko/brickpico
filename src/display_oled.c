/* display_oled.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "ss_oled.h"
#include "brickpico.h"

#ifdef OLED_DISPLAY


static SSOLED oled;
static uint8_t ucBuffer[(128*128)/8];
static uint8_t oled_width = 128;
static uint8_t oled_height = 64;
static uint8_t oled_found = 0;
static uint8_t r_lines = 8;


void oled_display_init()
{
	int res;
	int retries = 0;
	int dtype = OLED_128x64;
	int invert = 0;
	int flip = 0;
	uint8_t brightness = 50; /* display brightness: 0..100 */
	uint8_t disp_brightness = 0;
	int val;
	char *tok, *args, *saveptr;
	const char *oled_ctrl;

	/* Check if display settings are configured using SYS:DISP command... */
	if (cfg) {
		args = strdup(cfg->display_type);
		if (args) {
			tok = strtok_r(args, ",", &saveptr);
			while (tok) {
				if (!strncmp(tok, "132x64", 6))
					dtype = OLED_132x64;
				if (!strncmp(tok, "128x128", 7)) {
					dtype = OLED_128x128;
					oled_height = 128;
					r_lines = 10;
				}
				else if (!strncmp(tok, "invert", 6))
					invert = 1;
				else if (!strncmp(tok, "flip", 4))
					flip =1;
				else if (!strncmp(tok, "brightness=", 11)) {
					if (str_to_int(tok + 11, &val, 10)) {
						if (val >=0 && val <= 100)
							brightness = val;
					}
				}

				tok = strtok_r(NULL, ",", &saveptr);
			}
			free(args);
		}
	}


	disp_brightness = (brightness / 100.0) * 255;
	log_msg(LOG_DEBUG, "Set display brightness: %u%% (0x%x)\n",
		brightness, disp_brightness);

	log_msg(LOG_NOTICE, "Initializing OLED Display...");
	do {
		sleep_ms(50);
		res = oledInit(&oled, dtype, -1, flip, invert, I2C_HW,
			SDA_PIN, SCL_PIN, -1, 1000000L, false);
	} while (res == OLED_NOT_FOUND && retries++ < 10);

	if (res == OLED_NOT_FOUND) {
		log_msg(LOG_ERR, "No OLED Display Connected!");
		return;
	}

	switch (res) {
	case OLED_SSD1306_3C:
		oled_ctrl = "SSD1306 (at 0x3c)";
		break;
	case OLED_SSD1306_3D:
		oled_ctrl = "SSD1306 (at 0x3d)";
		break;
	case OLED_SH1106_3C:
		oled_ctrl = "SH1106 (at 0x3c)";
		break;
	case OLED_SH1106_3D:
		oled_ctrl = "SH1106 (at 0x3d)";
		break;
	case OLED_SH1107_3C:
		oled_ctrl = "SH1107 (at 0x3c)";
		break;
	case OLED_SH1107_3D:
		oled_ctrl = "SH1107 (at 0x3d)";
		break;
	default:
		oled_ctrl = "Unknown";
	}
	log_msg(LOG_NOTICE, "%ux%u OLED display: %s", oled_width, oled_height, oled_ctrl);

	/* Initialize screen. */
	oledSetBackBuffer(&oled, ucBuffer);
	oledFill(&oled, 0, 1);
	oledSetContrast(&oled, disp_brightness);

	/* Display model and firmware version. */
	int y_o = (oled_height > 64 ? 4 : 0);
	oledWriteString(&oled, 0, 15, y_o + 1, "BrickPico-" BRICKPICO_MODEL, FONT_8x8, 0, 1);
	oledWriteString(&oled, 0, 40, y_o + 3, "v" BRICKPICO_VERSION, FONT_8x8, 0, 1);
	oledWriteString(&oled, 0, 20, y_o + 6, "Initializing...", FONT_6x8, 0, 1);

	oled_found = 1;
}

void oled_clear_display()
{
	if (!oled_found)
		return;

	oledFill(&oled, 0, 1);
}

void oled_display_status(const struct brickpico_state *state,
	const struct brickpico_config *conf)
{
	char buf[64];
	int i;
	struct tm t;
	static int bg_drawn = 0;


	if (!oled_found || !state)
		return;

	int out_row_offset = (oled_height > 64 ? 1 : 0);

	if (!bg_drawn) {
		/* Draw "background" only once... */
		oled_clear_display();

		if (oled_height > 64) {
			oledWriteString(&oled, 0,  0, 0, "Outputs", FONT_6x8, 0, 1);
			oledDrawLine(&oled, 0, 64, oled_width - 1, 64, 1);
		}

		bg_drawn = 1;
	}

	/* Output port states (PWM) */
	for (i = 0; i < OUTPUT_COUNT; i++) {
		uint pwm = state->pwm[i];
		uint pwr = state->pwr[i];
		int row = i / 3 + out_row_offset;
		int col = i % 3;
		if (pwr) {
			snprintf(buf, sizeof(buf), "%2d:%-3u", i + 1, pwm);
		} else {
			snprintf(buf, sizeof(buf), "%2d:---", i + 1);
		}
		oledWriteString(&oled, 0 , col * 7 * 6 + col * 4 , row, buf, FONT_6x8, 0, 1);
	}


	/* IP */
	const char *ip = network_ip();
	if (ip) {
		int row = (oled_height > 64 ? 15 : 7);

		/* Center align the IP string */
		int delta = (15 + 1) - strlen(ip);
		if (delta < 0)
			delta = 0;
		int offset = delta / 2;
		memset(buf, ' ', 8);
		snprintf(buf + offset, sizeof(buf), " %s", ip);
		oledWriteString(&oled, 0, 10 + (delta % 2 ? 3 : 0), row, buf, FONT_6x8, 0, 1);
	}

	/* Uptime & NTP time */
	uint32_t secs = to_us_since_boot(get_absolute_time()) / 1000000;
	uint32_t mins =  secs / 60;
	uint32_t hours = mins / 60;
	uint32_t days = hours / 24;
	snprintf(buf, sizeof(buf), "%03lu+%02lu:%02lu:%02lu",
		days,
		hours % 24,
		mins % 60,
		secs % 60);
	if (rtc_get_tm(&t)) {
		if (oled_height > 64)
			oledWriteString(&oled, 0, 28, 14, buf, FONT_6x8, 0, 1);
		if (oled_height > 64) {
			snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
			oledWriteString(&oled, 0, 16, 11, buf, FONT_12x16, 0, 1);
		}
		else {
			snprintf(buf, sizeof(buf), " %02d:%02d:%02d ", t.tm_hour, t.tm_min, t.tm_sec);
			oledWriteString(&oled, 0, 24, 6, buf, FONT_8x8, 0, 1);
		}
	} else {
		int row = (oled_height > 64 ? 12 : 6);
		oledWriteString(&oled, 0, 28, row, buf, FONT_6x8, 0, 1);
	}

}

void oled_display_message(int rows, const char **text_lines)
{
	int screen_rows = oled_height / 8;
	int start_row = 0;

	if (!oled_found)
		return;

	oled_clear_display();

	if (rows < screen_rows) {
		start_row = (screen_rows - rows) / 2;
	}

	for(int i = 0; i < rows; i++) {
		int row = start_row + i;
		char *text = (char*)text_lines[i];

		if (row >= screen_rows)
			break;
		oledWriteString(&oled, 0, 0, row, (text ? text : ""), FONT_6x8, 0, 1);
	}
}

#endif

/* eof :-) */
