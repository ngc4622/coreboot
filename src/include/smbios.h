#ifndef SMBIOS_H
#define SMBIOS_H

#include <types.h>

unsigned long smbios_write_tables(unsigned long start);
int smbios_add_string(char *start, const char *str);
int smbios_string_table_len(char *start);

/* Used by mainboard to add an on-board device */
int smbios_write_type41(unsigned long *current, int *handle,
			const char *name, u8 instance, u16 segment,
			u8 bus, u8 device, u8 function);

const char *smbios_mainboard_serial_number(void);
const char *smbios_mainboard_version(void);
#ifdef CONFIG_MAINBOARD_FAMILY
const char *smbios_mainboard_family(void);
#endif

#define BIOS_CHARACTERISTICS_PCI_SUPPORTED  (1 << 7)
#define BIOS_CHARACTERISTICS_PC_CARD  (1 << 8)
#define BIOS_CHARACTERISTICS_PNP  (1 << 9)
#define BIOS_CHARACTERISTICS_APM (1 << 10)
#define BIOS_CHARACTERISTICS_UPGRADEABLE      (1 << 11)
#define BIOS_CHARACTERISTICS_SHADOW           (1 << 12)
#define BIOS_CHARACTERISTICS_BOOT_FROM_CD     (1 << 15)
#define BIOS_CHARACTERISTICS_SELECTABLE_BOOT  (1 << 16)
#define BIOS_CHARACTERISTICS_BIOS_SOCKETED    (1 << 17)

#define BIOS_EXT1_CHARACTERISTICS_ACPI    (1 << 0)
#define BIOS_EXT2_CHARACTERISTICS_TARGET  (1 << 2)

typedef enum {
	MEMORY_BUS_WIDTH_8 = 0,
	MEMORY_BUS_WIDTH_16 = 1,
	MEMORY_BUS_WIDTH_32 = 2,
	MEMORY_BUS_WIDTH_64 = 3,
	MEMORY_BUS_WIDTH_128 = 4,
	MEMORY_BUS_WIDTH_256 = 5,
	MEMORY_BUS_WIDTH_512 = 6,
	MEMORY_BUS_WIDTH_1024 = 7,
	MEMORY_BUS_WIDTH_MAX = 7,
} smbios_memory_bus_width;

typedef enum {
	MEMORY_DEVICE_OTHER = 0x01,
	MEMORY_DEVICE_UNKNOWN = 0x02,
	MEMORY_DEVICE_DRAM = 0x03,
	MEMORY_DEVICE_EDRAM = 0x04,
	MEMORY_DEVICE_VRAM = 0x05,
	MEMORY_DEVICE_SRAM = 0x06,
	MEMORY_DEVICE_RAM = 0x07,
	MEMORY_DEVICE_ROM = 0x08,
	MEMORY_DEVICE_FLASH = 0x09,
	MEMORY_DEVICE_EEPROM = 0x0A,
	MEMORY_DEVICE_FEPROM = 0x0B,
	MEMORY_DEVICE_EPROM = 0x0C,
	MEMORY_DEVICE_CDRAM = 0x0D,
	MEMORY_DEVICE_3DRAM = 0x0E,
	MEMORY_DEVICE_SDRAM = 0x0F,
	MEMORY_DEVICE_SGRAM = 0x10,
	MEMORY_DEVICE_RDRAM = 0x11,
	MEMORY_DEVICE_DDR = 0x12,
	MEMORY_DEVICE_DDR2 = 0x13,
	MEMORY_DEVICE_DDR2_FB_DIMM = 0x14,
	MEMORY_DEVICE_DDR3 = 0x18,
	MEMORY_DEVICE_DBD2 = 0x19,
	MEMORY_DEVICE_DDR4 = 0x1A,
	MEMORY_DEVICE_LPDDR = 0x1B,
	MEMORY_DEVICE_LPDDR2 = 0x1C,
	MEMORY_DEVICE_LPDDR3 = 0x1D,
	MEMORY_DEVICE_LPDDR4 = 0x1E,
} smbios_memory_device_type;

