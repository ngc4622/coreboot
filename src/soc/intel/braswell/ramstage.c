/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2013 Google Inc.
 * Copyright (C) 2015 Intel Corp.
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

#include <arch/cpu.h>
#include <arch/acpi.h>
#include <bootstate.h>
#include <cbmem.h>
#include <console/console.h>
#include <cpu/intel/microcode.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/msr.h>
#include <device/device.h>
#include <device/pci_def.h>
#include <device/pci_ops.h>
#include <fsp_util.h>
#include <romstage_handoff.h>
#include <soc/gpio.h>
#include <soc/lpc.h>
#include <soc/msr.h>
#include <soc/nvs.h>
#include <soc/pattrs.h>
#include <soc/pci_devs.h>
#include <soc/pm.h>
#include <soc/ramstage.h>
#include <stdlib.h>

#define SHOW_PATTRS 1

struct pattrs __global_pattrs;

static void detect_num_cpus(struct pattrs *attrs)
{
	int ecx = 0;

	while (1) {
		struct cpuid_result leaf_b;

		leaf_b = cpuid_ext(0xb, ecx);

		/*
		 * The SOC doesn't have hyperthreading so just determine the
		 * number of cores by from level type (ecx[15:8] == * 2).
		 */
		if ((leaf_b.ecx & 0xff00) == 0x0200) {
			attrs->num_cpus = leaf_b.ebx & 0xffff;
			break;
		}
		ecx++;
	}
}

static inline void fill_in_msr(msr_t *msr, int idx)
{
	*msr = rdmsr(idx);
	if (SHOW_PATTRS) {
		printk(BIOS_DEBUG, "msr(%x) = %08x%08x\n",
		       idx, msr->hi, msr->lo);
	}
}

static const char * const stepping_str[] = {
	"A0", "A1", "B0", "B1", "B2", "B3", "C0"
};

static void fill_in_pattrs(void)
{
	device_t dev;
	msr_t msr;
	struct pattrs *attrs = (struct pattrs *)pattrs_get();

	attrs->cpuid = cpuid_eax(1);
	dev = dev_find_slot(0, PCI_DEVFN(LPC_DEV, LPC_FUNC));
	attrs->revid = pci_read_config8(dev, REVID);
	/* The revision to stepping IDs have two values per metal stepping. */
	if (attrs->revid >= RID_C_STEPPING_START) {
		attrs->stepping = (attrs->revid - RID_C_STEPPING_START) / 2;
		attrs->stepping += STEP_C0;
	} else if (attrs->revid >= RID_B_STEPPING_START) {
		attrs->stepping = (attrs->revid - RID_B_STEPPING_START) / 2;
		attrs->stepping += STEP_B0;
	} else {
		attrs->stepping = (attrs->revid - RID_A_STEPPING_START) / 2;
		attrs->stepping += STEP_A0;
	}

	attrs->microcode_patch = intel_microcode_find();
	attrs->address_bits = cpuid_eax(0x80000008) & 0xff;
	detect_num_cpus(attrs);

	if (SHOW_PATTRS) {
		printk(BIOS_DEBUG, "Cpuid %08x cpus %d rid %02x step %s\n",
		       attrs->cpuid, attrs->num_cpus, attrs->revid,
		       (attrs->stepping >= ARRAY_SIZE(stepping_str)) ? "??" :
		       stepping_str[attrs->stepping]);
	}

	fill_in_msr(&attrs->platform_id, MSR_IA32_PLATFORM_ID);
	fill_in_msr(&attrs->platform_info, MSR_PLATFORM_INFO);

	/* Set IA core speed ratio and voltages */
	fill_in_msr(&msr, MSR_IACORE_RATIOS);
	attrs->iacore_ratios[IACORE_MIN] = msr.lo & 0x7f;
	attrs->iacore_ratios[IACORE_LFM] = (msr.lo >> 8) & 0x7f;
	attrs->iacore_ratios[IACORE_MAX] = (msr.lo >> 16) & 0x7f;
	fill_in_msr(&msr, MSR_IACORE_TURBO_RATIOS);
	attrs->iacore_ratios[IACORE_TURBO] = (msr.lo & 0xff); /* 1 core max */

	fill_in_msr(&msr, MSR_IACORE_VIDS);
	attrs->iacore_vids[IACORE_MIN] = msr.lo & 0x7f;
	attrs->iacore_vids[IACORE_LFM] = (msr.lo >> 8) & 0x7f;
	attrs->iacore_vids[IACORE_MAX] = (msr.lo >> 16) & 0x7f;
	fill_in_msr(&msr, MSR_IACORE_TURBO_VIDS);
	attrs->iacore_vids[IACORE_TURBO] = (msr.lo & 0xff); /* 1 core max */

	/* Set bus clock speed */
	attrs->bclk_khz = BUS_FREQ_KHZ;
}

void braswell_init_pre_device(struct soc_intel_braswell_config *config)
{
	struct soc_gpio_config *gpio_config;

	fill_in_pattrs();

	/* Allow for SSE instructions to be executed. */
	write_cr4(read_cr4() | CR4_OSFXSR | CR4_OSXMMEXCPT);

	/* Get GPIO initial states from mainboard */
	gpio_config = mainboard_get_gpios();
	setup_soc_gpios(gpio_config, config->enable_xdp_tap);
}

static void fsp_hang(void *arg)
{
	post_code(0x35);
	die("Hang_after_fsp_notify2!");
}

BOOT_STATE_INIT_ENTRIES(fsp_bscbs) = {
	BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, fsp_hang, NULL)
};
