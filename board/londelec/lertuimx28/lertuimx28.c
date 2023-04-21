// SPDX-License-Identifier: GPL-2.0+
/*
 * Londelec RTU i.MX28
 *
 * Copyright (C) 2014 Londelec UK Ltd.
 */

#include <common.h>
#include <env.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/iomux-mx28.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <linux/delay.h>
#include <linux/stringify.h>
#include <miiphy.h>
#include <netdev.h>
#include <errno.h>
#include <nand.h>


DECLARE_GLOBAL_DATA_PTR;

#define MBD2_BOARD_ID_MASK	0x00f00000	/* Pins GPIO_3_20..GPIO_3_23 */

#define MBD2_BOARD_ID_SHIFT	20

#define PHY_OUI_MASK		0xfffffc00

#define MXS_PIN_BIT(pad) (1 <<(((pad) & MXS_PAD_PIN_MASK) >> MXS_PAD_PIN_SHIFT))


enum phy_type {
	PHY_NONE = 0,
	PHY_SMSC_LAN87X,
	PHY_DP83640,
};

enum dp83640_addr {
	DP83640_PHY0_ADDR = 0x0a,
	DP83640_PHY1_ADDR = 0x03,
};

enum board_strap {
	STRAP_MBD2_V01 = 0,
	STRAP_MBD2_V02 = 0x01,
};

static const u32 phy_ouis[] = {
	[PHY_SMSC_LAN87X] = 0x0007c000,	/* [2] = 0x0007; [3] = 0xc0xx */
	[PHY_DP83640] = 0x20005c00,		/* [2] = 0x2000; [3] = 0x5cxx */
};

static const struct {
	enum board_strap strap;
	enum phy_type pt;
	const char *suffix;
} board_compat[] = {
	{.strap = STRAP_MBD2_V01, .pt = PHY_SMSC_LAN87X, .suffix = "v01"},
	{.strap = STRAP_MBD2_V02, .pt = PHY_DP83640, .suffix = "v02"},
};

#if CONFIG_IS_ENABLED(CMD_NET)
int board_eth_init(struct bd_info *bis)
{
	struct mxs_clkctrl_regs *clkctrl_regs =
		(struct mxs_clkctrl_regs *)MXS_CLKCTRL_BASE;
	struct mxs_pinctrl_regs *pinctrl_regs =
		(struct mxs_pinctrl_regs *)MXS_PINCTRL_BASE;
	int ret;

	ret = cpu_eth_init(bis);
	if (ret)
		return ret;

	/* Disable ENET_CLK_OUT immediately, enabled by cpu_eth_init() */
	clrsetbits_le32(&clkctrl_regs->hw_clkctrl_enet,
		CLKCTRL_ENET_TIME_SEL_MASK | CLKCTRL_ENET_CLK_OUT_EN,
		CLKCTRL_ENET_TIME_SEL_RMII_CLK);

	/*
	 * Reset PHYs
	 * gpio_direction_output(MX28_PAD_PWM3__GPIO_3_28, 0);
	 */
	writel(MXS_PIN_BIT(MX28_PAD_PWM3__GPIO_3_28), &pinctrl_regs->hw_pinctrl_dout3_clr);
	writel(MXS_PIN_BIT(MX28_PAD_PWM3__GPIO_3_28), &pinctrl_regs->hw_pinctrl_doe3_set);
	udelay(10000);

	/*
	 * gpio_set_value(MX28_PAD_PWM3__GPIO_3_28, 1);
	 */
	writel(MXS_PIN_BIT(MX28_PAD_PWM3__GPIO_3_28), &pinctrl_regs->hw_pinctrl_dout3_set);
	udelay(10000);
	return ret;
}

static int dp83640_init_leds(void)
{
	int val;
	u8 i, found = 0;
	u32 oui;
	struct mii_dev *bus;

	bus = mdio_get_current_dev();
	if (!bus || !bus->read || !bus->write)
		return -1;

	for (i = 0; i < PHY_MAX_ADDR; i++) {
		val = bus->read(bus, i, MDIO_DEVAD_NONE, MII_PHYSID1);
		if (val < 0)
			continue;

		oui = val << 16;

		val = bus->read(bus, i, MDIO_DEVAD_NONE, MII_PHYSID2);
		if (val < 0)
			continue;

		oui |= val;

		if ((oui & PHY_OUI_MASK) != phy_ouis[PHY_DP83640])
			continue;

		switch (i) {
		case DP83640_PHY1_ADDR:
		case DP83640_PHY0_ADDR:
			found++;
			break;

		default:
			break;
		}
		debug("%s: PHY[%u] disabling LED_SPEED by writing LEDCR[0x18] register.\n", __func__, i);

		if (bus->write(bus, i, MDIO_DEVAD_NONE, 0x18, 0x0800))
			printf("PHY[%u]: DP83640 LEDCR[0x18] register write failed.\n", i);
	}
	return (found == 2) ? 0 : -1;
}

static enum phy_type phy_get_type(void)
{
	struct mii_dev *bus;
	enum phy_type pt;
	u8 i;
	u32 oui = 0UL;

	bus = mdio_get_current_dev();
	if (bus == NULL) {
		printf("Error: Can't check PHY type, MDIO bus device is not initialized.\n");
		return PHY_NONE;
	}

