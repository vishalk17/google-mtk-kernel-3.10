/*
* Copyright (C) 2011-2015 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <mach/board.h>
#include <mach/mt_gpt.h>
#include <asm/io.h>

#include <linux/seq_file.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/scatterlist.h>
#include <mach/mt_typedefs.h>

#include "dbg.h"
#ifndef FPGA_PLATFORM
#include <mach/mt_clkmgr.h>
#endif

#include <mach/mt_gpio.h>

#if defined(GPIO_SDHC_EINT_PIN) && defined(GPIO86_CARD_DETECTION_MIXED_IN_MIPI_LCD)
extern int msdc_disable_eint_while_suspend;
#endif

#ifdef MTK_IO_PERFORMANCE_DEBUG
unsigned int g_mtk_mmc_perf_dbg = 0;
unsigned int g_mtk_mmc_dbg_range = 0;
unsigned int g_dbg_range_start = 0;
unsigned int g_dbg_range_end = 0;
unsigned int g_mtk_mmc_dbg_flag = 0;
unsigned int g_dbg_req_count = 0;
unsigned int g_dbg_raw_count = 0;
unsigned int g_dbg_write_count = 0;
unsigned int g_dbg_raw_count_old = 0;
unsigned int g_mtk_mmc_clear = 0;
int g_check_read_write = 0;
int g_i = 0;
unsigned long long g_req_buf[4000][30] = {{0} };
unsigned long long g_req_write_buf[4000][30] = {{0} };
unsigned long long g_req_write_count[4000] = {0};

unsigned long long g_mmcqd_buf[400][300] = {{0} };
char *g_time_mark[] = {
    "--start fetch request",
    "--end fetch request",
    "--start dma map this request",
    "--end dma map this request",
    "--start request",
    "--DMA start",
    "--DMA transfer done",
    "--start dma unmap request",
    "--end dma unmap request",
    "--end of request",
};
char *g_time_mark_vfs_write[] = {
    "--in vfs_write",
    "--before generic_segment_checks",
    "--after generic_segment_checks",
    "--after vfs_check_frozen",
    "--after generic_write_checks",
    "--after file_remove_suid",
    "--after file_update_time",
    "--after generic_file_direct_write",
    "--after generic_file_buffered_write",
    "--after filemap_write_and_wait_range",
    "--after invalidate_mapping_pages",
    "--after 2nd generic_file_buffered_write",
    "--before generic_write_sync",
    "--after generic_write_sync",
    "--out vfs_write"
};

#endif

/* for get transfer time with each trunk size, default not open */
#ifdef MTK_MMC_PERFORMANCE_TEST
unsigned int g_mtk_mmc_perf_test = 0;
#endif

typedef enum
{
    SDHC_HIGHSPEED = 0,   /* 0x1 Host supports HS mode */
    UHS_SDR12,            /* 0x2 Host supports UHS SDR12 mode */
    UHS_SDR25,            /* 0x3 Host supports UHS SDR25 mode */
    UHS_SDR50,            /* 0x4 Host supports UHS SDR50 mode */
    UHS_SDR104,           /* 0x5 Host supports UHS SDR104/EMMC HS200 mode */
    UHS_DDR50,            /* 0x6 Host supports UHS DDR50 mode */
    EMMC_HS400,           /* 0x7 Host supports EMMC HS400 mode */
    CAPS_SPEED_NULL,
} HOST_CAPS_SPEED_MODE;

typedef enum
{
    DRIVER_TYPE_A = 0,    /* 0x7 Host supports Driver Type A */
    DRIVER_TYPE_B,        /* 0x8 Host supports Driver Type B */
    DRIVER_TYPE_C,        /* 0x9 Host supports Driver Type C */
    DRIVER_TYPE_D,        /* 0xA Host supports Driver Type D */
    CAPS_DRIVE_NULL,
} HOST_CAPS_DRIVE_TYPE;

typedef enum
{
    MAX_CURRENT_200 = 0,      /* 0xB Host max current limit is 200mA */
    MAX_CURRENT_400,      /* 0xC Host max current limit is 400mA */
    MAX_CURRENT_600,      /* 0xD Host max current limit is 600mA */
    MAX_CURRENT_800,      /* 0xE Host max current limit is 800mA */
    CAPS_CURRENT_NULL,
} HOST_CAPS_MAX_CURRENT;

typedef enum
{
    SDXC_NO_POWER_CONTROL = 0,/*0xF   Host not supports >150mA current at 3.3V /3.0V/1.8V*/
    SDXC_POWER_CONTROL,   /*0x10 Host supports >150mA current at 3.3V /3.0V/1.8V*/
    CAPS_POWER_NULL,
} HOST_CAPS_POWER_CONTROL;

static char cmd_buf[256];


#ifdef SDIO_LIMIT_FREQ
#if defined(USE_SDIO_1V8) && (defined(MSDC1_SDIO_UT) || defined(AUTOK_UT))
unsigned int sdio_max_clock = 2000000;
#else
unsigned int sdio_max_clock = 25000000;
#endif
#endif

#if defined(MSDC1_SDIO_UT) || defined(AUTOK_UT)
unsigned int sdio_driving = 5;
unsigned int sdio_card_inserted = 0;
extern int sdio_memcpy_fromio(struct sdio_func *func, void *dst, unsigned int addr, int count);
extern int sdio_set_block_size(struct sdio_func *func, unsigned blksz);
extern int sdio_enable_func(struct sdio_func *func);
extern int mmc_io_rw_direct(struct mmc_card *card, int write, unsigned fn, unsigned addr, u8 in, u8 *out);
extern int mmc_io_rw_extended(struct mmc_card *card, int write, unsigned fn, unsigned addr, int incr_addr, u8 *buf, unsigned blocks, unsigned blksz);
#endif

#ifdef AUTOK_UT
extern int wait_sdio_autok_ready(void *data);
#endif

/* for debug zone */
unsigned int sd_debug_zone[HOST_MAX_NUM] = {
    0,
    0
};

/* mode select */
u32 dma_size[HOST_MAX_NUM] = {
    512,
    512
};
u32 msdc_perf_dbg[HOST_MAX_NUM] = {
    0,
    0
};

msdc_mode drv_mode[HOST_MAX_NUM] = {
    MODE_SIZE_DEP, /* using DMA or not depend on the size */
    MODE_SIZE_DEP
};

unsigned char msdc_clock_src[HOST_MAX_NUM] = {
    0,
    0
};

unsigned int msdc_clock_divisor[HOST_MAX_NUM] = {
    0,
    0
};

unsigned char msdc_clock_ddr_sdr_select[HOST_MAX_NUM] = {
    1,
    1
};

int msdc_enable_print_all_msdc_send_stop = 0;

drv_mod msdc_drv_mode[HOST_MAX_NUM];

u32 msdc_host_mode[HOST_MAX_NUM] = {
    0,
    0
};

u32 msdc_host_mode2[HOST_MAX_NUM] = {
    0,
    0
};

u32 msdc_base[HOST_MAX_NUM] = {
#if defined(CFG_DEV_MSDC0)
    MSDC_0_BASE,
#endif
#if defined(CFG_DEV_MSDC1)
    MSDC_1_BASE,
#endif
#if defined(CFG_DEV_MSDC2)
    MSDC_2_BASE,
#endif
#if defined(CFG_DEV_MSDC3)
    MSDC_3_BASE,
#endif
#if defined(CFG_DEV_MSDC4)
    MSDC_4_BASE,
#endif
};

int sdio_cd_result = 1;

/* for driver profile */
#define TICKS_ONE_MS  (13000)
u32 gpt_enable = 0;
u32 sdio_pro_enable = 0;   /* make sure gpt is enabled */
static unsigned long long sdio_pro_time = 30;     /* no more than 30s */
static unsigned long long sdio_profiling_start = 0;
struct sdio_profile sdio_perfomance = {0};

u32 sdio_enable_tune = 0;
u32 sdio_iocon_dspl = 0;
u32 sdio_iocon_w_dspl = 0;
u32 sdio_iocon_rspl = 0;
u32 sdio_pad_tune_rrdly = 0;
u32 sdio_pad_tune_rdly = 0;
u32 sdio_pad_tune_wrdly = 0;
u32 sdio_dat_rd_dly0_0 = 0;
u32 sdio_dat_rd_dly0_1 = 0;
u32 sdio_dat_rd_dly0_2 = 0;
u32 sdio_dat_rd_dly0_3 = 0;
u32 sdio_dat_rd_dly1_0 = 0;
u32 sdio_dat_rd_dly1_1 = 0;
u32 sdio_dat_rd_dly1_2 = 0;
u32 sdio_dat_rd_dly1_3 = 0;
u32 sdio_clk_drv = 0;
u32 sdio_cmd_drv = 0;
u32 sdio_data_drv = 0;
u32 sdio_tune_flag = 0;


extern u32 msdc_dump_padctl0(u32 id);
extern u32 msdc_dump_padctl1(u32 id);
extern u32 msdc_dump_padctl2(u32 id);
extern struct msdc_host *mtk_msdc_host[];
extern cg_clk_id msdc_cg_clk_id[];
#ifndef FPGA_PLATFORM
extern void msdc_set_driving(struct msdc_host* host, struct msdc_hw* hw, bool sd_18);
extern void msdc_set_sr(struct msdc_host *host, int clk, int cmd, int dat);
extern void msdc_set_smt(struct msdc_host *host, int set_smt);
extern void msdc_set_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 value);
extern u32  msdc_get_tune(struct msdc_host *host);
extern void msdc_set_tune(struct msdc_host *host, u32 value);
#endif

#ifdef MSDC1_SDIO
#if defined(CONFIG_SDIOAUTOK_SUPPORT)
extern int ettagent_init(void);
extern void ettagent_exit(void);
#endif
#if defined(MSDC1_SDIO_UT)
extern void mmc_detect_change(struct mmc_host *, unsigned long delay);
#endif
#endif

