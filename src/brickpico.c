/* brickpico.c
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
#include <malloc.h>
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "pico/multicore.h"
#include "pico/util/datetime.h"
#ifdef LIB_PICO_CYW43_ARCH
#include "pico/cyw43_arch.h"
#endif
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/rtc.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/vreg.h"

#include "brickpico.h"


static struct brickpico_state transfer_state;

static struct brickpico_state system_state;
struct brickpico_state *brickpico_state = &system_state;

bool rebooted_by_watchdog = false;


void boot_reason()
{
	printf("     CHIP_RESET: %08lx\n", vreg_and_chip_reset_hw->chip_reset);
	printf("WATCHDOG_REASON: %08lx\n", watchdog_hw->reason);
}

void setup()
{
	int i = 0;

	rtc_init();

	stdio_usb_init();
	/* Wait a while for USB Serial to connect... */
	while (i++ < 40) {
		if (stdio_usb_connected())
			break;
		sleep_ms(50);
	}


	read_config(false);

#if TTL_SERIAL > 0
	stdio_uart_init_full(TTL_SERIAL_UART,
			TTL_SERIAL_SPEED, TX_PIN, RX_PIN);
#endif
	printf("\n\n");
#ifndef NDEBUG
	boot_reason();
#endif
	if (watchdog_enable_caused_reboot()) {
		printf("[Rebooted by watchdog]\n");
		rebooted_by_watchdog = true;
	}
	printf("\n");

	/* Run "SYStem:VERsion" command... */
	cmd_version(NULL, NULL, 0, NULL);
	printf("Hardware Model: BRICKPICO-%s\n", BRICKPICO_MODEL);
	printf("         Board: %s\n", PICO_BOARD);
	printf("           MCU: %s @ %0.0fMHz\n",
		rp2040_model_str(),
		clock_get_hz(clk_sys) / 1000000.0);
	printf(" Serial Number: %s\n\n", pico_serial_str());

	display_init();
	network_init(&system_state);

	/* Setup GPIO pins... */
	log_msg(LOG_NOTICE, "Initialize GPIO...");

	/* Initialize status LED... */
	if (LED_PIN > 0) {
		gpio_init(LED_PIN);
		gpio_set_dir(LED_PIN, GPIO_OUT);
		gpio_put(LED_PIN, 0);
	}
#ifdef LIB_PICO_CYW43_ARCH
	/* On pico_w, LED is connected to the radio GPIO... */
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#endif

	/* Configure PWM pins... */
	setup_pwm_outputs();

	for (i = 0; i < OUTPUT_COUNT; i++) {
		uint8_t duty = cfg->outputs[i].default_pwm;
		set_pwm_duty_cycle(i, duty);
		brickpico_state->pwm[i] = duty;
	}

	log_msg(LOG_NOTICE, "System initialization complete.");
}


void clear_state(struct brickpico_state *s)
{
	int i;

	for (i = 0; i < OUTPUT_MAX_COUNT; i++) {
		s->pwm[i] = 0;
	}
}


void core1_main()
{
	absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(t_last, 0);
	absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(t_tick, 0);
	absolute_time_t t_now;
	int64_t max_delta = 0;
	int64_t delta;

	log_msg(LOG_INFO, "core1: started...");

	/* Allow core0 to pause this core... */
	multicore_lockout_victim_init();

	t_tick = t_last = get_absolute_time();

	while (1) {
		t_now = get_absolute_time();
		delta = absolute_time_diff_us(t_last, t_now);
		t_last = t_now;

		if (delta > max_delta) {
			max_delta = delta;
			log_msg(LOG_INFO, "core1: max_loop_time=%lld", max_delta);
		}

		if (time_passed(&t_tick, 5000)) {
			log_msg(LOG_DEBUG, "tick");
		}
	}
}


int main()
{
	absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(t_led, 0);
	absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(t_network, 0);
	absolute_time_t t_now, t_last, t_display;
	uint8_t led_state = 0;
	int64_t max_delta = 0;
	int64_t delta;
	int c;
	char input_buf[1024 + 1];
	int i_ptr = 0;


	set_binary_info();
	clear_state(&system_state);
	clear_state(&transfer_state);

	/* Initialize MCU and other hardware... */
	if (get_debug_level() >= 2)
		print_mallinfo();
	setup();
	if (get_debug_level() >= 2)
		print_mallinfo();

	/* Start second core (core1)... */
	multicore_launch_core1(core1_main);

#if WATCHDOG_ENABLED
	watchdog_enable(WATCHDOG_REBOOT_DELAY, 1);
	log_msg(LOG_NOTICE, "Watchdog enabled.");
#endif

	t_last = get_absolute_time();
	t_display = t_last;

	while (1) {
		t_now = get_absolute_time();
		delta = absolute_time_diff_us(t_last, t_now);
		t_last = t_now;

		if (delta > max_delta || delta > 1000000) {
			max_delta = delta;
			log_msg(LOG_INFO, "core0: max_loop_time=%lld", max_delta);
		}

		if (time_passed(&t_network, 1)) {
			network_poll();
		}

		/* Toggle LED every 1000ms */
		if (time_passed(&t_led, 1000)) {
			if (cfg->led_mode == 0) {
				/* Slow blinking */
				led_state = (led_state > 0 ? 0 : 1);
			} else if (cfg->led_mode == 1) {
				/* Always on */
				led_state = 1;
			} else {
				/* Always off */
				led_state = 0;
			}
#if LED_PIN > 0
			gpio_put(LED_PIN, led_state);
#endif
#ifdef LIB_PICO_CYW43_ARCH
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
#endif
		}

		/* Update display every 1000ms */
		if (time_passed(&t_display, 1000)) {
			log_msg(LOG_DEBUG, "Update display");
			display_status(brickpico_state, cfg);
		}

		/* Process any (user) input */
		while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
			if (c == 0xff || c == 0x00)
				continue;
			if (c == 0x7f || c == 0x08) {
				if (i_ptr > 0) i_ptr--;
				if (cfg->local_echo) printf("\b \b");
				continue;
			}
			if (c == 10 || c == 13 || i_ptr >= sizeof(input_buf) - 1) {
				if (cfg->local_echo) printf("\r\n");
				input_buf[i_ptr] = 0;
				if (i_ptr > 0) {
					process_command(brickpico_state, (struct brickpico_config *)cfg, input_buf);
					i_ptr = 0;
				}
				continue;
			}
			input_buf[i_ptr++] = c;
			if (cfg->local_echo) printf("%c", c);
		}
#if WATCHDOG_ENABLED
		watchdog_update();
#endif
	}
}


/* eof :-) */
