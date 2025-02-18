/*
 * Copyright (C) 2019-2021  Advanced Micro Devices, Inc.
 */
#ifndef _vangogh_lite_ip_offset_HEADER
#define _vangogh_lite_ip_offset_HEADER

#define MAX_INSTANCE                                        7
#define MAX_SEGMENT                                         5

struct IP_BASE_INSTANCE
{
    unsigned int segment[MAX_SEGMENT];
};

struct IP_BASE
{
    struct IP_BASE_INSTANCE instance[MAX_INSTANCE];
};


static const struct IP_BASE GC_BASE = { { { { 0x00001260, 0x0000A000, 0x0, 0x0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } } } };
static const struct IP_BASE NBIO_BASE = { { { { 0x00000000, 0x00000014, 0x00000D20, 0x00010400, 0x0241B000 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } },
                                        { { 0, 0, 0, 0, 0 } } } };

#define GC_BASE__INST0_SEG0                        0x00001260
#define GC_BASE__INST0_SEG1                        0x0000A000
#define GC_BASE__INST0_SEG2                        0
#define GC_BASE__INST0_SEG3                        0
#define GC_BASE__INST0_SEG4                        0

#define GC_BASE__INST1_SEG0                        0
#define GC_BASE__INST1_SEG1                        0
#define GC_BASE__INST1_SEG2                        0
#define GC_BASE__INST1_SEG3                        0
#define GC_BASE__INST1_SEG4                        0

#define GC_BASE__INST2_SEG0                        0
#define GC_BASE__INST2_SEG1                        0
#define GC_BASE__INST2_SEG2                        0
#define GC_BASE__INST2_SEG3                        0
#define GC_BASE__INST2_SEG4                        0

#define GC_BASE__INST3_SEG0                        0
#define GC_BASE__INST3_SEG1                        0
#define GC_BASE__INST3_SEG2                        0
#define GC_BASE__INST3_SEG3                        0
#define GC_BASE__INST3_SEG4                        0

#define GC_BASE__INST4_SEG0                        0
#define GC_BASE__INST4_SEG1                        0
#define GC_BASE__INST4_SEG2                        0
#define GC_BASE__INST4_SEG3                        0
#define GC_BASE__INST4_SEG4                        0

#define GC_BASE__INST5_SEG0                        0
#define GC_BASE__INST5_SEG1                        0
#define GC_BASE__INST5_SEG2                        0
#define GC_BASE__INST5_SEG3                        0
#define GC_BASE__INST5_SEG4                        0

#define GC_BASE__INST6_SEG0                        0
#define GC_BASE__INST6_SEG1                        0
#define GC_BASE__INST6_SEG2                        0
#define GC_BASE__INST6_SEG3                        0
#define GC_BASE__INST6_SEG4                        0

#define NBIO_BASE__INST0_SEG0                      0x00000000
#define NBIO_BASE__INST0_SEG1                      0x00000014
#define NBIO_BASE__INST0_SEG2                      0x00000D20
#define NBIO_BASE__INST0_SEG3                      0x00010400
#define NBIO_BASE__INST0_SEG4                      0x0241B000

#define NBIO_BASE__INST1_SEG0                      0
#define NBIO_BASE__INST1_SEG1                      0
#define NBIO_BASE__INST1_SEG2                      0
#define NBIO_BASE__INST1_SEG3                      0
#define NBIO_BASE__INST1_SEG4                      0

#define NBIO_BASE__INST2_SEG0                      0
#define NBIO_BASE__INST2_SEG1                      0
#define NBIO_BASE__INST2_SEG2                      0
#define NBIO_BASE__INST2_SEG3                      0
#define NBIO_BASE__INST2_SEG4                      0

#define NBIO_BASE__INST3_SEG0                      0
#define NBIO_BASE__INST3_SEG1                      0
#define NBIO_BASE__INST3_SEG2                      0
#define NBIO_BASE__INST3_SEG3                      0
#define NBIO_BASE__INST3_SEG4                      0

#define NBIO_BASE__INST4_SEG0                      0
#define NBIO_BASE__INST4_SEG1                      0
#define NBIO_BASE__INST4_SEG2                      0
#define NBIO_BASE__INST4_SEG3                      0
#define NBIO_BASE__INST4_SEG4                      0

#define NBIO_BASE__INST5_SEG0                      0
#define NBIO_BASE__INST5_SEG1                      0
#define NBIO_BASE__INST5_SEG2                      0
#define NBIO_BASE__INST5_SEG3                      0
#define NBIO_BASE__INST5_SEG4                      0

#define NBIO_BASE__INST6_SEG0                      0
#define NBIO_BASE__INST6_SEG1                      0
#define NBIO_BASE__INST6_SEG2                      0
#define NBIO_BASE__INST6_SEG3                      0
#define NBIO_BASE__INST6_SEG4                      0

#endif