static void msdc_dump_reg(int id)
{
    u32 base = msdc_base[id];

    printk(KERN_ERR "[SD_Debug][Host%d]Reg[00] MSDC_CFG       = 0x%.8x\n", id, sdr_read32(base + 0x00));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[04] MSDC_IOCON     = 0x%.8x\n", id, sdr_read32(base + 0x04));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[08] MSDC_PS        = 0x%.8x\n", id, sdr_read32(base + 0x08));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[0C] MSDC_INT       = 0x%.8x\n", id, sdr_read32(base + 0x0C));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[10] MSDC_INTEN     = 0x%.8x\n", id, sdr_read32(base + 0x10));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[14] MSDC_FIFOCS    = 0x%.8x\n", id, sdr_read32(base + 0x14));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[18] MSDC_TXDATA    = not read\n", id);
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[1C] MSDC_RXDATA    = not read\n", id);
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[30] SDC_CFG        = 0x%.8x\n", id, sdr_read32(base + 0x30));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[34] SDC_CMD        = 0x%.8x\n", id, sdr_read32(base + 0x34));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[38] SDC_ARG        = 0x%.8x\n", id, sdr_read32(base + 0x38));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[3C] SDC_STS        = 0x%.8x\n", id, sdr_read32(base + 0x3C));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[40] SDC_RESP0      = 0x%.8x\n", id, sdr_read32(base + 0x40));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[44] SDC_RESP1      = 0x%.8x\n", id, sdr_read32(base + 0x44));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[48] SDC_RESP2      = 0x%.8x\n", id, sdr_read32(base + 0x48));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[4C] SDC_RESP3      = 0x%.8x\n", id, sdr_read32(base + 0x4C));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[50] SDC_BLK_NUM    = 0x%.8x\n", id, sdr_read32(base + 0x50));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[58] SDC_CSTS       = 0x%.8x\n", id, sdr_read32(base + 0x58));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[5C] SDC_CSTS_EN    = 0x%.8x\n", id, sdr_read32(base + 0x5C));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[60] SDC_DATCRC_STS = 0x%.8x\n", id, sdr_read32(base + 0x60));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[70] EMMC_CFG0      = 0x%.8x\n", id, sdr_read32(base + 0x70));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[74] EMMC_CFG1      = 0x%.8x\n", id, sdr_read32(base + 0x74));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[78] EMMC_STS       = 0x%.8x\n", id, sdr_read32(base + 0x78));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[7C] EMMC_IOCON     = 0x%.8x\n", id, sdr_read32(base + 0x7C));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[80] SD_ACMD_RESP   = 0x%.8x\n", id, sdr_read32(base + 0x80));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[84] SD_ACMD19_TRG  = 0x%.8x\n", id, sdr_read32(base + 0x84));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[88] SD_ACMD19_STS  = 0x%.8x\n", id, sdr_read32(base + 0x88));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[90] DMA_SA         = 0x%.8x\n", id, sdr_read32(base + 0x90));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[94] DMA_CA         = 0x%.8x\n", id, sdr_read32(base + 0x94));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[98] DMA_CTRL       = 0x%.8x\n", id, sdr_read32(base + 0x98));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[9C] DMA_CFG        = 0x%.8x\n", id, sdr_read32(base + 0x9C));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[A0] SW_DBG_SEL     = 0x%.8x\n", id, sdr_read32(base + 0xA0));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[A4] SW_DBG_OUT     = 0x%.8x\n", id, sdr_read32(base + 0xA4));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[A8] DMA_LEN        = 0x%.8x\n", id, sdr_read32(base + 0xA8));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[B0] PATCH_BIT0     = 0x%.8x\n", id, sdr_read32(base + 0xB0));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[B4] PATCH_BIT1     = 0x%.8x\n", id, sdr_read32(base + 0xB4));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[E0] SD_PAD_CTL0    = 0x%.8x\n", id, msdc_dump_padctl0(id));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[E4] SD_PAD_CTL1    = 0x%.8x\n", id, msdc_dump_padctl1(id));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[E8] SD_PAD_CTL2    = 0x%.8x\n", id, msdc_dump_padctl2(id));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[EC] PAD_TUNE       = 0x%.8x\n", id, sdr_read32(base + 0xEC));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[F0] DAT_RD_DLY0    = 0x%.8x\n", id, sdr_read32(base + 0xF0));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[F4] DAT_RD_DLY1    = 0x%.8x\n", id, sdr_read32(base + 0xF4));
    printk(KERN_ERR "[SD_Debug][Host%d]Reg[F8] HW_DBG_SEL     = 0x%.8x\n", id, sdr_read32(base + 0xF8));
    printk(KERN_ERR "[SD_Debug][Host%d]Rg[100] MAIN_VER       = 0x%.8x\n", id, sdr_read32(base + 0x100));
    printk(KERN_ERR "[SD_Debug][Host%d]Rg[104] ECO_VER        = 0x%.8x\n", id, sdr_read32(base + 0x104));
}
static void msdc_set_field(unsigned int address, unsigned int start_bit, unsigned int len, unsigned int value)
{
    unsigned long field;
    if (start_bit > 31 || start_bit < 0 || len > 32 || len <= 0)
	printk("[****SD_Debug****]reg filed beyoned (0~31) or length beyoned (1~32)\n");
    else{
        field = ((1 << len) - 1) << start_bit;
        value &= (1 << len) - 1;
        printk("[****SD_Debug****]Original:0x%x (0x%x)\n", address, sdr_read32(address));
        sdr_set_field(address, field, value);
        printk("[****SD_Debug****]Modified:0x%x (0x%x)\n", address, sdr_read32(address));
    }
}
static void msdc_get_field(unsigned int address, unsigned int start_bit, unsigned int len, unsigned int value)
{
    unsigned long field;
    if (start_bit > 31 || start_bit < 0 || len > 32 || len <= 0)
	printk("[****SD_Debug****]reg filed beyoned (0~31) or length beyoned (1~32)\n");
    else{
        field = ((1 << len) - 1) << start_bit;
        sdr_get_field(address, field, value);
        printk("[****SD_Debug****]Reg:0x%x start_bit(%d)len(%d)(0x%x)\n", address, start_bit, len, value);
    }
}
static void msdc_init_gpt(void)
{
#if 0
    GPT_CONFIG config;

    config.num  = GPT6;
    config.mode = GPT_FREE_RUN;
    config.clkSrc = GPT_CLK_SRC_SYS;
    config.clkDiv = GPT_CLK_DIV_1;   /* 13MHz GPT6 */

    if (GPT_Config(config) == FALSE)
	return;

    GPT_Start(GPT6);
#endif
}

u32 msdc_time_calc(u32 old_L32, u32 old_H32, u32 new_L32, u32 new_H32)
{
    u32 ret = 0;

    if (new_H32 == old_H32) {
	ret = new_L32 - old_L32;
    } else if (new_H32 == (old_H32 + 1)) {
	if (new_L32 > old_L32) {
	    printk("msdc old_L<0x%x> new_L<0x%x>\n", old_L32, new_L32);
	}
	ret = (0xffffffff - old_L32);
	ret += new_L32;
    } else {
	printk("msdc old_H<0x%x> new_H<0x%x>\n", old_H32, new_H32);
    }

    return ret;
}

void msdc_sdio_profile(struct sdio_profile *result)
{
    struct cmd_profile *cmd;
    u32 i;

    printk("sdio === performance dump ===\n");
    printk("sdio === total execute tick<%d> time<%dms> Tx<%dB> Rx<%dB>\n",
	result->total_tc, result->total_tc / TICKS_ONE_MS,
	result->total_tx_bytes, result->total_rx_bytes);

    /* CMD52 Dump */
    cmd = &result->cmd52_rx;
    printk("sdio === CMD52 Rx <%d>times tick<%d> Max<%d> Min<%d> Aver<%d>\n", cmd->count, cmd->tot_tc,
	cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count);
    cmd = &result->cmd52_tx;
    printk("sdio === CMD52 Tx <%d>times tick<%d> Max<%d> Min<%d> Aver<%d>\n", cmd->count, cmd->tot_tc,
	cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count);

    /* CMD53 Rx bytes + block mode */
    for (i = 0; i < 512; i++) {
	cmd = &result->cmd53_rx_byte[i];
	if (cmd->count) {
	    printk("sdio<%6d><%3dB>_Rx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc,
		cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
		cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));
	}
    }
    for (i = 0; i < 100; i++) {
	cmd = &result->cmd53_rx_blk[i];
	if (cmd->count) {
	    printk("sdio<%6d><%3d>B_Rx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc,
		cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
		cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));
	}
    }

    /* CMD53 Tx bytes + block mode */
    for (i = 0; i < 512; i++) {
	cmd = &result->cmd53_tx_byte[i];
	if (cmd->count) {
	    printk("sdio<%6d><%3dB>_Tx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc,
		cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
		cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));
	}
    }
    for (i = 0; i < 100; i++) {
	cmd = &result->cmd53_tx_blk[i];
	if (cmd->count) {
	    printk("sdio<%6d><%3d>B_Tx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc,
		cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
		cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));
	}
    }

    printk("sdio === performance dump done ===\n");
}

/* ========= sdio command table =========== */
void msdc_performance(u32 opcode, u32 sizes, u32 bRx, u32 ticks)
{
    struct sdio_profile *result = &sdio_perfomance;
    struct cmd_profile *cmd;
    u32 block;
    long long endtime;

    if (sdio_pro_enable == 0) {
	return;
    }

    if (opcode == 52) {
	cmd = bRx ?  &result->cmd52_rx : &result->cmd52_tx;
    } else if (opcode == 53) {
	if (sizes < 512) {
	    cmd = bRx ?  &result->cmd53_rx_byte[sizes] : &result->cmd53_tx_byte[sizes];
	} else {
	    block = sizes / 512;
	    if (block >= 99) {
		printk("cmd53 error blocks\n");
		while (1);
	    }
	    cmd = bRx ?  &result->cmd53_rx_blk[block] : &result->cmd53_tx_blk[block];
	}
    } else {
	return;
    }

    /* update the members */
    if (ticks > cmd->max_tc) {
	cmd->max_tc = ticks;
    }
    if (cmd->min_tc == 0 || ticks < cmd->min_tc) {
	cmd->min_tc = ticks;
    }
    cmd->tot_tc += ticks;
    cmd->tot_bytes += sizes;
    cmd->count++;

    if (bRx) {
	result->total_rx_bytes += sizes;
    } else {
	result->total_tx_bytes += sizes;
    }
    result->total_tc += ticks;
#if 0
    /* dump when total_tc > 30s */
    if (result->total_tc >= sdio_pro_time * TICKS_ONE_MS * 1000) {
	msdc_sdio_profile(result);
	memset(result, 0 , sizeof(struct sdio_profile));
    }
#endif

     endtime = sched_clock();
     if ((endtime-sdio_profiling_start) >= sdio_pro_time * 1000000000) {
	 msdc_sdio_profile(result);
	 memset(result, 0 , sizeof(struct sdio_profile));
	 sdio_profiling_start = endtime;
     }

}



#define COMPARE_ADDRESS_MMC   0x0
#define COMPARE_ADDRESS_SD    0x2000
#define COMPARE_ADDRESS_SDIO  0x0
#define COMPARE_ADDRESS_SD_COMBO  0x2000

#define MSDC_MULTI_BUF_LEN  (4*1024)
u32 msdc_multi_wbuf[MSDC_MULTI_BUF_LEN];
u32 msdc_multi_rbuf[MSDC_MULTI_BUF_LEN];

#define is_card_present(h)     (((struct msdc_host *)(h))->card_inserted)

static int sd_multi_rw_compare_slave(int host_num, int read, uint address)
{
    struct scatterlist msdc_sg;
    struct mmc_data  msdc_data;
    struct mmc_command msdc_cmd;
    struct mmc_command msdc_stop;
    struct mmc_request  msdc_mrq;
    struct msdc_host *host_ctl;
    /* struct msdc_host *host = mtk_msdc_host[host_num]; */
    int result = 0, forIndex = 0;
    u8 *wPtr;
    u8 wData[16] = {
	0x67, 0x45, 0x23, 0x01,
	0xef, 0xcd, 0xab, 0x89,
	0xce, 0x8a, 0x46, 0x02,
	0xde, 0x9b, 0x57, 0x13 };

    host_ctl = mtk_msdc_host[host_num];
    if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
    {
        printk(" there is no card initialized in host[%d]\n", host_num);
	return -1;
    }

    if (!is_card_present(host_ctl))
    {
	printk("  [%s]: card is removed!\n", __func__);
	return -1;
    }

    mmc_claim_host(host_ctl->mmc);

    memset(&msdc_data, 0, sizeof(struct mmc_data));
    memset(&msdc_mrq, 0, sizeof(struct mmc_request));
    memset(&msdc_cmd, 0, sizeof(struct mmc_command));
    memset(&msdc_stop, 0, sizeof(struct mmc_command));

    msdc_mrq.cmd = &msdc_cmd;
    msdc_mrq.data = &msdc_data;

    if (read) {
	/* init read command */
	msdc_data.flags = MMC_DATA_READ;
	msdc_cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
	msdc_data.blocks = sizeof(msdc_multi_rbuf) / 512;
        wPtr = (u8*)msdc_multi_rbuf;
	/* init read buffer */
        for (forIndex = 0; forIndex < MSDC_MULTI_BUF_LEN*4; forIndex++)
	    *(wPtr + forIndex) = 0x0;
	/* for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++) */
	/* printk("R_buffer[0x%x]\n",msdc_multi_rbuf[forIndex]); */
    } else {
	/* init write command */
	msdc_data.flags = MMC_DATA_WRITE;
	msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
	msdc_data.blocks = sizeof(msdc_multi_wbuf) / 512;
	/* init write buffer */
        wPtr = (u8*)msdc_multi_wbuf;
        for (forIndex = 0; forIndex < MSDC_MULTI_BUF_LEN*4; forIndex++)
	    *(wPtr + forIndex) = wData[forIndex%16];
	/* for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++) */
	/* printk("W_buffer[0x%x]\n",msdc_multi_wbuf[forIndex]); */
    }

    msdc_cmd.arg = address;

    BUG_ON(!host_ctl->mmc->card);
    if (!mmc_card_blockaddr(host_ctl->mmc->card)) {
	/* printk("this device use byte address!!\n"); */
	msdc_cmd.arg <<= 9;
    }
    msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

    msdc_stop.opcode = MMC_STOP_TRANSMISSION;
    msdc_stop.arg = 0;
    msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

    msdc_data.stop = &msdc_stop;
    msdc_data.blksz = 512;
    msdc_data.sg = &msdc_sg;
    msdc_data.sg_len = 1;

    if (read)
	sg_init_one(&msdc_sg, msdc_multi_rbuf, sizeof(msdc_multi_rbuf));
    else
	sg_init_one(&msdc_sg, msdc_multi_wbuf, sizeof(msdc_multi_wbuf));

    mmc_set_data_timeout(&msdc_data, host_ctl->mmc->card);
    mmc_wait_for_req(host_ctl->mmc, &msdc_mrq);

    if (read) {
        for (forIndex = 0; forIndex < MSDC_MULTI_BUF_LEN; forIndex++) {
	    /* printk("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\t\n", forIndex, msdc_multi_wbuf[forIndex], msdc_multi_rbuf[forIndex]); */
            if (msdc_multi_wbuf[forIndex] != msdc_multi_rbuf[forIndex]) {
		printk("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\tfailed\n", forIndex, msdc_multi_wbuf[forIndex], msdc_multi_rbuf[forIndex]);
                result =  -1;
	    }
	}
	/*if(result == 0)
	    printk("pid[%d][%s]: data compare successed!!\n", current->pid, __func__);
	else
	    printk("pid[%d][%s]: data compare failed!!\n", current->pid, __func__);*/
    }

    mmc_release_host(host_ctl->mmc);

    if (msdc_cmd.error)
	result = msdc_cmd.error;

    if (msdc_data.error) {
	result = msdc_data.error;
    } else {
	result = 0;
    }

    return result;
}

