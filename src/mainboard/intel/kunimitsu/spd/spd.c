/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2014 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
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
 * Foundation, Inc.
 */

#include <arch/byteorder.h>
#include <cbfs.h>
#include <console/console.h>
#include <string.h>
#include <soc/gpio.h>
#include <soc/pei_data.h>
#include <soc/romstage.h>
#include <ec/google/chromeec/ec.h>
#include <mainboard/intel/kunimitsu/spd/spd.h>

static void mainboard_print_spd_info(uint8_t spd[])
{
	const int spd_banks[8] = {  8, 16, 32, 64, -1, -1, -1, -1 };
	const int spd_capmb[8] = {  1,  2,  4,  8, 16, 32, 64,  0 };
	const int spd_rows[8]  = { 12, 13, 14, 15, 16, -1, -1, -1 };
	const int spd_cols[8]  = {  9, 10, 11, 12, -1, -1, -1, -1 };
	const int spd_ranks[8] = {  1,  2,  3,  4, -1, -1, -1, -1 };
	const int spd_devw[8]  = {  4,  8, 16, 32, -1, -1, -1, -1 };
	const int spd_busw[8]  = {  8, 16, 32, 64, -1, -1, -1, -1 };
	char spd_name[SPD_PART_LEN+1] = { 0 };

	int banks = spd_banks[(spd[SPD_DENSITY_BANKS] >> 4) & 7];
	int capmb = spd_capmb[spd[SPD_DENSITY_BANKS] & 7] * 256;
	int rows  = spd_rows[(spd[SPD_ADDRESSING] >> 3) & 7];
	int cols  = spd_cols[spd[SPD_ADDRESSING] & 7];
	int ranks = spd_ranks[(spd[SPD_ORGANIZATION] >> 3) & 7];
	int devw  = spd_devw[spd[SPD_ORGANIZATION] & 7];
	int busw  = spd_busw[spd[SPD_BUS_DEV_WIDTH] & 7];

	/* Module type */
	printk(BIOS_INFO, "SPD: module type is ");
	switch (spd[SPD_DRAM_TYPE]) {
	case SPD_DRAM_DDR3:
		printk(BIOS_INFO, "DDR3\n");
		break;
	case SPD_DRAM_LPDDR3:
		printk(BIOS_INFO, "LPDDR3\n");
		break;
	default:
		printk(BIOS_INFO, "Unknown (%02x)\n", spd[SPD_DRAM_TYPE]);
		break;
	}

	/* Module Part Number */
	memcpy(spd_name, &spd[SPD_PART_OFF], SPD_PART_LEN);
	spd_name[SPD_PART_LEN] = 0;
	printk(BIOS_INFO, "SPD: module part is %s\n", spd_name);

	printk(BIOS_INFO,
		"SPD: banks %d, ranks %d, rows %d, columns %d, density %d Mb\n",
		banks, ranks, rows, cols, capmb);
	printk(BIOS_INFO, "SPD: device width %d bits, bus width %d bits\n",
		devw, busw);

	if (capmb > 0 && busw > 0 && devw > 0 && ranks > 0) {
		/* SIZE = DENSITY / 8 * BUS_WIDTH / SDRAM_WIDTH * RANKS */
		printk(BIOS_INFO, "SPD: module size is %u MB (per channel)\n",
		       capmb / 8 * busw / devw * ranks);
	}
}

/* Copy SPD data for on-board memory */
void mainboard_fill_spd_data(struct pei_data *pei_data)
{
	int spd_index;
	int spd_file_len;
	struct cbfs_file *spd_file;

	/* Load SPD data from CBFS */
	spd_file = cbfs_get_file(CBFS_DEFAULT_MEDIA, "spd.bin");
	if (!spd_file)
		die("SPD data not found.");
	spd_file_len = ntohl(spd_file->len);

	/* make sure we have at least one SPD in the file. */
	if (spd_file_len < SPD_LEN)
		die("Missing SPD data.");

	/* Add board SKU detection here.  Currently we only support one. */
	spd_index = 0;

	/* Make sure we did not overrun the buffer */
	if (spd_file_len < ((spd_index + 1) * SPD_LEN)) {
		printk(BIOS_ERR, "SPD index override to 0 - old hardware?\n");
		spd_index = 0;
	}

	/* Assume same memory in both channels */
	spd_index *= SPD_LEN;
	memcpy(pei_data->spd_data[0][0],
		((char *)CBFS_SUBHEADER(spd_file)) + spd_index, SPD_LEN);
	memcpy(pei_data->spd_data[1][0],
		((char *)CBFS_SUBHEADER(spd_file)) + spd_index, SPD_LEN);

	/* Make sure a valid SPD was found */
	if (pei_data->spd_data[0][0][0] == 0)
		die("Invalid SPD data.");

	mainboard_print_spd_info(pei_data->spd_data[0][0]);
}
