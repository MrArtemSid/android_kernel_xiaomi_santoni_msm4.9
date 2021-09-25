/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "focaltech_core.h"
#include "focaltech_flash.h"
struct ft_chip_t chip_types;

u8 CTPM_FW[] = {
#include FTS_UPGRADE_FW_APP
};

#if (FTS_GET_VENDOR_ID_NUM >= 2)
u8 CTPM_FW2[] =
{
#include FTS_UPGRADE_FW2_APP
};
#endif

#if (FTS_GET_VENDOR_ID_NUM >= 3)
u8 CTPM_FW3[] = {
#include FTS_UPGRADE_FW3_APP
};
#endif

u8 aucFW_PRAM_BOOT[] = {
#ifdef FTS_UPGRADE_PRAMBOOT
#include FTS_UPGRADE_PRAMBOOT
#endif
};

u8 CTPM_LCD_CFG[] = {
#ifdef FTS_UPGRADE_LCD_CFG
#include FTS_UPGRADE_LCD_CFG
#endif
};

struct fts_upgrade_fun  fts_updatefun_curr;
struct workqueue_struct *touch_wq;
struct work_struct fw_update_work;
u8 *g_fw_file;
int g_fw_len;

void fts_ctpm_upgrade_delay(u32 i)
{
	do {
		i--;
	}
	while (i>0);
}

int fts_ctpm_i2c_hid2std(struct i2c_client *client)
{
#if (FTS_CHIP_IDC)
	return 0;
#else
	u8 buf[5] = {0};
	int bRet = 0;

	buf[0] = 0xeb;
	buf[1] = 0xaa;
	buf[2] = 0x09;
	bRet =fts_i2c_write(client, buf, 3);
	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	fts_i2c_read(client, buf, 0, buf, 3);

	if ((0xeb == buf[0]) && (0xaa == buf[1]) && (0x08 == buf[2])) {
		FTS_DEBUG("hidi2c change to stdi2c successful!!");
		bRet = 1;
	} else {
		FTS_ERROR("hidi2c change to stdi2c error!!");
		bRet = 0;
	}

	return bRet;
#endif
}

void fts_get_chip_types(void)
{
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	int ic_type = 0;

	if (sizeof(ctype) != sizeof(struct ft_chip_t)) /* only one array */
		ic_type = IC_SERIALS - 1;

	chip_types = ctype[ic_type];

	FTS_INFO("CHIP TYPE ID = 0x%02x%02x", chip_types.chip_idh, chip_types.chip_idl);
}

void fts_ctpm_get_upgrade_array(void)
{

	FTS_FUNC_ENTER();

	fts_get_chip_types();

	fts_ctpm_i2c_hid2std(fts_i2c_client);


	memcpy(&fts_updatefun_curr, &fts_updatefun, sizeof(struct fts_upgrade_fun));

	FTS_FUNC_EXIT();
}