static int sd_multi_rw_compare(int host_num, uint address, int count)
{
    int i = 0, j = 0;
    int error = 0;

    for (i = 0; i < count; i++)
    {
	/* printk("============ cpu[%d] pid[%d]: start the %d time compare ============\n", task_cpu(current), current->pid, i); */

	error = sd_multi_rw_compare_slave(host_num, 0, address); /* write */
	if (error)
	{
	    printk("[%s]: failed to write data, error=%d\n", __func__, error);
	    break;
	}

        for (j = 0; j < 1; j++) {
	    error = sd_multi_rw_compare_slave(host_num, 1, address); /* read */
	    if (error)
	    {
		printk("[%s]: failed to read data, error=%d\n", __func__, error);
		break;
	    }
	}
	if (error)
	    printk("============ cpu[%d] pid[%d]: FAILED the %d time compare ============\n", task_cpu(current), current->pid, i);
	else
	    printk("============ cpu[%d] pid[%d]: FINISH the %d time compare ============\n", task_cpu(current), current->pid, i);
    }

    if (i == count)
	printk("pid[%d]: successed to compare data within %d times\n", current->pid, count);

    return error;
}



#define MAX_THREAD_NUM_FOR_SMP 20

static uint smp_address_on_sd[MAX_THREAD_NUM_FOR_SMP] =
{
    0x2000,
    0x20000,
    0x200000,
    0x2000000,
    0x2200000,
    0x2400000,
    0x2800000,
    0x2c00000,
    0x4000000,
    0x4200000,
    0x4400000,
    0x4800000,
    0x4c00000,
    0x8000000,
    0x8200000,
    0x8400000,
    0x8800000,
    0x8c00000,
    0xc000000,
    0xc200000
};

static uint smp_address_on_mmc[MAX_THREAD_NUM_FOR_SMP] =
{
    0x200,
    0x2000,
    0x20000,
    0x200000,
    0x2000000,
    0x2200000,
    0x2400000,
    0x2800000,
    0x2c00000,
    0x4000000,
    0x4200000,
    0x4400000,
    0x4800000,
    0x4c00000,
    0x8000000,
    0x8200000,
    0x8400000,
    0x8800000,
    0x8c00000,
    0xc000000,
};

static uint smp_address_on_sd_combo[MAX_THREAD_NUM_FOR_SMP] =
{
    0x2000,
    0x20000,
    0x200000,
    0x2000000,
    0x2200000,
    0x2400000,
    0x2800000,
    0x2c00000,
    0x4000000,
    0x4200000,
    0x4400000,
    0x4800000,
    0x4c00000,
    0x8000000,
    0x8200000,
    0x8400000,
    0x8800000,
    0x8c00000,
    0xc000000,
    0xc200000
};
struct write_read_data {
    int host_id;        /* the target host you want to do SMP test on. */
    uint start_address; /* where you want to do write/read of the memory card */
    int count;          /* how many times you want to do read after write bit by bit comparison */
};

static struct write_read_data wr_data[HOST_MAX_NUM][MAX_THREAD_NUM_FOR_SMP];
/*
 * 2012-03-25
 * the SMP thread function
 * do read after write the memory card, and bit by bit comparison
 */
static int write_read_thread(void *ptr)
{
    struct write_read_data *data = (struct write_read_data *)ptr;
    sd_multi_rw_compare(data->host_id, data->start_address, data->count);
    return 0;
}

/*
 * 2012-03-25
 * function:         do SMP test on the same one MSDC host
 * thread_num:       the number of thread you want to trigger on this host.
 * host_id:          the target host you want to do SMP test on.
 * count:            how many times you want to do read after write bit by bit comparison in each thread.
 * multi_address:    whether do read/write the same/different address of the memory card in each thread.
 */
static int smp_test_on_one_host(int thread_num, int host_id, int count, int multi_address)
{
    int i = 0, ret = 0;
    char thread_name[128];
    struct msdc_host *host_ctl;

    printk("============================[%s] start ================================\n\n", __func__);
    printk(" host %d run %d thread, each thread run %d RW comparison\n", host_id, thread_num, count);
    if (host_id >= HOST_MAX_NUM || host_id < 0)
    {
	printk(" bad host id: %d\n", host_id);
	ret = -1;
	goto out;
    }

    if (thread_num > MAX_THREAD_NUM_FOR_SMP)/* && (multi_address != 0)) */
    {
	printk(" too much thread for SMP test, thread_num=%d\n", thread_num);
	ret = -1;
	goto out;
    }

    host_ctl = mtk_msdc_host[host_id];
    if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
    {
        printk(" there is no card initialized in host[%d]\n", host_id);
	ret = -1;
	goto out;
    }


    for (i = 0; i < thread_num; i++)
    {
	switch (host_ctl->mmc->card->type)
	{
	    case MMC_TYPE_MMC:
		if (!multi_address)
		    wr_data[host_id][i].start_address = COMPARE_ADDRESS_MMC;
		else
		    wr_data[host_id][i].start_address = smp_address_on_mmc[i];
		if (i == 0)
		    printk(" MSDC[%d], MMC:\n", host_id);
		break;

	    case MMC_TYPE_SD:
		if (!multi_address)
		    wr_data[host_id][i].start_address = COMPARE_ADDRESS_SD;
		else
		    wr_data[host_id][i].start_address = smp_address_on_sd[i];
		if (i == 0)
		    printk(" MSDC[%d], SD:\n", host_id);
		break;

	    case MMC_TYPE_SDIO:
		if (i == 0)
		{
		    printk(" MSDC[%d], SDIO:\n", host_id);
		    printk("   please manually trigger wifi application instead of write/read something on SDIO card\n");
		}
		ret = -1;
		goto out;

	    case MMC_TYPE_SD_COMBO:
		if (!multi_address)
		    wr_data[host_id][i].start_address = COMPARE_ADDRESS_SD_COMBO;
		else
		    wr_data[host_id][i].start_address = smp_address_on_sd_combo[i];
		if (i == 0)
		    printk(" MSDC[%d], SD_COMBO:\n", host_id);
		break;

	    default:
		if (i == 0)
		    printk(" MSDC[%d], cannot recognize this card\n", host_id);
		ret = -1;
		goto out;
	}
	wr_data[host_id][i].host_id = host_id;
	wr_data[host_id][i].count = count;
	sprintf(thread_name, "msdc_H%d_T%d", host_id, i);
	kthread_run(write_read_thread, &wr_data[host_id][i], thread_name);
	printk("   start thread: %s, at address 0x%x\n", thread_name, wr_data[host_id][i].start_address);
    }
out:
    printk("============================[%s] end ================================\n\n", __func__);
    return ret;
}

/*
 * 2012-03-25
 * function:         do SMP test on all MSDC hosts
 * thread_num:       the number of thread you want to trigger on this host.
 * count:            how many times you want to do read after write bit by bit comparison in each thread.
 * multi_address:    whether do read/write the same/different address of the memory card in each thread.
 */
static int smp_test_on_all_host(int thread_num, int count, int multi_address)
{
    int i = 0;
    int j = 0;
    int ret = 0;
    char thread_name[128];
    struct msdc_host *host_ctl;

    printk("============================[%s] start ================================\n\n", __func__);
    printk(" each host run %d thread, each thread run %d RW comparison\n", thread_num, count);
    if (thread_num > MAX_THREAD_NUM_FOR_SMP) /* && (multi_address != 0)) */
    {
	printk(" too much thread for SMP test, thread_num=%d\n", thread_num);
	ret = -1;
	goto out;
    }

    for (i = 0; i < HOST_MAX_NUM; i++)
    {
	host_ctl = mtk_msdc_host[i];
	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
	{
	    printk(" MSDC[%d], no card is initialized\n", i);
	    continue;
	}

	if (host_ctl->mmc->card->type == MMC_TYPE_SDIO)
	{
	    printk(" MSDC[%d], SDIO, please manually trigger wifi application instead of write/read something on SDIO card\n", i);
	    continue;
	}

        for (j = 0; j < thread_num; j++)
	{
	    wr_data[i][j].host_id = i;
	    wr_data[i][j].count = count;
	    switch (host_ctl->mmc->card->type)
	    {
		case MMC_TYPE_MMC:
		    if (!multi_address)
			wr_data[i][j].start_address = COMPARE_ADDRESS_MMC;
		    else
			wr_data[i][j].start_address = smp_address_on_mmc[i];
		    if (j == 0)
			printk(" MSDC[%d], MMC:\n ", i);
		    break;

		case MMC_TYPE_SD:
		    if (!multi_address)
			wr_data[i][j].start_address = COMPARE_ADDRESS_SD;
		    else
			wr_data[i][j].start_address = smp_address_on_sd[i];
		    if (j == 0)
			printk(" MSDC[%d], SD:\n", i);
		    break;

		case MMC_TYPE_SDIO:
		    if (j == 0)
		    {
			printk(" MSDC[%d], SDIO:\n", i);
			printk("   please manually trigger wifi application instead of write/read something on SDIO card\n");
		    }
		    ret = -1;
		    goto out;

		case MMC_TYPE_SD_COMBO:
		    if (!multi_address)
			wr_data[i][j].start_address = COMPARE_ADDRESS_SD_COMBO;
		    else
			wr_data[i][j].start_address = smp_address_on_sd_combo[i];
		    if (j == 0)
			printk(" MSDC[%d], SD_COMBO:\n", i);
		    break;

		default:
		    if (j == 0)
			printk(" MSDC[%d], cannot recognize this card\n", i);
		    ret = -1;
		    goto out;
	    }
	    sprintf(thread_name, "msdc_H%d_T%d", i, j);
	    kthread_run(write_read_thread, &wr_data[i][j], thread_name);
	    printk("   start thread: %s, at address: 0x%x\n", thread_name, wr_data[i][j].start_address);
	}
    }
out:
    printk("============================[%s] end ================================\n\n", __func__);
    return ret;
}


