/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX
 */

#include "inc/dbgprint.h"
#include "inc/tfa_service.h"
#include "inc/tfa98xx_tfafieldnames.h"

/* support for error code translation into text */
#define ERROR_MAX_LENGTH 64

static char latest_errorstr[ERROR_MAX_LENGTH];

const char *tfa98xx_get_error_string(enum tfa98xx_error error)
{
	const char *p_err_str;

	switch (error) {
	case TFA98XX_ERROR_OK:
		p_err_str = "Ok";
		break;
	case TFA98XX_ERROR_DSP_NOT_RUNNING:
		p_err_str = "DSP_not_running";
		break;
	case TFA98XX_ERROR_BAD_PARAMETER:
		p_err_str = "Bad_Parameter";
		break;
	case TFA98XX_ERROR_NOT_OPEN:
		p_err_str = "NotOpen";
		break;
	case TFA98XX_ERROR_IN_USE:
		p_err_str = "InUse";
		break;
	case TFA98XX_ERROR_RPC_BUSY:
		p_err_str = "RpcBusy";
		break;
	case TFA98XX_ERROR_RPC_MOD_ID:
		p_err_str = "RpcModId";
		break;
	case TFA98XX_ERROR_RPC_PARAM_ID:
		p_err_str = "RpcParamId";
		break;
	case TFA98XX_ERROR_RPC_INVALID_CC:
		p_err_str = "RpcInvalidCC";
		break;
	case TFA98XX_ERROR_RPC_INVALID_SEQ:
		p_err_str = "RpcInvalidSeq";
		break;
	case TFA98XX_ERROR_RPC_INVALID_PARAM:
		p_err_str = "RpcInvalidParam";
		break;
	case TFA98XX_ERROR_RPC_BUFFER_OVERFLOW:
		p_err_str = "RpcBufferOverflow";
		break;
	case TFA98XX_ERROR_RPC_CALIB_BUSY:
		p_err_str = "RpcCalibBusy";
		break;
	case TFA98XX_ERROR_RPC_CALIB_FAILED:
		p_err_str = "RpcCalibFailed";
		break;
	case TFA98XX_ERROR_NOT_SUPPORTED:
		p_err_str = "Not_Supported";
		break;
	case TFA98XX_ERROR_I2C_FATAL:
		p_err_str = "I2C_Fatal";
		break;
	case TFA98XX_ERROR_I2C_NON_FATAL:
		p_err_str = "I2C_NonFatal";
		break;
	case TFA98XX_ERROR_STATE_TIMED_OUT:
		p_err_str = "WaitForState_TimedOut";
		break;
	default:
		snprintf(latest_errorstr, ERROR_MAX_LENGTH,
			"Unspecified error (%d)", (int)error);
		p_err_str = latest_errorstr;
		break;
	}

	return p_err_str;
}

/********************/
/* bitfield lookups */
/*
 * generic table lookup functions
 * lookup bf in table
 * return 'unknown' if not found
 */
static char *tfa_bf2name(struct tfa_bf_name *table, uint16_t bf)
{
	int n = 0;

	do {
		if ((table[n].bf_enum & 0xfff0) == (bf & 0xfff0))
			return table[n].bf_name;
	} while (table[n++].bf_enum != 0xffff);

	return table[n - 1].bf_name; /* last name says unknown */
}

/*
 * lookup name in table
 *   return 0xffff if not found
 */
static uint16_t tfa_name2bf(struct tfa_bf_name *table, const char *name)
{
	int n = 0;

	do {
		if (strcasecmp(name, table[n].bf_name) == 0)
			return table[n].bf_enum;
	} while (table[n++].bf_enum != 0xffff);

	return 0xffff;
}

/*
 * tfa2 bitfield name table
 */
static TFA2_NAMETABLE;
static TFA2_BITNAMETABLE;

/*
 * tfa1 bitfield name tables
 */
static TFA1_NAMETABLE;
static TFA9896_NAMETABLE;
static TFA9872_NAMETABLE;
static TFA9874_NAMETABLE;
static TFA9878_NAMETABLE;
static TFA9890_NAMETABLE;
static TFA9891_NAMETABLE;
static TFA9887_NAMETABLE;
static TFA1_BITNAMETABLE;
static TFA9912_NAMETABLE;
static TFA9894_NAMETABLE;
static TFA9896_BITNAMETABLE;
static TFA9872_BITNAMETABLE;
static TFA9874_BITNAMETABLE;
static TFA9878_BITNAMETABLE;
static TFA9912_BITNAMETABLE;
static TFA9890_BITNAMETABLE;
static TFA9891_BITNAMETABLE;
static TFA9887_BITNAMETABLE;
static TFA9894_BITNAMETABLE;

