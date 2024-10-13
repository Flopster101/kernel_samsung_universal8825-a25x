#ifndef _UFS_EXYNOS_FMP_H_
#define _UFS_EXYNOS_FMP_H_

#define FMP_DRV_VERSION "4.0.0"

/* FMP IDs (second or third parameter to FMP/SMU Ctrls) */
#define FMP_EMBEDDED			0
#define FMP_UFSCARD			1
#define FMP_SDCARD			2

/* SMU commands (second parameter to SMC_CMD_SMU) */
#define SMU_INIT			0
#define SMU_SET				1
#define SMU_ABORT			2

/* Fourth parameter to SMC_CMD_FMP_SECURITY */
#define CFG_DESCTYPE_0			0
#define CFG_DESCTYPE_3			3

/* Fourth parameter to SMC_CMD_FMP_KW_MODE */
#define WAP0_NS				(1 << 8)
#define UNWRAP_EN			(0 << 4)
#define UNWRAP_BYPASS			(1 << 4)
#define SWKEY_MODE			0
#define SECUREKEY_MODE			1

/* Fourth parameter to SMC_CMD_FMP_KW_INDATASWAP */
#define RAW_FILEKEY_WORDSWAP            (1 << 22)
#define RAW_TWEAKKEY_WORDSWAP           (1 << 21)
#define PBK_WORDSWAP                    (1 << 20)
#define IV_WORDSWAP                     (1 << 19)
#define TAG_WORDSWAP                    (1 << 18)
#define SECURE_FILEKEY_WORDSWAP         (1 << 17)
#define SECURE_TWEAKKEY_WORDSWAP        (1 << 16)
#define RAW_FILEKEY_BYTESWAP            (1 << 6)
#define RAW_TWEAKKEY_BYTESWAP           (1 << 5)
#define PBK_BYTESWAP                    (1 << 4)
#define IV_BYTESWAP                     (1 << 3)
#define TAG_BYTESWAP                    (1 << 2)
#define SECURE_FILEKEY_BYTESWAP         (1 << 1)
#define SECURE_TWEAKKEY_BYTESWAP        (1 << 0)

/* Advertise crypto quirks to ufshcd-core. */
#ifdef CONFIG_KEYS_IN_PRDT
#define UFSHCD_QUIRK_FMP_MODE_SPECIFIC	UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER |\
					UFSHCD_QUIRK_KEYS_IN_PRDT
#endif

#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
#define UFSHCD_QUIRK_FMP_MODE_SPECIFIC	UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER
#endif

#ifdef CONFIG_KEYS_IN_CUSTOM_KEYSLOT
#define UFSHCD_QUIRK_FMP_MODE_SPECIFIC	UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER
#endif

#ifdef CONFIG_SOC_EXYNOS2100
#define UFSHCD_QUIRK_FMP_SOC_SPECIFIC UFSHCD_QUIRK_BROKEN_CRYPTO_ENABLE
#else
#define UFSHCD_QUIRK_FMP_SOC_SPECIFIC 0
#endif

enum fmp_crypto_algo_mode {
	FMP_BYPASS_MODE = 0,
	FMP_ALGO_MODE_AES_CBC = 1,
	FMP_ALGO_MODE_AES_XTS = 2,
};

#define FMP_DATA_UNIT_SIZE 4096

struct fmp_sg_entry {
	/* The first four fields correspond to those of ufshcd_sg_entry. */
	__le32 des0;
	__le32 des1;
	__le32 des2;
	/*
	 * The algorithm and key length are configured in the high bits of des3,
	 * whose low bits already contain ufshcd_sg_entry::size.
	 */
	__le32 des3;
#define FKL			(1 << 26)
#define SET_KEYLEN(ent, v)	((ent)->des3 |= cpu_to_le32(v))
#define SET_FAS(ent, v)		((ent)->des3 |= cpu_to_le32((v) << 28))

	/* The IV with all bytes reversed */
	__be32 file_iv[4];

	/*
	 * The key with all bytes reversed.  For XTS, the two halves of the key
	 * are given separately and are byte-reversed separately.
	 */
	__be32 file_enckey[8];
	__be32 file_twkey[8];

	/* Not used */
	__be32 disk_iv[4];
	__le32 reserved[4];
};

/* Number of custom keyslots */
/* 0-14 keylots: keyslot for crypto io, 15: reserved keyslot for FIPS */
#define UFS_KEYSLOTS	16

#ifdef CONFIG_EXYNOS_FMP_FIPS
#define FIPS_KEYSLOT	1
#else
#define FIPS_KEYSLOT	0
#endif

#define NUM_KEYSLOTS	(UFS_KEYSLOTS - FIPS_KEYSLOT)

#define AES_256_XTS_KEY_SIZE		64
#define SECRET_SIZE			32
#define AES_GCM_TAG_SIZE		16
#define AES_256_XTS_TWK_OFFSET		8
#define HW_WRAPPED_KEY_TAG_OFFSET	16
#define HW_WRAPPED_KEY_SECRET_OFFSET	80

/* UFSHCD register map  */
#define CRYPTOCFG	0x0440
/* 0-CRYPTOCFG - CFGE_CAPIDX_DUSIZE */
union exynos_ufs_crypto_cfg_entry {
	__le32 reg_val;
	struct {
		u8 data_unit_size;
		u8 crypto_cap_idx;
		u8 reserved_1;
		u8 config_enable;
	};
};