static int msdc_help_proc_show(struct seq_file *m, void *v)
{
    seq_puts(m, "\n====================[msdc_help]=====================\n");

    seq_printf(m, "\n   LOG control:           echo %x [host_id] [debug_zone] > msdc_debug\n", SD_TOOL_ZONE);
    seq_printf(m, "          [debug_zone]       DMA:0x%x,  CMD:0x%x,  RSP:0x%x,   INT:0x%x,   CFG:0x%x,  FUC:0x%x,\n",
						  DBG_EVT_DMA, DBG_EVT_CMD, DBG_EVT_RSP, DBG_EVT_INT, DBG_EVT_CFG, DBG_EVT_FUC);
    seq_printf(m, "                             OPS:0x%x, FIO:0x%x, WRN:0x%x, PWR:0x%x, CLK:0x%x, RW:0x%x, NRW:0x%x\n",
						  DBG_EVT_OPS, DBG_EVT_FIO, DBG_EVT_WRN, DBG_EVT_PWR, DBG_EVT_CLK, DBG_EVT_RW, DBG_EVT_NRW);
    seq_puts(m, "\n   DMA mode:\n");
    seq_printf(m, "          set DMA mode:      echo %x 0 [host_id] [dma_mode] [dma_size] > msdc_debug\n", SD_TOOL_DMA_SIZE);
    seq_printf(m, "          get DMA mode:      echo %x 1 [host_id] > msdc_debug\n", SD_TOOL_DMA_SIZE);
    seq_puts(m, "            [dma_mode]       0:PIO, 1:DMA, 2:SIZE_DEP\n");
    seq_puts(m, "            [dma_size]       valid for SIZE_DEP mode, the min size can trigger the DMA mode\n");
    seq_printf(m, "\n   SDIO profile:          echo %x [enable] [time] > msdc_debug\n", SD_TOOL_SDIO_PROFILE);
    seq_puts(m, "\n   CLOCK control: \n");
    seq_printf(m, "          set clk src:       echo %x 0 [host_id] [clk_src] > msdc_debug\n", SD_TOOL_CLK_SRC_SELECT);
    seq_printf(m, "          get clk src:       echo %x 1 [host_id] > msdc_debug\n", SD_TOOL_CLK_SRC_SELECT);
    seq_puts(m, "            [clk_src]        0:SYS PLL(26Mhz), 1:3G PLL(197Mhz), 2:AUD PLL(208Mhz)\n");
    seq_puts(m, "\n   REGISTER control:\n");
    seq_printf(m, "          write register:    echo %x 0 [host_id] [register_offset] [value] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          read register:     echo %x 1 [host_id] [register_offset] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          write mask:        echo %x 2 [host_id] [register_offset] [start_bit] [len] [value] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          read mask:         echo %x 3 [host_id] [register_offset] [start_bit] [len] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          dump:              echo %x 4 [host_id]> msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_puts(m, "\n   DRVING control:\n");
    seq_printf(m, "          set driving:       echo %x 0 [host_id] [clk_drv] [cmd_drv] [dat_drv] > msdc_debug\n", SD_TOOL_SET_DRIVING);
    seq_printf(m, "          get driving:       echo %x 1 [host_id] > msdc_debug\n", SD_TOOL_SET_DRIVING);
    seq_puts(m, "\n   DESENSE control:\n");
    seq_printf(m, "          write register:    echo %x 0 [value] > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "          read register:     echo %x 1 > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "          write mask:        echo %x 2 [start_bit] [len] [value] > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "          read mask:         echo %x 3 [start_bit] [len] > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "\n   RW_COMPARE test:       echo %x [host_id] [compare_count] > msdc_debug\n", RW_BIT_BY_BIT_COMPARE);
    seq_puts(m, "          [compare_count]    how many time you want to \"write=>read=>compare\"\n");
    seq_printf(m, "\n   SMP_ON_ONE_HOST test:  echo %x [host_id] [thread_num] [compare_count] [multi_address] > msdc_debug\n", SMP_TEST_ON_ONE_HOST);
    seq_puts(m, "          [thread_num]       how many R/W comparision thread you want to run at host_id\n");
    seq_puts(m, "          [compare_count]    how many time you want to \"write=>read=>compare\" in each thread\n");
    seq_puts(m, "          [multi_address]    whether read/write different address in each thread, 0:No, 1:Yes\n");
    seq_printf(m, "\n   SMP_ON_ALL_HOST test:  echo %x [thread_num] [compare_count] [multi_address] > msdc_debug\n", SMP_TEST_ON_ALL_HOST);
    seq_puts(m, "          [thread_num]       how many R/W comparision thread you want to run at each host\n");
    seq_puts(m, "          [compare_count]    how many time you want to \"write=>read=>compare\" in each thread\n");
    seq_puts(m, "          [multi_address]    whether read/write different address in each thread, 0:No, 1:Yes\n");
    seq_puts(m, "\n   SPEED_MODE control:\n");
    seq_printf(m, "          set speed mode:    echo %x 0 [host_id] [speed_mode] [driver_type] [max_current] [power_control] > msdc_debug\n", SD_TOOL_MSDC_HOST_MODE);
    seq_printf(m, "          get speed mode:    echo %x 1 [host_id]\n", SD_TOOL_MSDC_HOST_MODE);
    seq_puts(m, "            [speed_mode]       ff:N/A,  0:HS,      1:SDR12,   2:SDR25,   3:SDR:50,  4:SDR104,  5:DDR, 6:HS400\n");
    seq_puts(m, "            [driver_type]      ff:N/A,  0: type A, 1:type B,  2:type C,  3:type D\n");
    seq_puts(m, "            [max_current]      ff:N/A,  0:200mA,   1:400mA,   2:600mA,   3:800mA\n");
    seq_puts(m, "            [power_control]    ff:N/A,  0:disable, 1:enable\n");
    seq_printf(m, "\n   DMA viloation:         echo %x [host_id] [ops]> msdc_debug\n", SD_TOOL_DMA_STATUS);
    seq_puts(m, "          [ops]              0:get latest dma address,  1:start violation test\n");
    seq_printf(m, "\n   SET Slew Rate:         echo %x [host_id] [clk] [cmd] [dat] [rst] [ds]> msdc_debug\n", SD_TOOL_ENABLE_SLEW_RATE);
    seq_puts(m, "\n   TD/RD SEL:\n");
    seq_printf(m, "          set rdsel:             echo %x [host_id] 0 [value] > msdc_debug\n", SD_TOOL_SET_RDTDSEL);
    seq_printf(m, "          set tdsel:             echo %x [host_id] 1 [value] > msdc_debug\n", SD_TOOL_SET_RDTDSEL);
    seq_printf(m, "          get tdsel/rdsel:       echo %x [host_id] 2 > msdc_debug\n", SD_TOOL_SET_RDTDSEL);
    seq_puts(m, "            [value]              rdsel: 0x0<<4 ~ 0x3f<<4,    tdsel: 0x0~0xf\n");
    seq_puts(m, "\n   NOTE: All input data is Hex number!\n");

    seq_puts(m, "\n======================================================\n\n");

    return 0;
}

/* ========== driver proc interface =========== */
static int msdc_debug_proc_show(struct seq_file *m, void *v)
{

    seq_puts(m, "\n=========================================\n");

    seq_puts(m, "Index<0> + Id + Zone\n");
    seq_puts(m, "-> PWR<9> WRN<8> | FIO<7> OPS<6> FUN<5> CFG<4> | INT<3> RSP<2> CMD<1> DMA<0>\n");
    seq_puts(m, "-> echo 0 3 0x3ff >msdc_bebug -> host[3] debug zone set to 0x3ff\n");
    seq_printf(m, "-> MSDC[0] Zone: 0x%.8x\n", sd_debug_zone[0]);
    seq_printf(m, "-> MSDC[1] Zone: 0x%.8x\n", sd_debug_zone[1]);

    seq_puts(m, "Index<1> + ID:4|Mode:4 + DMA_SIZE\n");
    seq_puts(m, "-> 0)PIO 1)DMA 2)SIZE\n");
    seq_puts(m, "-> echo 1 22 0x200 >msdc_bebug -> host[2] size mode, dma when >= 512\n");
    seq_printf(m, "-> MSDC[0] mode<%d> size<%d>\n", drv_mode[0], dma_size[0]);
    seq_printf(m, "-> MSDC[1] mode<%d> size<%d>\n", drv_mode[1], dma_size[1]);

    seq_puts(m, "MSDC0 Regs\n");
    seq_printf(m, "-> Clk SRC Reg: %x\n", sdr_read32(0xF0000000));
    seq_printf(m, "-> Reg00: %x\n", sdr_read32(0xF1120000));
    seq_printf(m, "-> Reg04: %x\n", sdr_read32(0xF1120004));
    seq_printf(m, "-> Reg08: %x\n", sdr_read32(0xF1120008));
    seq_printf(m, "-> Reg0C: %x\n", sdr_read32(0xF112000C));
    seq_puts(m, "\nMSDC1 Regs\n");
    seq_printf(m, "-> Clk SRC Reg: %x\n", sdr_read32(0xF0000000));
    seq_printf(m, "-> Reg00: %x\n", sdr_read32(0xF1130000));
    seq_printf(m, "-> Reg04: %x\n", sdr_read32(0xF1130004));
    seq_printf(m, "-> Reg08: %x\n", sdr_read32(0xF1130008));
    seq_printf(m, "-> Reg0C: %x\n", sdr_read32(0xF113000C));

    seq_puts(m, "Index<3> + SDIO_PROFILE + TIME\n");
    seq_puts(m, "-> echo 3 1 0x1E >msdc_bebug -> enable sdio_profile, 30s\n");
    seq_printf(m, "-> SDIO_PROFILE<%d> TIME<%llu s>\n", sdio_pro_enable, sdio_pro_time);

    seq_puts(m, "=========================================\n\n");

    return 0;
}

static int msdc_debug_proc_write(struct file *file, const char *buf, size_t count, loff_t *data)
{
    int ret;
    int cmd, p1, p2, p3, p4, p5, p6;
    int id, zone;
    int mode, size;
    int thread_num, compare_count, multi_address;
    unsigned int base;
    unsigned int offset = 0;
    unsigned int reg_value;
    HOST_CAPS_SPEED_MODE spd_mode = CAPS_SPEED_NULL;
    HOST_CAPS_DRIVE_TYPE drv_type = CAPS_DRIVE_NULL;
    HOST_CAPS_MAX_CURRENT current_limit = CAPS_CURRENT_NULL;
    HOST_CAPS_POWER_CONTROL pw_cr = CAPS_POWER_NULL;
    struct msdc_host *host = NULL;
    struct dma_addr *dma_address, *p_dma_address;
    int dma_status;

    if (count == 0)return -1;
    if (count > 255)count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;

    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%x %x %x %x %x %x %x", &cmd, &p1, &p2, &p3, &p4, &p5, &p6);

    if (cmd == SD_TOOL_ZONE) {
	id = p1; zone = p2; /* zone &= 0x3ff; */
	printk("[****SD_Debug****]msdc host_id<%d> zone<0x%.8x>\n", id, zone);
        if (id >= 0 && id <= HOST_MAX_NUM-1) {
	    sd_debug_zone[id] = zone;
	}
	else if (id == HOST_MAX_NUM) {
	    sd_debug_zone[0] = sd_debug_zone[1] = zone;
	}
	else{
	    printk("[****SD_Debug****]msdc host_id error when set debug zone\n");
	}
	if (zone & DBG_EVT_REP_STOP) {
	    /* All host use common setting for msdc_print_all_msdc_send_stop */
            msdc_enable_print_all_msdc_send_stop = 1;
	} else {
            msdc_enable_print_all_msdc_send_stop = 0;
	}
	if (zone & DBG_EVT_PERF_DBG) {
            msdc_perf_dbg[id] = 1;
	} else {
            msdc_perf_dbg[id] = 0;
	}
    } else if (cmd == SD_TOOL_DMA_SIZE) {
	id = p2;  mode = p3; size = p4;
        if (id >= 0 && id <= HOST_MAX_NUM-1) {
            if (p1 == 0) {
	      drv_mode[id] = mode;
	      dma_size[id] = size;
	    } else{
	      printk("-> MSDC[%d] mode<%d> size<%d>\n", id, drv_mode[id], dma_size[id]);
	    }
	}
	else{
	    printk("[****SD_Debug****]msdc host_id error when select mode\n");
	}
    } else if (cmd == SD_TOOL_SDIO_PROFILE) {
	if (p1 == 1) { /* enable profile */
	    if (gpt_enable == 0) {
		msdc_init_gpt();
		gpt_enable = 1;
	    }
	    sdio_pro_enable = 1;
	    if (p2 == 0) p2 = 1; if (p2 >= 30) p2 = 30;
	    sdio_pro_time = p2;
	} else if (p1 == 0) {
	    /* todo */
	    sdio_pro_enable = 0;
	}
    } else if (cmd == SD_TOOL_CLK_SRC_SELECT) {
	id = p2;
        if (id >= 0 && id < HOST_MAX_NUM) {
	    if (p1 == 0) {
		/* Set cuurent MSDC source */
                if (p3 >= 0 && p3 < CLK_SRC_MAX_NUM) {
		    msdc_clock_src[id] = p3;
                    if (p4 == 0) msdc_clock_ddr_sdr_select[id] = 1;
                    else if (p4 > 0) msdc_clock_ddr_sdr_select[id] = 2;
                    if (p5 >= 1) {
			msdc_clock_divisor[id] = p5;
			printk("[****SD_Debug****]msdc%d's clock divisor set as %d\n", id, p5);
		    }
		    printk("[****SD_Debug****]msdc%d's pll source changed to %d\n", id, msdc_clock_src[id]);
		    printk("[****SD_Debug****]to enable the above settings, please suspend and resume the phone again\n");
		} else {
		    printk("[****SD_Debug****] invalide clock src id:%d, check /proc/msdc_help\n", p3);
		}
	    }
	    else if (p1 == 1) {
		/* Get cuurent MSDC source */
		switch (id)
		{
		    case 0:
			printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
			break;
		    case 1:
			printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
			break;
		    case 2:
			printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
			break;
		    case 3:
			printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
			break;
		}
	    }
	}
	else {
	    printk("[****SD_Debug****]msdc host_id error when select clock source\n");
	}
    } else if (cmd == SD_TOOL_REG_ACCESS) {
	id = p2;
	offset = (unsigned int)p3;

	if (id >= HOST_MAX_NUM || id < 0) {
	    printk("[****SD_Debug****]msdc host_id error when modify msdc reg\n");
	} else {

            base = msdc_base[id];

	    if ((offset == 0x18 || offset == 0x1C) && p1 != 4) {
		printk("[****SD_Debug****]Err: Accessing TXDATA and RXDATA is forbidden\n");
		return count;
	    }
#ifndef FPGA_PLATFORM
	    enable_clock(msdc_cg_clk_id[id], "SD");
#endif
	    if (p1 == 0) {
		reg_value = p4;
		if (offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
		    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
		}
		else{
                    printk("[****SD_Debug****][MSDC Reg]Original:0x%x+0x%x (0x%x)\n", base, offset, sdr_read32(base+offset));
		    sdr_write32(base+offset, reg_value);
                    printk("[****SD_Debug****][MSDC Reg]Modified:0x%x+0x%x (0x%x)\n", base, offset, sdr_read32(base+offset));
		}
	    } else if (p1 == 1) {
		if (offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
		    if (offset == 0xE0)
                        printk("[****SD_Debug****][MSDC Reg]Reg:0x%x+0x%x (0x%x)\n", base, offset, msdc_dump_padctl0(id));
		    if (offset == 0xE4)
                        printk("[****SD_Debug****][MSDC Reg]Reg:0x%x+0x%x (0x%x)\n", base, offset, msdc_dump_padctl1(id));
		    if (offset == 0xE8)
                        printk("[****SD_Debug****][MSDC Reg]Reg:0x%x+0x%x (0x%x)\n", base, offset, msdc_dump_padctl2(id));
		}
		else{
                    printk("[****SD_Debug****][MSDC Reg]Reg:0x%x+0x%x (0x%x)\n", base, offset, sdr_read32(base+offset));
		}
	    } else if (p1 == 2) {
		if (offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
		    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
		}
		else{
                    msdc_set_field(base+offset, p4, p5, p6);
		}
	    }
	    else if (p1 == 3) {
		if (offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
		    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
		}
		else{
                    msdc_get_field(base+offset, p4, p5, p6);
		}
	    } else if (p1 == 4)
		msdc_dump_reg(id);
#ifndef FPGA_PLATFORM
	    disable_clock(msdc_cg_clk_id[id], "SD");
#endif
	}

    }
    else if (cmd == SD_TOOL_SET_DRIVING) {
	id = p2;
	if (id >= HOST_MAX_NUM || id < 0) {
	    printk("[****SD_Debug****]msdc host_id error when modify msdc driving\n");
	} else{
	    host = mtk_msdc_host[id];
	    if (p1 == 0) {
                if ((unsigned char)p3 > 7 || (unsigned char)p4 > 7 || (unsigned char)p5 > 7)
		    printk("[****SD_Debug****]Some drving value was not right(correct:0~7)\n");
		else{
		    #ifndef FPGA_PLATFORM
		    if (p6 == 0x33) {
			host->hw->clk_drv = (unsigned char)p3;
			host->hw->cmd_drv = (unsigned char)p4;
			host->hw->dat_drv = (unsigned char)p5;
                        msdc_set_driving(host, host->hw, 0);
		    }
		    else if (p6 == 0x18) {
			host->hw->clk_drv_sd_18 = (unsigned char)p3;
			host->hw->cmd_drv_sd_18 = (unsigned char)p4;
			host->hw->dat_drv_sd_18 = (unsigned char)p5;
                        msdc_set_driving(host, host->hw, 1);
		    }
		    #endif
		    printk("[****SD_Debug****]clk_drv=%d, cmd_drv=%d, dat_drv=%d\n", p3, p4, p5);
		}
	    } else if (p1 == 1) {
		printk("[****SD_Debug****]msdc%d: clk_drv=%d, cmd_drv=%d, dat_drv=%d\n", id, msdc_drv_mode[id].clk_drv, msdc_drv_mode[id].cmd_drv, msdc_drv_mode[id].dat_drv);
	    }
	}
    }
    else if (cmd == SD_TOOL_ENABLE_SLEW_RATE) {
	id = p1;
	host = mtk_msdc_host[id];
	if (id >= HOST_MAX_NUM || id < 0)
	    printk("[****SD_Debug****]msdc host_id error when modify msdc sr\n");
	else{

            if ((unsigned char)p2 > 1 || (unsigned char)p3 > 1 || (unsigned char)p4 > 1)
		printk("[****SD_Debug****]Some sr value was not right(correct:0(disable),1(enable))\n");
	    else{
		#ifndef FPGA_PLATFORM
                msdc_set_sr(host, p2, p3, p4);
		#endif
		printk("[****SD_Debug****]clk_sr=%d, cmd_sr=%d, dat_sr=%d\n", p2, p3, p4);
	    }
	}
    }
    else if (cmd == SD_TOOL_SET_RDTDSEL) {
	id = p1;
	if (id >= HOST_MAX_NUM || id < 0)
	    printk("[****SD_Debug****]msdc host_id error when modify msdc sr\n");
	else{
	    host = mtk_msdc_host[id];
	    if (p2 != 0 && p2 != 1) {
		printk("[****SD_Debug****]Some rd/td seletec was not right(set rd:1,set td:0)\n");
            } else if ((p2 == 0 && (unsigned char)p3 > 0xF) || (p2 == 1 && (unsigned char)p3 > 0x3F)) {
		printk("[****SD_Debug****]Some rd/td value was not right(rd:0~0x3F,td:0~0xF)\n");
	    } else{
		#ifndef FPGA_PLATFORM
                msdc_set_rdtdsel_dbg(host, p2, p3);
		#endif
		printk("[****SD_Debug****]rd/td=%d, value=%d\n", p2, p3);
	    }
	}
    }
    else if (cmd == SD_TOOL_ENABLE_SMT) {
	id = p1;
	host = mtk_msdc_host[id];
	if (id >= HOST_MAX_NUM || id < 0) {
	    printk("[****SD_Debug****]msdc host_id error when enable/disable msdc smt\n");
	} else{
	    #ifndef FPGA_PLATFORM
            msdc_set_smt(host, p2);
	    #endif
            printk("[****SD_Debug****]smt=%d\n", p2);
	}
    }
    else if (cmd == SD_TOOL_SET_TUNE) {
	id = p1;
	if (id >= HOST_MAX_NUM || id < 0)
	    printk("[****SD_Debug****]msdc host_id error when modify msdc sr\n");
	else{
	    host = mtk_msdc_host[id];
	    if (p2 != 0 && p2 != 1)
		printk("[****SD_Debug****]Some read/write cmd was not right(read:1,set:0)\n");
	    else if (p2 == 1) {
		printk("[****SD_Debug****]Tune value is\n");/* ,msdc_get_tune(host)); */
	    }
	    else if (p2 == 0) {
		if ((unsigned char)p3 > 0xF)
		    printk("[****SD_Debug****]Some tune value was not right(0~0xF\n");
		else{
		    #ifndef FPGA_PLATFORM
		    /* msdc_set_tune(host,p3); */
		    #endif
                    printk("[****SD_Debug****]set tune value=%d\n", p3);
		}
	    }
	}
    }
    else if (cmd == SD_TOOL_DESENSE) {
	if (p1 == 0) {
	    reg_value = p2;
            printk("[****SD_Debug****][De-Sense Reg]Original:0x%x(0x%x)\n", MSDC_DESENSE_REG, sdr_read32(MSDC_DESENSE_REG));
            sdr_write32(MSDC_DESENSE_REG, reg_value);
            printk("[****SD_Debug****][De-Sense Reg]Modified:0x%x(0x%x)\n", MSDC_DESENSE_REG, sdr_read32(MSDC_DESENSE_REG));
	} else if (p1 == 1) {
            printk("[****SD_Debug****][De-Sense Reg]Reg:0x%x(0x%x)\n", MSDC_DESENSE_REG, sdr_read32(MSDC_DESENSE_REG));
	} else if (p1 == 2) {
            msdc_set_field(MSDC_DESENSE_REG, p2, p3, p4);
	} else if (p1 == 3) {
            msdc_get_field(MSDC_DESENSE_REG, p2, p3, p4);
	}
    } else if (cmd == RW_BIT_BY_BIT_COMPARE) {
	id = p1;
	compare_count = p2;
	if (id >= HOST_MAX_NUM || id < 0)
	{
	    printk("[****SD_Debug****]: bad host id: %d\n", id);
	    return 0;
	}
	if (compare_count < 0)
	{
	    printk("[****SD_Debug****]: bad compare count: %d\n", compare_count);
	    return 0;
	}

	if (id == 0) /* for msdc0 */
	{
#ifdef CONFIG_MTK_EMMC_SUPPORT
	    sd_multi_rw_compare(0, COMPARE_ADDRESS_MMC, compare_count);/* test the address 0 of eMMC card, since there a little memory. */
#else
	    sd_multi_rw_compare(0, COMPARE_ADDRESS_SD, compare_count); /* test a larger address of SD card */
#endif
	} else {
	    sd_multi_rw_compare(id, COMPARE_ADDRESS_SD, compare_count);
	}
    }
    else if (cmd == SMP_TEST_ON_ONE_HOST) {
	id = p1;
	thread_num = p2;
	compare_count = p3;
	multi_address = p4;
	smp_test_on_one_host(thread_num, id, compare_count, multi_address);
    }
    else if (cmd == SMP_TEST_ON_ALL_HOST) {
	thread_num = p1;
	compare_count = p2;
	multi_address = p3;
	smp_test_on_all_host(thread_num, compare_count, multi_address);
    }
    else if (cmd == SD_TOOL_MSDC_HOST_MODE) {
	id = p2;
	if (id >= HOST_MAX_NUM || id < 0)
	    printk("[****SD_Debug****]msdc host_id error when modify msdc host mode\n");
	else {
	    if (p1 == 0) {
		if (p3 <= UHS_DDR50 && p3 >= SDHC_HIGHSPEED)
		    spd_mode = p3;
		if (p4 <= DRIVER_TYPE_D && p4 >= DRIVER_TYPE_A)
		    drv_type = p4;
		if (p5 <= MAX_CURRENT_800 && p5 >= MAX_CURRENT_200)
		    current_limit = p5;
		if (p6 <= SDXC_POWER_CONTROL && p6 >= SDXC_NO_POWER_CONTROL)
		    pw_cr = p6;
		if (spd_mode != CAPS_SPEED_NULL) {
		    switch (spd_mode) {
			case SDHC_HIGHSPEED:
			    msdc_host_mode[id] |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;
			    msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR12)&(~MMC_CAP_UHS_SDR25)&(~MMC_CAP_UHS_SDR50)&(~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
			    printk("[****SD_Debug****]host will support Highspeed\n");
			    break;
			case UHS_SDR12:
			    msdc_host_mode[id] |= MMC_CAP_UHS_SDR12;
			    msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR25)&(~MMC_CAP_UHS_SDR50)&(~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
			    printk("[****SD_Debug****]host will support UHS-SDR12\n");
			    break;
			case UHS_SDR25:
			    msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25;
			    msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR50)&(~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
			    printk("[****SD_Debug****]host will support UHS-SDR25\n");
			    break;
                        case UHS_SDR50:
			    msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_SDR50;
			    msdc_host_mode[id] &= (~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
			    printk("[****SD_Debug****]host will support UHS-SDR50\n");
			    break;
                        case UHS_SDR104:
			    msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_SDR50|MMC_CAP_UHS_SDR104;
			    msdc_host_mode2[id] |= MMC_CAP2_HS200_1_8V_SDR;
#ifdef CONFIG_EMMC_50_FEATURE
			    msdc_host_mode2[id] &= (~MMC_CAP2_HS400_1_8V_DDR);
#endif
			    printk("[****SD_Debug****]host will support UHS-SDR104\n");
			    break;
                        case UHS_DDR50:
			    msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_DDR50|MMC_CAP_1_8V_DDR;
			    printk("[****SD_Debug****]host will support UHS-DDR50\n");
			    break;
#ifdef CONFIG_EMMC_50_FEATURE
			case EMMC_HS400:
			    msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_SDR50|MMC_CAP_UHS_DDR50|MMC_CAP_1_8V_DDR|MMC_CAP_UHS_SDR104;
			    msdc_host_mode2[id] |= MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS400_1_8V_DDR;
			    printk("[****SD_Debug****]host will support EMMC_HS400\n");
			    break;
#endif
                        default:
			    printk("[****SD_Debug****]invalid sd30_mode:%d\n", spd_mode);
			    break;
		    }
		}
	    if (drv_type != CAPS_DRIVE_NULL) {
		switch (drv_type) {
                    case DRIVER_TYPE_A:
			msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_A;
			msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_C)&(~MMC_CAP_DRIVER_TYPE_D);
			printk("[****SD_Debug****]host will support DRIVING TYPE A\n");
			break;
                    case DRIVER_TYPE_B:
			msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A)&(~MMC_CAP_DRIVER_TYPE_C)&(~MMC_CAP_DRIVER_TYPE_D);
			printk("[****SD_Debug****]host will support DRIVING TYPE B\n");
			break;
                    case DRIVER_TYPE_C:
			msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_C;
			msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A)&(~MMC_CAP_DRIVER_TYPE_D);
			printk("[****SD_Debug****]host will support DRIVING TYPE C\n");
			break;
                    case DRIVER_TYPE_D:
			msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_D;
			msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A)&(~MMC_CAP_DRIVER_TYPE_C);
			printk("[****SD_Debug****]host will support DRIVING TYPE D\n");
			break;
		    default:
			printk("[****SD_Debug****]invalid drv_type:%d\n", drv_type);
			break;
		}
	    }
	    if (current_limit != CAPS_CURRENT_NULL) {
#if 0 /* cause MMC_CAP_MAX??? and MMC_CAP_SET??? removed from linux3.6 */
		switch (current_limit) {
                    case MAX_CURRENT_200:
			msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200;
			msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_400)&(~MMC_CAP_MAX_CURRENT_600)&(~MMC_CAP_MAX_CURRENT_800);
			printk("[****SD_Debug****]host will support MAX_CURRENT_200\n");
			break;
                    case MAX_CURRENT_400:
			msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400;
			msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_600)&(~MMC_CAP_MAX_CURRENT_800);
			printk("[****SD_Debug****]host will support MAX_CURRENT_400\n");
			break;
                    case MAX_CURRENT_600:
			msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400|MMC_CAP_MAX_CURRENT_600;
			msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_800);
			printk("[****SD_Debug****]host will support MAX_CURRENT_600\n");
			break;
                    case MAX_CURRENT_800:
			msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400|MMC_CAP_MAX_CURRENT_600|MMC_CAP_MAX_CURRENT_800;
			printk("[****SD_Debug****]host will support MAX_CURRENT_800\n");
			break;
                    default:
			printk("[****SD_Debug****]invalid current_limit:%d\n", current_limit);
			break;
		}
