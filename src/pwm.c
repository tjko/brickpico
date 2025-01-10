/* pwm.c
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
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "lightness.h"
#include "brickpico.h"



/* Map of PWM signal output pins.
   Every two pins must be the A and B pins of same PWM slice.
 */
uint8_t output_gpio_pwm_map[OUTPUT_MAX_COUNT] = {
	PWM1_PIN,
	PWM2_PIN,
	PWM3_PIN,
	PWM4_PIN,
	PWM5_PIN,
	PWM6_PIN,
	PWM7_PIN,
	PWM8_PIN,
	PWM9_PIN,
	PWM10_PIN,
	PWM11_PIN,
	PWM12_PIN,
	PWM13_PIN,
	PWM14_PIN,
	PWM15_PIN,
	PWM16_PIN,
};

#define PWM_TOP_MAX (1<<16)
#define LIGHTNESS_MAX 100

static uint16_t pwm_out_top = 0;
static uint16_t pwm_lightness_map[LIGHTNESS_MAX + 1];


/**
 * Set PMW output signal duty cycle.
 *
 * @param out Output port.
 * @param duty Duty cycle (0..100).
 */
void set_pwm_duty_cycle(uint out, float duty)
{
	uint level, pin;

	assert(out < OUTPUT_COUNT);
	pin = output_gpio_pwm_map[out];
	if (duty >= 100.0) {
		level = pwm_out_top + 1;
	} else if (duty > 0.0) {
		level = (duty * (pwm_out_top + 1) / 100);
	} else {
		level = 0;
	}
	pwm_set_gpio_level(pin, level);
}


/**
 * Set PWM output signal to approximate desired lightness level.
 *
 * @param out Output port.
 * @param lightness value (0..100).
 */
void set_pwm_lightness(uint out, uint lightness)
{
	uint pin;
	uint16_t level;

	assert(out < OUTPUT_COUNT);
	pin = output_gpio_pwm_map[out];
	level = pwm_lightness_map[lightness > LIGHTNESS_MAX ? LIGHTNESS_MAX : lightness];

	pwm_set_gpio_level(pin, level);
}


/**
 * Precalculate PWM level values for each lightness value.
 *
 * @param pwm_wrap PWM Counter wrap value.
 */
static void calculate_pwm_lightness(uint16_t pwm_wrap, double gamma)
{
	int i;
	double l;

	for (i = 0; i <= LIGHTNESS_MAX; i++) {
		if (gamma >= 1.0)
			l = gamma_lightness_inverse(gamma, i, LIGHTNESS_MAX);
		else
			l = cie_1931_lightness_inverse(i, LIGHTNESS_MAX);
		pwm_lightness_map[i] = (pwm_wrap * l) / LIGHTNESS_MAX;
#if 0
		double l_r;
		if (gamma >= 1.0)
			l_r = gamma_lightness(gamma, l, LIGHTNESS_MAX);
		else
			l_r = cie_1931_lightness(l, LIGHTNESS_MAX);
		printf("[%3d] %lf : %lf\n", i, l, l_r);
#endif
	}
}



/**
 * Initialize PWM hardware to generate 25kHz PWM signal on output pins.
 */
void setup_pwm_outputs()
{
	uint32_t sys_clock = clock_get_hz(clk_sys);
	pwm_config config = pwm_get_default_config();
	uint pwm_freq = cfg->pwm_freq;
	uint clk_div = 1;
	uint slice_num, top;
	double gamma = -1.0;
	int i;


	if (pwm_freq < 10)
		pwm_freq = 10;
	else if (pwm_freq > 100000)
		pwm_freq = 100000;

	log_msg(LOG_NOTICE, "Initializing PWM outputs...");
	log_msg(LOG_NOTICE, "PWM Frequency: %u Hz", pwm_freq);

	top = sys_clock / clk_div / pwm_freq / 2 - 1;  /* for phase-correct PWM signal */
	if (top >= PWM_TOP_MAX) {
		clk_div = top / PWM_TOP_MAX + 1;
		log_msg(LOG_INFO, "Set PWM clock divider: %u", clk_div);
		top = sys_clock / clk_div / pwm_freq / 2 - 1;  /* for phase-correct PWM signal */
	}
	pwm_out_top = top;

	/* Lightness (Gamma Correction) */
	if (strlen(cfg->gamma) > 0) {
		float val;
		if (!strncasecmp(cfg->gamma, "cie", 4)) {
			gamma = 0.0;
			log_msg(LOG_INFO, "Output PWM mapping: CIE (1931)");
		}
		if (str_to_float(cfg->gamma, &val)) {
			if (val >= 1.0 && val <= 10.0) {
				gamma = val;
				log_msg(LOG_INFO, "Output PWM mapping: Gamma %1.1f", val);
			}
		}
	}
	if (gamma < 0.0) {
		log_msg(LOG_INFO, "Output PWM mapping: default");
	}
	calculate_pwm_lightness(top, gamma);

	log_msg(LOG_DEBUG, "PWM: TOP=%u (max %u), CLK_DIV=%u", pwm_out_top, PWM_TOP_MAX, clk_div);
	pwm_config_set_clkdiv_int(&config, clk_div);
	pwm_config_set_phase_correct(&config, 1);
	pwm_config_set_wrap(&config, pwm_out_top);

	/* Configure PWM outputs */

	for (i = 0; i < OUTPUT_COUNT; i=i+2) {
		uint pin1 = output_gpio_pwm_map[i];
		uint pin2 = output_gpio_pwm_map[i + 1];

		gpio_set_function(pin1, GPIO_FUNC_PWM);
		gpio_set_function(pin2, GPIO_FUNC_PWM);
		slice_num = pwm_gpio_to_slice_num(pin1);
		/* two consecutive pins must belong to same PWM slice... */
		assert(slice_num == pwm_gpio_to_slice_num(pin2));
		pwm_init(slice_num, &config, true);
	}

}




