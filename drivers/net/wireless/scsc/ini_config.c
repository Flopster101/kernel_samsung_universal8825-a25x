/***************************************************************************
 *
 * Copyright (c) 2021 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/firmware.h>

#include "dev.h"
#include "utils.h"
#include "debug.h"
#include "mib.h"
#include "mlme.h"

#define SLSI_INI_CONFIG_BUFF_SIZE            1024
#define SET_HIGHEST_BYTE                     -1
#define SLSI_INI_KEY_LEN_MAX                 1000
#define SLSI_INI_TRY_FALLBACK                1

#if (KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE)
#define INI_CONFIG_FILE_PATH            "/vendor/firmware/wlan-connection-roaming.ini"
#define INI_CONFIG_FILE_BACKUP_PATH     "/vendor/firmware/wlan-connection-roaming-backup.ini"
#else
#define INI_CONFIG_FILE_PATH            "../firmware/wlan-connection-roaming.ini"
#define INI_CONFIG_FILE_BACKUP_PATH     "../firmware/wlan-connection-roaming-backup.ini"
#endif

struct ini_lookup {
	const char *key;
	int (*func_p)(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value, struct ini_lookup *lookup_entry);
	u16 psid;
	s8  index;
	int unit_con_factor;
	int min_value;
	int max_value;
};

static void log_failed_psids(struct slsi_dev *sdev, u8 *mib_buffer, unsigned int mib_buffer_len)
{
	size_t mib_decode_result                = 0;
	size_t offset                           = 0;
	struct slsi_mib_entry decoded_mib_value = {0};
	struct slsi_mib_data  mibdata           = {0};

	FUNC_ENTER(sdev);
	SLSI_ERR(sdev, "MIB buffer length: %u. MIB Error (decoded)\n", mib_buffer_len);

	if (!mib_buffer) {
		SLSI_ERR(sdev, "MIB buffer pointer is NULL - can not decode MIB keys\n");
		return;
	}

	SLSI_ERR(sdev, "failed mibs:\n");

	while (offset < mib_buffer_len) {
		mibdata.data = &mib_buffer[offset];
		mibdata.dataLength = mib_buffer_len - offset;

		mib_decode_result = slsi_mib_decode(&mibdata, &decoded_mib_value);
		if (!mib_decode_result) {
			SLSI_ERR_HEX(sdev, mibdata.data, mibdata.dataLength, "slsi_mib_decode() Failed to Decode:\n");
			break;
		}

		offset += mib_decode_result;
		SLSI_ERR(sdev, "%d\n", (int)(decoded_mib_value.psid));
	}

	SLSI_ERR(sdev, "\n");
	FUNC_EXIT(sdev);
}

static void set_mibs(struct slsi_dev *sdev)
{
	struct sk_buff *cfm = NULL;

	if (sdev->ini_conf_struct.ini_conf_buff_pos == 0)
		return;

	cfm = slsi_mlme_set_with_cfm(sdev, NULL, sdev->ini_conf_struct.ini_conf_buff,
				     sdev->ini_conf_struct.ini_conf_buff_pos + 1);

	if (fapi_get_datalen(cfm)) {
		SLSI_ERR(sdev, "Err Setting MIB failed.\n");
		log_failed_psids(sdev, fapi_get_data(cfm), fapi_get_datalen(cfm));
	} else {
		SLSI_INFO(sdev, "setting MIB successfully\n");
	}

	sdev->ini_conf_struct.ini_conf_buff_pos = 0;
	kfree_skb(cfm);
}

static void append_mibdata_to_buffer(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *mib, int mib_len)
{
	if (((*pos + mib_len) > (SLSI_INI_CONFIG_BUFF_SIZE - 1)) && *pos > 0)
		set_mibs(sdev);

	memcpy(buf + (*pos), mib, mib_len);
	*pos += mib_len;
}

static int encode_int_mib(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
			  struct ini_lookup *lookup_entry, bool is_signed)
{
	struct slsi_mib_data mib_data = { 0, NULL };
	int error                     = SLSI_MIB_STATUS_FAILURE;
	u32 value_uint                = 0;
	s32 value_sint                = 0;
	int res                       = 0;
	int index                     = 0;

	res = slsi_str_to_int(value, is_signed ? &value_sint : (int*)&value_uint);
	if (!res) {
		SLSI_ERR_NODEV("key_val is invalid for key %s\n", lookup_entry->key);
		return -EINVAL;
	}

	if (lookup_entry->index > 0)
		index = lookup_entry->index;

	if (is_signed) {
		value_sint *= lookup_entry->unit_con_factor;
		if (slsi_mib_encode_int(&mib_data, lookup_entry->psid,
					value_sint, index) != SLSI_MIB_STATUS_SUCCESS) {
			SLSI_ERR_NODEV("Failed to encode mib with key = %s and psid = %d\n",
				       lookup_entry->key, lookup_entry->psid);
			return error;
		}
	} else {
		value_uint *= lookup_entry->unit_con_factor;
		if (slsi_mib_encode_uint(&mib_data, lookup_entry->psid,
				         value_uint, index) != SLSI_MIB_STATUS_SUCCESS) {
			SLSI_ERR_NODEV("Failed to encode mib with key = %s and psid = %d\n",
				       lookup_entry->key, lookup_entry->psid);
			return error;
		}
	}

	if (!mib_data.dataLength) {
		SLSI_ERR_NODEV("Failed to encode mib with key = %s and psid = %d\n",
			       lookup_entry->key, lookup_entry->psid);
		return error;
	}

	append_mibdata_to_buffer(sdev, buf, pos, mib_data.data, mib_data.dataLength);

	kfree(mib_data.data);

	return 0;
}

static int encode_uint_mib(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
			   struct ini_lookup *lookup_entry)
{
	return encode_int_mib(sdev, buf, pos, value, lookup_entry, false);
}

static int encode_sint_mib(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
			   struct ini_lookup *lookup_entry)
{
	return encode_int_mib(sdev, buf, pos, value, lookup_entry, true);
}

static int encode_mib_gen_with_two_psids(struct slsi_dev *sdev, u8 *buf, u16 *pos, s8 index1, s8 index2,
					 u16 psid1, u16 psid2, u32 value_int)
{
	struct slsi_mib_data mib_data1 = { 0, NULL };
	struct slsi_mib_data mib_data2 = { 0, NULL };
	int  error = SLSI_MIB_STATUS_FAILURE;

	if (slsi_mib_encode_uint(&mib_data1, psid1, value_int, index1) != SLSI_MIB_STATUS_SUCCESS) {
		SLSI_ERR_NODEV("Failed to encode mib with psid = %d\n", psid1);
		return error;
	}

	if (!mib_data1.dataLength) {
		SLSI_ERR_NODEV("Failed to encode mib with psid = %d\n", psid1);
		return error;
	}

	if (slsi_mib_encode_uint(&mib_data2, psid2, value_int, index2) != SLSI_MIB_STATUS_SUCCESS) {
		SLSI_ERR_NODEV("Failed to encode mib with psid = %d\n", psid2);
		goto exit_with_err1;
	}

	if (!mib_data2.dataLength) {
		SLSI_ERR_NODEV("Failed to encode mib with psid = %d\n", psid2);
		goto exit_with_err1;
	}

	append_mibdata_to_buffer(sdev, buf, pos, mib_data2.data, mib_data2.dataLength);

	append_mibdata_to_buffer(sdev, buf, pos, mib_data1.data, mib_data1.dataLength);

	kfree(mib_data2.data);

exit_with_err1:
	kfree(mib_data1.data);
	return 0;
}

static int encode_mib_con_dtim_skipping_number(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int  error = SLSI_MIB_STATUS_FAILURE;
	u32 value_int = 0;
	int res = 0;
	int index = 0;

	res = slsi_str_to_int(value, &value_int);
	if (!res) {
		SLSI_ERR_NODEV("key_val is invalid for key %s\n", lookup_entry->key);
		return -EINVAL;
	}

	if (lookup_entry->index > 0)
		index = lookup_entry->index;

	value_int *= lookup_entry->unit_con_factor;

	error = encode_mib_gen_with_two_psids(sdev, buf, pos, index, index, lookup_entry->psid,
					      SLSI_PSID_UNIFI_IDLEMODE_LISTEN_INTERVAL_SKIPPING_DTIM,
					      value_int);

	return error;
}

static int encode_mib_roamcu_trig(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
				  struct ini_lookup *lookup_entry)
{
	int  error = SLSI_MIB_STATUS_FAILURE;
	u32 value_int = 0;
	int res = 0;

	res = slsi_str_to_int(value, &value_int);
	if (!res) {
		SLSI_ERR_NODEV("key_val is invalid for key %s\n", lookup_entry->key);
		return -EINVAL;
	}

	value_int *= lookup_entry->unit_con_factor;

	error = encode_mib_gen_with_two_psids(sdev, buf, pos, 1, 2, lookup_entry->psid,
					      lookup_entry->psid, value_int);

	return error;
}

static int encode_mib_octet_with_2indices(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value, s8 index1, s8 index2,
					  s8 octet_index, u16 psid, int unit_con_factor)
{
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_data mib_val_set = { 0, NULL };
	struct slsi_mib_data mib_val_local = {0, NULL};
	struct slsi_mib_value *values = NULL;
	struct slsi_mib_get_entry get_values[] = {{ psid, { index1, index2 }}};
	u32 value_int = 0;
	int res = 0;
	struct slsi_mib_entry v = {0};

	if (!value) {
		SLSI_ERR_NODEV("value points to null\n");
		return 0;
	}
	res = slsi_str_to_int(value, &value_int);
	if (!res) {
		SLSI_ERR_NODEV("key_val is invalid for psid:%d\n", psid);
		return -EINVAL;
	}

	value_int *= unit_con_factor;

	mibrsp.dataLength = 15;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR_NODEV("Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		return -ENOMEM;
	}

	values = slsi_read_mibs(sdev, NULL, get_values, 1, &mibrsp);

	if (!values) {
		SLSI_ERR_NODEV("values in return from read_mibs is null\n");
		goto exit_with_mibrsp;
	}

	if (values[0].type != SLSI_MIB_TYPE_OCTET) {
		SLSI_ERR_NODEV("invalid mib type for psid:%d\n", psid);
		goto exit_with_value;
	}

	if (!(values[0].u.octetValue.data)) {
		SLSI_ERR_NODEV("values[0].u.octetValue.data in read_mibs is null\n");
		goto exit_with_value;
	}

	/* copying mib value to a local buffer to use */
	mib_val_local.dataLength = values[0].u.octetValue.dataLength;
	mib_val_local.data = values[0].u.octetValue.data;

	if (octet_index < 0)
		octet_index = mib_val_local.dataLength - 1;

	if (octet_index > mib_val_local.dataLength - 1)
		goto exit_with_value;

	mib_val_local.data[octet_index] &= value_int & 0xFF;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = index1;
	v.index[1] = index2;
	v.value.type = SLSI_MIB_TYPE_OCTET;
	v.value.u.octetValue.dataLength = (u32)mib_val_local.dataLength;
	v.value.u.octetValue.data = (u8 *)mib_val_local.data;

	if (slsi_mib_encode(&mib_val_set, &v) != SLSI_MIB_STATUS_SUCCESS) {
		SLSI_ERR_NODEV("Failed to encode mib with psid = %d\n", psid);
		goto exit_with_value;
	}

	if (!mib_val_set.dataLength) {
		SLSI_ERR_NODEV("Failed to encode mib with psid = %d\n", psid);
		goto exit_with_value;
	}

	append_mibdata_to_buffer(sdev, buf, pos, mib_val_set.data, mib_val_set.dataLength);

	kfree(mib_val_set.data);