#endif
	    }
	    if (pw_cr != CAPS_POWER_NULL)
#if 0
		switch (pw_cr) {
		    case SDXC_NO_POWER_CONTROL:
			msdc_host_mode[id] &= (~MMC_CAP_SET_XPC_330) & (~MMC_CAP_SET_XPC_300) & (~MMC_CAP_SET_XPC_180);
			printk("[****SD_Debug****]host will not support SDXC power control\n");
			break;
		    case SDXC_POWER_CONTROL:
			msdc_host_mode[id] |= MMC_CAP_SET_XPC_330 | MMC_CAP_SET_XPC_300 | MMC_CAP_SET_XPC_180;
			printk("[****SD_Debug****]host will support SDXC power control\n");
			break;
		    default:
			printk("[****SD_Debug****]invalid pw_cr:%d\n", pw_cr);
			break;
		}
#endif
		printk("[****SD_Debug****]to enable the above settings, please suspend and resume the phone again\n");
	    } else {
		printk("[****SD_Debug****]msdc[%d] supports:\n", id);
		{
		    printk("[****SD_Debug****]      speed mode: ");
		    if ((msdc_host_mode[id] & MMC_CAP_MMC_HIGHSPEED) || (msdc_host_mode[id] & MMC_CAP_SD_HIGHSPEED)) printk("HS, ");
		    if (msdc_host_mode[id] & MMC_CAP_UHS_SDR12) printk("SDR12, ");
		    if (msdc_host_mode[id] & MMC_CAP_UHS_SDR25) printk("SDR25, ");
		    if (msdc_host_mode[id] & MMC_CAP_UHS_SDR50) printk("SDR50, ");
		    if (msdc_host_mode[id] & MMC_CAP_UHS_SDR104) printk("SDR104, ");
		    if (msdc_host_mode[id] & MMC_CAP_UHS_DDR50) printk("DDR50 ");
		    if (!(msdc_host_mode[id] & (MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED | MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104 | MMC_CAP_UHS_DDR50))) printk("N/A");
		    printk("\n");
		}
		{
		    printk("[****SD_Debug****]      driver_type: ");
		    if (msdc_host_mode[id] & MMC_CAP_DRIVER_TYPE_A) printk("A, ");
		    printk("B, ");
		    if (msdc_host_mode[id] & MMC_CAP_DRIVER_TYPE_C) printk("C, ");
		    if (msdc_host_mode[id] & MMC_CAP_DRIVER_TYPE_D) printk("D, ");
		    printk("\n");
		}
		{
#if 0
		    printk("[****SD_Debug****]      current limit: ");
		    if (msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_200) printk("200mA, ");
		    if (msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_400) printk("400mA, ");
		    if (msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_600) printk("600mA, ");
		    if (msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_800) printk("800mA, ");
		    if (!(msdc_host_mode[id] & (MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400 | MMC_CAP_MAX_CURRENT_600 | MMC_CAP_MAX_CURRENT_800))) printk("N/A");
		    printk("\n");
#endif
		}
		{
#if 0
		    printk("[****SD_Debug****]      power control: ");
		    if (msdc_host_mode[id] & MMC_CAP_SET_XPC_330) printk("3.3v ");
		    if (msdc_host_mode[id] & MMC_CAP_SET_XPC_300) printk("3v ");
		    if (msdc_host_mode[id] & MMC_CAP_SET_XPC_180) printk("1.8v ");
		    if (!(msdc_host_mode[id] & (MMC_CAP_SET_XPC_330 | MMC_CAP_SET_XPC_300 | MMC_CAP_SET_XPC_180))) printk("N/A");
		    printk("\n");
#endif
		}
	    }
	}
    }
    else if (cmd == SD_TOOL_DMA_STATUS)
    {
	id = p1;
	if (p2 == 0) {
	    dma_status = msdc_get_dma_status(id);
	    printk(">>>> msdc%d: dma_status=%d, ", id, dma_status);
	    if (dma_status == 0) {
		printk("DMA mode is disabled Now\n");
	    } else if (dma_status == 1) {
		printk("Write data from SD to DRAM within DMA mode\n");
	    } else if (dma_status == 2) {
		printk("Write data from DRAM to SD within DMA mode\n");
	    } else if (dma_status == -1) {
		printk("No data transaction or the device is not present until now\n");
	    }

	    if (dma_status > 0)
	    {
		dma_address = msdc_get_dma_address(id);
		if (dma_address) {
		    printk(">>>> msdc%d:\n", id);
		    p_dma_address = dma_address;
		    while (p_dma_address) {
			printk(">>>>     addr=0x%x, size=%d\n", p_dma_address->start_address, p_dma_address->size);
			if (p_dma_address->end)
			    break;
			p_dma_address = p_dma_address->next;
		    }
		} else {
		    printk(">>>> msdc%d: BD count=0\n", id);
		}
	   }
       } else if (p2 == 1) {
	   printk(">>>> msdc%d: start dma violation test\n", id);
	   g_dma_debug[id] = 1;
	   sd_multi_rw_compare(id, COMPARE_ADDRESS_SD, 3);
       }
    }
