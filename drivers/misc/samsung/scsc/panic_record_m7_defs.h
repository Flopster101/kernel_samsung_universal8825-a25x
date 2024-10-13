/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef M7_SYSTEM_ERROR_RECORD_H__
#define M7_SYSTEM_ERROR_RECORD_H__
/**
 * @file
 * M7 System Error Record.
 *
 * This record is used to pass summary information about the context of
 * Maxwell M7 firmware system errors to the host and to the gdb-stub/monitor.
 *
 * Notes:-
 * - The host system error handler should _not_ expect the M7 record to be
 * written prior to a system error indication from Maxwell, and it may never
 * be written at all. The checksum should indicate a valid record.
 */

/* Defines */

/*
 * version 1 mr7 panic record defs
 */

#define M7_PANIC_RECORD_MAX_LENGTH 305
/**
 * The current version of the system error record structure defined below.
 *
 * Written to version field by firmware, checked by host.
 * This also serves as a rudimentary endianess check.
 */
#define M7_SYSTEM_ERROR_RECORD_VERSION 1

/**
 * Checksum seed to prevent false match on all zeros or ones.
 */
#define M7_SYSTEM_ERROR_RECORD_CKSUM_SEED 0xa5a5a5a5

/* Types */

/**
 * M7 System Error record (type)
 *
 * Fields are packed, integers are little-endian.
 */
typedef struct
{
    /**
     * Record Version.
     *
     * Written by firmware. Should be checked by readers.
     */
    uint32_t version;

    /**
     * Total record length in bytes.
     *
     * Includes initial version and final checksum field.
     */
    uint32_t length;

    /**
     * Info fields from firmware system error.
     *
     * The first field is the byte index of a printf-style, null-terminated
     * ascii string relative to the start of the log_strings section.
     *
     * Other fields match any the format specifiers in the format string
     * assuming an ILP32 data model:-
     *
     *  %x %X %d %s %p %uz
     *
     * N.B. Any arguments matching a "%s" specifier will also be the index of a
     * null-terminated ascii string in the log-string section.
     *
     * Any extra fields not matched by format specifiers will contain
     * arbitrary data and should be ignored.
     */
    uint32_t info[4];

    /**
     * Timestamps
     */
    struct
    {
        uint32_t fast_clock;
        uint32_t slow_clock;
    } timestamps;

    /**
     * Interrupted context.
     *
     * The interrupted ARMV7-M context preserved on entry to
     * the exception handler.
     */
    struct
    {
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t r8;
        uint32_t r9;
        uint32_t r10;
        uint32_t r11;
        uint32_t r12;
        uint32_t sp;
        uint32_t lr;
        uint32_t pc;
        uint32_t xpsr;
    } interrupted_context;

    /**
     * Exception context.
     *
     * Key ARMV7-M registers on entry to the exception.
     */
    struct
    {
        uint32_t psp;
        uint32_t msp;
        uint32_t lr; /* e.g. 0xfffffff9 */

        uint32_t primask;
        uint32_t faultmask;
        uint32_t basepri;
        uint32_t control;

        uint32_t icsr;
        uint32_t shcsr;
        uint32_t cfsr;
        uint32_t hfsr;
        uint32_t dfsr;
        uint32_t mmfar;
        uint32_t bfar;
        uint32_t afsr;
        uint32_t abfsr;
    } exception_context;

    /**
     * MPU Configuration when exception is hit.
     */
    struct
    {
        uint32_t ctrl;
        uint32_t rnr;
        struct
        {
            uint32_t rbar;
            uint32_t rasr;
        } regions[16];
    } mpu_config;

    /* Add new fields above here */

    /**
     * XOR checksum over all fields above.
     *
     * Seeded with M7_SYSTEM_ERROR_RECORD_CKSUM_SEED.
     */
    uint32_t cksum;

} m7_system_error_record;

#endif /* M7_SYSTEM_ERROR_RECORD_H__ */
