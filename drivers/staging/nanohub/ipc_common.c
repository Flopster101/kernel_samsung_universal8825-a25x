// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Common IPC Driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#include "ipc_common.h"

#define NAME_PREFIX "cipc"
#define EVT_WAIT_TIME (1)
#define WAIT_CHUB_MS (100)

static struct cipc_info cipc; /* local cipc */

enum lock_type {
	LOCK_ADD_EVT,
	LOCK_WT_DATA,
};

#include "chub.h"
#define CIPC_PRINT(fmt, ...) nanohub_info(fmt, ##__VA_ARGS__)
#define CIPC_PRINT_DEBUG(fmt, ...) nanohub_debug(fmt, ##__VA_ARGS__)
/* #define CIPC_PRINT(fmt, ...) cipc.user_func->print(fmt, ##__VA_ARGS__) */

static inline void CIPC_USER_MEMCPY(void *dst, void *src, int size, int dst_io,
				    int src_io)
{
	cipc.user_func->memcpy(dst, src, size, dst_io, src_io);
}

static inline void CIPC_USER_STRNCPY(char *dst, char *src, int size, int dst_io,
				     int src_io)
{
	cipc.user_func->strncpy(dst, src, size, dst_io, src_io);
}

static inline int CIPC_USER_STRNCMP(char *dst, char *src, int size)
{
	return cipc.user_func->strncmp(dst, src, size);
}

static inline void CIPC_USER_MSLEEP(unsigned int ms)
{
	cipc.user_func->mbusywait(ms);
}

static inline void CIPC_USER_MEMSET(void *buf, int val, int size, int io)
{
	cipc.user_func->memset(buf, val, size, io);
}

static inline void CIPC_USER_PUTLOCK(enum lock_type lock, unsigned long *flag)
{
	if ((lock == LOCK_ADD_EVT) && (cipc.user_func->putlock_evt))
		cipc.user_func->putlock_evt(flag);
	else if ((lock == LOCK_WT_DATA) && (cipc.user_func->putlock_data))
		cipc.user_func->putlock_data(flag);
}

static inline void CIPC_USER_GETLOCK(enum lock_type lock, unsigned long *flag)
{
	if ((lock == LOCK_ADD_EVT) && (cipc.user_func->putlock_evt))
		cipc.user_func->getlock_evt(flag);
	else if ((lock == LOCK_WT_DATA) && (cipc.user_func->putlock_data))
		cipc.user_func->getlock_data(flag);
}

void *cipc_get_base(enum cipc_region area)
{
	return cipc.cipc_addr[area].base;
}

unsigned int cipc_get_size(enum cipc_region area)
{
	return cipc.cipc_addr[area].size;
}

static unsigned int cipc_get_offset(enum cipc_region area)
{
	if (cipc.sram_base != cipc.cipc_addr[CIPC_REG_SRAM_BASE].base)
		CIPC_PRINT("%s error %p, %p\n", __func__, cipc.sram_base,
			   cipc.cipc_addr[CIPC_REG_SRAM_BASE].base);
	return (unsigned int)(cipc.cipc_addr[area].base -
			      cipc.cipc_addr[CIPC_REG_SRAM_BASE].base);
}

static unsigned int cipc_get_offset_addr(void *addr)
{
	return (unsigned int)(addr - cipc.cipc_addr[CIPC_REG_SRAM_BASE].base);
}

static void cipc_set_info(enum cipc_region area, void *base, int size,
			  char *name)
{
	/* update local cipc */
	cipc.cipc_addr[area].base = base;
	cipc.cipc_addr[area].size = size;
	CIPC_USER_STRNCPY(cipc.cipc_addr[area].name, name, IPC_NAME_MAX, 0, 0);
	CIPC_PRINT("%s: reg:%d, name:%s, off:+%x\n", __func__, area, name, cipc_get_offset(area));
}

static inline enum cipc_region user_to_reg(enum cipc_user_id id)
{
	switch (id) {
	case CIPC_USER_AP2CHUB:
		return CIPC_REG_AP2CHUB;
	case CIPC_USER_CHUB2AP:
		return CIPC_REG_CHUB2AP;
#ifdef CIPC_DEF_USER_ABOX
	case CIPC_USER_ABOX2CHUB:
		return CIPC_REG_ABOX2CHUB;
	case CIPC_USER_CHUB2ABOX:
		return CIPC_REG_CHUB2ABOX;
#endif
	default:
		CIPC_PRINT("%s: invalid id:%d\n", __func__, id);
		break;
	}
	return CIPC_REG_MAX;
}

static inline enum cipc_region data_to_evt(enum cipc_region reg)
{
	switch (reg) {
	case CIPC_REG_DATA_CHUB2AP_BATCH:
	case CIPC_REG_DATA_CHUB2AP:
		return CIPC_REG_EVT_CHUB2AP;
	case CIPC_REG_DATA_AP2CHUB:
		return CIPC_REG_EVT_AP2CHUB;
#ifdef CIPC_DEF_USER_ABOX
	case CIPC_REG_DATA_CHUB2ABOX:
		return CIPC_REG_EVT_CHUB2ABOX;
	case CIPC_REG_DATA_ABOX2CHUB_AUD:
	case CIPC_REG_DATA_ABOX2CHUB:
		return CIPC_REG_EVT_ABOX2CHUB;
#endif
	default:
		CIPC_PRINT("%s: invalid data reg:%d\n", __func__, reg);
		break;
	}
	return CIPC_REG_MAX;
}

static inline enum cipc_user_id reg_to_user(enum cipc_region reg)
{
	switch (reg) {
	case CIPC_REG_AP2CHUB:
	case CIPC_REG_EVT_AP2CHUB:
	case CIPC_REG_DATA_AP2CHUB:
		return CIPC_USER_AP2CHUB;
	case CIPC_REG_CHUB2AP:
	case CIPC_REG_EVT_CHUB2AP:
	case CIPC_REG_DATA_CHUB2AP:
	case CIPC_REG_DATA_CHUB2AP_BATCH:
		return CIPC_USER_CHUB2AP;
#ifdef CIPC_DEF_USER_ABOX
	case CIPC_REG_ABOX2CHUB:
	case CIPC_REG_EVT_ABOX2CHUB:
	case CIPC_REG_DATA_ABOX2CHUB:
	case CIPC_REG_DATA_ABOX2CHUB_AUD:
		return CIPC_USER_ABOX2CHUB;
	case CIPC_REG_CHUB2ABOX:
	case CIPC_REG_EVT_CHUB2ABOX:
	case CIPC_REG_DATA_CHUB2ABOX:
		return CIPC_USER_CHUB2ABOX;
		break;
#endif
	default:
		CIPC_PRINT("%s: invalid reg id:%d\n", __func__, reg);
		break;
	}
	return CIPC_USER_MAX;
}

/* get size of struct cipc_data_channel */
static inline int cipc_get_data_ch_size(int size, int cnt)
{
	return (size + sizeof(unsigned int)) * cnt;
}

static inline int is_valid_cipc_map(enum cipc_region reg)
{
	if (reg >= CIPC_REG_MAX) {
		CIPC_PRINT("%s: invalid reg:%d\n", __func__, reg);
		return CIPC_FALSE;
	}

	if (cipc.cipc_map->magic != CIPC_MAGIC) {
		CIPC_PRINT("%s: wrong magic, reg:%d, offset:+%x\n",
			   __func__, reg, cipc_get_offset_addr(cipc.cipc_map));
		return CIPC_FALSE;
	}
	return CIPC_TRUE;
}

static inline int is_evt(enum cipc_region reg)
{
	if (is_valid_cipc_map(reg)) {
		if ((reg == CIPC_REG_EVT_AP2CHUB)
		    || (reg == CIPC_REG_EVT_CHUB2AP)
#ifdef CIPC_DEF_USER_ABOX
		    || (reg == CIPC_REG_EVT_ABOX2CHUB)
		    || (reg == CIPC_REG_EVT_CHUB2ABOX)
#endif
        )
			return CIPC_TRUE;
		else
			return CIPC_FALSE;
	} else {
		CIPC_PRINT("%s: wrong is_valid_cipc_map, reg:%d\n", __func__,
			   reg);
	}
	return CIPC_FALSE;
}

#define MAX_TRY_CNT (5)