typedef enum {
	MEMORY_FORMFACTOR_OTHER = 0x01,
	MEMORY_FORMFACTOR_UNKNOWN = 0x02,
	MEMORY_FORMFACTOR_SIMM = 0x03,
	MEMORY_FORMFACTOR_SIP = 0x04,
	MEMORY_FORMFACTOR_CHIP = 0x05,
	MEMORY_FORMFACTOR_DIP = 0x06,
	MEMORY_FORMFACTOR_ZIP = 0x07,
	MEMORY_FORMFACTOR_PROPRIETARY_CARD = 0x08,
	MEMORY_FORMFACTOR_DIMM = 0x09,
	MEMORY_FORMFACTOR_TSOP = 0x0a,
	MEMORY_FORMFACTOR_ROC = 0x0b,
	MEMORY_FORMFACTOR_RIMM = 0x0c,
	MEMORY_FORMFACTOR_SODIMM = 0x0d,
	MEMORY_FORMFACTOR_SRIMM = 0x0e,
	MEMORY_FORMFACTOR_FBDIMM = 0x0f,
} smbios_memory_form_factor;

#define SMBIOS_STATE_SAFE 3
typedef enum {
	SMBIOS_BIOS_INFORMATION=0,
	SMBIOS_SYSTEM_INFORMATION=1,
	SMBIOS_SYSTEM_ENCLOSURE=3,
	SMBIOS_PROCESSOR_INFORMATION=4,
	SMBIOS_CACHE_INFORMATION=7,
	SMBIOS_SYSTEM_SLOTS=9,
	SMBIOS_EVENT_LOG=15,
	SMBIOS_PHYS_MEMORY_ARRAY=16,
	SMBIOS_MEMORY_DEVICE=17,
	SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS=19,
	SMBIOS_SYSTEM_BOOT_INFORMATION=32,
	SMBIOS_ONBOARD_DEVICES_EXTENDED_INFORMATION=41,
	SMBIOS_END_OF_TABLE=127,
} smbios_struct_type_t;

struct smbios_entry {
	u8 anchor[4];
	u8 checksum;
	u8 length;
	u8 major_version;
	u8 minor_version;
	u16 max_struct_size;
	u8 entry_point_rev;
	u8 formwatted_area[5];
	u8 intermediate_anchor_string[5];
	u8 intermediate_checksum;
	u16 struct_table_length;
	u32 struct_table_address;
	u16 struct_count;
	u8 smbios_bcd_revision;
} __attribute__((packed));

struct smbios_type0 {
	u8 type;
	u8 length;
	u16 handle;
	u8 vendor;
	u8 bios_version;
	u16 bios_start_segment;
	u8 bios_release_date;
	u8 bios_rom_size;
	u64 bios_characteristics;
	u8 bios_characteristics_ext1;
	u8 bios_characteristics_ext2;
	u8 system_bios_major_release;
	u8 system_bios_minor_release;
	u8 ec_major_release;
	u8 ec_minor_release;
	char eos[2];
} __attribute__((packed));

struct smbios_type1 {
	u8 type;
	u8 length;
	u16 handle;
	u8 manufacturer;
	u8 product_name;
	u8 version;
	u8 serial_number;
	u8 uuid[16];
	u8 wakeup_type;
	u8 sku;
	u8 family;
	char eos[2];
} __attribute__((packed));

struct smbios_type3 {
	u8 type;
	u8 length;
	u16 handle;
	u8 manufacturer;
	u8 _type;
	u8 version;
	u8 serial_number;
	u8 asset_tag_number;
	u8 bootup_state;
	u8 power_supply_state;
	u8 thermal_state;
	u8 security_status;
	u32 oem_defined;
	u8 height;
	u8 number_of_power_cords;
	u8 element_count;
	u8 element_record_length;
	char eos[2];
} __attribute__((packed));