void fts_LockDownInfo_get(struct i2c_client *client, char *pProjectCode)
{
	u8 reg_val[10] = {0};
	u32 i = 0;
	u8 j = 0;
	u8 auc_i2c_write_buf[4];
	int i_ret;

	u8 uc_tp_vendor_id;

	fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &uc_tp_vendor_id);
	FTS_DEBUG("ft5435_ctpm_LockDownInfo_get_from_boot, uc_tp_vendor_id=%x\n", uc_tp_vendor_id);

	i_ret = fts_ctpm_i2c_hid2std(client);
	if (i_ret == 0) {
		FTS_ERROR("HidI2c change to StdI2c fail ! \n");
	}
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {

		fts_i2c_write_reg(client, 0xfc, FTS_UPGRADE_AA);
		msleep(30);
		fts_i2c_write_reg(client, 0xfc, FTS_UPGRADE_55);
		msleep(200);

		i_ret = fts_ctpm_i2c_hid2std(client);
		if (i_ret == 0) {
			FTS_ERROR("HidI2c change to StdI2c fail ! \n");
		}
		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write(client, auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			FTS_ERROR("failed writing  0x55 and 0xaa ! \n");
			continue;
		}

		msleep(10);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
		if (reg_val[0] == 0x54 && reg_val[1] == 0x2c) {
			FTS_DEBUG("[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
			continue;
		}
	}
	if (i >= FTS_UPGRADE_LOOP) {
		FTS_DEBUG("ft5435_ctpm_LockDownInfo_get_from_boot, i=%d\n", i);
		return;
	}

	msleep(10);

	FTS_DEBUG("ft5435_ctpm_LockDownInfo_get_from_boot, msleep(10)\n");

	auc_i2c_write_buf[0] = 0x03;
	auc_i2c_write_buf[1] = 0x00;

	for(i = 0;i < FTS_UPGRADE_LOOP; i++) {
		auc_i2c_write_buf[2] = 0xd7;
		auc_i2c_write_buf[3] = 0xa0;
		i_ret = fts_i2c_write(client, auc_i2c_write_buf, 4);
		if (i_ret < 0) {
			FTS_ERROR( "[FTS] Step 4: read lcm id from flash error when i2c write, i_ret = %d\n", i_ret);
			continue;
		}

		msleep(10);

		i_ret = fts_i2c_read(client, auc_i2c_write_buf, 0, reg_val, 8);
		if (i_ret < 0) {
			FTS_DEBUG( "[FTS] Step 4: read lcm id from flash error when i2c write, i_ret = %d\n", i_ret);
			continue;
		}

		for(j = 0; j < 8; j++) {
			FTS_DEBUG("%s: REG VAL = 0x%02x, j=%d\n", __func__, reg_val[j], j);
		}

				sprintf(pProjectCode, "%02x%02x%02x%02x%02x%02x%02x%02x", \
					reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6], reg_val[7]);
			FTS_DEBUG("%s:%s\n", __func__, pProjectCode);
		break;
	}

	msleep(50);

	FTS_DEBUG("Step 5: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(200);                        /* make sure CTP startup normally */
	i_ret = fts_ctpm_i2c_hid2std(client);   /* Android to Std i2c. */
	if (i_ret == 0) {
		FTS_ERROR("HidI2c change to StdI2c fail ! \n");
	}
	msleep(10);
	return;
}

void fts_ctpm_rom_or_pram_reset(struct i2c_client *client)
{
	u8 rst_cmd = FTS_REG_RESET_FW;

	FTS_INFO("[UPGRADE]******Reset to romboot/bootloader******");
	fts_i2c_write(client, &rst_cmd, 1);

	msleep(300);
}

int fts_ctpm_auto_clb(struct i2c_client *client)
{
#if FTS_AUTO_CLB_EN
	u8 uc_temp = 0x00;
	u8 i = 0;


	msleep(200);

	fts_i2c_write_reg(client, 0, FTS_REG_WORKMODE_FACTORY_VALUE);

	msleep(100);

	fts_i2c_write_reg(client, 2, 0x4);
	msleep(300);
	if ((chip_types.chip_idh==0x11) ||(chip_types.chip_idh==0x12) ||(chip_types.chip_idh==0x13) ||(chip_types.chip_idh==0x14)) {
		for (i=0; i<100; i++) {
			fts_i2c_read_reg(client, 0x02, &uc_temp);
			if (0x02 == uc_temp ||
				0xFF == uc_temp) {
				break;
			}
			msleep(20);
		}
	} else {
		for (i=0; i<100; i++) {
			fts_i2c_read_reg(client, 0, &uc_temp);
			if (0x0 == ((uc_temp&0x70)>>4)) {
				break;
			}
			msleep(20);
		}
	}
	fts_i2c_write_reg(client, 0, 0x40);
	msleep(200);
	fts_i2c_write_reg(client, 2, 0x5);
	msleep(300);
	fts_i2c_write_reg(client, 0, FTS_REG_WORKMODE_WORK_VALUE);
	msleep(300);
#endif

	return 0;
}

