/*
 * Copyright 2019 dvanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __GFX_V10_0_H__
#define __GFX_V10_0_H__

struct gc_down_config {
	enum eval_mode_config       config_mode;
	bool                        change_adapter_info;
	u32                         wgps_per_sa;
	u32                         rbs_per_sa;
	u32                         se_count;
	u32                         sas_per_se;
	u32                         packers_per_sc;
	u32                         num_scs;
	bool                        write_shader_array_config;
	bool                        use_reset_lowest_vgt_fw;
	bool                        force_gl1_miss;
};

extern const struct amdgpu_ip_block_version gfx_v10_0_ip_block;

#endif
