# credits.s
# Copyright (C) 2023 Timo Kokkonen <tjko@iki.fi>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of BrickPico.
#

#
# Stub for embedding credits.txt in the executable.
#

	.global brickpico_credits_text
	.global brickpico_credits_text_end

	.section .rodata

	.p2align 2
brickpico_credits_text:	
	.incbin	"credits.txt"
	.byte	 0x00
brickpico_credits_text_end:

# eof