int fts_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[FILE_NAME_LENGTH];

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTXXXX_INI_FILEPATH_CONFIG, firmware_name);
	if (NULL == pfile) {
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_ERROR("error occured while opening file %s", filepath);
		return -EIO;
	}
	inode = pfile->f_path.dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

int fts_ReadFirmware(char *firmware_name, u8 *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[FILE_NAME_LENGTH];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTXXXX_INI_FILEPATH_CONFIG, firmware_name);
	if (NULL == pfile) {
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_ERROR("[UPGRADE] Error occured while opening file %s.\n", filepath);
		return -EIO;
	}
	inode = pfile->f_path.dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);
	return 0;
}

u32 fts_getsize(u8 fw_type)
{
	int fw_len = 0;

	if (fw_type == FW_SIZE)
		fw_len = sizeof(CTPM_FW);
#if (FTS_GET_VENDOR_ID_NUM >= 2)
	else if (fw_type == FW2_SIZE)
		fw_len = sizeof(CTPM_FW2);
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 3)
	else if (fw_type == FW3_SIZE)
		fw_len = sizeof(CTPM_FW3);
#endif
#if FTS_CHIP_IDC
	else if (fw_type == PRAMBOOT_SIZE)
		fw_len = sizeof(aucFW_PRAM_BOOT);
#endif
#if (FTS_CHIP_TYPE == _FT8006)
	else if (fw_type == LCD_CFG_SIZE)
		fw_len = sizeof(CTPM_LCD_CFG);
#endif

	return fw_len;
}

enum FW_STATUS fts_ctpm_get_pram_or_rom_id(struct i2c_client *client)
{
	u8 buf[4];
	u8 reg_val[2] = {0};
	enum FW_STATUS inRomBoot = FTS_RUN_IN_ERROR;

	fts_ctpm_i2c_hid2std(client);



	buf[0] = FTS_UPGRADE_55;
	buf[1] = FTS_UPGRADE_AA;
	fts_i2c_write(client, buf, 2);

	msleep(20);

	buf[0] = 0x90;
	buf[1] = buf[2] = buf[3] =0x00;
	fts_i2c_read(client, buf, 4, reg_val, 2);

	FTS_DEBUG("[UPGRADE] Read ROM/PRAM/Bootloader id:0x%02x%02x", reg_val[0], reg_val[1]);
	if ((reg_val[0] == 0x00) || (reg_val[0] == 0xFF)) {
		inRomBoot = FTS_RUN_IN_ERROR;
	} else if (reg_val[0] == chip_types.pramboot_idh && reg_val[1] == chip_types.pramboot_idl) {
		inRomBoot = FTS_RUN_IN_PRAM;
	} else if (reg_val[0] == chip_types.rom_idh && reg_val[1] == chip_types.rom_idl) {
		inRomBoot = FTS_RUN_IN_ROM;
	} else if (reg_val[0] == chip_types.bootloader_idh && reg_val[1] == chip_types.bootloader_idl) {
		inRomBoot = FTS_RUN_IN_BOOTLOADER;
	}

	return inRomBoot;
}

int fts_ctpm_get_i_file(struct i2c_client *client, int fw_valid)
{
	int ret;

	if (fts_updatefun_curr.get_i_file)
		ret = fts_updatefun_curr.get_i_file(client, fw_valid);
	else
		ret = -EIO;

	return ret;
}

int fts_ctpm_get_app_ver(void)
{
	int i_ret = 0;

	if (fts_updatefun_curr.get_app_i_file_ver)
		i_ret = fts_updatefun_curr.get_app_i_file_ver();

	return i_ret;
}

int fts_ctpm_fw_upgrade(struct i2c_client *client)
{
	int i_ret = 0;

	if (fts_updatefun_curr.upgrade_with_app_i_file)
		i_ret = fts_updatefun_curr.upgrade_with_app_i_file(client);

	return i_ret;
}

int fts_ctpm_lcd_cfg_upgrade(struct i2c_client *client)
{
	int i_ret = 0;

	if (fts_updatefun_curr.upgrade_with_lcd_cfg_i_file)
		i_ret = fts_updatefun_curr.upgrade_with_lcd_cfg_i_file(client);

	return i_ret;
}

