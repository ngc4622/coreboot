/*
 * This file is part of the coreboot project.
 *
 * Copyright 2014 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __SOC_NVIDIA_TEGRA132_FUNIT_CFG_H
#define __SOC_NVIDIA_TEGRA132_FUNIT_CFG_H

#include <stdint.h>
#include <soc/nvidia/tegra132/pinmux.h>
#include <soc/padconfig.h>
#include <soc/clock.h>

#define FUNIT_INDEX(_name)  FUNIT_##_name

enum {
	FUNIT_NONE = 0,
	FUNIT_INDEX(SBC1),
	FUNIT_INDEX(SBC4),
	FUNIT_INDEX(I2C2),
	FUNIT_INDEX(I2C3),
	FUNIT_INDEX(I2C5),
	FUNIT_INDEX(SDMMC3),
	FUNIT_INDEX(SDMMC4),
	FUNIT_INDEX_MAX,
};

struct funit_cfg {
	uint32_t funit_index;
	uint32_t clk_src_id;
	uint32_t clk_dev_freq_khz;
	struct pad_config const* pad_cfg;
	size_t pad_cfg_size;
};

#define FUNIT_CFG(_funit,_clk_src,_clk_freq,_cfg,_cfg_size)		\
	{								\
		.funit_index = FUNIT_INDEX(_funit),			\
		.clk_src_id = _clk_src,					\
		.clk_dev_freq_khz = _clk_freq,				\
		.pad_cfg = _cfg,					\
		.pad_cfg_size = _cfg_size,				\
	}

/*
 * Configure the funits associated with entry according to the configuration.
 */
void soc_configure_funits(const struct funit_cfg * const entries, size_t num);

#endif /* __SOC_NVIDIA_TEGRA132_FUNIT_CFG_H */
