/* lightness.h
   Copyright (C) 2025 Timo Kokkonen <tjko@iki.fi>

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

#ifndef BRICKPICO_LIGHTNESS_H
#define BRICKPICO_LIGHTNESS_H 1


double cie_1931_lightness(double y, double y_white);
double cie_1931_lightness_inverse(double l, double y_white);

double gamma_lightness(double g, double y, double y_white);
double gamma_lightness_inverse(double g, double l, double y_white);


#endif /* BRICKPICO_LIGHTNESS_H */