static inline int cipc_check_lock(void)
{
	if (cipc.lock) {
		int trycnt = 0;

		CIPC_PRINT("%s: wait for unlock:lock:%d, trycnt:%d\n",
			   __func__, cipc.lock, trycnt);
		do {
			CIPC_USER_MSLEEP(100);
		} while (cipc.lock && (trycnt++ < MAX_TRY_CNT));
		if (cipc.lock)
			return CIPC_ERR;
	}
	return 0;
}

void cipc_set_lock(int lock)
{
	CIPC_PRINT("%s: org:%d ->%d\n", __func__, cipc.lock, lock);
	cipc.lock = lock;
}

static inline int is_evt_valid(enum cipc_region reg)
{
	struct cipc_evt *evt;

	if (cipc_check_lock()) {
		CIPC_PRINT("%s: is locked\n", __func__);
		return CIPC_NULL;
	}

	if (is_evt(reg)) {
		evt = cipc_get_base(reg);
		if (evt->magic == CIPC_MAGIC_USER_EVT)
			return CIPC_TRUE;
		else
			CIPC_PRINT("%s: wrong magic:%x, +%x\n", __func__,
				   evt->magic, cipc_get_offset_addr(evt));
	}
	CIPC_PRINT("%s: reg:%d fails\n", __func__, reg);
	return CIPC_FALSE;
}

static inline int is_data(enum cipc_region reg)
{
	if (is_valid_cipc_map(reg)) {
		if ((reg == CIPC_REG_DATA_AP2CHUB) ||
		    (reg == CIPC_REG_DATA_CHUB2AP) ||
		    (reg == CIPC_REG_DATA_CHUB2AP_BATCH)
#ifdef CIPC_DEF_USER_ABOX
		    || (reg == CIPC_REG_DATA_ABOX2CHUB) ||
		    (reg == CIPC_REG_DATA_ABOX2CHUB_AUD) ||
		    (reg == CIPC_REG_DATA_CHUB2ABOX)
#endif
        )
			return CIPC_TRUE;
		else
			return CIPC_FALSE;
	} else {
		CIPC_PRINT("%s: wrong is_valid_cipc_map, reg:%d\n", __func__,
			   reg);
	}
	return CIPC_FALSE;
}

static inline int is_data_valid(enum cipc_region reg)
{
	struct cipc_data *data;

	if (cipc_check_lock()) {
		CIPC_PRINT("%s: is locked\n", __func__);
		return CIPC_NULL;
	}

	if (is_data(reg)) {
		data = cipc_get_base(reg);
		if (data->magic == CIPC_MAGIC_USER_DATA)
			return CIPC_TRUE;
		else
			CIPC_PRINT("%s: wrong magic:%x, +%x\n", __func__,
				   data->magic, cipc_get_offset_addr(data));
	} else {
		CIPC_PRINT("%s: wrong is_valid_cipc_map, reg:%d\n", __func__,
			   reg);
	}
	CIPC_PRINT("%s: reg:%d fails\n", __func__, reg);
	return CIPC_FALSE;
}

static inline int is_user(enum cipc_region reg)
{
	if (is_valid_cipc_map(reg)) {
		if ((reg == CIPC_REG_AP2CHUB) || (reg == CIPC_REG_CHUB2AP)
#ifdef CIPC_DEF_USER_ABOX
		    || (reg == CIPC_REG_ABOX2CHUB) || (reg == CIPC_REG_CHUB2ABOX)
#endif
        )
			return CIPC_TRUE;
		else
			return CIPC_FALSE;
	} else {
		CIPC_PRINT("%s: wrong is_valid_cipc_map, reg:%d\n", __func__,
			   reg);
	}
	return CIPC_FALSE;
}

static inline int is_user_valid(enum cipc_user_id id)
{
	struct cipc_user *user;
	enum cipc_region reg = user_to_reg(id);

	if (cipc_check_lock()) {
		CIPC_PRINT("%s: is locked\n", __func__);
		return CIPC_NULL;
	}

	if (is_user(reg)) {
		user = cipc_get_base(reg);
		if (user->magic == CIPC_MAGIC_USER)
			return CIPC_TRUE;
		else
			CIPC_PRINT("%s: wrong magic:%x, +%x\n", __func__,
				   user->magic, cipc_get_offset_addr(user));
	}
	CIPC_PRINT("%s: reg:%d fails\n", __func__, reg);
	return CIPC_FALSE;
}

static int is_cipc_valid(void)
{
	int i, j;
	enum cipc_region reg;
	struct cipc_user_info *user_info;

	for (i = 0; i < cipc.user_cnt; i++) {
		user_info = &cipc.user_info[i];
		CIPC_PRINT("%s:is_user_check(%s,data_cnt:%d)\n",
			   __func__, user_info->map_info.name,
			   user_info->map_info.data_pool_cnt);
		if (is_user_valid(i) == CIPC_FALSE)
			return CIPC_FALSE;

		reg = user_info->map_info.evt_reg;
		CIPC_PRINT("%s:is_evt_valid: reg:%d\n", __func__, reg);
		if (is_evt_valid(reg) == CIPC_FALSE)
			return CIPC_FALSE;

		CIPC_PRINT("%s:is_data_valid: data pool cnt: %d, reg:%d\n",
			__func__, user_info->map_info.data_pool_cnt, reg);
		for (j = 0; j < user_info->map_info.data_pool_cnt; j++) {
			reg = user_info->map_info.data_reg[j];
			CIPC_PRINT("%s:is_data_valid: reg:%d\n", __func__, reg);
			if (is_data_valid(reg) == CIPC_FALSE)
				return CIPC_FALSE;
		}
	}
	return CIPC_TRUE;
}

static void print_cipc_map(void)
{
	struct cipc_user_map_info *user_map_info = CIPC_NULL;
	struct cipc_user_map_info *pre_user_map_info = CIPC_NULL;
	struct cipc_user_map_info *cipc_user_ival =
		cipc.chub_bootargs->cipc_init_map + cipc.sram_base;
	struct cipc_user_info *user_info = CIPC_NULL;
	int i;
	int j;

	for (i = 0; i < cipc.user_cnt; i++, cipc_user_ival++) {
		user_map_info = &cipc.user_info[i].map_info;
		if ((cipc_user_ival->reg != user_map_info->reg) ||
		    (cipc_user_ival->evt_reg != user_map_info->evt_reg) ||
		    (cipc_user_ival->data_reg[0] != user_map_info->data_reg[0])) {
			CIPC_PRINT
			    ("%s: CIPC user map: error reg:%d, %d, %d -> %d, %d, %d \n",
			     __func__, user_map_info->reg,
			     user_map_info->evt_reg, user_map_info->data_reg[0],
			     cipc_user_ival->reg,
			     cipc_user_ival->evt_reg,
			     cipc_user_ival->data_reg[0]);
		}
	}

	/* step: debug log out */
	CIPC_PRINT(">>> CIPC map info(addr_info): magic:%x, user_cnt:%d\n",
		   cipc.cipc_map->magic, cipc.user_cnt);
	for (i = 0; i < CIPC_REG_MAX; i++)
		CIPC_PRINT("%s(+%x %d)\n", cipc.cipc_addr[i].name,
			   cipc_get_offset(i), cipc_get_size(i));

	CIPC_PRINT(">>> CIPC map info(user_info)\n");
	for (i = 0; i < cipc.user_cnt; i++) {
		user_info = &cipc.user_info[i];
		user_map_info = &user_info->map_info;

		CIPC_PRINT
		    ("user-id:%d: user_map_info:(%s, reg:%d,tx:%d,rx:%d,owner:%d)\n",
		     i, user_map_info->name, user_map_info->reg,
		     user_info->map_info.evt_reg, user_map_info->evt_reg,
		     user_map_info->owner);
		CIPC_PRINT("reg(%d) (+%x, %d(remain_size:%d))\n",
			   user_map_info->reg,
			   cipc_get_offset(user_map_info->reg),
			   cipc_get_size(user_map_info->reg),
			   cipc_get_size(user_map_info->reg) -
			   cipc_get_size(user_map_info->evt_reg) -
			   cipc_get_size(user_map_info->data_reg[0]) -
			   cipc_get_size(user_map_info->data_reg[1]));
		CIPC_PRINT("evt(%d) (+%x, %d)\n", user_map_info->evt_reg,
			   cipc_get_offset(user_map_info->evt_reg),
			   cipc_get_size(user_map_info->evt_reg));

		for (j = 0; j < user_map_info->data_pool_cnt; j++)
			CIPC_PRINT("data(%d) (+%x, %d)\n", user_map_info->data_reg[j],
			   cipc_get_offset(user_map_info->data_reg[j]),
			   cipc_get_size(user_map_info->data_reg[j]));

		if (pre_user_map_info) {
			if ((cipc_get_offset(pre_user_map_info->reg) +
			     cipc_get_size(pre_user_map_info->reg)) >
			    cipc_get_offset(user_map_info->reg))
				CIPC_PRINT
				    ("CIPC invalid map: pre_offset:+%x, pre_size:%d, > cur_offset:+%x\n",
				     cipc_get_offset(pre_user_map_info->reg),
				     cipc_get_size(pre_user_map_info->reg),
				     cipc_get_offset(user_map_info->reg));
		}
		pre_user_map_info = user_map_info;
	}
}