#if (!(FTS_UPGRADE_STRESS_TEST))
static int fts_ctpm_check_fw_status(struct i2c_client *client)
{
	u8 chip_id1 = 0;
	u8 chip_id2 = 0;
	int fw_status = FTS_RUN_IN_ERROR;
	int i = 0;
	int ret = 0;
	int i2c_noack_retry = 0;

	for (i = 0; i < 5; i++) {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id1);
		if (ret < 0) {
			i2c_noack_retry++;
			continue;
		}
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id2);
		if (ret < 0) {
			i2c_noack_retry++;
			continue;
		}

		if ((chip_id1 == chip_types.chip_idh)
#if FTS_CHIP_IDC
			&& (chip_id2 == chip_types.chip_idl)
#endif
		   ) {
			fw_status = FTS_RUN_IN_APP;
			break;
		}
	}

	FTS_DEBUG("[UPGRADE]: chip_id = %02x%02x, chip_types.chip_idh = %02x%02x",
			 chip_id1, chip_id2, chip_types.chip_idh, chip_types.chip_idl);
	if (i2c_noack_retry >= 5)
		return -EIO;
	if (i >= 5) {
		fw_status = fts_ctpm_get_pram_or_rom_id(client);
	}

	return fw_status;
}

static int fts_ctpm_check_fw_ver(struct i2c_client *client)
{
	u8 uc_tp_fm_ver;
	u8 uc_host_fm_ver = 0;

	fts_i2c_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
	uc_host_fm_ver = fts_ctpm_get_app_ver();

	FTS_DEBUG("[UPGRADE]: uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x!!", uc_tp_fm_ver, uc_host_fm_ver);
	if (uc_tp_fm_ver < uc_host_fm_ver ) {
		return 1;
	} else {
		return 0;
	}
}

static int fts_ctpm_check_need_upgrade(struct i2c_client *client) {
	int fw_status = 0;
	int bUpgradeFlag = false;

	FTS_FUNC_ENTER();


	fw_status = fts_ctpm_check_fw_status(client);
	FTS_DEBUG( "[UPGRADE]: fw_status = %d!!", fw_status);
	if (fw_status < 0) {

		FTS_ERROR("[UPGRADE]******I2C NO ACK,exit upgrade******");
		return -EIO;
	} else if (fw_status == FTS_RUN_IN_ERROR) {
		FTS_ERROR("[UPGRADE]******IC Type Fail******");
	} else if (fw_status == FTS_RUN_IN_APP) {
		FTS_INFO("[UPGRADE]**********FW APP valid**********");

		if (fts_ctpm_get_i_file(client, 1) != 0) {
			FTS_DEBUG("[UPGRADE]******Get upgrade file(fw) fail******");
			return -EIO;
		}

		if (fts_ctpm_check_fw_ver(client) == 1) {
			FTS_DEBUG("[UPGRADE]**********need upgrade fw**********");
			bUpgradeFlag = true;
		} else {
			FTS_DEBUG("[UPGRADE]**********Don't need upgrade fw**********");
			bUpgradeFlag = false;
		}
	} else {

		FTS_INFO("[UPGRADE]**********FW APP invalid**********");
		fts_ctpm_rom_or_pram_reset(client);
		if (fts_ctpm_get_i_file(client, 0) != 0) {
			FTS_DEBUG("[UPGRADE]******Get upgrade file(flash) fail******");
			fts_ctpm_rom_or_pram_reset(client);
			return -EIO;
		}
		fts_ctpm_rom_or_pram_reset(client);
		bUpgradeFlag = true;
	}

	FTS_FUNC_EXIT();

	return bUpgradeFlag;
}

