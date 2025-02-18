#include "include/pmucal_chub.h"
#include "include/pmucal_rae.h"

/**
 *  pmucal_chub_on - chub block power on.
 *		        exposed to PWRCAL interface.

 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_chub_on(void)
{
	int ret;

	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);
	ret = pmucal_rae_handle_seq(pmucal_chub_list.on, pmucal_chub_list.num_on);

	return ret;
}

/**
 *  pmucal_chub_init - init chub.
 *		        exposed to PWRCAL interface.

 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_chub_init(void)
{
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	return 0;
}

/**
 *  pmucal_chub_reset_assert - reset assert chub.
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_chub_reset_assert(void)
{
	int ret;

	if (!pmucal_chub_list.reset_assert) {
		pr_err("%s there is no sequence element for chub-reset_assert.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_seq(pmucal_chub_list.reset_assert,
				pmucal_chub_list.num_reset_assert);
	if (ret) {
		pr_err("%s %s: error on handling chub-reset_assert sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	pr_info("%s done\n", __func__);
	return 0;
}

/**
 *  pmucal_chub_reset_release_config - reset_release_config chub except CHUB CPU reset
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_chub_reset_release_config(void)
{
	int ret;

	if (!pmucal_chub_list.reset_release_config) {
		pr_err("%s there is no sequence element for chub-reset_release_config.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_seq(pmucal_chub_list.reset_release_config,
				pmucal_chub_list.num_reset_release_config);
	if (ret) {
		pr_err("%s %s: error on handling chub-reset_release_config sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	pr_info("%s done\n", __func__);
	return 0;
}
int pmucal_chub_reset_release(void)
{
	int ret;

	if (!pmucal_chub_list.reset_release) {
		pr_err("%s there is no sequence element for chub-reset_release.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_seq(pmucal_chub_list.reset_release,
				pmucal_chub_list.num_reset_release);
	if (ret) {
		pr_err("%s %s: error on handling chub_reset_release sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	pr_info("%s done\n", __func__);
	return 0;

}
int pmucal_is_chub_regs(int reg)
{
	int i;
	int is_chub_regs = 0;

	for (i = 0; i < pmucal_chub_list.num_init; i++) {
		if (reg == pmucal_chub_list.init[i].base_pa + pmucal_chub_list.init[i].offset) {
			is_chub_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_chub_list.num_reset_assert; i++) {
		if (reg == pmucal_chub_list.reset_assert[i].base_pa + pmucal_chub_list.reset_assert[i].offset) {
			is_chub_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_chub_list.num_reset_release; i++) {
		if (reg == pmucal_chub_list.reset_release[i].base_pa + pmucal_chub_list.reset_release[i].offset) {
			is_chub_regs = 1;
			goto out;
		}
	}

out:
	return is_chub_regs;
}

/**
 *  pmucal_chub_initialize - Initialize function of PMUCAL CHUB common logic.
 *		            exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_chub_initialize(void)
{
	int ret = 0;

	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_chub_list_size) {
		pr_err("%s %s: there is no chub list. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		return -ENOENT;
	}

	/* convert physical base address to virtual addr */
	ret = pmucal_rae_phy2virt(pmucal_chub_list.init,
				pmucal_chub_list.num_init);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_chub_list.status,
				pmucal_chub_list.num_status);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting status...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_chub_list.reset_assert,
				pmucal_chub_list.num_reset_assert);

//investigating virtual address of assrtion

	pr_info("%s %p\n", PMUCAL_PREFIX, pmucal_chub_list.reset_assert[0].base_va);

	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_assert...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_chub_list.reset_release_config,
				pmucal_chub_list.num_reset_release_config);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_release_config...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_chub_list.reset_release,
				pmucal_chub_list.num_reset_release);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_release...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}
out:
	return ret;
}
