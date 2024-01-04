/* temp.c
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
#include <string.h>
#include <math.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "brickpico.h"


#define ADC_REF_VOLTAGE 3.338
#define ADC_MAX_VALUE   (1 << 12)
#define ADC_AVG_WINDOW  16
#define ADC_PIN         4  /* ADC4 - Internal temperature sensor on RP2040 */


double get_temperature(double adc_ref_voltage, double temp_offset, double temp_coefficient)
{
	uint32_t raw = 0;
	uint64_t start, end;
	double t, volt;
	int i;

	start = to_us_since_boot(get_absolute_time());

	adc_select_input(ADC_PIN);
	for (i = 0; i < ADC_AVG_WINDOW; i++) {
		raw += adc_read();
	}
	raw /= ADC_AVG_WINDOW;
	volt = raw * (adc_ref_voltage / ADC_MAX_VALUE);

	t = 27.0 - ((volt - 0.706) / 0.001721);
	t = t * temp_coefficient + temp_offset;

	end = to_us_since_boot(get_absolute_time());

	log_msg(LOG_DEBUG, "get_temperature(): raw=%lu,  volt=%lf, temp=%lf (duration=%llu)",
		raw, volt, t, end - start);

	return t;
}




void update_temp(const struct brickpico_config *conf, struct brickpico_state *state)
{
	state->temp = get_temperature(conf->adc_ref_voltage,
				conf->temp_offset, conf->temp_coefficient);
}