char *tfa_cont_bit_name(uint16_t num, unsigned short rev)
{
	char *name;
	/* end of list for the unknown string */
	int table_length = sizeof(tfa1_datasheet_names)
		/ sizeof(struct tfa_bf_name);
	const char *unknown = tfa1_datasheet_names[table_length - 1].bf_name;

	switch (rev & 0xff) {
	case 0x88:
		name = tfa_bf2name(tfa2_bit_names, num);
		break;
	case 0x97:
		name = tfa_bf2name(tfa1_bit_names, num);
		break;
	case 0x96:
		name = tfa_bf2name(tfa9896_bit_names, num);
		break;
	case 0x72:
		name = tfa_bf2name(tfa9872_bit_names, num);
		break;
	case 0x74:
		name = tfa_bf2name(tfa9874_bit_names, num);
		break;
	case 0x78:
		name = tfa_bf2name(tfa9878_bit_names, num);
		break;
	case 0x92:
		name = tfa_bf2name(tfa9891_bit_names, num);
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		/* my tabel 1st */
		name = tfa_bf2name(tfa9890_bit_names, num);
		if (strcmp(unknown, name) == 0)
			/* try generic table */
			name = tfa_bf2name(tfa1_bit_names, num);
		break;
	case 0x12:
		/* my tabel 1st */
		name = tfa_bf2name(tfa9887_bit_names, num);
		if (strcmp(unknown, name) == 0)
			/* try generic table */
			name = tfa_bf2name(tfa1_bit_names, num);
		break;
	case 0x13:
		name = tfa_bf2name(tfa9912_bit_names, num);
		break;
	case 0x94:
#if defined(USE_TFA9894N2)
		if (((rev >> 8) & 0xff) == 0x2a || ((rev >> 8) & 0xff) == 0x3a)
			name = tfa_bf2name(tfa9894N2_bit_names, num);
		else
			name = tfa_bf2name(tfa9894_bit_names, num);
#else
		name = tfa_bf2name(tfa9894_bit_names, num);
#endif
		break;
	default:
		pr_err("unknown REVID:0x%0x\n", rev);
		table_length = sizeof(tfa1_bit_names)
			/ sizeof(struct tfa_bf_name); /* end of list */
		name = (char *)unknown;
		break;
	}
	return name;
}

char *tfa_cont_ds_name(uint16_t num, unsigned short rev)
{
	char *name;
	/* end of list for the unknown string */
	int table_length = sizeof(tfa1_datasheet_names)
		/ sizeof(struct tfa_bf_name);
	const char *unknown = tfa1_datasheet_names[table_length - 1].bf_name;

	switch (rev & 0xff) {
	case 0x88:
		name = tfa_bf2name(tfa2_datasheet_names, num);
		break;
	case 0x97:
		name = tfa_bf2name(tfa1_datasheet_names, num);
		break;
	case 0x96:
		name = tfa_bf2name(tfa9896_datasheet_names, num);
		break;
	case 0x72:
		name = tfa_bf2name(tfa9872_datasheet_names, num);
		break;
	case 0x74:
		name = tfa_bf2name(tfa9874_datasheet_names, num);
		break;
	case 0x78:
		name = tfa_bf2name(tfa9878_datasheet_names, num);
		break;
	case 0x92:
		name = tfa_bf2name(tfa9891_datasheet_names, num);
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		/* my tabel 1st */
		name = tfa_bf2name(tfa9890_datasheet_names, num);
		if (strcmp(unknown, name) == 0)
			/* try generic table */
			name = tfa_bf2name(tfa1_datasheet_names, num);
		break;
	case 0x12:
		/* my tabel 1st */
		name = tfa_bf2name(tfa9887_datasheet_names, num);
		if (strcmp(unknown, name) == 0)
			/* try generic table */
			name = tfa_bf2name(tfa1_datasheet_names, num);
		break;
	case 0x13:
		name = tfa_bf2name(tfa9912_datasheet_names, num);
		break;
	case 0x94:
#if defined(USE_TFA9894N2)
		if (((rev >> 8) & 0xff) == 0x2a || ((rev >> 8) & 0xff) == 0x3a)
			name = tfa_bf2name(tfa9894N2_datasheet_names, num);
		else
			name = tfa_bf2name(tfa9894_datasheet_names, num);
#else
		name = tfa_bf2name(tfa9894_datasheet_names, num);
#endif
		break;
	default:
		pr_err("unknown REVID:0x%0x\n", rev);
		table_length = sizeof(tfa1_datasheet_names)
			/ sizeof(struct tfa_bf_name); /* end of list */
		name = (char *)unknown;
		break;
	}
	return name;
}