#ifdef MTK_IO_PERFORMANCE_DEBUG
    else if (cmd == MMC_PERF_DEBUG) {
	/* 1 enable; 0 disable */
	g_mtk_mmc_perf_dbg = p1;

	g_mtk_mmc_dbg_range = p2;

	if (2 == g_mtk_mmc_dbg_range) {
	    g_dbg_range_start = p3;
	    g_dbg_range_end   = p3 + p4;
	    g_check_read_write = p5;
	}
	printk("g_mtk_mmc_perf_dbg = 0x%x, g_mtk_mmc_dbg_range = 0x%x, start = 0x%x, end = 0x%x\n", g_mtk_mmc_perf_dbg, g_mtk_mmc_dbg_range, g_dbg_range_start, g_dbg_range_end);
    } else if (cmd == MMC_PERF_DEBUG_PRINT) {
	int i, j, k, num = 0;

	if (p1 == 0) {
	    g_mtk_mmc_clear = 0;
	    return count;
	}
        printk(KERN_ERR "msdc g_dbg_req_count<%d>\n", g_dbg_req_count);
	for (i = 1; i <= g_dbg_req_count; i++) {
            printk("anslysis: %s 0x%x %d block, PGh %d\n", (g_check_read_write == 18 ? "read" : "write"), (unsigned int)g_mmcqd_buf[i][298], (unsigned int)g_mmcqd_buf[i][299], (unsigned int)(g_mmcqd_buf[i][297] * 2));
	    if (g_check_read_write == 18) {
		for (j = 1; j <= g_mmcqd_buf[i][296] * 2; j++) {
		    printk("page %d:\n", num+1);
		    for (k = 0; k < 5; k++) {
			printk("%d %llu\n", k, g_req_buf[num][k]);
		    }
		    num += 1;
		}
	    }
	    printk(KERN_ERR "-------------------------------------------\n");
	    for (j = 0; j < sizeof(g_time_mark)/sizeof(char *); j++) {
                printk(KERN_ERR "%d. %llu %s\n", j, g_mmcqd_buf[i][j], g_time_mark[j]);
	    }
	    printk(KERN_ERR "===========================================\n");
	}
	if (g_check_read_write == 25) {
            printk(KERN_ERR "msdc g_dbg_write_count<%d>\n", g_dbg_write_count);
            for (i = 1; i <= g_dbg_write_count; i++) {
		printk(KERN_ERR "********************************************\n");
                printk(KERN_ERR "write count: %llu\n", g_req_write_count[i]);
		for (j = 0; j < sizeof(g_time_mark_vfs_write)/sizeof(char *); j++)
                    printk(KERN_ERR "%d. %llu %s\n", j, g_req_write_buf[i][j], g_time_mark_vfs_write[j]);
	    }
	    printk(KERN_ERR "********************************************\n");
	}
	g_mtk_mmc_clear = 0;
    }
#endif
#if defined(GPIO_SDHC_EINT_PIN) && defined(GPIO86_CARD_DETECTION_MIXED_IN_MIPI_LCD)
    else if (cmd == SD_SUSPEND_EINT_DISABLE) {
	if (GPIO_SDHC_EINT_PIN == GPIO86) {
            if (p1 == 0) {
		/* Set setting */
                if (p2 == 0) {
                    msdc_disable_eint_while_suspend = 0;
		} else {
                    msdc_disable_eint_while_suspend = 1;
		}
	    }
	     /* Get setting */
            if (msdc_disable_eint_while_suspend == 0) {
		printk("SD EINT will be enabled during suspend\n");
	    } else {
		printk("SD EINT will be disabled during suspend\n");
	    }
	}
    }
#endif

#ifdef MTK_MMC_PERFORMANCE_TEST
    else if (cmd == MMC_PERF_TEST) {
	/* 1 enable; 0 disable */
	g_mtk_mmc_perf_test = p1;
    }
#endif

#ifdef MSDC1_SDIO_UT
    else if (cmd == SDIO_UT_CARD_DET) {
	#ifdef SDIO_LIMIT_FREQ
        sdio_max_clock = p1;
	#endif
        sdio_driving = p2;
	host = mtk_msdc_host[1];
        sdio_card_inserted = 1;
	sdr_set_field(0xF0005128, (1<<1), 1);
	mb();
	sdr_set_field(0xF0005024, (1<<1), 1);
	mb();
	sdr_set_field(0xF0005380, (7<<4), 0);
	mb();

	sdr_set_field(0xF0005128, (1<<2), 1);
	mb();
	sdr_set_field(0xF0005024, (1<<2), 1);
	mb();
	sdr_set_field(0xF0005380, (7<<8), 0);
	mb();

	sdr_set_field(0xF0005124, (1<<1), 1);
	mb();
	sdr_set_field(0xF0005124, (1<<2), 1);
	mb();
	mdelay(100);
	sdr_set_field(MSDC1_PAD_CTRL_PULLSEL_CFG, 0x3F0000, 0x3E);
	/* host->mmc->rescan_disable = 0; -->Will cause KE if COMBO driver exist */
	mmc_detect_change(host->mmc, msecs_to_jiffies(200));
    }
#endif

#ifdef SDIO_LIMIT_FREQ
    else if (cmd == SDIO_UT_SET_MAX_CLK) {
        sdio_max_clock = p1;
    }
#endif