/* initialize cipc's users */
static int cipc_set_map(int master)
{
	struct cipc_user_map_info *cipc_user_ival;
	struct cipc_user_map_info local_cipc_user_ival;
	struct cipc_map_area *cipc_map = cipc.cipc_map;
	struct cipc_user *user_map;
	unsigned int data_pool_size[CIPC_USER_DATA_MAX];
	unsigned int data_pool_total;
	struct cipc_data *ipc_data = CIPC_NULL;
	enum cipc_user_id userid;
	struct cipc_user_info *user_info;
	int next_user_pool_off = 0;
	char *user_map_base;
	int j;
	int remain = 0;

	if (CIPC_USER_STRNCMP(cipc.chub_bootargs->magic, OS_UPDT_MAGIC, sizeof(OS_UPDT_MAGIC))) {
		CIPC_PRINT("%s: invalid chub_bootargs: init_map:+%x\n", __func__, cipc.chub_bootargs->cipc_init_map);
		return CIPC_ERR;
	}

	/* step: set user_map address */
	cipc_user_ival = cipc.chub_bootargs->cipc_init_map + cipc.sram_base;
	user_map_base = (char *)&cipc_map->user_pool[0];

	CIPC_PRINT("%s: master:%d, map_offset:+%x\n", __func__, master, cipc.chub_bootargs->cipc_init_map);
	for (userid = 0; userid < CIPC_USER_MAX; userid++, cipc_user_ival++) {
		/* set user_info from inital user info */
		if (cipc_user_ival->reg) {
			/* step: set first user map address == user_map_base */
			user_map =
			    (struct cipc_user *)(user_map_base + next_user_pool_off);

			/* step: set user_info into local */
			user_info = &cipc.user_info[userid];

			/* step: set user name into local */
			CIPC_USER_MEMCPY(&local_cipc_user_ival, cipc_user_ival, sizeof(struct cipc_user_map_info), 0, 1);
			CIPC_USER_MEMCPY(&user_info->map_info, &local_cipc_user_ival, sizeof(struct cipc_user_map_info), 1, 0);

			CIPC_PRINT
			    (">> %s (name:%s(id:%d,owner:%d)): user_info(reg:%d,%d,%d, mb:(src)%d,%d,(dst)%d,%d), data_pool_cnt:%d\n",
			     __func__, user_info->map_info.name, userid,
			     user_info->map_info.owner, user_info->map_info.reg,
			     user_info->map_info.evt_reg, user_info->map_info.evt_reg,
			     user_info->map_info.src.id, user_info->map_info.src.mb_id,
			     user_info->map_info.dst.id, user_info->map_info.dst.mb_id,
			     user_info->map_info.data_pool_cnt);

			/* step: set  the Reg"evt_reg" cipc addr into local cipc addr */
			cipc_set_info(user_info->map_info.evt_reg,
				      (void *)&user_map->evt, sizeof(struct cipc_evt), user_info->map_info.name);
			/* step: set magic for evt_reg into user_map */
			if (master)
				user_map->evt.magic = CIPC_MAGIC_USER_EVT;

			/* step: find data pool in user_map */
			data_pool_total = 0;
			for (j = 0; j < CIPC_USER_DATA_MAX; j++) {
				if (user_info->map_info.data_size[j]) {
					/* step: set ipc_data base! */
					ipc_data = (void *)(&user_map->data_pool[0] + data_pool_total);

					/* step:  Calc data1 pool size */
					data_pool_size[j] =
					    sizeof(struct cipc_data) +
					    cipc_get_data_ch_size(user_info->map_info.data_size[j],
								  user_info->map_info.data_cnt[j]);

					if (!user_info->map_info.data_reg[j]) {
						CIPC_PRINT("%s: %d: No reg error\n", __func__, j);
						return CIPC_ERR;
					}

					/* step: set  the Reg"data_reg" cipc addr into local cipc addr */
					cipc_set_info(user_info->map_info.data_reg[j], ipc_data,
						      data_pool_size[j], user_info->map_info.name);
					/* step: set magic for tx_data into user_map */
					if (master) {
						ipc_data->magic = CIPC_MAGIC_USER_DATA;
						CIPC_PRINT("%s3: data_pool(%d th) size:%d = size:%d,cnt:%d\n",
							   __func__, j, data_pool_size[j],
							   user_info->map_info.data_size[j], user_info->map_info.data_cnt[j]);

						/* step: set cnt, size into ipc_data map */
						ipc_data->ctrl.cnt = user_info->map_info.data_cnt[j];
						ipc_data->ctrl.size = user_info->map_info.data_size[j];
					}
					data_pool_total += data_pool_size[j];
				}
			}

			/* step: set  the Reg"reg" cipc addr into local cipc addr */
			cipc_set_info(user_info->map_info.reg, user_map,
				      sizeof(struct cipc_user) + data_pool_total, user_info->map_info.name);
			/* step: set user magic into user_map */
			if (master)
				user_map->magic = CIPC_MAGIC_USER;
#ifdef DEBUT_VER
			CIPC_PRINT
			    ("%s : cipc_map:+%x: reg:+%x(%d), tx: reg-evt:+%x(%d), reg-data:+%x(%d), reg-data2:+%x(%d)\n",
			     __func__, cipc_get_offset_addr(cipc_map),
			     cipc_get_offset(user_info->map_info.reg),
			     cipc_get_size(user_info->map_info.reg),
			     cipc_get_offset(user_info->map_info.evt_reg),
			     cipc_get_size(user_info->map_info.evt_reg),
			     cipc_get_offset(user_info->map_info.data_reg[0]),
			     cipc_get_size(user_info->map_info.data_reg[0]),
			     cipc_get_offset(user_info->map_info.data_reg[1]),
			     cipc_get_size(user_info->map_info.data_reg[1]));
#endif
			/* step: calc next user's offset */
			/* calc next user addr */
			next_user_pool_off +=
			    cipc.cipc_addr[user_info->map_info.reg].size;

			remain = 0;
			if (userid == CIPC_USER_CHUB2AP) {
				remain = ((cipc_get_offset_addr(user_map_base) + next_user_pool_off) % IPC_ALIGN_SIZE);
				if (remain)
					next_user_pool_off += (IPC_ALIGN_SIZE - remain);

				if (cipc_get_offset_addr(user_map_base + next_user_pool_off) % IPC_ALIGN_SIZE)
					CIPC_PRINT
					    ("%s: align_check(%d) error: base:+%x, remain:%d\n",
					    __func__,
					    cipc_get_offset_addr(user_map_base + next_user_pool_off),
					    cipc_get_offset_addr(user_map_base + next_user_pool_off) % IPC_ALIGN_SIZE);
				CIPC_PRINT
					("%s4: align_check on last user of each owner(%d), remain:%d cipc_map->next_user_pool_off:%d\n",
					 __func__, remain, next_user_pool_off);
			}
			CIPC_PRINT("%s5: %d user done: next offset:%d\n", __func__, userid, next_user_pool_off);
			cipc.user_cnt++;
		}
	}

	/* step: set cipc magic into cipc map */
	if (master) {
		cipc_map->magic = CIPC_MAGIC;
		print_cipc_map();
	}
	return 0;
}