struct exynos_fmp {
	u8 valid_check;
};

/* FMP register map  */
#define FMP_REGS	38

#define UFSPRCTRL	0x000
#define UFSPRSTAT0	0x008
#define UFSPRSTAT1	0x00C
#define UFSPRSECURITY0	0x010
#define UFSPWCTRL	0x100
#define UFSPWSTAT0	0x108
#define UFSPWSTAT1	0x10C
#define UFSPSBEGIN0	0x2000
#define UFSPSEND0	0x2004
#define UFSPSLUN0	0x2008
#define UFSPSCTRL0	0x200C
#define UFSPSBEGIN1	0x2010
#define UFSPSEND1	0x2014
#define UFSPSLUN1	0x2018
#define UFSPSCTRL1	0x201C
#define UFSPSBEGIN2	0x2020
#define UFSPSEND2	0x2024
#define UFSPSLUN2	0x2028
#define UFSPSCTRL2	0x202C
#define UFSPSBEGIN3	0x2030
#define UFSPSEND3	0x2034
#define UFSPSLUN3	0x2038
#define UFSPSCTRL3	0x203C
#define UFSPSBEGIN4	0x2040
#define UFSPSEND4	0x2044
#define UFSPSLUN4	0x2048
#define UFSPSCTRL4	0x204C
#define UFSPSBEGIN5	0x2050
#define UFSPSEND5	0x2054
#define UFSPSLUN5	0x2058
#define UFSPSCTRL5	0x205C
#define UFSPSBEGIN6	0x2060
#define UFSPSEND6	0x2064
#define UFSPSLUN6	0x2068
#define UFSPSCTRL6	0x206C
#define UFSPSBEGIN7	0x2070
#define UFSPSEND7	0x2074
#define UFSPSLUN7	0x2078
#define UFSPSCTRL7	0x207C

#define FMP_KW_SECUREKEY	0x8000
#define FMP_KW_TAG		0x8040
#define FMP_KW_CONTROL		0xA008
#define FMP_KW_KEYVALID		0xA00C
#define FMP_KW_TAGABORT		0xA010
#define FMP_KW_PBKABORT		0xA014

#define FMP_KW_SECUREKEY_OFFSET	0x80
#define FMP_KW_TAG_OFFSET	0x80

#define EXYNOS_FMP_FIPS_KEYSLOT 0xF

/* Definition to dump fmp registers */
struct exynos_ufs_fmp_sfr_log {
        const char name[15];
        const u32 offset;
        u64 val;
};

static struct exynos_ufs_fmp_sfr_log ufs_fmp_log_sfr[FMP_REGS] = {
	{"UFSPRCTRL",		UFSPRCTRL},
	{"UFSPRSTAT0",		UFSPRSTAT0},
	{"UFSPRSTAT1",		UFSPRSTAT1},
	{"UFSPRSECURITY0",	UFSPRSECURITY0},
	{"UFSPWCTRL",		UFSPWCTRL},
	{"UFSPWSTAT0",		UFSPWSTAT0},
	{"UFSPWSTAT1",		UFSPWSTAT1},
	{"UFSPSBEGIN0",		UFSPSBEGIN0},
	{"UFSPSEND0",		UFSPSEND0},
	{"UFSPSLUN0",		UFSPSLUN0},
	{"UFSPSCTRL0",		UFSPSCTRL0},
	{"UFSPSBEGIN1",		UFSPSBEGIN1},
	{"UFSPSEND1",		UFSPSEND1},
	{"UFSPSLUN1",		UFSPSLUN1},
	{"UFSPSCTRL1",		UFSPSCTRL1},
	{"UFSPSBEGIN2",		UFSPSBEGIN2},
	{"UFSPSEND2",		UFSPSEND2},
	{"UFSPSLUN2",		UFSPSLUN2},
	{"UFSPSCTRL2",		UFSPSCTRL2},
	{"UFSPSBEGIN3",		UFSPSBEGIN3},
	{"UFSPSEND3",		UFSPSEND3},
	{"UFSPSLUN3",		UFSPSLUN3},
	{"UFSPSCTRL3",		UFSPSCTRL3},
	{"UFSPSBEGIN4",		UFSPSBEGIN4},
	{"UFSPSLUN4",		UFSPSLUN4},
	{"UFSPSCTRL4",		UFSPSCTRL4},
	{"UFSPSBEGIN5",		UFSPSBEGIN5},
	{"UFSPSEND5",		UFSPSEND5},
	{"UFSPSLUN5",		UFSPSLUN5},
	{"UFSPSCTRL5",		UFSPSCTRL5},
	{"UFSPSBEGIN6",		UFSPSBEGIN6},
	{"UFSPSEND6",		UFSPSEND6},
	{"UFSPSLUN6",		UFSPSLUN6},
	{"UFSPSCTRL6",		UFSPSCTRL6},
	{"UFSPSBEGIN7",		UFSPSBEGIN7},
	{"UFSPSEND7",		UFSPSEND7},
	{"UFSPSLUN7",		UFSPSLUN7},
	{"UFSPSCTRL7",		UFSPSCTRL7},
};

#endif /* _UFS_EXYNOS_FMP_H_ */