#if defined(MSDC1_SDIO_UT) || defined(AUTOK_UT)
    else if (cmd == SDIO_UT_CMD52_CMD53) {
        host = mtk_msdc_host[1];
	if (host->mmc && host->mmc->card) {

            if (p1 >= 0) {
		unsigned int *uptr;
                printk(KERN_ERR "cccr sdio_vsn: %u\n", host->mmc->card->cccr.sdio_vsn);
                printk(KERN_ERR "cccr sd_vsn: %u\n", host->mmc->card->cccr.sd_vsn);
                uptr =  &(host->mmc->card->cccr.sd_vsn);
                printk(KERN_ERR "cccr cap: %u\n", *(uptr+1));

                printk(KERN_ERR "cccr cis vendor: %u\n", (unsigned int)host->mmc->card->cis.vendor);
                printk(KERN_ERR "cccr cis devices %u\n", (unsigned int)host->mmc->card->cis.device);
                printk(KERN_ERR "cccr cis blksize: %u\n", (unsigned int)host->mmc->card->cis.blksize);
                printk(KERN_ERR "cccr cis max_dtr: %u\n", host->mmc->card->cis.max_dtr);
	    }

	    if (host->mmc->card->sdio_func[0]) {
		unsigned char sdio_tmp[16];
		    unsigned int i, j;

		mmc_claim_host(host->mmc);
		sdio_enable_func(host->mmc->card->sdio_func[0]);

                if (p1 >= 1) {
			    unsigned int addr[] = {0x100C, 0x0, 0x4, 0x8, 0xc, 0x10};
			    unsigned int *ptr;

		    printk(KERN_ERR "Dump CCCR:\n");
			    for (i = 0; i < 6; i++) {
			mmc_io_rw_extended(host->mmc->card, 0, 0, addr[i], 1, sdio_tmp, 0, 4);
                        printk(KERN_ERR "Addr %x = %x\n", addr[i], *((unsigned int*)sdio_tmp));
		    }
		}

                if (p1 >= 2) {
		    printk(KERN_ERR "Function 1\n");
			    mmc_io_rw_extended(host->mmc->card, 0, 1, 0, 1, sdio_tmp, 0, 4);
			    printk(KERN_ERR "Addr 0= %x\n", 1, *((unsigned int *)sdio_tmp));

		    #if 0
			    mmc_io_rw_extended(host->mmc->card, 0, 1, 4, 1, sdio_tmp, 0, 4);
			    printk(KERN_ERR "Addr 4= %x\n", 1, *((unsigned int *)sdio_tmp));

			    mmc_io_rw_extended(host->mmc->card, 0, 1, 8, 1, sdio_tmp, 0, 4);
			    printk(KERN_ERR "Addr 8= %x\n", 1, *((unsigned int *)sdio_tmp));

			    mmc_io_rw_extended(host->mmc->card, 0, 1, 12, 1, sdio_tmp, 0, 4);
			    printk(KERN_ERR "Addr c= %x\n", 1, *((unsigned int *)sdio_tmp));

			    mmc_io_rw_extended(host->mmc->card, 0, 1, 0, 1, sdio_tmp, 0, 16);
			    ptr = (unsigned int*)sdio_tmp;
			    for (j = 0; j < 4; j++) {
			        printk(KERN_ERR "%x ", *ptr);
				ptr++;
			    }
			    printk(KERN_ERR "\n");
			    #endif

			}
		#ifdef AUTOK_UT
                if (p1 >= 3) {
		    /* Note: autokd shall start first either by init.*.rc or manually run */
		    wait_sdio_autok_ready(host->mmc);
		}
		#endif

		mmc_release_host(host->mmc);
	    } else {
		printk(KERN_ERR "no func 0\n");
	    }
	} else {
	    if (!host->mmc) printk(KERN_ERR "no mmc\n");
	    else if (!host->mmc->card) printk(KERN_ERR "no card\n");
	}
    }
#endif

    return count;
}

static int msdc_debug_proc_read_FT_show(struct seq_file *m, void *data)
{
#if !defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
    seq_puts(m, "\n=========================================\n");
    seq_puts(m, "There is no WCN SDIO SLOT.\n");
    seq_puts(m, "=========================================\n\n");
    return 0;
#else
    int msdc_id = 0;
    unsigned int base = MSDC_0_BASE;
    unsigned char  cmd_edge;
    unsigned char  data_edge;
    unsigned char  clk_drv1 = 0, clk_drv2 = 0;
    /* unsigned cmd_drv1,cmd_drv2,dat_drv1,dat_drv2; */
    u32 cur_rxdly0;
    u8 u8_dat0, u8_dat1, u8_dat2, u8_dat3;
    u8 u8_wdat, u8_cmddat;
    u8 u8_DDLSEL;

    if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
    {
	base = MSDC_0_BASE;
	msdc_id = 0;
    }
    else if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
    {
	base = MSDC_1_BASE;
	msdc_id = 1;
    }
    #if defined(CFG_DEV_MSDC2)
    else if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
    {
	base = MSDC_2_BASE;
	msdc_id = 2;
    }
    #endif
    #if defined(CFG_DEV_MSDC3)
    else if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
    {
	base = MSDC_3_BASE;
	msdc_id = 3;
    }
    #endif

#ifndef FPGA_PLATFORM
    enable_clock(msdc_cg_clk_id[msdc_id], "SD");
#endif

    sdr_get_field((base+0x04), MSDC_IOCON_RSPL, cmd_edge);
    sdr_get_field((base+0x04), MSDC_IOCON_DSPL, data_edge);

    if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0) {
	sdr_get_field(MSDC0_PAD_CTRL_DRV_CFG, MSDC0_DRV_CK_CMD_DAT0_3_MASK, clk_drv1);
	sdr_get_field(MSDC0_PAD_CTRL_DRV_CFG, MSDC0_DRV_DAT4_7_MASK, clk_drv2);
    } else if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1) {
	sdr_get_field(MSDC1_PAD_CTRL_DRV_CFG, MSDC1_DRV_CK_CMD_DAT0_3_MASK, clk_drv1);
    }

    sdr_get_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, u8_DDLSEL);
    cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
    if (sdr_read32(MSDC_ECO_VER) >= 4)
    {
	u8_dat0 = (cur_rxdly0 >> 24) & 0x1F;
	u8_dat1 = (cur_rxdly0 >> 16) & 0x1F;
	u8_dat2 = (cur_rxdly0 >>  8) & 0x1F;
	u8_dat3 = (cur_rxdly0 >>  0) & 0x1F;
    }
    else
    {
	u8_dat0 = (cur_rxdly0 >>  0) & 0x1F;
	u8_dat1 = (cur_rxdly0 >>  8) & 0x1F;
	u8_dat2 = (cur_rxdly0 >> 16) & 0x1F;
	u8_dat3 = (cur_rxdly0 >> 24) & 0x1F;
    }

    sdr_get_field((base + 0xec), MSDC_PAD_TUNE_DATWRDLY, u8_wdat);
    sdr_get_field((base + 0xec), MSDC_PAD_TUNE_CMDRRDLY, u8_cmddat);

    seq_puts(m, "\n=========================================\n");

    seq_printf(m, "(1) WCN SDIO SLOT is at msdc<%d>\n", CONFIG_MTK_WCN_CMB_SDIO_SLOT);

    seq_puts(m, "-----------------------------------------\n");
    seq_puts(m, "(2) clk settings\n");
    seq_puts(m, "mt2601 only using internal clock\n");

    seq_puts(m, "-----------------------------------------\n");
    seq_puts(m, "(3) settings of driving current\n");

    seq_printf(m, "driving1(clk, cmd, dat0~dat3) is  <%d>\n", clk_drv1);
    if (msdc_id == 0) {
	seq_printf(m, "driving1(dat4~dat7) is  <%d>\n", clk_drv2);
    }

    seq_puts(m, "-----------------------------------------\n");
    seq_puts(m, "(4) edge settings\n");
    if (cmd_edge)
	seq_puts(m, "cmd_edge is falling\n");
    else
	seq_puts(m, "cmd_edge is rising\n");
    if (data_edge)
	seq_puts(m, "data_edge is falling\n");
    else
	seq_puts(m, "data_edge is rising\n");

    seq_puts(m, "-----------------------------------------\n");
    seq_puts(m, "(5) data delay info\n");
    seq_printf(m, "Read (MSDC_DAT_RDDLY0) is <0x%x> and (MSDC_IOCON_DDLSEL) is <0x%x>\n", cur_rxdly0, u8_DDLSEL);
    seq_printf(m, "data0<0x%x>  data1<0x%x>  data2<0x%x>  data3<0x%x>\n", u8_dat0, u8_dat1, u8_dat2, u8_dat3);
    seq_printf(m, "Write is <0x%x>\n", u8_wdat);
    seq_printf(m, "Cmd is <0x%x>\n", u8_cmddat);
    seq_puts(m, "=========================================\n\n");

    return 0;
#endif
}

static int msdc_debug_proc_write_FT(struct file *file, const char __user *buf, size_t count, loff_t *data)
{
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
    int ret;

    int i_case = 0, i_par1 =  -1, i_par2 =  -1, i_clk = 0, i_driving = 0, i_edge = 0, i_data = 0, i_delay = 0;
    u32 cur_rxdly0;
    u8 u8_dat0, u8_dat1, u8_dat2, u8_dat3;
    unsigned int base = MSDC_0_BASE;
    if (count == 0)return -1;
    if (count > 255)count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;

    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%d %d %d ", &i_case, &i_par1, &i_par2);

    if (i_par2 == -1)
	return -1;

    printk("i_case=%d i_par1=%d i_par2=%d\n", i_case, i_par1, i_par2);

    if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
	base = MSDC_0_BASE;
    if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
	base = MSDC_1_BASE;
    #if defined(CFG_DEV_MSDC2)
    if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
	base = MSDC_2_BASE;
    #endif
    #if defined(CFG_DEV_MSDC3)
    if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
	base = MSDC_3_BASE;
    #endif

    if (i_case == 1)
    {
        if (!((i_par1 == 0) || (i_par1 == 1)))
	    return -1;
	i_clk = i_par1;

	/* sdr_set_field(MSDC_PATCH_BIT0, MSDC_PATCH_BIT_CKGEN_CK, i_clk); */

	printk("i_clk=%d\n", i_clk);
    }
    else if (i_case == 2)
    {
        if (!((i_par1 >= 0) && (i_par1 <= 7)))
	    return -1;
	i_driving = i_par1;

	if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0) {
	    sdr_set_field(MSDC0_PAD_CTRL_DRV_CFG, MSDC0_DRV_CK_CMD_DAT0_3_MASK, i_driving);
	    sdr_set_field(MSDC0_PAD_CTRL_DRV_CFG, MSDC0_DRV_DAT4_7_MASK, i_driving);
	} else if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1) {
	    sdr_set_field(MSDC1_PAD_CTRL_DRV_CFG, MSDC1_DRV_CK_CMD_DAT0_3_MASK, i_driving);
	}

	printk("i_driving=%d\n", i_driving);
    }
    else if (i_case == 3)
    {
        if (!((i_par1 >= 0) && (i_par1 <= 3)))
	    return -1;
        if (!((i_par2 >= 0) && (i_par2 <= 31)))
	    return -1;
	i_data = i_par1;
	i_delay = i_par2;

	cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
	if (sdr_read32(MSDC_ECO_VER) >= 4)
	{
	    u8_dat0 = (cur_rxdly0 >> 24) & 0x1F;
	    u8_dat1 = (cur_rxdly0 >> 16) & 0x1F;
	    u8_dat2 = (cur_rxdly0 >>  8) & 0x1F;
	    u8_dat3 = (cur_rxdly0 >>  0) & 0x1F;
	}
	else
	{
	    u8_dat0 = (cur_rxdly0 >>  0) & 0x1F;
	    u8_dat1 = (cur_rxdly0 >>  8) & 0x1F;
	    u8_dat2 = (cur_rxdly0 >> 16) & 0x1F;
	    u8_dat3 = (cur_rxdly0 >> 24) & 0x1F;
	}

        if (i_data == 0) {
	    u8_dat0 = i_delay;
        } else if (i_data == 1) {
	    u8_dat1 = i_delay;
        } else if (i_data == 2) {
	    u8_dat2 = i_delay;
        } else if (i_data == 3) {
	    u8_dat3 = i_delay;
        } else if (i_data == 4) {
	    /* write data */
	    sdr_set_field((base + 0xec), MSDC_PAD_TUNE_DATWRDLY, i_delay);
        } else if (i_data == 5) {
	    /* cmd data */
	    sdr_set_field((base + 0xec), MSDC_PAD_TUNE_CMDRRDLY, i_delay);
	} else {
	    return -1;
	}

	if (sdr_read32(MSDC_ECO_VER) >= 4)
	{
	    cur_rxdly0 = ((u8_dat0 & 0x1F) << 24) | ((u8_dat1 & 0x1F) << 16) |
			 ((u8_dat2 & 0x1F) << 8)  | ((u8_dat3 & 0x1F) << 0);
	}
	else
	{
	    cur_rxdly0 = ((u8_dat3 & 0x1F) << 24) | ((u8_dat2 & 0x1F) << 16) |
			 ((u8_dat1 & 0x1F) << 8)  | ((u8_dat0 & 0x1F) << 0);
	}
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
	sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);

	printk("i_data=%d i_delay=%d\n", i_data, i_delay);
    }
    else if (i_case == 4)
    {
        if (!((i_par1 == 0) || (i_par1 == 1)))
	    return -1;
	i_edge = i_par1;

	sdr_set_field((base+0x04), MSDC_IOCON_RSPL, i_edge);
	sdr_set_field((base+0x04), MSDC_IOCON_DSPL, i_edge);

	printk("i_edge=%d\n", i_edge);
    }
    else
    {
	return -1;
    }

    return 1;

#else
    return -1;
#endif
}

#ifdef MSDC1_SDIO
#ifdef ONLINE_TUNING_DVTTEST
static int msdc_debug_proc_read_DVT_show(struct seq_file *m, void *data)
{
    return 0;
}