int cipc_get_offset_owner(enum ipc_owner owner, unsigned int *start_off,
			int *size)
{
	int owner_size = 0;
	unsigned int onwer_start_off = 0;
	int i;
	struct cipc_user_info *user_info;
	struct cipc_user *user;

	/* step: find start addr of user and size: ABOX -> ABOX2CHUB + CHUB2ABOX */
	/* Find start addr for BAAW setting !!! */
	user_info = &cipc.user_info[0];

	CIPC_PRINT("%s enter: owner:%d\n", __func__, owner);
	for (i = 0; i < CIPC_USER_MAX; i++, user_info++) {
#ifdef DEBUT_VER
		CIPC_PRINT
		    ("%s loop(%d): (reg:%d,tx:%d,rx:%d, owner_id:%d), addr:+%x, size:%d\n",
		     __func__, i, user_info->map_info.reg,
		     user_info->map_info.evt_reg, user_info->map_info.data_reg[0],
		     user_info->map_info.owner, onwer_start_off, owner_size);
#endif
		if (user_info->map_info.reg != user_to_reg(i)) {
			CIPC_PRINT("%s invalid reg:%d, %d\n", __func__,
				   user_info->map_info.reg, user_to_reg(i));
			return CIPC_ERR;
		}
		if (user_info->map_info.owner == owner) {
			if (!onwer_start_off) {
				CIPC_PRINT("%s: update#1 start_off: +%x\n",
					   __func__,
					   cipc_get_offset(user_info->
							   map_info.reg));
				onwer_start_off =
				    cipc_get_offset(user_info->map_info.reg);
			}

			if (cipc_get_offset(user_info->map_info.reg) < onwer_start_off) {
				CIPC_PRINT
				    ("%s: new reg:%d is smaller than previous on start_off:+%x -> +%x\n", __func__,
				     onwer_start_off,
				     cipc_get_offset(user_info->map_info.reg));
				onwer_start_off =
				    cipc_get_offset(user_info->map_info.reg);
			}
			owner_size += cipc_get_size(user_info->map_info.reg);
			user = (void *)cipc_get_base(user_info->map_info.reg);
			CIPC_PRINT
			    ("%s loop(%d-%s) find: user-magic:%x, owner_id:%d, owner(addr:+%x, size:%d), cur-user(addr:+%x, size:%d)\n",
			     __func__, i, user_info->map_info.name, user->magic,
			     user_info->map_info.owner,
			     onwer_start_off, owner_size,
			     cipc_get_offset(user_info->map_info.reg), cipc_get_size(user_info->map_info.reg));
		}
	}

	if (onwer_start_off) {
		*start_off = onwer_start_off;
		*size = owner_size + (IPC_ALIGN_SIZE - owner_size % IPC_ALIGN_SIZE);

		CIPC_PRINT
		    ("%s done: magic(cipc:%x) owner_id:%d, addr:+%x(+%x), size:%d(%d)\n",
		     __func__, cipc.cipc_map->magic, owner, (int)*start_off, onwer_start_off, *size, owner_size);
		if ((owner != IPC_OWN_MASTER) && (owner != IPC_OWN_MASTER)) {
			if (*start_off % IPC_ALIGN_SIZE || (*size % IPC_ALIGN_SIZE))
				CIPC_PRINT("%s: err start_offset(+%x,remain:%d) or size(%d,remain:%d) isn't 4KB align.",
					__func__, *start_off, *start_off % IPC_ALIGN_SIZE, *size, *size % IPC_ALIGN_SIZE);
		}
	}  else {
		*start_off = 0;
		*size = 0;
		CIPC_PRINT("%s : owner:%d no start_off:+%x, size:%d\n",
			__func__, owner, *start_off, *size);
	}
	return 0;
}

void cipc_register_callback(client_isr_func cb)
{
	cipc.cb = cb;
}

static void cipc_clear_map(enum cipc_user_id tx, enum cipc_user_id rx)
{
	struct cipc_user_info *user_info;
	struct cipc_evt *evt;
	struct cipc_data *data;
	enum cipc_region reg;
	int j;
	int i;

	CIPC_PRINT("%s: tx user:%d, rx user:%d\n", __func__, tx, rx);
	user_info = &cipc.user_info[tx];
	for (i = 0; i < 2; i++) {
		CIPC_PRINT("%s: clear reg:%d, %d, %d\n",
			   __func__, user_info->map_info.evt_reg,
			   user_info->map_info.data_reg[0], user_info->map_info.data_reg[1]);
		reg = user_info->map_info.evt_reg;
		evt = cipc_get_base(reg);
		evt->ctrl.qctrl.dq = 0;
		evt->ctrl.qctrl.eq = 0;

		for (j = 0; j < user_info->map_info.data_pool_cnt; j++) {
			reg = user_info->map_info.data_reg[j];
			data = cipc_get_base(reg);
			if (data) {
				data->ctrl.qctrl.dq = 0;
				data->ctrl.qctrl.eq = 0;
			}
		}
		user_info = &cipc.user_info[rx];
	}
}

int cipc_register(void *mb_addr, enum cipc_user_id tx, enum cipc_user_id rx,
		  rx_isr_func isr, void *priv, unsigned int *start_offset,
		  int *size)
{
	enum ipc_owner my_owner_id = cipc.user_info[rx].map_info.owner;
	enum ipc_mb_id my_mb_id = cipc.user_info[rx].map_info.dst.mb_id;

	if (my_owner_id > IPC_OWN_MAX) {
		CIPC_PRINT("%s: fails invalid cipc\n", __func__);
		return CIPC_ERR;
	}

	if (cipc_check_lock()) {
		CIPC_PRINT("%s: is locked\n", __func__);
		return CIPC_NULL;
	}

	/* step: set local user info into local cipc */
	cipc.cb = CIPC_NULL;
	cipc.user_info[rx].mb_base = mb_addr;
	cipc.user_info[tx].mb_base = mb_addr;
	cipc.user_info[rx].rx_isr = isr;
	cipc.user_info[rx].priv = priv;
	cipc.user_info[tx].rx_isr = isr;
	cipc.user_info[tx].priv = priv;
	CIPC_PRINT("%s: owner:%d, mb_id:%d done: user:tx:%d, rx:%d\n",
		__func__, my_owner_id, my_mb_id, tx, rx);

	/* mailbox HW clear */
	if (my_mb_id == IPC_SRC_MB0) {
		ipc_hw_read_int_status_reg_all((void *)mb_addr, IPC_SRC_MB0);
		ipc_hw_read_int_status_reg_all((void *)mb_addr, IPC_DST_MB1);
		ipc_hw_set_mcuctrl((void *)mb_addr, 0x1);
	}
	/* clean up my ipc */
	if (my_owner_id == IPC_OWN_ABOX) {
		CIPC_PRINT("%s: owner:%d, clear my ipc\n", my_owner_id);
		ipc_hw_read_int_status_reg_all((void *)mb_addr, IPC_SRC_MB0);
		ipc_hw_read_int_status_reg_all((void *)mb_addr, IPC_DST_MB1);
		ipc_hw_set_mcuctrl((void *)mb_addr, 0x1);
		cipc_clear_map(tx, rx);
	}

	if ((my_owner_id != IPC_OWN_MASTER) && (my_owner_id != IPC_OWN_HOST) &&
		((*start_offset % IPC_ALIGN_SIZE) || (*size % IPC_ALIGN_SIZE))) {
		CIPC_PRINT("%s: owner:%d, fails get invalid addr:+%x, size:+%x\n",
			__func__, my_owner_id, start_offset, size);
		return CIPC_ERR;
	}

	/* step: check valid of cipc map */
	if (is_cipc_valid() != CIPC_TRUE) {
		CIPC_PRINT("%s: owner:%d, fails is_cipc_valid \n",
			__func__);
		return CIPC_ERR;
	}

	/* step: set 'cipc addr' into local cipc.  'cipc addr' is from cipc map. I'm the owner of rx */
	if (cipc_get_offset_owner(my_owner_id, start_offset, size)) {
		CIPC_PRINT("%s: fails to get cipc_get_offset_owner", __func__);
		return CIPC_ERR;
	}

	/* step: print out cipc_map */
	CIPC_PRINT("%s: owner:%d done: printout cipc local info: start_offset:+%x, size;%d\n",
		   __func__, my_owner_id, *start_offset, *size);

	return 0;
}

