/*
 ============================================================================
 Name        : rtuimx287.c
 Author      : AK
 Version     : V1.01
 Copyright   : Property of Londelec UK Ltd
 Description : Board specific initialization functions

  Change log :

  *********V1.01 23/01/2021**************
  'ethaddr=' and 'eth1addr=' no longer have to be at a specific location in NAND flash
  
  *********V1.00 04/12/2014**************
  Initial revision

 ============================================================================
 */

#include <common.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/iomux-mx28.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <linux/mii.h>
#include <miiphy.h>
#include <netdev.h>
#include <errno.h>
#include <nand.h>


DECLARE_GLOBAL_DATA_PTR;

/*
 * Functions
 */
int board_early_init_f(void)
{
	/* IO0 clock at 480MHz */
	mxs_set_ioclk(MXC_IOCLK0, 480000);
	/* IO1 clock at 480MHz */
	mxs_set_ioclk(MXC_IOCLK1, 480000);

	/* SSP0 clock at 96MHz */
	mxs_set_sspclk(MXC_SSPCLK0, 96000, 0);
	/* SSP2 clock at 160MHz */
	mxs_set_sspclk(MXC_SSPCLK2, 160000, 0);

#ifdef	CONFIG_CMD_USB
	//mxs_iomux_setup_pad(MX28_PAD_SSP2_SS1__USB1_OVERCURRENT);
	//mxs_iomux_setup_pad(MX28_PAD_AUART3_TX__GPIO_3_13 |
	//		MXS_PAD_12MA | MXS_PAD_3V3 | MXS_PAD_PULLUP);
	//gpio_direction_output(MX28_PAD_AUART3_TX__GPIO_3_13, 0);

	//mxs_iomux_setup_pad(MX28_PAD_AUART3_RX__GPIO_3_12 |
	//		MXS_PAD_12MA | MXS_PAD_3V3 | MXS_PAD_PULLUP);
	//gpio_direction_output(MX28_PAD_AUART3_RX__GPIO_3_12, 0);
#endif

	gpio_direction_output(MX28_PAD_SSP3_MISO__GPIO_2_26, 1);	/* Set Heartbeat pin to output and value 1 */
	//gpio_direction_output(MX28_PAD_LCD_D08__GPIO_1_8, 1);
	//gpio_direction_output(MX28_PAD_LCD_D10__GPIO_1_10, 0);
	return 0;
}

