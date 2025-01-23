# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_EXYNOS) += s5e8825.dtb

ifeq ($(CONFIG_ARCH_EXYNOS),y)
dtbo-y += samsung/a25x/a25x_swa_open_w00_r00.dtbo
dtbo-y += samsung/a25x/a25x_swa_open_w00_r04.dtbo
dtbo-y += samsung/a25x/a25x_swa_open_w00_r05.dtbo

dtbo-y += samsung/a33x/a33x_eur_open_w00_r00.dtbo
dtbo-y += samsung/a33x/a33x_eur_open_w00_r01.dtbo
dtbo-y += samsung/a33x/a33x_eur_open_w00_r02.dtbo
dtbo-y += samsung/a33x/a33x_eur_open_w00_r03.dtbo
dtbo-y += samsung/a33x/a33x_eur_open_w00_r04.dtbo

dtbo-y += samsung/a33x/a33x_ltn_open_w00_r00.dtbo
dtbo-y += samsung/a33x/a33x_ltn_open_w00_r01.dtbo
dtbo-y += samsung/a33x/a33x_ltn_open_w00_r02.dtbo
dtbo-y += samsung/a33x/a33x_ltn_open_w00_r03.dtbo
dtbo-y += samsung/a33x/a33x_ltn_open_w00_r04.dtbo

dtbo-y += samsung/a53x/a53x_eur_open_w00_r00.dtbo
dtbo-y += samsung/a53x/a53x_eur_open_w00_r01.dtbo
dtbo-y += samsung/a53x/a53x_eur_open_w00_r02.dtbo
dtbo-y += samsung/a53x/a53x_eur_open_w00_r05.dtbo
dtbo-y += samsung/a53x/a53x_eur_open_w00_r06.dtbo

dtbo-y += samsung/gta4xls/gta4xls_eur_open_w00_r00.dtbo
dtbo-y += samsung/gta4xls/gta4xls_eur_open_w00_r02.dtbo
dtbo-y += samsung/gta4xls/gta4xls_eur_open_w00_r03.dtbo

dtbo-y += samsung/gta4xlswifi/gta4xlswifi_chn_open_w00_r00.dtbo
dtbo-y += samsung/gta4xlswifi/gta4xlswifi_chn_open_w00_r02.dtbo
dtbo-y += samsung/gta4xlswifi/gta4xlswifi_chn_open_w00_r03.dtbo
dtbo-y += samsung/gta4xlswifi/gta4xlswifi_eur_open_w00_r00.dtbo
dtbo-y += samsung/gta4xlswifi/gta4xlswifi_eur_open_w00_r02.dtbo
dtbo-y += samsung/gta4xlswifi/gta4xlswifi_eur_open_w00_r03.dtbo

dtbo-y += samsung/m33x/m33x_eur_open_w00_r00.dtbo
dtbo-y += samsung/m33x/m33x_eur_open_w00_r01.dtbo
dtbo-y += samsung/m33x/m33x_eur_open_w00_r02.dtbo

dtbo-y += samsung/m34x/m34x_eur_open_w00_r00.dtbo
dtbo-y += samsung/m34x/m34x_eur_open_w00_r01.dtbo
dtbo-y += samsung/m34x/m34x_swa_ins_w00_r00.dtbo
dtbo-y += samsung/m34x/m34x_swa_ins_w00_r01.dtbo
endif

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always-y    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