int cipc_reset_map(void)
{
	int i;
	int j;
	struct cipc_evt *evt;
	struct cipc_data *data;
	struct cipc_user *user;
	enum cipc_region reg;
	struct cipc_user_info *user_info;

	for (i = 0; i < cipc.user_cnt; i++) {
		user_info = &cipc.user_info[i];
		CIPC_PRINT("%s:(user:%d/%d, %s,data_cnt:%d)\n", __func__, i,
			   cipc.user_cnt, user_info->map_info.name,
			   user_info->map_info.data_pool_cnt);

		/* step: set user magic */
		reg = user_info->map_info.reg;
		CIPC_USER_MEMSET(cipc_get_base(reg), 0, cipc_get_size(reg), 1);
		user = cipc_get_base(reg);
		if (user)
			user->magic = CIPC_MAGIC_USER;

		/* step: set evt magic */
		reg = user_info->map_info.evt_reg;
		evt = cipc_get_base(reg);
		if (evt)
			evt->magic = CIPC_MAGIC_USER_EVT;

		/* step: set data magic */
		for (j = 0; j < user_info->map_info.data_pool_cnt; j++) {
			reg = user_info->map_info.data_reg[j];
			data = cipc_get_base(reg);
			if (data) {
				data->ctrl.size = user_info->map_info.data_size[j];
				data->ctrl.cnt = user_info->map_info.data_cnt[j];
				data->magic = CIPC_MAGIC_USER_DATA;
			}
		}
	}
	is_cipc_valid();
	return 0;
}

struct cipc_info *cipc_init(enum ipc_owner owner, void *sram_base,
			    struct cipc_funcs *funcs)
{
	unsigned int cipc_total_size = 0;
	int ret;
	int i;

	/* check input parameter */
	if (!funcs)
		return CIPC_NULL;

	/* step: set local cipc info */
	cipc.sram_base = sram_base;
	cipc.owner = owner;
	cipc.user_func = funcs;
	cipc.user_cnt = 0;

	if (cipc_check_lock()) {
		CIPC_PRINT("%s: is locked\n", __func__);
		return CIPC_NULL;
	}

	/*set cipc start */
	cipc.chub_bootargs = cipc.sram_base + MAP_INFO_OFFSET;
	cipc.cipc_map = cipc.sram_base + cipc.chub_bootargs->ipc_start + CIPC_START_OFFSET;

	/* step: set sram_base into local cipc addr */
	cipc_set_info(CIPC_REG_SRAM_BASE, sram_base, 0, "sram_base");
	cipc_set_info(CIPC_REG_IPC_BASE, cipc.sram_base + cipc.chub_bootargs->ipc_start,
		cipc.chub_bootargs->ipc_end - cipc.chub_bootargs->ipc_start, "ipc_base");

	/* step: set cipc_map into local cipc addr */
	ret = cipc_set_map(owner == IPC_OWN_MASTER);
	if (ret) {
		CIPC_PRINT("%s: fails to set map\n", __func__);
		return CIPC_NULL;
	}
	for (i = 0; i < CIPC_REG_MAX; i++) {
		if (is_user(i)) {
			cipc_total_size += cipc_get_size(i);
			CIPC_PRINT("%s: add user(%d) size:%d -> cipc_total_size:%d\n",
				__func__, i, cipc_get_size(i), cipc_total_size);
		}
	}

	CIPC_PRINT("%s: cipc ends:0x%x, ipc size:0x%x\n", __func__,
		   CIPC_START_OFFSET + cipc_total_size, ipc_get_size(IPC_REG_IPC));
	if (CIPC_START_OFFSET + cipc_total_size > ipc_get_size(IPC_REG_IPC)) {
		CIPC_PRINT("%s: cipc size exceeds ipc area!\n", __func__);
		return CIPC_NULL;
	}

	cipc_set_info(CIPC_REG_CIPC_BASE, cipc.cipc_map, cipc_total_size, "cipc_base");
	CIPC_PRINT("%s: owner:%d, sram_base:+%x, ipc_base:+%x, cipc_base:+%x, total_size:+%x\n",
		   __func__, owner, cipc_get_offset(CIPC_REG_SRAM_BASE), cipc_get_offset(CIPC_REG_IPC_BASE),
		   cipc_get_offset(CIPC_REG_CIPC_BASE), cipc_total_size);

	return &cipc;
}

/* evt functions */
enum {
	IPC_EVT_DQ,		/* empty */
	IPC_EVT_EQ,		/* fill */
};

static inline void *get_data_ch_addr(struct cipc_data *ipc_data, int ch_num)
{
	return (void *)(&ipc_data->ch_pool[0] + (ch_num * ipc_data->ctrl.size));
}

static inline int __ipc_queue_empty(struct cipc_queue_ctrl *qctrl)
{
	return (CIPC_RAW_READL(&qctrl->eq) == qctrl->dq);
}

static inline int __ipc_queue_full(struct cipc_queue_ctrl *qctrl,
				   unsigned int q_cnt)
{
	return (((qctrl->eq + 1) % q_cnt) == qctrl->dq);
}

static inline int __ipc_queue_index_check(struct cipc_queue_ctrl *qctrl,
					  unsigned int q_cnt)
{
	return ((qctrl->eq >= q_cnt) || (qctrl->dq >= q_cnt));
}

static inline int __ipc_wait_full(struct cipc_queue_ctrl *qctrl,
				  unsigned int q_cnt)
{
	volatile unsigned int pass = 0;
	int trycnt = 0;

	if (__ipc_queue_index_check(qctrl, q_cnt)) {
		CIPC_PRINT("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX,
			   __func__);
		return CIPC_ERR;
	}

	/* don't sleep on ap */
	do {
		pass = __ipc_queue_full(qctrl, q_cnt);
	} while (pass && (trycnt++ < MAX_TRY_CNT));

	if (pass) {
		CIPC_PRINT("%s:%s: fails full:0x%x\n", NAME_PREFIX, __func__,
			   pass);
		return CIPC_ERR;
	} else {
		CIPC_PRINT("%s:%s: ok full:0x%x, cnt:%d\n", NAME_PREFIX,
			   __func__, pass, trycnt);
		return 0;
	}
}

static inline int __ipc_queue_cnt(struct cipc_queue_ctrl *qctrl,
				  unsigned int q_cnt)
{
	if (qctrl->eq >= qctrl->dq)
		return qctrl->eq - qctrl->dq;
	else
		return qctrl->eq + (q_cnt - qctrl->dq);
}

int cipc_get_remain_qcnt(enum cipc_region reg)
{
	if (is_evt_valid(reg)) {
		struct cipc_evt *ipc_evt = cipc_get_base(reg);

		return __ipc_queue_cnt(&ipc_evt->ctrl.qctrl, IPC_EVT_NUM);
	} else if (is_data_valid(reg)) {
		struct cipc_data *ipc_data = cipc_get_base(reg);

		return __ipc_queue_cnt(&ipc_data->ctrl.qctrl,
				       ipc_data->ctrl.cnt);
	}
	CIPC_PRINT("%s: invalid reg:%d\n", __func__, reg);
	return 0;
}

static void cipc_evt_fail_dbg(struct cipc_evt *ipc_evt, enum cipc_region reg)
{
	struct cipc_user_info *user;
	enum cipc_user_id userid = reg_to_user(reg);

	if (is_user_valid(userid)) {
		user = &cipc.user_info[userid];

		CIPC_PRINT("%s: evt(0x%x) (empty:%d,full:%d,eq:%d,dq:%d). user:%d(%s), reg:%d, SR:0x%x\n",
			__func__, cipc_get_offset_addr(ipc_evt),
			__ipc_queue_empty(&ipc_evt->ctrl.qctrl),
			__ipc_queue_full(&ipc_evt->ctrl.qctrl, IPC_EVT_NUM),
			ipc_evt->ctrl.qctrl.eq, ipc_evt->ctrl.qctrl.dq,
			reg_to_user(reg), user->map_info.name,
			reg, ipc_hw_read_int_status_reg_all(user->mb_base, user->map_info.dst.mb_id));
	} else {
		CIPC_PRINT("%s: evt(0x%x) (eq:%d,dq:%d). user:%d, reg:%d\n",
			__func__, cipc_get_offset_addr(ipc_evt), ipc_evt->ctrl.qctrl.eq, ipc_evt->ctrl.qctrl.dq,
			reg_to_user(reg), reg);
	}
	cipc_dump(reg_to_user(reg));
}

#ifdef IPC_DBG_DUMP
#include "ipc_chub.h"
struct ipc_dbg_dump ipc_dbg;

