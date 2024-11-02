/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* uses */
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"

/* Implements */
#include "mifmboxman.h"

int mifmboxman_init(struct mifmboxman *mbox)
{
	if (mbox->in_use)
		return -EBUSY;

	mutex_init(&mbox->lock);
	mbox->mbox_free = MIFMBOX_NUM;
	mbox->in_use = true;
	bitmap_zero(mbox->bitmap, MIFMBOX_NUM);

	return 0;
}

bool mifmboxman_alloc_mboxes(struct mifmboxman *mbox, int n, int *first_mbox_index)
{
	unsigned int index = 0;
	unsigned int available;
	u8           i;

	mutex_lock(&mbox->lock);

	if ((n > MIFMBOX_NUM) || (n == 0) || !mbox->in_use)
		goto error;

	while (index <= (MIFMBOX_NUM - n)) {
		available = 0;

		/* Search consecutive blocks */
		for (i = 0; i < n; i++) {
			if (test_bit((i + index), mbox->bitmap))
				break;
			available++;
		}

		if (available == n) {
			*first_mbox_index = index;

			for (i = 0; i < n; i++)
				set_bit(index++, mbox->bitmap);

			mbox->mbox_free -= n;
			goto exit;
		} else
			index = index + available + 1;
	}
error:
	SCSC_TAG_ERR(MIF, "Error allocating mbox\n");
	mutex_unlock(&mbox->lock);
	return false;
exit:
	mutex_unlock(&mbox->lock);
	return true;

}

void mifmboxman_free_mboxes(struct mifmboxman *mbox, int first_mbox_index, int n)
{
	int index = 0;
	int total_free = 0;

	mutex_lock(&mbox->lock);
	if ((n > MIFMBOX_NUM) ||
	    ((n + first_mbox_index) > MIFMBOX_NUM) ||
	    (n == 0) ||
	    !mbox->in_use)
		goto error;

	for (index = first_mbox_index; index < (first_mbox_index + n); index++)
		if (test_bit(index, mbox->bitmap)) {
			clear_bit(index, mbox->bitmap);
			total_free++;
		}

	mbox->mbox_free += total_free;
	mutex_unlock(&mbox->lock);
	return;
error:
	SCSC_TAG_ERR(MIF, "Error freeing mbox\n");
	mutex_unlock(&mbox->lock);
}

/**TOREFACTOR**/
u32 *mifmboxman_get_mbox_ptr(struct mifmboxman *mbox,  struct scsc_mif_abs *mif_abs, int mbox_index)
{
	/* Avoid unused parameter error */
	(void)mbox;

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	return mif_abs->get_mbox_ptr(mif_abs, mbox_index, SCSC_MIF_ABS_TARGET_WLAN);
#else
	return mif_abs->get_mbox_ptr(mif_abs, mbox_index);
#endif
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
u32 *mifmboxman_get_mbox_ptr_wpan(struct mifmboxman *mbox,  struct scsc_mif_abs *mif_abs, int mbox_index)
{
	/* Avoid unused parameter error */
	(void)mbox;

	return mif_abs->get_mbox_ptr(mif_abs, mbox_index, SCSC_MIF_ABS_TARGET_WPAN);
}
#endif

#if defined(CONFIG_WLBT_DCXO_TUNE)
int mifmboxman_set_dcxo_tune_value(struct scsc_mif_abs *mif_abs, u32 value)
{
	int ret;
	if (!mif_abs) {
		SCSC_TAG_ERR(MX_PROC, "Argument error\n");
		return -EINVAL;
	}

	ret = mif_abs->irq_register_mbox_apm(mif_abs);
	if (ret) {
		SCSC_TAG_ERR(MX_PROC, "error to register APM MAILBOX\n");
		return -ECOMM;
	}

	mif_abs->send_dcxo_cmd(mif_abs, MIFMBOX_APM_OP_SET_TUNE, value);

	ret = mif_abs->check_dcxo_ack(mif_abs, MIFMBOX_APM_OP_SET_TUNE, NULL);

	mif_abs->irq_unregister_mbox_apm(mif_abs);
	return ret;
}

int mifmboxman_get_dcxo_tune_value(struct scsc_mif_abs *mif_abs, u32* value)
{
	int ret;
	if (!mif_abs) {
		SCSC_TAG_ERR(MX_PROC, "Argument error\n");
		return -EINVAL;
	}

	ret = mif_abs->irq_register_mbox_apm(mif_abs);
	if (ret) {
		SCSC_TAG_ERR(MX_PROC, "error to register APM MAILBOX\n");
		return -ECOMM;
	}

	mif_abs->send_dcxo_cmd(mif_abs, MIFMBOX_APM_OP_GET_TUNE, 0);

	ret = mif_abs->check_dcxo_ack(mif_abs, MIFMBOX_APM_OP_GET_TUNE, value);
	if (ret) {
		SCSC_TAG_ERR(MX_PROC, "Failure to get DCXO Tune(cause: %d)\n", ret);
	} else {
		SCSC_TAG_INFO(MX_PROC, "Succeed to get DCXO Tune, and read value: 0x%x\n", *value);
	}

	mif_abs->irq_unregister_mbox_apm(mif_abs);
	return ret;
}
#endif

int mifmboxman_deinit(struct mifmboxman *mbox)
{
	mutex_lock(&mbox->lock);
	if (!mbox->in_use) {
		mutex_unlock(&mbox->lock);
		return -ENODEV;
	}
	mbox->in_use = false;
	mutex_unlock(&mbox->lock);
	return 0;
}