	for (i = 0; i < ARRAY_SIZE(bus->phymap); i++) {
		if (!bus->phymap[i])
			continue;

		debug("%s: PHY[%u] ID=0x%08x\n", __func__, i, bus->phymap[i]->phy_id);
		oui = bus->phymap[i]->phy_id & PHY_OUI_MASK;

		for (pt = PHY_NONE + 1; pt < ARRAY_SIZE(phy_ouis); pt++) {
			if (phy_ouis[pt] != oui)
				continue;

			debug("%s: PHY OID type found=%u\n", __func__, pt);
			return pt;
		}
		printf("PHY[%u]: Unknown chip ID=0x%08x\n", i, bus->phymap[i]->phy_id);
	}

	if (!oui)
		printf("MDIO %s: No PHY devices found.\n", bus->name);
	return PHY_NONE;
}

static void dp83640_phy0_changeover(void)
{
	struct udevice *fec0;
	struct phy_device *phydev;
	struct mii_dev *bus;

	fec0 = eth_get_dev_by_name("eth0");
	if (!fec0)
		return;

	phydev = mdio_phydev_for_ethname(fec0->name);
	if (!phydev)
		return;
	
	bus = phydev->bus;

	printf("PHY: %s:%u changed to %s:%u\n",
			bus->name, phydev->addr,
			bus->name, DP83640_PHY0_ADDR);

	bus->phymap[phydev->addr] = NULL;
	bus->phymap[DP83640_PHY0_ADDR] = phydev;
	phydev->addr = DP83640_PHY0_ADDR;
}
#endif	/* CONFIG_CMD_NET */

/*
 * Called from initcall_run_list() in common/board_f.c
 */
int board_early_init_f(void)
{
	struct mxs_pinctrl_regs *pinctrl_regs =
		(struct mxs_pinctrl_regs *)MXS_PINCTRL_BASE;

	/* IO0 clock at 480MHz */
	mxs_set_ioclk(MXC_IOCLK0, 480000);
	/* IO1 clock at 480MHz */
	mxs_set_ioclk(MXC_IOCLK1, 480000);

	/* SSP0 clock at 96MHz */
	mxs_set_sspclk(MXC_SSPCLK0, 96000, 0);

#if 0
	/* SSP2 clock at 160MHz */
	mxs_set_sspclk(MXC_SSPCLK2, 160000, 0);
#endif

	/*
	 * Make Heartbeat pin output and set value 1
	 * gpio_direction_output(MX28_PAD_SSP3_MISO__GPIO_2_26, 1);
	 */
	writel(MXS_PIN_BIT(MX28_PAD_SSP3_MISO__GPIO_2_26), &pinctrl_regs->hw_pinctrl_dout2_set);
	writel(MXS_PIN_BIT(MX28_PAD_SSP3_MISO__GPIO_2_26), &pinctrl_regs->hw_pinctrl_doe2_set);
	return 0;
}

/*
 * Called from initcall_run_list() in common/board_f.c
 * Defined in include/initcall.h
 */
int dram_init(void)
{
	return mxs_dram_init();
}

/*
 * Called from initcall_run_list() in common/board_r.c
 */
int board_init(void)
{
	/* Adress of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

#if CONFIG_IS_ENABLED(CMD_NET)
	board_eth_init(gd->bd);
#endif
	return 0;
}

/*
 * Called from initcall_run_list() in common/board_r.c
 */
int last_stage_init(void)
{
	u8 i;
	u32 bval;
	char *envver;
	enum phy_type pt = PHY_NONE;
	struct mxs_pinctrl_regs *pinctrl_regs =
		(struct mxs_pinctrl_regs *)MXS_PINCTRL_BASE;

	/* debug("%s: GPIO3_IN=0x%08x\n", __func__, readl(0x80018930)); */

#if CONFIG_IS_ENABLED(CMD_NET)
	pt = phy_get_type();
#endif

	bval = readl(&pinctrl_regs->hw_pinctrl_din3);
	bval = (bval & MBD2_BOARD_ID_MASK) >> MBD2_BOARD_ID_SHIFT;

	for (i = 0; i < ARRAY_SIZE(board_compat); i++) {
		if (board_compat[i].strap == bval) {
			printf("Board: D2 version: %s\n", board_compat[i].suffix);

			if ((pt != PHY_NONE) && (board_compat[i].pt != pt))
				printf("Warning: PHY chip doesn't match board version.\n");

			/* 
			 * Autoboot will fail to load the *.dtb file if this variable is not set.
			 * Suffix is added to the *.dtb file name e.g. imx28-lertu-v01.dtb
			 */
			env_set("boardversion", board_compat[i].suffix);
			break;
		}
	}

	if (i == ARRAY_SIZE(board_compat))
		printf("Error: Unknown D2 board version, need to upgrade U-Boot. Strap value 0x%02x\n", bval);

#if CONFIG_IS_ENABLED(CMD_NET)
	if (pt == PHY_DP83640) {
		if (!dp83640_init_leds()) {
			if (bval == STRAP_MBD2_V02)
				dp83640_phy0_changeover();
		}
	}
#endif

	if (!(gd->flags & GD_FLG_ENV_DEFAULT)) {
		envver = env_get("leuenvver");
		/*
		 * Reset to Default environment if leuenvver variable is not found.
		 * It may be foreign environment.
		 */
		if (envver == NULL)
			env_set_default("leuenvver missing", 0);
	}
	return 0;
}
