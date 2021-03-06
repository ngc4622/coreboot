/*
 * This file is part of the coreboot project.
 *
 * Copyright 2015 Google Inc.
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

#include <cbmem.h>
#include <cbfs.h>
#include <console/console.h>
#include <fsp_util.h>
#include <lib.h>
#include <ramstage_cache.h>
#include <romstage_handoff.h>
#include <soc/intel/common/memmap.h>
#include <soc/intel/common/ramstage.h>
#include <soc/intel/common/util.h>
#include <timestamp.h>

/* SOC initialization after FSP silicon init */
__attribute__((weak)) void soc_after_silicon_init(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/*
 * SMM Memory Map:
 *
 * +--------------------------+ smm_region_size() ----.
 * |     FSP Cache            | CONFIG_FSP_CACHE_SIZE |
 * +--------------------------+                       |
 * |     SMM Ramstage Cache   |                       + CONFIG_SMM_RESERVED_SIZE
 * +--------------------------+  ---------------------'
 * |     SMM Code             |
 * +--------------------------+ smm_base
 *
 */

static void *smm_fsp_cache_base(size_t *size)
{
	long cache_size;
	u8 *cache_base;

	/* Determine the location of the FSP cache */
	cache_base = (u8 *)ramstage_cache_location(&cache_size);
	*size = CONFIG_FSP_CACHE_SIZE;
	return &cache_base[cache_size];
}

/* Display SMM memory map */
static void smm_memory_map(void)
{
	u8 *smm_base;
	size_t smm_bytes;
	size_t smm_code_bytes;
	u8 *fsp_cache;
	size_t fsp_cache_bytes;
	u8 *ramstage_cache;
	long ramstage_cache_bytes;
	u8 *smm_reserved;
	size_t smm_reserved_bytes;

	/* Locate the SMM regions */
	smm_region((void **)&smm_base, &smm_bytes);
	fsp_cache = smm_fsp_cache_base(&fsp_cache_bytes);
	ramstage_cache = (u8 *)ramstage_cache_location(&ramstage_cache_bytes);
	smm_code_bytes = ramstage_cache - smm_base;
	smm_reserved = fsp_cache + fsp_cache_bytes;
	smm_reserved_bytes = smm_bytes - fsp_cache_bytes - ramstage_cache_bytes
		- smm_code_bytes;

	/* Display the SMM regions */
	printk(BIOS_SPEW, "\nLocation          SMM Memory Map        Offset\n");
	if (smm_reserved_bytes) {
		printk(BIOS_SPEW, "0x%p +--------------------------+ 0x%08x\n",
			&smm_reserved[smm_reserved_bytes], smm_bytes);
		printk(BIOS_SPEW, "           |   Other reserved region  |\n");
	}
	printk(BIOS_SPEW, "0x%p +--------------------------+ 0x%08x\n",
		smm_reserved, smm_reserved - smm_base);
	printk(BIOS_SPEW, "           |   FSP binary cache       |\n");
	printk(BIOS_SPEW, "0x%p +--------------------------+ 0x%08x\n",
		fsp_cache, fsp_cache - smm_base);
	printk(BIOS_SPEW, "           |   ramstage cache         |\n");
	printk(BIOS_SPEW, "0x%p +--------------------------+ 0x%08x\n",
		ramstage_cache, ramstage_cache - smm_base);
	printk(BIOS_SPEW, "           |   SMM code               |\n");
	printk(BIOS_SPEW, "0x%p +--------------------------+ 0x%08x\n",
		smm_base, 0);
	printk(BIOS_ERR, "\nCONFIG_FSP_CACHE_SIZE: 0x%08x bytes\n\n",
		CONFIG_FSP_CACHE_SIZE);
}

struct smm_fsp_cache_header {
	void *start;
	size_t size;
	FSP_INFO_HEADER *fih;
};

/* SoC implementation for caching support code. */
static void soc_save_support_code(void *start, size_t size,
	FSP_INFO_HEADER *fih)
{
	u8 *fsp_cache;
	size_t fsp_cache_length;
	struct smm_fsp_cache_header *header;
	size_t smm_fsp_cache_length;

	if (IS_ENABLED(CONFIG_DISPLAY_SMM_MEMORY_MAP))
		smm_memory_map();

	/* Locate the FSP cache in SMM */
	fsp_cache = smm_fsp_cache_base(&smm_fsp_cache_length);

	/* Initialize the FSP cache header */
	header = (struct smm_fsp_cache_header *)fsp_cache;
	fsp_cache += sizeof(*header);
	header->start = start;
	header->size = size;
	header->fih = fih;

	/* Validate the CONFIG_FSP_CACHE_SIZE value */
	fsp_cache_length = sizeof(*header) + size;
	if (smm_fsp_cache_length < fsp_cache_length) {
		printk(BIOS_ERR, "CONFIG_FSP_CACHE_SIZE < 0x%08x bytes\n",
			fsp_cache_length);
		die("ERROR: Insufficent space to cache FSP binary!\n");
	}

	/* Copy the FSP binary into the SMM region for safe keeping */
	memcpy(fsp_cache, start, size);
}

/* SoC implementation for restoring support code after S3 resume. Returns
 * previously passed fih pointer from soc_save_support_code(). */
static FSP_INFO_HEADER *soc_restore_support_code(void)
{
	u8 *fsp_cache;
	struct smm_fsp_cache_header *header;
	size_t smm_fsp_cache_length;

	/* Locate the FSP cache in SMM */
	fsp_cache = smm_fsp_cache_base(&smm_fsp_cache_length);

	/* Get the FSP cache header */
	header = (struct smm_fsp_cache_header *)fsp_cache;
	fsp_cache += sizeof(*header);

	/* Copy the FSP binary from the SMM region back into RAM */
	memcpy(header->start, fsp_cache, header->size);

	/* Return the FSP_INFO_HEADER address */
	return header->fih;
}

static void fsp_run_silicon_init(struct romstage_handoff *handoff)
{
	FSP_INFO_HEADER *fsp_info_header;
	FSP_SILICON_INIT fsp_silicon_init;
	SILICON_INIT_UPD *original_params;
	SILICON_INIT_UPD silicon_init_params;
	EFI_STATUS status;
	UPD_DATA_REGION *upd_ptr;
	VPD_DATA_REGION *vpd_ptr;

	/* Find the FSP image */
	timestamp_add_now(TS_FSP_FIND_START);
	fsp_info_header = fsp_get_fih();
	timestamp_add_now(TS_FSP_FIND_END);

	if (fsp_info_header == NULL) {
		printk(BIOS_ERR, "FSP_INFO_HEADER not set!\n");
		return;
	}
	print_fsp_info(fsp_info_header);

	/* Initialize the UPD values */
	vpd_ptr = (VPD_DATA_REGION *)(fsp_info_header->CfgRegionOffset +
					fsp_info_header->ImageBase);
	printk(BIOS_DEBUG, "0x%p: VPD Data\n", vpd_ptr);
	upd_ptr = (UPD_DATA_REGION *)(vpd_ptr->PcdUpdRegionOffset +
					fsp_info_header->ImageBase);
	printk(BIOS_DEBUG, "0x%p: UPD Data\n", upd_ptr);
	original_params = (void *)((u8 *)upd_ptr +
		upd_ptr->SiliconInitUpdOffset);
	memcpy(&silicon_init_params, original_params,
		sizeof(silicon_init_params));
	timestamp_add_now(TS_FSP_GET_ORIGINAL_UPD_DATA);
	soc_silicon_init_params(&silicon_init_params);
	timestamp_add_now(TS_FSP_UPD_SOC_UPDATE);

	/* Locate VBT and pass to FSP GOP */
	if (IS_ENABLED(CONFIG_GOP_SUPPORT)) {
		load_vbt(handoff->s3_resume, &silicon_init_params);
		timestamp_add_now(TS_FSP_LOAD_VBT);
	}
	mainboard_silicon_init_params(&silicon_init_params);
	timestamp_add_now(TS_FSP_UPD_MAINBOARD_UPDATE);

	/* Display the UPD data */
	if (IS_ENABLED(CONFIG_DISPLAY_UPD_DATA))
		soc_display_silicon_init_params(original_params,
			&silicon_init_params);

	/* Perform silicon initialization after RAM is configured */
	fsp_silicon_init = (FSP_SILICON_INIT)(fsp_info_header->ImageBase
		+ fsp_info_header->FspSiliconInitEntryOffset);
	timestamp_add_now(TS_FSP_SILICON_INIT_START);
	printk(BIOS_DEBUG, "Calling FspSiliconInit(0x%p) at 0x%p\n",
		&silicon_init_params, fsp_silicon_init);
	status = fsp_silicon_init(&silicon_init_params);
	timestamp_add_now(TS_FSP_SILICON_INIT_END);
	printk(BIOS_DEBUG, "FspSiliconInit returned 0x%08x\n", status);

#if IS_ENABLED(CONFIG_DISPLAY_HOBS)
	/* Verify the HOBs */
	const EFI_GUID graphics_info_guid = EFI_PEI_GRAPHICS_INFO_HOB_GUID;
	void *hob_list_ptr = get_hob_list();
	int missing_hob = 0;

	if (hob_list_ptr == NULL)
		die("ERROR - HOB pointer is NULL!\n");
	print_hob_type_structure(0, hob_list_ptr);

	/*
	 * Verify that FSP is generating the required HOBs:
	 *	7.1: FSP_BOOTLOADER_TEMP_MEMORY_HOB only produced for FSP 1.0
	 *	7.2: FSP_RESERVED_MEMORY_RESOURCE_HOB verified by raminit
	 *	7.3: FSP_NON_VOLATILE_STORAGE_HOB verified by raminit
	 *	7.4: FSP_BOOTLOADER_TOLUM_HOB verified by raminit
	 *	7.5: EFI_PEI_GRAPHICS_INFO_HOB verified below,
	 *	     if the ImageAttribute bit is set
	 *	FSP_SMBIOS_MEMORY_INFO HOB verified by raminit
	 */
	if ((fsp_info_header->ImageAttribute & GRAPHICS_SUPPORT_BIT) &&
		!get_next_guid_hob(&graphics_info_guid, hob_list_ptr)) {
		printk(BIOS_ERR, "7.5: EFI_PEI_GRAPHICS_INFO_HOB missing!\n");
		missing_hob = 1;
	}
	if (missing_hob)
		die("ERROR - Missing one or more required FSP HOBs!\n");
#endif

	soc_display_mtrrs();
	soc_after_silicon_init();
}

static void fsp_cache_save(void)
{
	const struct cbmem_entry *fsp_entry;
	FSP_INFO_HEADER *fih;

	fsp_entry = cbmem_entry_find(CBMEM_ID_REFCODE);

	if (fsp_entry == NULL) {
		printk(BIOS_ERR, "ERROR: FSP not found in CBMEM.\n");
		return;
	}

	fih = fsp_get_fih();

	if (fih == NULL) {
		printk(BIOS_ERR, "ERROR: No FIH found.\n");
		return;
	}

	soc_save_support_code(cbmem_entry_start(fsp_entry),
				cbmem_entry_size(fsp_entry), fih);
}

static int fsp_find_and_relocate(void)
{
	struct cbfs_file *file;
	void *fih;

	file = cbfs_get_file(CBFS_DEFAULT_MEDIA, "fsp.bin");

	if (file == NULL) {
		printk(BIOS_ERR, "Couldn't find fsp.bin in CBFS.\n");
		return -1;
	}
	timestamp_add_now(TS_FSP_RW_FOUND);

	fih = fsp_relocate(CBFS_SUBHEADER(file), ntohl(file->len));
	timestamp_add_now(TS_FSP_RELOCATED_REMAINING_ITEMS);

	fsp_update_fih(fih);

	return 0;
}

void intel_silicon_init(void)
{
	struct romstage_handoff *handoff;

	handoff = cbmem_find(CBMEM_ID_ROMSTAGE_INFO);

	if (handoff != NULL && handoff->s3_resume) {
		printk(BIOS_DEBUG, "FSP: Loading binary from cache\n");
		timestamp_add_now(TS_FSP_COPIED_FROM_CACHE_TO_MEMORY);
		fsp_update_fih(soc_restore_support_code());
		timestamp_add_now(TS_FSP_UPDATED_FIH_POINTER);
	} else {
		timestamp_add_now(TS_FSP_START_RELOCATION);
		fsp_find_and_relocate();
		timestamp_add_now(TS_FSP_UPDATED_FIH_POINTER);
		printk(BIOS_DEBUG, "FSP: Saving binary in cache\n");
		fsp_cache_save();
		timestamp_add_now(TS_FSP_COPIED_FROM_MEMORY_TO_CACHE);
	}

	fsp_run_silicon_init(handoff);
}

/* Initialize the UPD parameters for SiliconInit */
__attribute__((weak)) void mainboard_silicon_init_params(
	SILICON_INIT_UPD *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
};

/* Display the UPD parameters for SiliconInit */
__attribute__((weak)) void soc_display_silicon_init_params(
	const SILICON_INIT_UPD *old, SILICON_INIT_UPD *new)
{
	printk(BIOS_SPEW, "UPD values for SiliconInit:\n");
	hexdump32(BIOS_SPEW, new, sizeof(*new));
}

/* Initialize the UPD parameters for SiliconInit */
__attribute__((weak)) void soc_silicon_init_params(SILICON_INIT_UPD *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}