char *tfa_cont_bf_name(uint16_t num, unsigned short rev)
{
	char *name;
	/* end of list for the unknown string */
	int table_length = sizeof(tfa1_datasheet_names)
		/ sizeof(struct tfa_bf_name);
	const char *unknown = tfa1_datasheet_names[table_length - 1].bf_name;

	/* if datasheet name does not exist look for bitfieldname */
	name = tfa_cont_ds_name(num, rev);
	if (strcmp(unknown, name) == 0)
		name = tfa_cont_bit_name(num, rev);

	return name;
}

uint16_t tfa_cont_bf_enum(const char *name, unsigned short rev)
{
	uint16_t bfnum;

	switch (rev & 0xff) {
	case 0x88:
		bfnum = tfa_name2bf(tfa2_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa2_bit_names, name);
		break;
	case 0x97:
		bfnum = tfa_name2bf(tfa1_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try generic table */
			bfnum = tfa_name2bf(tfa1_bit_names, name);
		break;
	case 0x96:
		bfnum = tfa_name2bf(tfa9896_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try generic table */
			bfnum = tfa_name2bf(tfa9896_bit_names, name);
		break;
	case 0x72:
		bfnum = tfa_name2bf(tfa9872_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa9872_bit_names, name);
		break;
	case 0x74:
		bfnum = tfa_name2bf(tfa9874_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa9874_bit_names, name);
		break;
	case 0x78:
		bfnum = tfa_name2bf(tfa9878_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa9878_bit_names, name);
		break;
	case 0x92:
		bfnum = tfa_name2bf(tfa9891_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa9891_bit_names, name);
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		/* my tabel 1st */
		bfnum = tfa_name2bf(tfa9890_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try generic table */
			bfnum = tfa_name2bf(tfa1_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try 2nd generic table */
			bfnum = tfa_name2bf(tfa1_bit_names, name);
		break;
	case 0x12:
		/* my tabel 1st */
		bfnum = tfa_name2bf(tfa9887_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try generic table */
			bfnum = tfa_name2bf(tfa1_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try 2nd generic table */
			bfnum = tfa_name2bf(tfa1_bit_names, name);
		break;
	case 0x13:
		bfnum = tfa_name2bf(tfa9912_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa9912_bit_names, name);
		break;
	case 0x94:
#if defined(USE_TFA9894N2)
		if (((rev >> 8) & 0xff) == 0x2a
			|| ((rev >> 8) & 0xff) == 0x3a) {
			bfnum = tfa_name2bf(tfa9894N2_datasheet_names, name);
			if (bfnum == 0xffff)
				/* try long bitname table */
				bfnum = tfa_name2bf(tfa9894N2_bit_names, name);
		} else {
			bfnum = tfa_name2bf(tfa9894_datasheet_names, name);
			if (bfnum == 0xffff)
				/* try long bitname table */
				bfnum = tfa_name2bf(tfa9894_bit_names, name);
		}
#else
		bfnum = tfa_name2bf(tfa9894_datasheet_names, name);
		if (bfnum == 0xffff)
			/* try long bitname table */
			bfnum = tfa_name2bf(tfa9894_bit_names, name);
#endif
		break;
	default:
		pr_err("unknown REVID:0x%0x\n", rev);
		bfnum = 0xffff;
		break;
	}

	return bfnum;
}

/*
 * check all lists for a hit
 * this is for the parser to know if it's an existing bitname
 */
uint16_t tfa_cont_bf_enum_any(const char *name)
{
	uint16_t bfnum;

	/* datasheet names first */
	bfnum = tfa_name2bf(tfa2_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa1_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9891_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9890_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9887_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9872_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9874_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9878_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9896_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9912_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
#if defined(USE_TFA9894N2)
	bfnum = tfa_name2bf(tfa9894N2_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
#else
	bfnum = tfa_name2bf(tfa9894_datasheet_names, name);
	if (bfnum != 0xffff)
		return bfnum;
#endif

	/* and then bitfield names */
	bfnum = tfa_name2bf(tfa2_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa1_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9891_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9890_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9887_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9872_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9874_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9878_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9896_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
	bfnum = tfa_name2bf(tfa9912_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
#if defined(USE_TFA9894N2)
	bfnum = tfa_name2bf(tfa9894N2_bit_names, name);
	if (bfnum != 0xffff)
		return bfnum;
#else
	bfnum = tfa_name2bf(tfa9894_bit_names, name);
#endif

	return bfnum;
}
