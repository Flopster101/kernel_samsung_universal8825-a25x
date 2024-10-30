/*
 * Copyright 2019-2021 Advanced Micro Devices, Inc.
 */

#ifndef MOBILE1_ASIC_INIT_H
#define MOBILE1_ASIC_INIT_H

struct __reg_setting_m1 {
	unsigned int reg;
	unsigned int val;
};

/* From the mobile1 Gopher build, cl72071 */
struct __reg_setting_m1 emu_mobile1_setting_cl72071[] = {
	{0x000098F8, 0x00000142},
	{0x00033440, 0x0000008D},
	{0x00033640, 0x0000008D},
	{0x00009508, 0x010B0000},
	{0x00009510, 0x00000000},
	{0x0003380C, 0xFFFFFFF3},
	{0x00033884, 0xFFFFFFF3},
	{0x00033894, 0x32103210},
	{0x00033898, 0x32103210},
	{0x00009F80, 0x00000500},
};

/* From $out/linux_3.10.0_64.VCS/mobile1/common/pub/include/reg/golden_register_value.h */
struct __reg_setting_m1 emu_mobile1_setting_cl87835[] = {
	{0x000098F8, 0x00000142},/* GB_ADDR_CONFIG */
	{0x00033440, 0x0000008D},/* GL1_PIPE_STEER */
	{0x00033640, 0x0000008D},/* CH_PIPE_STEER */
	{0x00009508, 0x010B0000},/* TA_CNTL_AUX */
	{0x00009510, 0x00000000},/* TA_CNTL2 */
	{0x0003380C, 0xFFFFFFF3},/* GL2C_ADDR_MATCH_MASK */
	{0x00033884, 0xFFFFFFF3},/* GL2A_ADDR_MATCH_MASK */
	{0x00033894, 0x32103210},/* GL2_PIPE_STEER_0 */
	{0x00033898, 0x32103210},/* GL2_PIPE_STEER_1 */
	{0x00009834, 0x00000421},/* DB_DEBUG2 */
	{0x00009F80, 0x00000500},/* GCR_GENERAL_CNTL */
};
#endif