struct smbios_type4 {
	u8 type;
	u8 length;
	u16 handle;
	u8 socket_designation;
	u8 processor_type;
	u8 processor_family;
	u8 processor_manufacturer;
	u32 processor_id[2];
	u8 processor_version;
	u8 voltage;
	u16 external_clock;
	u16 max_speed;
	u16 current_speed;
	u8 status;
	u8 processor_upgrade;
	u16 l1_cache_handle;
	u16 l2_cache_handle;
	u16 l3_cache_handle;
	u8 serial_number;
	u8 asset_tag;
	u8 part_number;
	u8 core_count;
	u8 core_enabled;
	u8 thread_count;
	u16 processor_characteristics;
	u16 processor_family2;
	char eos[2];
} __attribute__((packed));

struct smbios_type15 {
	u8 type;
	u8 length;
	u16 handle;
	u16 area_length;
	u16 header_offset;
	u16 data_offset;
	u8 access_method;
	u8 log_status;
	u32 change_token;
	u32 address;
	u8 header_format;
	u8 log_type_descriptors;
	u8 log_type_descriptor_length;
	char eos[2];
} __attribute__((packed));

enum {
	SMBIOS_EVENTLOG_ACCESS_METHOD_IO8 = 0,
	SMBIOS_EVENTLOG_ACCESS_METHOD_IO8X2,
	SMBIOS_EVENTLOG_ACCESS_METHOD_IO16,
	SMBIOS_EVENTLOG_ACCESS_METHOD_MMIO32,
	SMBIOS_EVENTLOG_ACCESS_METHOD_GPNV,
};

enum {
	SMBIOS_EVENTLOG_STATUS_VALID = 1, /* Bit 0 */
	SMBIOS_EVENTLOG_STATUS_FULL  = 2, /* Bit 1 */
};

struct smbios_type16 {
	u8 type;
	u8 length;
	u16 handle;
	u8 location;
	u8 use;
	u8 memory_error_correction;
	u32 maximum_capacity;
	u16 memory_error_information_handle;
	u16 number_of_memory_devices;
	u64 extended_maximum_capacity;
	char eos[2];
} __attribute__((packed));

struct smbios_type17 {
	u8 type;
	u8 length;
	u16 handle;
	u16 phys_memory_array_handle;
	u16 memory_error_information_handle;
	u16 total_width;
	u16 data_width;
	u16 size;
	u8 form_factor;
	u8 device_set;
	u8 device_locator;
	u8 bank_locator;
	u8 memory_type;
	u16 type_detail;
	u16 speed;
	u8 manufacturer;
	u8 serial_number;
	u8 asset_tag;
	u8 part_number;
	u8 attributes;
	u16 extended_size;
	u16 clock_speed;

	char eos[2];
} __attribute__((packed));

struct smbios_type32 {
	u8 type;
	u8 length;
	u16 handle;
	u8 reserved[6];
	u8 boot_status;
	u8 eos[2];
} __attribute__((packed));

struct smbios_type38 {
	u8 type;
	u8 length;
	u16 handle;
	u8 interface_type;
	u8 ipmi_rev;
	u8 i2c_slave_addr;
	u8 nv_storage_addr;
	u64 base_address;
	u8 base_address_modifier;
	u8 irq;
} __attribute__((packed));

typedef enum {
	SMBIOS_DEVICE_TYPE_OTHER = 0x01,
	SMBIOS_DEVICE_TYPE_UNKNOWN,
	SMBIOS_DEVICE_TYPE_VIDEO,
	SMBIOS_DEVICE_TYPE_SCSI,
	SMBIOS_DEVICE_TYPE_ETHERNET,
	SMBIOS_DEVICE_TYPE_TOKEN_RING,
	SMBIOS_DEVICE_TYPE_SOUND,
	SMBIOS_DEVICE_TYPE_PATA,
	SMBIOS_DEVICE_TYPE_SATA,
	SMBIOS_DEVICE_TYPE_SAS,
} smbios_onboard_device_type;

struct smbios_type41 {
	u8 type;
	u8 length;
	u16 handle;
	u8 reference_designation;
	u8 device_type: 7;
	u8 device_status: 1;
	u8 device_type_instance;
	u16 segment_group_number;
	u8 bus_number;
	u8 function_number: 3;
	u8 device_number: 5;
	char eos[2];
} __attribute__((packed));

struct smbios_type127 {
	u8 type;
	u8 length;
	u16 handle;
	u8 eos[2];
} __attribute__((packed));

#endif