enum ipc_dbg_caller {
	IPC_DBG_WT_IRQ,
	IPC_DBG_AP_SLEEP,
	IPC_DBG_AP_WAKE,
	IPC_DBG_RD_IRQ,
	IPC_DBG_WT_DATA,
	IPC_DBG_WT_EVT,
	IPC_DBG_RD_DATA,
	IPC_DBG_RD_EVT,
	IPC_DBG_APINT_SET,
	IPC_DBG_APINT_CLR,
};

struct ipc_dbg_dump_caller {
	enum ipc_dbg_caller caller;
	unsigned int req_irq;
	unsigned int evt;
};

#define IPC_DBG_CALL_MAX_IDX (48)
#define IPC_DBG_BUF_MAX_IDX (8)

struct ipc_dbg_dump {
	int caller_idx;
	int wt_buf_idx;
	int rd_buf_idx;
	char wt_buf[IPC_DBG_BUF_MAX_IDX][272];
	char rd_buf[IPC_DBG_BUF_MAX_IDX][272];
	struct ipc_dbg_dump_caller caller[IPC_DBG_CALL_MAX_IDX];
};

void ipc_dbg_put_dump(int caller, int val, char *buf, int length)
{
	if ((caller == IPC_DBG_AP_SLEEP) || (caller == IPC_DBG_AP_WAKE) ||
	    (ipc_get_ap_wake() == AP_SLEEP)) {
		int caller_idx = ipc_dbg.caller_idx++ % IPC_DBG_CALL_MAX_IDX;
		int buf_idx;

		ipc_dbg.caller[caller_idx].caller = caller;
		ipc_dbg.caller[caller_idx].req_irq = val;
		ipc_dbg.caller[caller_idx].evt = 0;
		if (caller == IPC_DBG_WT_DATA) {
			buf_idx = ipc_dbg.wt_buf_idx++ % IPC_DBG_CALL_MAX_IDX;
			CIPC_USER_MEMCPY(ipc_dbg.wt_buf[buf_idx], buf, length, 0, 0);
		} else if (caller == IPC_DBG_RD_DATA) {
			buf_idx = ipc_dbg.rd_buf_idx++ % IPC_DBG_CALL_MAX_IDX;
			CIPC_USER_MEMCPY(ipc_dbg.rd_buf[buf_idx], buf, length, 0, 0);
		} else {
			ipc_dbg.caller[caller_idx].evt = length;
		}
	}
}
#endif

struct cipc_evt_buf *cipc_get_evt(enum cipc_region reg)
{
	struct cipc_evt *ipc_evt;
	struct cipc_evt_buf *cur_evt = CIPC_NULL;
	int retried = 0;

	if (!is_evt_valid(reg)) {
		CIPC_PRINT("%s: invalid reg:%d\n", __func__, reg);
		return CIPC_NULL;
	}
	ipc_evt = cipc_get_base(reg);
	if (!ipc_evt) {
		CIPC_PRINT("%s: invalid ipc_evt\n", __func__);
		return CIPC_NULL;
	}

retry:
	if (__ipc_queue_index_check(&ipc_evt->ctrl.qctrl, IPC_EVT_NUM) || (retried > 1)) {
		CIPC_PRINT("%s: failed by ipc index corrupt. retried:%d\n", __func__, retried);
		cipc_evt_fail_dbg(ipc_evt, reg);
		return CIPC_NULL;
	}

	/* only called by isr CIPC_USER_PUTLOCK(); */
	if (!__ipc_queue_empty(&ipc_evt->ctrl.qctrl)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.qctrl.dq];
		ipc_evt->ctrl.qctrl.dq =
		    (ipc_evt->ctrl.qctrl.dq + 1) % IPC_EVT_NUM;
		if (cur_evt->status == IPC_EVT_EQ) {
			cur_evt->status = IPC_EVT_DQ;
		} else {
			CIPC_PRINT("%s: get empty evt. skip it: %d\n", __func__,
				   cur_evt->status);
			cur_evt->status = IPC_EVT_DQ;
			retried++;
			goto retry;
		}
	} else {
		CIPC_PRINT("%s: is empty. retry\n", __func__);
		retried++;
		goto retry;
	}

	return cur_evt;
}

int cipc_add_evt(enum cipc_region reg, unsigned int evt)
{
	struct cipc_evt *ipc_evt;
	struct cipc_evt_buf *cur_evt = CIPC_NULL;
	int ret = 0;
	unsigned long flag;
	unsigned int trycnt = 0;
	int i;
	int get_evt = 0;
	enum cipc_user_id userid = reg_to_user(reg);
	struct cipc_user_info *user;
	enum ipc_mb_id my_mb_id;
	enum ipc_mb_id opp_mb_id;
	int retried = 0;

	if (!is_evt(reg) || (userid == CIPC_USER_MAX)) {
		CIPC_PRINT("%s: invalid reg:%d, userid:%d\n", __func__, reg, userid);
		return CIPC_ERR;
	}
	ipc_evt = cipc_get_base(reg);
	if (!ipc_evt) {
		CIPC_PRINT("%s:invalid ipc_evt\n", __func__);
		return CIPC_ERR;
	}
	user = &cipc.user_info[userid];
	my_mb_id = user->map_info.src.mb_id;
	opp_mb_id = user->map_info.dst.mb_id;

retry:
	if (__ipc_queue_index_check(&ipc_evt->ctrl.qctrl, IPC_EVT_NUM) || (retried > 1)) {
		CIPC_PRINT("%s: failed by ipc index corrupt. retried:%d\n", __func__, retried);
		return CIPC_ERR;
	}
	CIPC_USER_GETLOCK(LOCK_ADD_EVT, &flag);
	if (!__ipc_queue_full(&ipc_evt->ctrl.qctrl, IPC_EVT_NUM)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.qctrl.eq];
		if (!cur_evt) {
			CIPC_PRINT("%s: invalid cur_evt\n", __func__);
			CIPC_USER_PUTLOCK(LOCK_ADD_EVT, &flag);
			return CIPC_ERR;
		}

		/* gen interrupt */
		do {
			for (i = 0; i < IRQ_NUM_EVT_END; i++) {
				if (!ipc_evt->ctrl.pending[i]) {
					if (!ipc_hw_read_int_status_reg
					    (user->mb_base, opp_mb_id, i)) {
						ipc_evt->ctrl.pending[i] = 1;
						/* set evt channel */
						cur_evt->mb_irq = i;
						cur_evt->evt = evt;
						cur_evt->status = IPC_EVT_EQ;
						__raw_writel((ipc_evt->ctrl.qctrl.eq + 1) % IPC_EVT_NUM,
							&ipc_evt->ctrl.qctrl.eq);
						ipc_hw_gen_interrupt(user->mb_base, opp_mb_id,
								     cur_evt->mb_irq);
						get_evt = 1;
						goto out;
					} else {
						CIPC_PRINT
						    ("%s: irq-num:%d, reg:%d, wrong pending:, sw-off, hw-on, irq_status:my:+%x(mb_id:%d),opp:+%x(mb_id:%d)\n",
						     __func__, reg, i,
						     ipc_hw_read_int_status_reg_all(user->mb_base, my_mb_id),
						     my_mb_id,
						     ipc_hw_read_int_status_reg_all(user->mb_base, opp_mb_id),
						     opp_mb_id);
						goto out;
					}
				}
			}
		} while (trycnt++ < MAX_TRY_CNT);
	} else {
		CIPC_USER_PUTLOCK(LOCK_ADD_EVT, &flag);
		ret = __ipc_wait_full(&ipc_evt->ctrl.qctrl, IPC_EVT_NUM);
		if (ret) {
			return CIPC_ERR;
		} else {
			retried++;
			goto retry;
		}
	}
out:
	if (trycnt > 2) {
		CIPC_PRINT("%s: ok pending wait (%d): irq:%d, evt:%d\n",
			   __func__, trycnt, cur_evt->mb_irq, cur_evt->evt);
	}
	if (!get_evt) {
		CIPC_PRINT
		    ("%s: fails pending wait (%d): irq:%d, evt:%d\n",
		     __func__, trycnt, cur_evt ? cur_evt->mb_irq : -1,
		     cur_evt ? cur_evt->evt : -1);
		cipc_evt_fail_dbg(ipc_evt, reg);
	}
	CIPC_USER_PUTLOCK(LOCK_ADD_EVT, &flag);
	if (!get_evt)
		ret = CIPC_ERR;
	return ret;
}

