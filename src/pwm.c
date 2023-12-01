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
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "brickpico.h"

#define PWM_IN_CLOCK_DIVIDER 100
#define PWM_IN_SAMPLE_INTERVAL 10 /* milliseconds */


/*
 * Functions for generating (emulating) fan PWM control signal and
 * reading (motherboard) PWM signal for controlling fans.
 * Pico PWM hardware is used for both.
 */


/* Map of fan PWM signal output pins.
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


uint pwm_out_top = 0;
float pwm_in_count_rate = 0;

/* Set PMW output signal duty cycle.
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



/* Initialize PWM hardware to generate 25kHz PWM signal on output pins.
 */
void setup_pwm_outputs()
{
	uint32_t sys_clock = clock_get_hz(clk_sys);
	pwm_config config = pwm_get_default_config();
	uint pwm_freq = 25000;
	uint slice_num;
	int i;

	log_msg(LOG_NOTICE, "Initializing PWM outputs...");
	log_msg(LOG_NOTICE, "PWM Frequency: %0.2f kHz", pwm_freq / 1000.0);

	pwm_out_top = (sys_clock / pwm_freq / 2) - 1;  /* for phase-correct PWM signal */

	pwm_config_set_clkdiv(&config, 1);
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




