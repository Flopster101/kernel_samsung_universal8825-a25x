# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/s5e8825.dtb
dtbo-y += samsung/a25x/a25x_eur_open_w00_r00.dtbo
dtbo-y += samsung/a25x/a25x_eur_open_w00_r05.dtbo
dtbo-y += samsung/a25x/a25x_eur_open_w00_r02.dtbo
dtbo-y += samsung/a25x/a25x_eur_open_w00_r04.dtbo
dtbo-y += samsung/a25x/a25x_eur_open_w00_r01.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always-y  := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
