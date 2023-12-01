/* bi_decl.c
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

#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "brickpico.h"


void set_binary_info()
{
	bi_decl(bi_program_description("BrickPico-" BRICKPICO_MODEL " - Smart LED Controller"));
	bi_decl(bi_program_version_string(BRICKPICO_VERSION " ("__DATE__")"));
	bi_decl(bi_program_url("https://github.com/tjko/brickpico/"));

#if LED_PIN >= 0
	bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED (output)"));
#endif

#if TX_PIN >= 0
	bi_decl(bi_1pin_with_name(TX_PIN, "TX (Serial)"));
	bi_decl(bi_1pin_with_name(RX_PIN, "RX (Serial)"));
#endif
#if SDA_PIN >= 0
	bi_decl(bi_1pin_with_name(SDA_PIN, "SDA (I2C)"));
	bi_decl(bi_1pin_with_name(SCL_PIN, "SCL (I2C)"));
#endif

	bi_decl(bi_1pin_with_name(PWM1_PIN, "PWM1 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM2_PIN, "PWM2 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM3_PIN, "PWM3 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM4_PIN, "PWM4 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM5_PIN, "PWM5 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM6_PIN, "PWM6 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM7_PIN, "PWM7 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM8_PIN, "PWM8 signal (output)"));
#if PWM9_PIN >= 0
	bi_decl(bi_1pin_with_name(PWM9_PIN, "PWM9 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM10_PIN, "PWM10 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM11_PIN, "PWM11 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM12_PIN, "PWM12 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM13_PIN, "PWM13 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM14_PIN, "PWM14 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM15_PIN, "PWM15 signal (output)"));
	bi_decl(bi_1pin_with_name(PWM16_PIN, "PWM16 signal (output)"));
#endif
}