extern int mt_msdc_online_tuning_test(struct msdc_host *host, u32 rawcmd, u32 rawarg, u8 rw);
static int msdc_debug_proc_write_DVT(struct file *file, const char __user *buf, size_t count, loff_t *data)
{
    int ret;
    int i_msdc_id = 0;
    struct msdc_host *host;

    if (count == 0) return -1;
    if (count > (sizeof(cmd_buf) - 1)) count = sizeof(cmd_buf) - 1;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret != 0)return -1;

    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%d", &i_msdc_id);

    if ((i_msdc_id < 0) || (i_msdc_id >= HOST_MAX_NUM))
    {
	printk("[****SD_Debug****]msdc id %d out of range [0~%d]\n", i_msdc_id, HOST_MAX_NUM-1);
	return -1;
    }

    host = mtk_msdc_host[i_msdc_id];

    printk("[****SD_Debug****] Start Online Tuning DVT test\n");
    mt_msdc_online_tuning_test(host, 0, 0, 0);
    printk("[****SD_Debug****] Finish Online Tuning DVT test\n");

    return count;
}

static int msdc_DVT_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_debug_proc_read_DVT_show, inode->i_private);
}

static const struct file_operations msdc_DVT_fops = {
    .open = msdc_DVT_open,
    .write = msdc_debug_proc_write_DVT,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif  /* ONLINE_TUNING_DVTTEST */
#endif

static int msdc_tune_proc_read_show(struct seq_file *m, void *data)
{
    seq_puts(m, "\n=========================================\n");
    seq_printf(m, "sdio_enable_tune: 0x%.8x\n", sdio_enable_tune);
    seq_printf(m, "sdio_iocon_dspl: 0x%.8x\n", sdio_iocon_dspl);
    seq_printf(m, "sdio_iocon_w_dspl: 0x%.8x\n", sdio_iocon_w_dspl);
    seq_printf(m, "sdio_iocon_rspl: 0x%.8x\n", sdio_iocon_rspl);
    seq_printf(m, "sdio_pad_tune_rrdly: 0x%.8x\n", sdio_pad_tune_rrdly);
    seq_printf(m, "sdio_pad_tune_rdly: 0x%.8x\n", sdio_pad_tune_rdly);
    seq_printf(m, "sdio_pad_tune_wrdly: 0x%.8x\n", sdio_pad_tune_wrdly);
    seq_printf(m, "sdio_dat_rd_dly0_0: 0x%.8x\n", sdio_dat_rd_dly0_0);
    seq_printf(m, "sdio_dat_rd_dly0_1: 0x%.8x\n", sdio_dat_rd_dly0_1);
    seq_printf(m, "sdio_dat_rd_dly0_2: 0x%.8x\n", sdio_dat_rd_dly0_2);
    seq_printf(m, "sdio_dat_rd_dly0_3: 0x%.8x\n", sdio_dat_rd_dly0_3);
    seq_printf(m, "sdio_dat_rd_dly1_0: 0x%.8x\n", sdio_dat_rd_dly1_0);
    seq_printf(m, "sdio_dat_rd_dly1_1: 0x%.8x\n", sdio_dat_rd_dly1_1);
    seq_printf(m, "sdio_dat_rd_dly1_2: 0x%.8x\n", sdio_dat_rd_dly1_2);
    seq_printf(m, "sdio_dat_rd_dly1_3: 0x%.8x\n", sdio_dat_rd_dly1_3);
    seq_printf(m, "sdio_clk_drv: 0x%.8x\n", sdio_clk_drv);
    seq_printf(m, "sdio_cmd_drv: 0x%.8x\n", sdio_cmd_drv);
    seq_printf(m, "sdio_data_drv: 0x%.8x\n", sdio_data_drv);
    seq_printf(m, "sdio_tune_flag: 0x%.8x\n", sdio_tune_flag);
    seq_puts(m, "=========================================\n\n");

    return 0;
}

static int msdc_tune_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *data)
{
    int ret;
    int cmd, p1, p2;

    if (count == 0)return -1;
    if (count > 255)count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;

    cmd_buf[count] = '\0';
    printk("msdc Write %s\n", cmd_buf);

    if (3  == sscanf(cmd_buf, "%x %x %x", &cmd, &p1, &p2))
    {
	switch (cmd)
	{
	    case 0:
		#if defined(MSDC1_SDIO) && defined(CONFIG_SDIOAUTOK_SUPPORT)
		if (p1 && p2) {
		    /* sdio_enable_tune = 1; */
		    ettagent_init();
		} else {
		    /* sdio_enable_tune = 0; */
		    ettagent_exit();
		}
		#endif
		break;
	    case 1:/* Cmd and Data latch edge */
		sdio_iocon_rspl = p1&0x1;
		sdio_iocon_dspl = p2&0x1;
		break;
	    case 2:/* Cmd Pad/Async */
                sdio_pad_tune_rrdly = (p1&0x1F);
                sdio_pad_tune_rdly = (p2&0x1F);
		break;
	    case 3:
                sdio_dat_rd_dly0_0 = (p1&0x1F);
                sdio_dat_rd_dly0_1 = (p2&0x1F);
		break;
	    case 4:
                sdio_dat_rd_dly0_2 = (p1&0x1F);
                sdio_dat_rd_dly0_3 = (p2&0x1F);
		break;
	    case 5:/* Write data edge/delay */
                sdio_iocon_w_dspl = p1&0x1;
                sdio_pad_tune_wrdly = (p2&0x1F);
		break;
	    case 6:
                sdio_dat_rd_dly1_2 = (p1&0x1F);
                sdio_dat_rd_dly1_3 = (p2&0x1F);
		break;
	    case 7:
                sdio_clk_drv = (p1&0x7);
		break;
	    case 8:
                sdio_cmd_drv = (p1&0x7);
                sdio_data_drv = (p2&0x7);
		break;
	}
    }

    return count;
}

static int msdc_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_debug_proc_show, inode->i_private);
}

static const struct file_operations msdc_proc_fops = {
    .open = msdc_proc_open,
    .write = msdc_debug_proc_write,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int msdc_help_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_help_proc_show, inode->i_private);
}
static const struct file_operations msdc_help_fops = {
    .open = msdc_help_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int msdc_FT_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_debug_proc_read_FT_show, inode->i_private);
}

static const struct file_operations msdc_FT_fops = {
    .open = msdc_FT_open,
    .write = msdc_debug_proc_write_FT,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int msdc_tune_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_tune_proc_read_show, inode->i_private);
}
static const struct file_operations msdc_tune_fops = {
    .open = msdc_tune_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write   = msdc_tune_proc_write,
};
static int msdc_tune_flag_proc_read_show(struct seq_file *m, void *data)
{
    seq_printf(m, "0x%X\n", sdio_tune_flag);
    return 0;
}
static int msdc_tune_flag_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_tune_flag_proc_read_show, inode->i_private);
}
static const struct file_operations msdc_tune_flag_fops = {
    .open = msdc_tune_flag_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

#ifdef SDIO_HQA
u32 sdio_vio18_flag = 0;
u32 sdio_vcore1_flag = 0;
u32 sdio_vcore2_flag = 0;
u32 vio18_reg = 0;
u32 vcore1_reg = 0;
u32 vcore2_reg = 0;
extern void pmic_config_interface(unsigned int, unsigned int, unsigned int, unsigned int);
extern void pmic_read_interface(unsigned int, unsigned int *, unsigned int, unsigned int);
static ssize_t msdc_voltage_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *data)
{
    int ret;

    if (count == 0) return -1;
    if (count > (sizeof(cmd_buf) - 1)) count = sizeof(cmd_buf) - 1;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret != 0)return -1;

    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%d %d %d", &sdio_vio18_flag, &sdio_vcore1_flag, &sdio_vcore2_flag);

    /* FIX ME: confirm pmic offset and voltage calculation */
    if (sdio_vio18_flag >= 1660 && sdio_vio18_flag <= 1960)
    {
	/* Refer to PMIC spec. for RG_VIO18_CAL for VIO18 values adjustment */
	/*  */
        if (sdio_vio18_flag <= 1800) vio18_reg = (1800-sdio_vio18_flag)/20;
        else vio18_reg = (2120-sdio_vio18_flag)/20;
	/* upmu_set_rg_vio18_cal(vio18_reg); */
	pmic_config_interface(0x558, vio18_reg, 0xF, 8);
    }
    #if 0
    /* For 6572/2601, how about Vcore2? */
    if (sdio_vcore1_flag > 900 && sdio_vcore1_flag < 1200)
    {

    }
    #endif
    /* For 6572/2601, Vcore is from Vproc */
    if (sdio_vcore2_flag > 900 && sdio_vcore2_flag < 1200)
    {
	unsigned int vproc_vosel_ctrl = 0;

	/* 0.00625V per step */
	/* Originally divied by 12.5, to avoid floating-point division, amplify numerator and denominator by 4 */
	vcore2_reg = ((sdio_vcore2_flag-700)<<2)/25;

	pmic_read_interface(0x216, &vproc_vosel_ctrl, 0x1, 0x1);
	if (vproc_vosel_ctrl == 0)
	    pmic_config_interface(0x21E, vcore2_reg, 0x7F, 0);
	else if (vproc_vosel_ctrl == 1)
	    pmic_config_interface(0x220, vcore2_reg, 0x7F, 0);
    }

    return count;
}
static int msdc_voltage_flag_proc_read_show(struct seq_file *m, void *data)
{
    seq_printf(m, "vio18: 0x%d 0x%X\n", sdio_vio18_flag, vio18_reg);
    seq_printf(m, "vcore1: 0x%d 0x%X\n", sdio_vcore1_flag,  vcore1_reg);
    seq_printf(m, "vcore2: 0x%d 0x%X\n", sdio_vcore2_flag, vcore2_reg);
    return 0;
}
static int msdc_voltage_flag_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, msdc_voltage_flag_proc_read_show, inode->i_private);
}
static const struct file_operations msdc_voltage_flag_fops = {
    .open = msdc_voltage_flag_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write = msdc_voltage_proc_write,
};
#endif


int msdc_debug_proc_init(void)
{
    struct proc_dir_entry *prEntry;
    struct proc_dir_entry *tune;
    struct proc_dir_entry *tune_flag;

#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_debug", 0660, NULL, &msdc_proc_fops);
#else
    prEntry = proc_create("msdc_debug", 0440, NULL, &msdc_proc_fops);
#endif
    if (prEntry)
    {
	printk("[%s]: successfully create /proc/msdc_debug\n", __func__);
	proc_set_user(prEntry, 0, 1001);
    } else{
	printk("[%s]: failed to create /proc/msdc_debug\n", __func__);
    }

#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_help", 0660, NULL, &msdc_help_fops);
#else
    prEntry = proc_create("msdc_help", 0440, NULL, &msdc_help_fops);
#endif
    if (prEntry)
    {
	printk("[%s]: successfully create /proc/msdc_help\n", __func__);
    } else{
	printk("[%s]: failed to create /proc/msdc_help\n", __func__);
    }

#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_FT", 0660, NULL, &msdc_FT_fops);
#else
    prEntry = proc_create("msdc_FT", 0440, NULL, &msdc_FT_fops);
#endif
    if (prEntry)
    {
	printk("[%s]: successfully create /proc/msdc_FT\n", __func__);
    } else{
	printk("[%s]: failed to create /proc/msdc_FT\n", __func__);
    }

#ifdef MSDC1_SDIO
#ifdef ONLINE_TUNING_DVTTEST
#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_DVT", 0660, NULL, &msdc_DVT_fops);
#else
    prEntry = proc_create("msdc_DVT", 0440, NULL, &msdc_DVT_fops);
#endif
    if (prEntry)
    {
	printk("[%s]: successfully create /proc/msdc_DVT\n", __func__);
    } else{
	printk("[%s]: failed to create /proc/msdc_DVT\n", __func__);
    }
#endif  /* ONLINE_TUNING_DVTTEST */
#endif

    memset(msdc_drv_mode, 0, sizeof(msdc_drv_mode));
#ifndef USER_BUILD_KERNEL
    tune = proc_create("msdc_tune", 0660, NULL, &msdc_tune_fops);
#else
    tune = proc_create("msdc_tune", 0440, NULL, &msdc_tune_fops);
#endif
    if (tune)
    {
	proc_set_user(tune, 0, 1001);
	printk("[%s]: successfully create /proc/msdc_tune\n", __func__);
    } else{
	printk("[%s]: failed to create /proc/msdc_tune\n", __func__);
    }

#ifndef USER_BUILD_KERNEL
    tune_flag = proc_create("msdc_tune_flag", 0660, NULL, &msdc_tune_flag_fops);
#else
    tune_flag = proc_create("msdc_tune_flag", 0440, NULL, &msdc_tune_flag_fops);
#endif
    if (tune_flag)
    {
	proc_set_user(tune, 0, 1001);
	printk("[%s]: successfully create /proc/msdc_tune_flag\n", __func__);
    } else{
	printk("[%s]: failed to create /proc/msdc_tune_flag\n", __func__);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(msdc_debug_proc_init);
