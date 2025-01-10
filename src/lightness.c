/* lightness.c
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


   Formulas in this code are based on a paper by Bill Claff titled
   "Psychometric Lightness and Gamma"
   <https://www.photonstophotos.net/GeneralTopics/Exposure/Psychometric_Lightness_and_Gamma.htm>
*/

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "lightness.h"


/**
 * CIE 1931 formula for psychometric lightness.
 *
 * @param y luminance.
 * @param y_white luminance of white reference (> 0).
 *
 * @return Lightness (value between 0 and 100).
 */
double cie_1931_lightness(double y, double y_white)
{
	double l, x;

	assert(y_white > 0.0);

	x = y / y_white;
	if (x < 0.0)
		l = 0.0;
	else if (x <= 0.008856)
		l = 903.3 * x;
	else if (x <= 1.0)
		l = 116.0 * cbrt(x) - 16.0;
	else
		l = 100.0;

	return l;
}


/**
 * Inverse of CIE 1931 psychometric lightness formula.
 *
 * @param l Lightness (0..100)
 * @param y_white Luminance of white reference.
 *
 * @return Luminance (value between 0 and y_white)
 */
double cie_1931_lightness_inverse(double l, double y_white)
{
	double y;

	assert(y_white > 0.0);

	if (l < 0.0)
		y = 0.0;
	else if (l <= 7.9996248)
		y = l / 903.3 * y_white;
	else if (l <= 100.0)
		y = pow((l + 16) / 116.0, 3) * y_white;
	else
		y = y_white;

	return y;
}


/**
 * Video Gamma function.
 *
 * @param g Gamma value (>=1.0).
 * @param y Luminance (0..y_white).
 * @param y_white Luminance of white reference (>0).
 *
 * @return Lightness (value between 0 and 100).
 */
double gamma_lightness(double g, double y, double y_white)
{
	double l, x;

	assert(g >= 1.0);
	assert(y_white > 0.0);

	x = y / y_white;
	if (x < 0.0)
		l = 0.0;
	else if (x <= 1.0)
		l = pow(x, 1.0 / g) * 100.0;
	else
		l = 100.0;

	return l;
}


/**
 * Inverse of Viedo Gamma function.
 *
 * @param g Gamma value (>=1.0)
 * @param l Lightness (0..100)
 * @param y_white Luminance of white reference.
 *
 * @return Luminance (value between 0 and y_white)
 */
double gamma_lightness_inverse(double g, double l, double y_white)
{
	double y;

	assert(g >= 1.0);
	assert(y_white > 0.0);

	if (l < 0.0)
		y = 0.0;
	else if (l <= 100.0)
		y = pow(l / 100.0, g) * y_white;
	else
		y = y_white;

	return y;
}