exit_with_value:
	kfree(values);
exit_with_mibrsp:
	kfree(mibrsp.data);
	return 0;
}

static int encode_mib_band1_rssi_factor_value1(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 1, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_value2(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 2, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_value3(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 3, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_value4(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 4, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_value5(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 5, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_score1(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 1, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_score2(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 2, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_score3(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 3, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_score4(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 4, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band1_rssi_factor_score5(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 5, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_value1(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 1, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_value2(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 2, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_value3(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 3, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_value4(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 4, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_value5(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 5, SET_HIGHEST_BYTE,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_score1(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 1, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_score2(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 2, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_score3(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 3, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_score4(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 4, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

static int encode_mib_band2_rssi_factor_score5(struct slsi_dev *sdev, u8 *buf, u16 *pos, u8 *value,
					       struct ini_lookup *lookup_entry)
{
	int error = SLSI_MIB_STATUS_FAILURE;

	error = encode_mib_octet_with_2indices(sdev, buf, pos, value, lookup_entry->index, 5, 0,
					       lookup_entry->psid, lookup_entry->unit_con_factor);
	if (error)
		SLSI_ERR_NODEV("Err Setting MIB %s failed. error = %d\n", lookup_entry->key, error);

	return error;
}

struct ini_lookup slsi_ini_config_lookup_table[] = {
	{.key = "RoamCommon_MinRoamDetla", .func_p = encode_uint_mib, .psid = 2322, .index = -1,
	 .unit_con_factor = 100, .min_value = 0, .max_value = 100},
	{.key = "RoamCommon_Delta", .func_p = encode_uint_mib, .psid = 2302, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 30},
	{.key = "RoamScan_FirstTimer", .func_p = encode_uint_mib, .psid = 2058, .index = -1,
	 .unit_con_factor = 1000000, .min_value = 0, .max_value = 20},
	{.key = "RoamScan_SecondTimer", .func_p = encode_uint_mib, .psid = 2052, .index = -1,
	 .unit_con_factor = 1000000, .min_value = 60, .max_value = 100},
	{.key = "RoamScan_InactiveTimer", .func_p = encode_uint_mib, .psid = 2059, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamScan_InactiveCount", .func_p = encode_uint_mib, .psid = 2319, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamScan_StepRSSI", .func_p = encode_uint_mib, .psid = 2062, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamRSSI_Trigger", .func_p = encode_sint_mib, .psid = 2050, .index = -1,
	 .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamCU_Trigger", .func_p = encode_mib_roamcu_trig, .psid = 2308, .index = 1,
	 .unit_con_factor = 1, .min_value = 60, .max_value = 90},
	{.key = "RoamCU_MonitorTime", .func_p = encode_uint_mib, .psid = 2311, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamCU_24GRSSIRange", .func_p = encode_sint_mib, .psid = 2307, .index = 1,
	 .unit_con_factor = 1, .min_value = -70, .max_value = -50},
	{.key = "RoamCU_5GRSSIRange", .func_p = encode_sint_mib, .psid = 2307, .index = 2,
	 .unit_con_factor = 1, .min_value = -70, .max_value = -50},
	{.key = "RoamIdle_TriggerBand", .func_p = encode_uint_mib, .psid = 2073, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 3},
	{.key = "RoamIdle_InactiveTime", .func_p = encode_uint_mib, .psid = 2066, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamIdle_MinRSSI", .func_p = encode_sint_mib, .psid = 2064, .index = -1,
	 .unit_con_factor = 1, .min_value = -70, .max_value = -50},
	{.key = "RoamIdle_RSSIVariation", .func_p = encode_uint_mib, .psid = 2063, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 10},
	{.key = "RoamIdle_InactivePacketCount", .func_p = encode_uint_mib, .psid = 2071, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamIdle_Delta", .func_p = encode_uint_mib, .psid = 2074, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamBeaconLoss_TargetMinRSSI", .func_p = encode_sint_mib, .psid = 2299, .index = -1,
	 .unit_con_factor = 1, .min_value = -127, .max_value = -70},
	{.key = "RoamEmergency_TargetMinRSSI", .func_p = encode_sint_mib, .psid = 2301, .index = -1,
	 .unit_con_factor = 1, .min_value = -127, .max_value = -70},
	{.key = "RoamBTM_Delta", .func_p = encode_uint_mib, .psid = 2304, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "RoamAPScore_RSSIWeight", .func_p = encode_uint_mib, .psid = 2305, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_CUWeight", .func_p = encode_uint_mib, .psid = 2303, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band1_RSSIFactorValue1", .func_p = encode_mib_band1_rssi_factor_value1, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band1_RSSIFactorValue2", .func_p = encode_mib_band1_rssi_factor_value2, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band1_RSSIFactorValue3", .func_p = encode_mib_band1_rssi_factor_value3, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band1_RSSIFactorValue4", .func_p = encode_mib_band1_rssi_factor_value4, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band1_RSSIFactorValue5", .func_p = encode_mib_band1_rssi_factor_value5, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band1_RSSIFactorScore1", .func_p = encode_mib_band1_rssi_factor_score1, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band1_RSSIFactorScore2", .func_p = encode_mib_band1_rssi_factor_score2, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band1_RSSIFactorScore3", .func_p = encode_mib_band1_rssi_factor_score3, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band1_RSSIFactorScore4", .func_p = encode_mib_band1_rssi_factor_score4, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band1_RSSIFactorScore5", .func_p = encode_mib_band1_rssi_factor_score5, .psid = 2306,
	 .index = 1, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band2_RSSIFactorValue1", .func_p = encode_mib_band2_rssi_factor_value1, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band2_RSSIFactorValue2", .func_p = encode_mib_band2_rssi_factor_value2, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band2_RSSIFactorValue3", .func_p = encode_mib_band2_rssi_factor_value3, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band2_RSSIFactorValue4", .func_p = encode_mib_band2_rssi_factor_value4, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band2_RSSIFactorValue5", .func_p = encode_mib_band2_rssi_factor_value5, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band2_RSSIFactorScore1", .func_p = encode_mib_band2_rssi_factor_score1, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band2_RSSIFactorScore2", .func_p = encode_mib_band2_rssi_factor_score2, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band2_RSSIFactorScore3", .func_p = encode_mib_band2_rssi_factor_score3, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band2_RSSIFactorScore4", .func_p = encode_mib_band2_rssi_factor_score4, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band2_RSSIFactorScore5", .func_p = encode_mib_band2_rssi_factor_score5, .psid = 2306,
	 .index = 2, .unit_con_factor = 1, .min_value = 0, .max_value = 100},
	{.key = "RoamNCHO_Trigger", .func_p = encode_sint_mib, .psid = 2092, .index = -1,
	 .unit_con_factor = 1, .min_value = -100, .max_value = -50},
	{.key = "RoamNCHO_Delta", .func_p = encode_uint_mib, .psid = 2075, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 30},
	{.key = "RoamNCHO_FullScanPeriod", .func_p = encode_uint_mib, .psid = 2053,
	 .index = -1, .unit_con_factor = 1000000, .min_value = 60, .max_value = 300},
	{.key = "RoamNCHO_PartialScanPeriod", .func_p = encode_uint_mib, .psid = 2292,
	 .index = -1, .unit_con_factor = 1000000, .min_value = 0, .max_value = 20},
	{.key = "RoamNCHO_ActiveCH_DwellTime", .func_p = encode_uint_mib, .psid = 2057,
	 .index = -1, .unit_con_factor = 1, .min_value = 0, .max_value = 200},
	{.key = "RoamNCHO_PassiveCH_DwellTime", .func_p = encode_uint_mib, .psid = 2644,
	 .index = -1, .unit_con_factor = 1, .min_value = 0, .max_value = 200},
	{.key = "RoamNCHO_HomeTime", .func_p = encode_uint_mib, .psid = 2069, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 200},
	{.key = "RoamNCHO_AwayTime", .func_p = encode_uint_mib, .psid = 2070, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 200},
	{.key = "ConBeaconLoss_TimeoutOnWakeUp", .func_p = encode_uint_mib, .psid = 2098,
	 .index = -1, .unit_con_factor = 1, .min_value = 0, .max_value = 20},
	{.key = "ConDTIMSkipping_Number", .func_p = encode_mib_con_dtim_skipping_number, .psid = 2518,
	 .index = -1, .unit_con_factor = 1, .min_value = 0, .max_value = 10},
	{.key = "ConKeepAlive_Interval", .func_p = encode_uint_mib, .psid = 2502, .index = -1,
	 .unit_con_factor = 1, .min_value = 0, .max_value = 120},
	{.key = "RoamWTC_ScanMode", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 2},
	{.key = "RoamWTC_HandlingRSSIThreshold", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -90, .max_value = -60},
	{.key = "RoamWTC_24GCandiRSSIThreshold", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -90, .max_value = -60},
	{.key = "RoamWTC_5GCandiRSSIThreshold", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -90, .max_value = -60},
	{.key = "RoamWTC_6GCandiRSSIThreshold", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -90, .max_value = -60},
	{.key = "RoamBTCoex_ScoreWeight", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 100},
	{.key = "RoamBTCoex_ETPWeight", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 100},
	{.key = "RoamBTCoex_TargetMinRSSI", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -90, .max_value = -60},
	{.key = "RoamBTCoex_Delta", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 20},
	{.key = "RoamAPScore_Band3_RSSIFactorValue1", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band3_RSSIFactorValue2", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band3_RSSIFactorValue3", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band3_RSSIFactorValue4", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = -100, .max_value = -50},
	{.key = "RoamAPScore_Band3_RSSIFactorScore1", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band3_RSSIFactorScore2", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band3_RSSIFactorScore3", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 100},
	{.key = "RoamAPScore_Band3_RSSIFactorScore4", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 100},
	{.key = "RoamScan_6G_PSC_DwellTime", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 200},
	{.key = "RoamScan_6G_NonPSC_DwellTime", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 200},
	{.key = "ConBeaconLoss_TimeoutOnSleep", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 20},
	{.key = "ConDTIMSkipping_MaxTime", .func_p = NULL, .psid = -100, .index = -1,
	 .unit_con_factor = 0, .min_value = 0, .max_value = 2}
};

static int slsi_ini_validate_parameter_range(struct slsi_dev *sdev, int key_val,
					     int lookup_table_index)
{
	if (key_val < slsi_ini_config_lookup_table[lookup_table_index].min_value ||
	    key_val > slsi_ini_config_lookup_table[lookup_table_index].max_value) {
		SLSI_ERR_NODEV("Parameter value %d is out of range for the key %s\n",
			       key_val, slsi_ini_config_lookup_table[lookup_table_index].key);
		return -EINVAL;
	}
	return 0;
}

static int slsi_ini_config_fn_lookup(u8 *key, int len)
{
	int i = 0;
	struct ini_lookup *p = NULL;

	for (i = 0; i < ARRAY_SIZE(slsi_ini_config_lookup_table); i++) {
		p = &slsi_ini_config_lookup_table[i];
		if (p->key && !strncasecmp(key, p->key, len))
			return i;
	}
	return -1;
}

static u8 *slsi_ini_trim_white_space(u8 *tmp, u8 *line, int len)
{
	while ((tmp - line) < len) {
		if (!(*tmp == ' ' || *tmp == '\t'))
			break;
		tmp++;
	}
	return tmp;
}

static int slsi_parse_ini_config(struct slsi_dev *sdev, u8 *line, int len, bool read_operation)
{
	int ret                                     = 0;
	int index                                   = -1;
	int key_len                                 = 0;
	u8 *key                                     = NULL;
	u8 *key_val                                 = NULL;
	u8 *tmp                                     = NULL;
	int key_value_int                           = 0;
	static u8 final_key[SLSI_INI_KEY_LEN_MAX];

	if (*line == '[' || *line == '#')
		return 0;

	tmp = line;
	tmp = slsi_ini_trim_white_space(tmp, line, len);
	if ((tmp - line) >= len) {
		SLSI_ERR_NODEV("parsing error/syntax error in ini conf file before finding the start of the key\n");
		return -EINVAL;
	}

	key = tmp;

	while ((tmp - line) < len) {
		if (*tmp == ' ' || *tmp == '\t' || *tmp == '=')
			break;
		tmp++;
	}

	if ((tmp - line) >= len) {
		SLSI_ERR_NODEV("parsing error/syntax error in ini conf file before finding the end of the key\n");
		return -EINVAL;
	}

	key_len = (tmp - key);
	if (key_len < SLSI_INI_KEY_LEN_MAX) {
		strncpy(final_key, key, key_len);
		final_key[key_len] = '\0';
	} else {
		SLSI_ERR_NODEV("Key length error in ini configuration file\n");
		return -EINVAL;
	}
	index = slsi_ini_config_fn_lookup(final_key, key_len);
	if (index < 0) {
		SLSI_ERR_NODEV("Parsed key : %.*s is not supported\n", key_len, key);
		return -EINVAL;
	}

	if (!slsi_ini_config_lookup_table[index].func_p) {
		SLSI_ERR_NODEV("handler is not present for key:%s\n", slsi_ini_config_lookup_table[index].key);
		return -ENOTSUPP;
	}

	tmp = slsi_ini_trim_white_space(tmp, line, len);
	if ((tmp - line) >= len) {
		SLSI_ERR_NODEV("parsing error/syntax error in ini conf file after the key found successfully\n");
		return -EINVAL;
	}

	if (*tmp != '=') {
		SLSI_ERR_NODEV("Error: Delimiter is missing\n");
		return -EINVAL;
	}

	tmp++;
	tmp = slsi_ini_trim_white_space(tmp, line, len);
	if ((tmp - line) >= len) {
		SLSI_ERR_NODEV("parsing error/syntax error in ini conf file before finding the key_val\n");
		return -ENOTSUPP;
	}

	key_val = tmp;
	if (read_operation) {
		ret = slsi_str_to_int(key_val, &key_value_int);
		if (!ret) {
			SLSI_ERR_NODEV("Key value is invalid for key:%s\n",
				       slsi_ini_config_lookup_table[index].key);
			return -EINVAL;
		}
		ret = slsi_ini_validate_parameter_range(sdev, key_value_int, index);
		return ret;
	}

	ret = slsi_ini_config_lookup_table[index].func_p(sdev, sdev->ini_conf_struct.ini_conf_buff,
							 &sdev->ini_conf_struct.ini_conf_buff_pos,
							 key_val, &slsi_ini_config_lookup_table[index]);
	if (ret) {
		SLSI_ERR_NODEV("Err occurred in handler for key: %s\n", slsi_ini_config_lookup_table[index].key);
		if (ret == -EINVAL)
			return ret;
	}
	return 0;
}

static int slsi_read_ini_config_file(struct slsi_dev *sdev, const struct firmware *e,
				     bool read_operation)
{
	u8 *line = NULL;
	u8 *tmp = NULL;
	int line_len = 0;
	int r = 0;

	if (!sdev->ini_conf_struct.ini_conf_buff)
		sdev->ini_conf_struct.ini_conf_buff = kmalloc(SLSI_INI_CONFIG_BUFF_SIZE, GFP_KERNEL);

	if (!sdev->ini_conf_struct.ini_conf_buff) {
		SLSI_ERR(sdev, "Couldn't allocate static buffer for ini configuration\n");
		return -ENOMEM;
	}

	sdev->ini_conf_struct.ini_conf_buff_pos = 0;

	tmp = (u8 *)e->data;
	while ((tmp - e->data) < e->size) {
		while (((tmp - e->data) < e->size) &&
		       ((*tmp == '\n') || (*tmp == '\r') || (*tmp == ' ') || (*tmp == '\t')))
			tmp++;

		line = tmp;
		while (((tmp - e->data) < e->size) && (*tmp != '\n') && (*tmp != '\r'))
			tmp++;

		line_len = tmp - line;
		if (line_len > 0) {
			r = slsi_parse_ini_config(sdev, line, line_len, read_operation);
			if (r == -EINVAL && read_operation) {
				sdev->ini_conf_struct.ini_conf_buff_pos = 0;
				return SLSI_INI_TRY_FALLBACK;
			}
		}
		tmp++;
	}

	if (read_operation)
		r = slsi_read_ini_config_file(sdev, e, false);
	else
		set_mibs(sdev);
	sdev->ini_conf_struct.ini_conf_buff_pos = 0;
	return r;
}

static int slsi_load_ini_config_file(struct slsi_dev *sdev, char *path, const struct firmware **e)
{
	int r = 0;

	r = mx140_request_file(sdev->maxwell_core, path, e);
	if (r != 0) {
		SLSI_ERR(sdev, "Couldn't open %s file r = %d\n", path, r);
		return SLSI_INI_TRY_FALLBACK;
	}

	SLSI_INFO(sdev, "Ini conf file opened successfully : Path = %s\n", path);
	if (!e) {
		SLSI_ERR(sdev, "mx140_request_file() returned success, but firmware was null\n");
		return SLSI_INI_TRY_FALLBACK;
	}
	return r;
}

void slsi_process_ini_config_file(struct slsi_dev *sdev)
{
	const struct firmware *e = NULL;
	int r = 0;

	r = slsi_load_ini_config_file(sdev, INI_CONFIG_FILE_PATH, &e);
	if (!r) {
		r = slsi_read_ini_config_file(sdev, e, true);
		mx140_release_file(sdev->maxwell_core, e);
	}

	if (r != SLSI_INI_TRY_FALLBACK)
		return;

	r = slsi_load_ini_config_file(sdev, INI_CONFIG_FILE_BACKUP_PATH, &e);
	if (!r) {
		r = slsi_read_ini_config_file(sdev, e, true);
		mx140_release_file(sdev->maxwell_core, e);
	}
}
