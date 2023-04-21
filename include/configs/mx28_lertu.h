/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2014 Londelec UK Ltd.
 */

#ifndef __CONFIGS_LERTUIMX28_H__
#define __CONFIGS_LERTUIMX28_H__

/* Memory configuration */
#define PHYS_SDRAM_1			0x40000000	/* Base address */
#define PHYS_SDRAM_1_SIZE		0x20000000	/* Max 512 MB RAM */
#define CFG_SYS_SDRAM_BASE		PHYS_SDRAM_1

/* SPL Power */
#define CFG_SPL_MXS_NO_VDD5V_SOURCE			/* Disable 4P2 Linear Regulator */

/* Environment */
#define DEFAULT_ETHADDR		c0:e5:4e:02:00:00
#define DEFAULT_ETH1ADDR		c0:e5:4e:02:00:01

/* Extra Environment */
#define CFG_EXTRA_ENV_SETTINGS \
	"fdt_addr=0x41000000\0" \
	"fdtname=imx28-lertu\0" \
	"bootscript=le-uboot.scr\0" \
	"keepprefix=.keep_\0" \
	"leuenvver=30\0" \
	"envsize=" __stringify(CONFIG_ENV_SIZE) "\0" \
	"update_nand_full_filename=le-uboot-img.nand\0" \
	"update_nand_fw_filename=le-uboot.sb\0" \
	"update_nand_firmware_maxsz=0x100000\0"	\
	"update_nand_stride=0x40\0"	/* MX28 datasheet ch. 12.12 */ \
	"update_nand_count=0x4\0"	/* MX28 datasheet ch. 12.12 */ \
	"update_nand_get_fcb_size="	/* Get size of FCB blocks */ \
		"nand device 0 ; " \
		"nand info ; " \
		"setexpr fcb_sz ${update_nand_stride} * ${update_nand_count};" \
		"setexpr update_nand_fcb ${fcb_sz} * ${nand_writesize}\0" \
	"update_nand_firmware_full=" /* Update FCB, DBBT and FW */ \
		"run update_nand_get_fcb_size ; " \
		"nand scrub -y 0x0 ${filesize} ; " \
		"nand write.raw ${loadaddr} 0x0 ${fcb_sz} ; " \
		"setexpr update_off ${loadaddr} + ${update_nand_fcb} ; " \
		"setexpr update_sz ${filesize} - ${update_nand_fcb} ; " \
		"nand write ${update_off} ${update_nand_fcb} ${update_sz}\0" \
	"update_nand_firmware="		/* Update only firmware */ \
		"run update_nand_get_fcb_size ; " \
		"setexpr fcb_sz ${update_nand_fcb} * 2 ; " /* FCB + DBBT */ \
		"setexpr fw_sz ${update_nand_firmware_maxsz} * 2 ; " \
		"setexpr fw_off ${fcb_sz} + ${update_nand_firmware_maxsz};" \
		"nand erase ${fcb_sz} ${fw_sz} ; " \
		"nand write ${loadaddr} ${fcb_sz} ${filesize} ; " \
		"nand write ${loadaddr} ${fw_off} ${filesize}\0" \
	"ethmac_denx_get=" \
		"echo Loading environment from NAND 0x800000 (mtdpart=248m(data)) ...; " \
		"nand read ${loadaddr} 0x800000 ${envsize}; " \
		"env import -c ${loadaddr} ${envsize}; " \
		"if env exists ethaddr; then; " \
		"else " /* Most likely incorrect CRC (values updated manually) or non-redundant env (flags byte missing) */ \
			"echo Trying to find ethaddr in NAND (mtdpart=248m(data)); " \
			"if ms.b ${loadaddr} 0x100 0x65 0x74 0x68 0x61 0x64 0x64 0x72 0x3d; then " \
				"env import -b ${memaddr} 0x1a; " \
			"fi; " \
			"if ms.b ${loadaddr} 0x100 0x65 0x74 0x68 0x31 0x61 0x64 0x64 0x72 0x3d; then " \
				"env import -b ${memaddr} 0x1b; " \
			"fi; " \
		"fi\0" \
	"ethmac_check=" \
		"if env exists ethaddr; then; " \
		"else " \
			"run ethmac_denx_get; " \
			"if env exists ethaddr; then; " \
			"else " \
				"setenv ethaddr " __stringify(DEFAULT_ETHADDR) "; " \
				"echo Warning: using default ethaddr=${ethaddr}; " \
			"fi; " \
		"fi; " \
		"if env exists eth1addr; then; " \
		"else " \
			"setenv eth1addr " __stringify(DEFAULT_ETH1ADDR) "; " \
			"echo Warning: using default eth1addr=${eth1addr}; " \
		"fi\0" \
	"cat_fdt_name=setenv fdtfile ${fdtname}-${boardversion}.dtb\0" \
	"miscargs=noinitrd nohlt panic=1\0" \
	"mmcdev=0\0" \
	"mmcpart=2\0" \
	"mmcroot=/dev/mmcblk0p3 rw rootwait\0" \
	"mmc_args=setenv bootargs ${bootargs} root=${mmcroot} ${miscargs} ubootver=\"\\\\\"${ver}\\\\\"\"\0" \
	"mmc_load_fdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdtfile}\0" \
	"mmc_load_image=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${bootfile}\0" \
	"mmc_load_update=" \
		"if fatsize mmc ${mmcdev}:${mmcpart} ${update_filename}; then " \
			"if itest ${filesize} > 0; then " \
				"if fatload mmc ${mmcdev}:${mmcpart} ${update_ramaddr} ${update_filename}; then " \
					"exit 0;" \
				"fi; " \
			"else " \
				"echo Error: ${update_filename} size is too small ${filesize}; " \
			"fi; " \
		"fi; " \
		"exit 1\0" \
	"mmc_cleanup_update=" \
		"if fatsize mmc ${mmcdev}:${mmcpart} ${keepprefix}${update_filename}; then; " \
		"else " \
			"echo Deleting ${update_filename}; " \
			"fatrm mmc ${mmcdev}:${mmcpart} ${update_filename}; " \
		"fi\0" \
	"try_update_nand=" \
		"setenv update_ramaddr ${loadaddr}; " \
		"setenv update_filename ${update_nand_full_filename}; " \
		"if run mmc_load_update; then " \
			"echo Updating U-boot in NAND (mtdpart=3m(u-boot)) with ${update_nand_full_filename}; " \
			"run update_nand_firmware_full; " \
			"run mmc_cleanup_update; " \
		"else " \
			"setenv update_filename ${update_nand_fw_filename}; " \
			"if run mmc_load_update; then " \
				"echo Updating U-boot in NAND (mtdpart=3m(u-boot)) with ${update_nand_fw_filename}; " \
				"run update_nand_firmware; " \
				"run mmc_cleanup_update; " \
			"fi; " \
		"fi\0" \
	"try_bootscript=" \
		"setenv update_ramaddr ${fdt_addr}; " \
		"setenv update_filename ${bootscript}; " \
		"if run mmc_load_update; then " \
			"echo Running ${bootscript} from mmc ...; " \
			"source ${fdt_addr}; " \
			"run mmc_cleanup_update; " \
		"fi\0" \
	"mmc_boot=echo Booting from mmc ...; " \
		"run mmc_args; " \
		"bootm ${loadaddr} - ${fdt_addr}\0" \
	"boot_command=" \
		"if mmc rescan; then " \
			"run try_update_nand;" \
			"run try_bootscript;" \
			"run ethmac_check;" \
			"run cat_fdt_name;" \
			"echo Loading ${fdtfile} from mmc ...; " \
			"if run mmc_load_fdt; then " \
				"echo Loading ${bootfile} from mmc ...; " \
				"if run mmc_load_image; then " \
					"run mmc_boot; " \
				"fi; " \
			"fi; " \
		"fi\0"


/* The rest of the configuration is shared */
#include <configs/mxs.h>

#endif /* __CONFIGS_LERTUIMX28_H__ */