#define INVALID_CHANNEL (0xffff)

int cipc_write_data(enum cipc_region reg, void *tx, unsigned int length)
{
	int ret = 0;
	struct cipc_data *ipc_data;
	char *data_ch_base;
	struct cipc_data_channel *data_ch = CIPC_NULL;
	enum cipc_region evtq = data_to_evt(reg);
	int retried = 0;
	unsigned long flag;

	if (!is_data_valid(reg)) {
		CIPC_PRINT("%s: invalid reg:%d\n", __func__, reg);
		return CIPC_NULL;
	}
	ipc_data = cipc_get_base(reg);
	data_ch_base = &ipc_data->ch_pool[0];

	if (length <= ipc_data->ctrl.size) {
retry:
		if (__ipc_queue_index_check(&ipc_data->ctrl.qctrl, ipc_data->ctrl.cnt)
									|| (retried > 1)) {
			CIPC_PRINT("%s: failed by ipc index corrupt: retired:%d\n",
				   __func__, retried);
			return CIPC_ERR;
		}
		CIPC_USER_GETLOCK(LOCK_WT_DATA, &flag);

		if ((ipc_data->ctrl.cnt == 1) ||
			!__ipc_queue_full(&ipc_data->ctrl.qctrl, ipc_data->ctrl.cnt)) {
			data_ch =
			    (void *)(data_ch_base +
				     (ipc_data->ctrl.qctrl.eq *
				      cipc_get_data_ch_size(ipc_data->ctrl.size, 1)));
			ipc_data->ctrl.qctrl.eq =
			    (ipc_data->ctrl.qctrl.eq + 1) % ipc_data->ctrl.cnt;
			data_ch->size = length;
		} else {
			CIPC_USER_PUTLOCK(LOCK_WT_DATA, &flag);
			ret =
			    __ipc_wait_full((void *)&ipc_data->ctrl.qctrl,
					    ipc_data->ctrl.cnt);
			if (ret) {
				CIPC_PRINT("%s: fails by full: trycnt:%d\n",
					   __func__, retried);
				retried++;
			} else {
				retried = 0;
			}
			goto retry;
		}

		CIPC_USER_PUTLOCK(LOCK_WT_DATA, &flag);
		CIPC_USER_MEMCPY(data_ch->buf, tx, length, 1, 0);
		retried = 0;
add_evt_try:
		ret = cipc_add_evt(evtq, reg);
		if (ret) {
			CIPC_PRINT("%s: fails by add_evt. try:%d\n", __func__,
				   retried);
			if (!retried) {
				retried++;
				goto add_evt_try;
			} else {
				data_ch->size = INVALID_CHANNEL;
			}
		}
		return ret;
	} else {
		CIPC_PRINT
		    ("%s: fails invalid size:%d, ipc cnt is %d, size:%d\n",
		     __func__, length, ipc_data->ctrl.cnt, ipc_data->ctrl.size);
		return CIPC_ERR;
	}
}

void *cipc_read_data(enum cipc_region reg, unsigned int *len)
{
	struct cipc_data *ipc_data;
	char *data_ch_base;
	struct cipc_data_channel *data_ch;
	void *buf = CIPC_NULL;
	int retried = 0;

	if (!is_data_valid(reg)) {
		CIPC_PRINT("%s: invalid reg:%d\n", __func__, reg);
		return CIPC_NULL;
	}
	ipc_data = cipc_get_base(reg);
	data_ch_base = &ipc_data->ch_pool[0];

retry:
	if (__ipc_queue_index_check(&ipc_data->ctrl.qctrl, ipc_data->ctrl.cnt) || (retried > 1)) {
		CIPC_PRINT("%s: failed by ipc index corrupt\n", __func__);
		return CIPC_NULL;
	}

	if ((ipc_data->ctrl.cnt == 1) || !__ipc_queue_empty(&ipc_data->ctrl.qctrl)) {
		data_ch =
		    (void *)(data_ch_base +
			     (ipc_data->ctrl.qctrl.dq *
			      cipc_get_data_ch_size(ipc_data->ctrl.size, 1)));
		if (data_ch->size != (unsigned int)INVALID_CHANNEL) {
			*len = data_ch->size;
			buf = data_ch->buf;
			ipc_data->ctrl.qctrl.dq =
			    (ipc_data->ctrl.qctrl.dq + 1) % ipc_data->ctrl.cnt;
		} else {
			CIPC_PRINT("%s: get empty data. goto next\n", __func__);
			ipc_data->ctrl.qctrl.dq =
			    (ipc_data->ctrl.qctrl.dq + 1) % ipc_data->ctrl.cnt;
			retried++;
			goto retry;
		}
	}
	return buf;
}

static void ipc_print_evt(enum cipc_region reg)
{
	struct cipc_evt *ipc_evt = cipc_get_base(reg);
	int i;

	CIPC_PRINT_DEBUG("%s: evt(0x%x,%s): eq:%d dq:%d\n",
		   __func__, cipc_get_offset_addr(ipc_evt), ipc_evt->info.name,
		   ipc_evt->ctrl.qctrl.eq, ipc_evt->ctrl.qctrl.dq);

	for (i = 0; i < IRQ_HW_MAX; i++)
		if (ipc_evt->ctrl.pending[i])
			CIPC_PRINT_DEBUG("%s: irq:%d filled\n", __func__, i);

	for (i = 0; i < IPC_EVT_NUM; i++) {
		if (ipc_evt->data[i].status)
			CIPC_PRINT_DEBUG("%s: evt%d(evt:%d,irq:%d,f:%d)\n",
				   __func__, i, ipc_evt->data[i].evt,
				   ipc_evt->data[i].mb_irq,
				   ipc_evt->data[i].status);
	}
}

static void ipc_print_databuf(enum cipc_region reg)
{
	struct cipc_data *ipc_data = cipc_get_base(reg);
	struct cipc_data_channel *data_ch;
	int i;

	CIPC_PRINT_DEBUG("%s: data(0x%x,%s): eq:%d dq:%d, size:%d, cnt:%d\n",
		   __func__, cipc_get_offset_addr(ipc_data), ipc_data->info.name,
		   ipc_data->ctrl.qctrl.eq, ipc_data->ctrl.qctrl.dq,
		   ipc_data->ctrl.size, ipc_data->ctrl.cnt);

	if (__ipc_queue_cnt(&ipc_data->ctrl.qctrl, ipc_data->ctrl.cnt))
		for (i = 0; i < ipc_data->ctrl.cnt; i++) {
			data_ch = get_data_ch_addr(ipc_data, i);
			CIPC_PRINT_DEBUG("%s:ch%d(addr:%p, size:0x%x)\n", __func__, i,
				   data_ch, data_ch->size);
		}
}

static void cipc_dump_mailbox_sfr(void *base)
{
	CIPC_PRINT_DEBUG("MB_SFR : mcutrl - 0x%x\n",
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_MCUCTL));
	CIPC_PRINT_DEBUG("MB_SFR0: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTGR0),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTCR0),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTMR0),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTSR0),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTMSR0));
	CIPC_PRINT_DEBUG("MB_SFR1: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTGR1),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTCR1),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTMR1),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTSR1),
		   (unsigned int)CIPC_RAW_READL(base + REG_MAILBOX_INTMSR1));
}

void cipc_dump(enum cipc_user_id id)
{
	int i;
	enum cipc_region reg = user_to_reg(id);

	for (i = (reg + 1); i < (reg + 3); i++) {
		CIPC_PRINT_DEBUG("%s: reg:%d, is_evt:%d, is_data:%d, is_user:%d\n",
			__func__, i, is_evt(i), is_data(i), is_user(i));
		if (is_evt(i))
			ipc_print_evt(i);
		else if (is_data(i))
			ipc_print_databuf(i);
	}
	cipc_dump_mailbox_sfr(cipc.user_info[id].mb_base);
}

