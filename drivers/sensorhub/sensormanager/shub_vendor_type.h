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

#ifndef __SHUB_VENDOR_TYPE_H_
#define __SHUB_VENDOR_TYPE_H_

#define VENDOR_NAME_SAMSUNG	"Samsung Inc."
#define VENDOR_NAME_AKM		"AKM"
#define VENDOR_NAME_YAMAHA	"YAMAHA"
#define VENDOR_NAME_STM		"STM"
#define VENDOR_NAME_MEMSIC	"Memsic"
#define VENDOR_NAME_AMS		"AMS"
#define VENDOR_NAME_CAPELLA	"Capella"
#define VENDOR_NAME_SHARP	"Sharp"
#define VENDOR_NAME_BOSCH	"BOSCH"
#define VENDOR_NAME_TDK		"TDK"
#define VENDOR_NAME_SITRONIX	"Sitronix"

enum {
	VENDOR_SAMSUNG = 0,
	VENDOR_AKM,
	VENDOR_YAMAHA,
	VENDOR_STM,
	VENDOR_MEMSIC,
	VENDOR_AMS,
	VENDOR_CAPELLA,
	VENDOR_SHARP,
	VENDOR_BOSCH,
	VENDOR_TDK,
	VENDOR_SITRONIX,
	VENDOR_MAX,
};

#define VENDOR_LIST		{	\
		VENDOR_NAME_SAMSUNG,	\
		VENDOR_NAME_AKM,	\
		VENDOR_NAME_YAMAHA,	\
		VENDOR_NAME_STM,	\
		VENDOR_NAME_MEMSIC,	\
		VENDOR_NAME_AMS,	\
		VENDOR_NAME_CAPELLA,	\
		VENDOR_NAME_SHARP,	\
		VENDOR_NAME_BOSCH,	\
		VENDOR_NAME_TDK,	\
		VENDOR_NAME_SITRONIX,	\
	}

#endif /* __SHUB_VENDOR_TYPE_H_ */
