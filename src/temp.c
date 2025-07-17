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


double get_temperature(double adc_ref_voltage, double temp_offset, double temp_coefficient)
{
	uint32_t raw = 0;
	uint64_t start, end;
	double t, volt;
	int i;

	start = to_us_since_boot(get_absolute_time());

	adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);
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

	if (check_for_change(state->temp_prev, state->temp, 0.5)) {
		log_msg(LOG_INFO, "sensor%d: Temperature change %.1fC --> %.1fC",
			1,
			state->temp_prev,
			state->temp);
		state->temp_prev = state->temp;
	}


	for (int i = 0; i < VSENSOR_COUNT; i++) {
		state->vtemp[i] = get_vsensor(i, conf, state);
		if (check_for_change(state->vtemp_prev[i], state->vtemp[i], 0.5)) {
			log_msg(LOG_INFO, "vsensor%d: Temperature change %.1fC --> %.1fC",
				i+1,
				state->vtemp_prev[i],
				state->vtemp[i]);
			state->vtemp_prev[i] = state->vtemp[i];
		}
	}

}


double get_vsensor(uint8_t i, const struct brickpico_config *config,
		struct brickpico_state *state)
{
	const struct vsensor_input *s = &config->vsensors[i];
	double t = state->vtemp[i];

	if (s->mode == VSMODE_MANUAL || s->mode == VSMODE_I2C) {
		/* Check if should reset temperature back to default due to lack of updates... */
		if (s->timeout > 0 && t != s->default_temp) {
			if (absolute_time_diff_us(state->vtemp_updated[i], get_absolute_time())/1000000
				> s->timeout) {
				log_msg(LOG_INFO,"sensor%d: timeout, temperature reset to default", i + 1);
				t = s->default_temp;
			}
		}
	} else if (s->mode == VSMODE_PICOTEMP) {
		t = state->temp;
	} else  {
		int count = 0;
		t = 0.0;

		for (int j = 0; j < VSENSOR_SOURCE_MAX_COUNT && s->sensors[j]; j++) {
			float val;

			if (s->sensors[j] > 100)
				val = state->temp;
			else
				val = state->vtemp[s->sensors[j] - 1];
			count++;

			if (s->mode == VSMODE_MAX) {
				if (count == 1 || val > t)
					t = val;
			}
			else if (s->mode == VSMODE_MIN) {
				if (count == 1 || val < t)
					t = val;
			}
			else if (s->mode == VSMODE_AVG) {
				t += val;
			}
			else if (s->mode == VSMODE_DELTA) {
				if (count == 1)
					t = val;
				else if (count == 2)
					t -= val;
			}
		}
		if (s->mode == VSMODE_AVG && count > 0) {
			t /= count;
		}

	}


	return t;
}