int cipc_irqhandler(enum cipc_user_id id, unsigned int status)
{
	struct cipc_user_info *user;
	int start_index;
	struct cipc_evt_buf *cur_evt;
	struct cipc_evt *cipc_evt;
	int irq_num;
	int evt;
	unsigned int org_status;

	if (is_user_valid(id) == CIPC_FALSE)
		return CIPC_ERR;

	user = &cipc.user_info[id];
	if (!is_evt_valid(user->map_info.evt_reg)) {
		CIPC_PRINT("%s: it's not a cipc evt\n", __func__);
		return CIPC_ERR;
	}

	start_index = ipc_hw_start_bit(user->map_info.dst.mb_id);
	cipc_evt = cipc_get_base(user->map_info.evt_reg);
	org_status = status;

	if (!status) {
		status =
		    ipc_hw_read_int_status_reg_all(user->mb_base,
						   user->map_info.dst.mb_id);
	}
	/* ipc interrupt handle */
	while (status) {
		cur_evt = cipc_get_evt(user->map_info.evt_reg);
		if (cur_evt) {
			evt = cur_evt->evt;
			irq_num = cur_evt->mb_irq + start_index;

			/* Check IRQ Validate */
			if (!cipc_evt->ctrl.pending[cur_evt->mb_irq]
			    || !(status & (1 << irq_num))) {
				CIPC_PRINT
				    ("%s: trigger-err(sw:%d,hw:%d), irq:%d(%d + %d), evt:%d, status:0x%x->0x%x(SR:0x%x)\n",
				     __func__,
				     cipc_evt->ctrl.pending[cur_evt->mb_irq],
				     (status & (1 << irq_num)), irq_num,
				     cur_evt->mb_irq, start_index, evt,
				     (unsigned int)org_status,
				     (unsigned int)status, (unsigned int)
				     ipc_hw_read_int_status_reg_all
				     (user->mb_base, user->map_info.dst.mb_id));
				cipc_evt->info.err_cnt++;
			}

			/* Handle IRQ */
			ipc_hw_clear_int_pend_reg(user->mb_base,
						  user->map_info.dst.mb_id,
						  irq_num);
			status &= ~(1 << irq_num);
			cipc_evt->ctrl.pending[cur_evt->mb_irq] = 0;
			user->rx_isr(evt, user->priv);
		} else {
			CIPC_PRINT
			    ("%s: eventq is empty, status:0x%x->0x%x(SR:0x%x)\n",
			     __func__, (unsigned int)org_status,
			     (unsigned int)status,
			     (unsigned int)
			     ipc_hw_read_int_status_reg_all(user->mb_base,
							    user->map_info.dst.mb_id));
			cipc_dump(id);
			/* no clear. handle next irq
			ipc_hw_clear_all_int_pend_reg(user->mb_base,
						      user->map_info.dst.mb_id); */
			cipc_evt->info.err_cnt++;
			return CIPC_ERR;
		}
	}
	return 0;
}

struct cipc_info *cipc_get_info(void)
{
	return &cipc;
}

#ifdef IPC_DEF_IPC_TEST
#define IPC_TEST_BUF_CNT (16)
struct ipc_test_buf {
	char magic[IPC_NAME_MAX];
	int start;
	int baaw_test;
	unsigned int buf[IPC_TEST_BUF_CNT];
};

#define IPC_TEST_MAGIC "IPC_LOOPBACK"
static struct ipc_test_buf ipc_testbuf = {
	"IPC_LOOPBACK", 0x1, 0x0,
	{0x1234, 0x4321, 0x1234, 0x4321,
	 0xbb00, 0xbb00, 0xbb00, 0xbb00,
	 0x4321, 0x1234, 0x4321, 0x1234,
	 0xabcd, 0xefff, 0x6452, 0x9876}
};

int cipc_loopback_test(int reg_val, int start)
{
	struct ipc_test_buf *buf;
	enum cipc_region reg_reply = 0;
	int i;
	int ret;
	unsigned int len;
	enum cipc_user_id user_id;
	int err = 0;
	struct ipc_test_buf local_ipc_testbuf;
	enum cipc_region reg = reg_val & ((1 << CIPC_TEST_BAAW_REQ_BIT) - 1);
	int baaw_test = reg_val >> CIPC_TEST_BAAW_REQ_BIT;

	CIPC_PRINT("%s: reg_val:%x, reg:%x, start:%d, baaw_t:%d\n",
		   __func__, reg_val, reg, start, baaw_test);

#ifdef CIPC_DEF_IPC_TEST_CHUB_ONLY_ABOX_TEST
if (reg == CIPC_REG_DATA_CHUB2ABOX) {
	CIPC_PRINT("%s: hack c2abox -> abox2chub\n", __func__, reg, start);
	reg = CIPC_REG_DATA_ABOX2CHUB;
}
#endif

	if (is_data(reg) != CIPC_TRUE) {
		CIPC_PRINT("%s: fails: reg:%d\n", __func__, reg);
		return 0;
	}

	if (start) {
		ipc_testbuf.start = 1;
		ipc_testbuf.baaw_test = baaw_test;

		ret = cipc_write_data(reg, &ipc_testbuf, sizeof(ipc_testbuf));
		if (ret)
			CIPC_PRINT("%s: fails to send data\n", __func__);
	} else {
		buf = cipc_read_data(reg, &len);
		CIPC_USER_MEMCPY(&local_ipc_testbuf, buf, len, 0, 1);
		CIPC_PRINT("%s: Read data: reg:%d %d bytes, buf:+%x, baaw_t:%d\n",
			   __func__, reg, len, cipc_get_offset_addr(buf),
			   local_ipc_testbuf.baaw_test);
		if (local_ipc_testbuf.baaw_test) {
			for (i = CIPC_REG_IPC_BASE; i >= CIPC_REG_SRAM_BASE; i--) {
				CIPC_PRINT("Baaw(reg:%d): WT '0xbb00' on unaccess: +%x(val:%x)\n",
					   i, cipc_get_offset(i), CIPC_RAW_READL(cipc_get_base(i)));
				CIPC_RAW_WRITEL(0xbb00, cipc_get_base(i));
				CIPC_PRINT("Baaw(reg:%d): Result: Base: +%x (val:%x)\n",
					   i, cipc_get_offset(i), CIPC_RAW_READL(cipc_get_base(i)));
			}
		}

		if (len != sizeof(ipc_testbuf)) {
			return CIPC_ERR;
		} else if (!CIPC_USER_STRNCMP(local_ipc_testbuf.magic, IPC_TEST_MAGIC, sizeof(IPC_TEST_MAGIC))) {
			CIPC_PRINT("%s: rcv data: reg:%d,magic:%s,start:%d\n",
				__func__, reg, local_ipc_testbuf.magic, local_ipc_testbuf.start);
			for (i = 0; i < IPC_TEST_BUF_CNT; i++) {
				if (local_ipc_testbuf.buf[i] != ipc_testbuf.buf[i]) {
					CIPC_PRINT("%s: loopback fails :%d, %d<->%d",
						__func__, i, local_ipc_testbuf.buf[i], ipc_testbuf.buf[i]);
					err++;
				}
			}
			CIPC_PRINT("[ %s: test result: err:%d ]\n", __func__, err);
#ifdef CIPC_DEF_IPC_TEST_CHUB_ONLY_ABOX_TEST
if (reg == CIPC_REG_DATA_ABOX2CHUB) {
	CIPC_PRINT("%s: hack return chub2abox\n", __func__);
	return 0;
}
#endif
			/* send reply */
			if (buf->start == 1) {
				user_id = reg_to_user(reg);
				switch (user_id) {
#ifdef CIPC_DEF_USER_ABOX
				case CIPC_USER_ABOX2CHUB:
					reg_reply = CIPC_REG_DATA_CHUB2ABOX;
					break;
				case CIPC_USER_CHUB2ABOX:
					reg_reply = CIPC_REG_DATA_ABOX2CHUB;
					break;
#endif
				case CIPC_USER_AP2CHUB:
					reg_reply = CIPC_REG_DATA_CHUB2AP_BATCH;
					break;
				case CIPC_USER_CHUB2AP:
					reg_reply = CIPC_REG_DATA_AP2CHUB;
					break;
				default:
					CIPC_PRINT("%s: not supprot reg:%d, user_id:%d\n", __func__, reg, user_id);
					break;
				}
				CIPC_PRINT("%s: send reply to reg: from:%d->to:%d\n", __func__, reg, reg_reply);
				if (reg_reply != CIPC_REG_SRAM_BASE) {
					ipc_testbuf.start = 0;
					cipc_write_data(reg_reply, &ipc_testbuf, sizeof(ipc_testbuf));
				}
			}
		} else {
			return CIPC_ERR;
		}
	}
	return 0;
}
#endif
