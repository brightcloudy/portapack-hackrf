/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "touch.hpp"

namespace touch {

struct Metrics {
	const float x;
	const float y;
	const float r;
};

static Metrics calculate_metrics(const Frame& frame) {
	/* TODO: Yikes! M0 doesn't have floating point, so this code is
	 * expensive! On the other hand, it seems to be working well (and
	 * fast *enough*?), so maybe leave it alone at least for now.
	 */

	const auto x_max = frame.x.xp;
	const auto x_min = frame.x.xn;
	const auto x_range = x_max - x_min;
	const auto x_position = (frame.x.yp + frame.x.yn) / 2;
	const float x_norm = float(x_position - x_min) / x_range;

	const auto y_max = frame.y.yn;
	const auto y_min = frame.y.yp;
	const auto y_range = y_max - y_min;
	const auto y_position = (frame.y.xp + frame.y.xn) / 2;
	const float y_norm = float(y_position - y_min) / y_range;

	const auto z_max = frame.pressure.yp;
	const auto z_min = frame.pressure.xn;
	const auto z_range = z_max - z_min;
	const auto z1_position = frame.pressure.xp;
	const float z1_norm = float(z1_position - z_min) / z_range;
	const auto z2_position = frame.pressure.yn;
	const float z2_norm = float(z2_position - z_min) / z_range;

	const float r_x_plate = 330.0f;
	//const float r_y_plate = 600.0f;
	const float r_touch = r_x_plate * x_norm * (z2_norm / z1_norm - 1.0f);

	return {
		.x = x_norm,
		.y = y_norm,
		.r = r_touch,
	};
}

void Manager::feed(const Frame& frame) {
	// touch_debounce.feed(touch_raw);
	const auto touch_raw = frame.touch;
	//const auto touch_stable = touch_debounce.state();
	const auto touch_stable = frame.touch;
	bool touch_pressure = false;

	// Only feed coordinate averaging if there's a touch.
	// TODO: Separate threshold to gate coordinates for filtering?
	if( touch_raw ) {
		const auto metrics = calculate_metrics(frame);

		// TODO: Add touch pressure hysteresis?
		touch_pressure = (metrics.r < r_touch_threshold);
		if( touch_pressure ) {
			const float x = width_pixels * (metrics.x - calib_x_low) / calib_x_range;
			filter_x.feed(x);
			const float y = height_pixels * (calib_y_high - metrics.y) / calib_y_range;
			filter_y.feed(y);
		}
	} else {
		filter_x.reset();
		filter_y.reset();
	}

	switch(state) {
	case State::NoTouch:
		if( touch_stable && touch_pressure ) {
			if( point_stable() ) {
				state = State::TouchDetected;
				touch_started();
			}
		}
		break;

	case State::TouchDetected:
		if( touch_stable && touch_pressure ) {
			touch_moved();
		} else {
			state = State::NoTouch;
			touch_ended();
		}
		break;

	default:
		state = State::NoTouch;
		break;
	}
}

} /* namespace touch */
