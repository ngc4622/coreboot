/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2012 Google Inc.
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

#include <fmap.h>
#include <types.h>
#include <string.h>
#include <arch/io.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <southbridge/intel/bd82x6x/pch.h>
#include <vendorcode/google/chromeos/cros_vpd.h>
#include "onboard.h"

static unsigned char get_hex_digit(u8 *offset)
{
	unsigned char retval = 0;

	retval = *offset - '0';
	if (retval > 0x09) {
		retval = *offset - 'A' + 0x0A;
		if (retval > 0x0F)
			retval = *offset - 'a' + 0x0a;
	}
	if (retval > 0x0F) {
		printk(BIOS_DEBUG, "Error: Invalid Hex digit found: %c - 0x%02x\n",
			*offset, *offset);
		retval = 0;
	}

	return retval;
}

static int get_mac_address(u32 *high_dword, u32 *low_dword)
{
	char key[] = "ethernet_mac";
	char buf[20];  /* hh:hh:hh:hh:ll:ll */
	u8 *value = (u8 *)buf;
	int i;

	if (!cros_vpd_gets(key, buf, sizeof(buf))) {
		printk(BIOS_DEBUG,
		       "Error: Could not locate '%s' in VPD\n", key);
		return 0;
	}
	printk(BIOS_DEBUG, "Located '%s' in VPD\n", key);

	*high_dword = 0;

	/* Fetch the MAC address and put the octets in the correct order to
	 * be programmed.
	 *
	 * From RTL8105E_Series_EEPROM-Less_App_Note_1.1
	 * If the MAC address is 001122334455h:
	 * Write 33221100h to I/O register offset 0x00 via double word access
	 * Write 00005544h to I/O register offset 0x04 via double word access
	 */

	for (i = 0; i < 4; i++) {
		*high_dword |= (get_hex_digit(value++) << (4 + (i * 8)));
		*high_dword |= (get_hex_digit(value++) << (i * 8));
		value++;  /* skip ':' */
	}

	*low_dword = 0;
	for (i = 0; i < 2; i++) {
		*low_dword |= (get_hex_digit(value++) << (4 + (i * 8)));
		*low_dword |= (get_hex_digit(value++) << (i * 8));
		value++;  /* skip ':' */
	}

	return *high_dword | *low_dword;
}

static void program_mac_address(u16 io_base)
{
	/* Default MAC Address of A0:00:BA:D0:0B:AD */
	u32 high_dword = 0xD0BA00A0;	/* high dword of mac address */
	u32 low_dword = 0x0000AD0B;	/* low word of mac address as a dword */

	get_mac_address(&high_dword, &low_dword);

	if (io_base) {
		printk(BIOS_DEBUG, "Realtek NIC io_base = 0x%04x\n", io_base);
		printk(BIOS_DEBUG, "Programming MAC Address\n");

		/* Disable register protection */
		outb(0xc0, io_base + 0x50);
		outl(high_dword, io_base);
		outl(low_dword, io_base + 0x04);
		outb(0x60, io_base + 54);
		/* Enable register protection again */
		outb(0x00, io_base + 0x50);
	}
}

void lan_init(void)
{
	u16 io_base = 0;
	struct device *ethernet_dev = NULL;

	/* Get NIC's IO base address */
	ethernet_dev = dev_find_device(BELTINO_NIC_VENDOR_ID,
				       BELTINO_NIC_DEVICE_ID, 0);
	if (ethernet_dev != NULL) {
		io_base = pci_read_config16(ethernet_dev, 0x10) & 0xfffe;

		/*
		 * Battery life time - LAN PCIe should enter ASPM L1 to save
		 * power when LAN connection is idle.
		 * enable CLKREQ: LAN pci config space 0x81h=01
		 */
		pci_write_config8(ethernet_dev, 0x81, 0x01);
	}

	if (io_base) {
		/* Program MAC address based on VPD data */
		program_mac_address(io_base);

		/*
		 * Program NIC LEDS
		 *
		 * RTL8105E Series EEPROM-Less Application Note,
		 * Section 5.6 LED Mode Configuration
		 *
		 * Step1: Write C0h to I/O register 0x50 via byte access to
		 *        disable 'register protection'
		 * Step2: Write xx001111b to I/O register 0x52 via byte access
		 *        (bit7 is LEDS1 and bit6 is LEDS0)
		 * Step3: Write 0x00 to I/O register 0x50 via byte access to
		 *        enable 'register protection'
		 */
		outb(0xc0, io_base + 0x50);	/* Disable protection */
		outb((BELTINO_NIC_LED_MODE << 6) | 0x0f, io_base + 0x52);
		outb(0x00, io_base + 0x50);	/* Enable register protection */
	}
}
