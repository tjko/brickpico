# default_config.s
# Copyright (C) 2023 Timo Kokkonen <tjko@iki.fi>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of BrickPico.
#

#
# Stub for embedding default_config.json in the executable.
#

	.global brickpico_default_config
	.global brickpico_default_config_end

	.section .rodata

	.p2align 2
brickpico_default_config:
	.incbin	"default_config.json"
	.byte	 0x00
brickpico_default_config_end:

# eof