int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_tp_fm_ver;
	int i_ret = 0;
	int bUpgradeFlag = false;
	u8 uc_upgrade_times = 0;
	char tp_info_buff[128];

	FTS_DEBUG("[UPGRADE]********************check upgrade need or not********************");
	bUpgradeFlag = fts_ctpm_check_need_upgrade(client);
	FTS_DEBUG("[UPGRADE]**********bUpgradeFlag = 0x%x**********", bUpgradeFlag);

	if (bUpgradeFlag <= 0) {
		FTS_DEBUG("[UPGRADE]**********No Upgrade, exit**********");
		return 0;
	} else {

		do {
			uc_upgrade_times++;
			FTS_DEBUG("[UPGRADE]********************star upgrade(%d)********************", uc_upgrade_times);

			i_ret = fts_ctpm_fw_upgrade(client);
			if (i_ret == 0) {
				fts_i2c_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
		sprintf(tp_info_buff, "[FW]0x%02x, [IC]FT5435", uc_tp_fm_ver);
				FTS_DEBUG("[UPGRADE]********************Success upgrade to new fw version 0x%x********************", uc_tp_fm_ver);

				fts_ctpm_auto_clb(client);
				break;
			} else {
				FTS_ERROR("[UPGRADE]********************upgrade fail, reset now********************");
				fts_ctpm_rom_or_pram_reset(client);
			}
		}
		while (uc_upgrade_times < 2);
	}

	return i_ret;
}
#endif

#if FTS_AUTO_UPGRADE_EN
static void fts_ctpm_update_work_func(struct work_struct *work)
{
	int i_ret = 0;

	FTS_DEBUG( "[UPGRADE]******************************FTS enter upgrade******************************");
	fts_irq_disable();


#if FTS_ESDCHECK_EN
	fts_esdcheck_switch(DISABLE);
#endif

	i_ret = fts_ctpm_auto_upgrade(fts_i2c_client);
	if (i_ret < 0)
		FTS_ERROR("[UPGRADE]**********TP FW upgrade failed**********");

#if FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN
	msleep(2000);

	i_ret = fts_ctpm_lcd_cfg_upgrade(fts_i2c_client);
	if (i_ret < 0)
		FTS_ERROR("[UPGRADE]**********LCD cfg upgrade failed*********");
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_switch(ENABLE);
#endif
	fts_irq_enable();

	FTS_DEBUG( "[UPGRADE]******************************FTS exit upgrade******************************");
}

void fts_ctpm_upgrade_init(void)
{
	FTS_FUNC_ENTER();

	touch_wq = create_singlethread_workqueue("touch_wq");
	if (touch_wq) {
		INIT_WORK(&fw_update_work, fts_ctpm_update_work_func);
		queue_work(touch_wq, &fw_update_work);
	} else {
		FTS_ERROR("[UPGRADE]create_singlethread_workqueue failed\n");
	}

	FTS_FUNC_EXIT();
}

void fts_ctpm_upgrade_exit(void)
{
	FTS_FUNC_ENTER();
	destroy_workqueue(touch_wq);
	FTS_FUNC_EXIT();
}
int fts_ctpm_fw_upgrade_ReadBootloadorID(struct i2c_client *client)
{
	u8 reg_val[4] = { 0 };
	u32 i = 0;
	int i_ret;
	u8 auc_i2c_write_buf[10];
	fts_ctpm_i2c_hid2std(client);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {

		fts_i2c_write_reg(client, 0xfc, FTS_UPGRADE_AA);
		msleep(30);
		fts_i2c_write_reg(client, 0xfc, FTS_UPGRADE_55);
		msleep(200);

		fts_ctpm_i2c_hid2std(client);
		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write(client, auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			FTS_DEBUG("failed writing  0x55 and 0xaa!!");
			continue;
		}
		msleep(10);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
			0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
		if (reg_val[0] == chip_types.chip_idh) {
			FTS_DEBUG("[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x!!",
					 reg_val[0], reg_val[1]);
			return 0;
		} else {
			FTS_DEBUG("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x!!",
					 reg_val[0], reg_val[1]);
			continue;
		}
	}
	 return 1 ;

}




#endif  /* #if FTS_AUTO_UPGRADE_EN */
