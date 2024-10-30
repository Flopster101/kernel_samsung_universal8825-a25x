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

/* CONVERT VALUE FOR SCONTEXT*/
#define SCONTEXT_AP_STATUS_SHUTDOWN                   0xD0
#define SCONTEXT_AP_STATUS_LCD_ON                     0xD1
#define SCONTEXT_AP_STATUS_LCD_OFF                    0xD2
#define SCONTEXT_AP_STATUS_RESUME                     0xD3
#define SCONTEXT_AP_STATUS_SUSPEND                    0xD4
#define SCONTEXT_AP_STATUS_RESET                      0xD5
#define SCONTEXT_AP_STATUS_POW_CONNECTED              0xD6
#define SCONTEXT_AP_STATUS_POW_DISCONNECTED           0xD7
#define SCONTEXT_AP_STATUS_CALL_IDLE                  0xD8
#define SCONTEXT_AP_STATUS_CALL_ACTIVE                0xD9

#define SCONTEXT_INST_LIBRARY_ADD                     0xB1
#define SCONTEXT_INST_LIBRARY_REMOVE                  0xB2
#define SCONTEXT_INST_LIB_NOTI                        0xB4
#define SCONTEXT_INST_LIB_SET_DATA                    0xC1
#define SCONTEXT_INST_LIB_GET_DATA                    0xB8

#define SCONTEXT_VALUE_CURRENTSYSTEMTIME              0x0E
#define SCONTEXT_VALUE_PEDOMETER_USERHEIGHT           0x12
#define SCONTEXT_VALUE_PEDOMETER_USERWEIGHT           0x13
#define SCONTEXT_VALUE_PEDOMETER_USERGENDER           0x14
#define SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME       0x15
#define SCONTEXT_VALUE_LIBRARY_DATA                   0x17
#define SCONTEXT_VALUE_SCREEN_STATE                   0x47

#define SCONTEXT_VALUE_CURRENTSTATUS                  0x01
#define SCONTEXT_VALUE_CURRENTSTATUS_BATCH            0x02
#define SCONTEXT_VALUE_VERSIONINFO                    0x03

#define SCONTEXT_NOTIFY_HUB_RESET                     0xD5
