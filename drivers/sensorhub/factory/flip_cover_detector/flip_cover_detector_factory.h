/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __FLIP_COVER_DETECTOR_FACTORY_H_
#define __FLIP_COVER_DETECTOR_FACTORY_H_

enum {
	OFF = 0,
	ON = 1
};

enum {
	X = 0,
	Y = 1,
	Z = 2,
	AXIS_MAX
};

#define ATTACH          "ATTACH"
#define DETACH          "DETACH"
#define FACTORY_PASS    "PASS"
#define FACTORY_FAIL    "FAIL"

#define AXIS_SELECT      X
#define THRESHOLD        700
#define DETACH_MARGIN    100
#define SATURATION_VALUE 4900
#define MAG_DELAY_MS     50

#define COVER_DETACH            0 // OPEN
#define COVER_ATTACH            1 // CLOSE
#define COVER_ATTACH_NFC_ACTIVE 2 // CLOSE

void check_cover_detection_factory(void);

#define FCD_SUBCMD_CAL_MATRIX_NUM	126

#endif