int board_init(void)
{
	/* Adress of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	return 0;
}

int dram_init(void)
{
	return mxs_dram_init();
}

#ifdef	CONFIG_CMD_MMC
static int m28_mmc_wp(int id)
{
	if (id != 0) {
		printf("MXS MMC: Invalid card selected (card id = %d)\n", id);
		return 1;
	}

	//return gpio_get_value(MX28_PAD_AUART2_CTS__GPIO_3_10);
	return 0;	// Always return the card is not write-protected
}

/***************************************************************************
* Return card is always present
* [02/03/2015]
***************************************************************************/
static int m28_mmc_cd(int id) {

	return 1;	// Always return the card is present
}


int board_mmc_init(bd_t *bis)
{
	/* Configure WP as input. */
	//gpio_direction_input(MX28_PAD_AUART2_CTS__GPIO_3_10);
	/* Turn on the power to the card. */
	//gpio_direction_output(MX28_PAD_PWM3__GPIO_3_28, 0);

	return mxsmmc_initialize(bis, 0, m28_mmc_wp, m28_mmc_cd);
}
#endif

#ifdef	CONFIG_CMD_NET

//#define	MII_OPMODE_STRAP_OVERRIDE	0x16
//#define	MII_PHY_CTRL1			0x1e
//#define	MII_PHY_CTRL2			0x1f

int fecmxc_mii_postcall(int phy)
{
//#if	defined(CONFIG_DENX_M28_V11) || defined(CONFIG_DENX_M28_V10)
	/* KZ8031 PHY on old boards. */
//	const uint32_t freq = 0x0080;
//#else
	/* KZ8021 PHY on new boards. */
//	const uint32_t freq = 0x0000;
//#endif

	miiphy_write("FEC1", phy, MII_BMCR, 0x9000);
	//miiphy_write("FEC1", phy, MII_OPMODE_STRAP_OVERRIDE, 0x0202);
	//if (phy == 3)
	//	miiphy_write("FEC1", 3, MII_PHY_CTRL2, 0x8100 | freq);
	return 0;
}

int board_eth_init(bd_t *bis)
{
	struct mxs_clkctrl_regs *clkctrl_regs =
		(struct mxs_clkctrl_regs *)MXS_CLKCTRL_BASE;
	struct eth_device *dev;
	int ret;

	ret = cpu_eth_init(bis);
	if (ret)
		return ret;

	clrsetbits_le32(&clkctrl_regs->hw_clkctrl_enet,
		CLKCTRL_ENET_TIME_SEL_MASK | CLKCTRL_ENET_CLK_OUT_EN,
		CLKCTRL_ENET_TIME_SEL_RMII_CLK);

//#if !defined(CONFIG_DENX_M28_V11) && !defined(CONFIG_DENX_M28_V10)
	/* Reset the new PHY */
	//gpio_direction_output(MX28_PAD_AUART2_RTS__GPIO_3_11, 0);
	gpio_direction_output(MX28_PAD_PWM3__GPIO_3_28, 0);		// rtu PHY reset pin
	udelay(10000);
	//gpio_set_value(MX28_PAD_AUART2_RTS__GPIO_3_11, 1);
	gpio_set_value(MX28_PAD_PWM3__GPIO_3_28, 1);			// rtu PHY reset pin
	udelay(10000);
//#endif

	ret = fecmxc_initialize_multi(bis, 0, 0, MXS_ENET0_BASE);
	if (ret) {
		printf("FEC MXS: Unable to init FEC0\n");
		return ret;
	}

	ret = fecmxc_initialize_multi(bis, 1, 3, MXS_ENET1_BASE);
	if (ret) {
		printf("FEC MXS: Unable to init FEC1\n");
		return ret;
	}

	dev = eth_get_dev_by_name("FEC0");
	if (!dev) {
		printf("FEC MXS: Unable to get FEC0 device entry\n");
		return -EINVAL;
	}

	ret = fecmxc_register_mii_postcall(dev, fecmxc_mii_postcall);
	if (ret) {
		printf("FEC MXS: Unable to register FEC0 mii postcall\n");
		return ret;
	}

	dev = eth_get_dev_by_name("FEC1");
	if (!dev) {
		printf("FEC MXS: Unable to get FEC1 device entry\n");
		return -EINVAL;
	}

	ret = fecmxc_register_mii_postcall(dev, fecmxc_mii_postcall);
	if (ret) {
		printf("FEC MXS: Unable to register FEC1 mii postcall\n");
		return ret;
	}


	// Get hardware address from onboard NAND and set in environment
	// Addresses will be loaded automatically by function eth_write_hwaddr() in net/eth.c
	// [23/01/2021]
	u_char buffer[256];
	loff_t offset = 0x800000;
	size_t rwsize = 128;
	size_t maxsize = 256;
	char eth0varname[] = "ethaddr=";
	char eth1varname[] = "eth1addr=";
	char *eth0varvalue = NULL;
	char *eth1varvalue = NULL;

	ret = nand_read_skip_bad(&nand_info[0], offset, &rwsize, NULL, maxsize, (u_char *) &buffer);

	//printf("DEBUG FLASH = %u\n", rwsize);
	size_t i;
	for (i = 0; i < (rwsize - 25); i++) {
		//printf("%02x ", buffer[i]);
		if (
			(!eth0varvalue) &&
			!(memcmp(&buffer[i], eth0varname, sizeof(eth0varname) - 1))) {
			eth0varvalue = (char *) &buffer[i + sizeof(eth0varname) - 1];
		}
		if (
			(!eth1varvalue) &&
			!(memcmp(&buffer[i], eth1varname, sizeof(eth1varname) - 1))) {
			eth1varvalue = (char *) &buffer[i + sizeof(eth1varname) - 1];
		}
	}

	eth0varname[sizeof(eth0varname) - 2] = 0;
	eth1varname[sizeof(eth1varname) - 2] = 0;
	if (eth0varvalue)
		setenv(eth0varname, eth0varvalue);
	else
		eth0varvalue = "?";
	if (eth1varvalue)
		setenv(eth1varname, eth1varvalue);
	else
		eth1varvalue = "?";

	printf("\nFEC0 HW: %s\nFEC1 HW: %s\n", eth0varvalue, eth1varvalue);
	return ret;
}

#endif
