/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <string.h>

#include "cluster.h"
#include "di.h"
#include "exocfg.h"
#include "flow.h"
#include "mc.h"
#include "nxboot.h"
#include "se.h"
#include "timers.h"

void nxboot_finish(uint32_t boot_memaddr) {
    volatile tegra_se_t *se = se_get_regs();
    
    /* Clear used keyslots. */
    clear_aes_keyslot(KEYSLOT_SWITCH_PACKAGE2KEY);
    clear_aes_keyslot(KEYSLOT_SWITCH_RNGKEY);
    
    /* Lock keyslots. */
    set_aes_keyslot_flags(KEYSLOT_SWITCH_MASTERKEY, 0xFF);
    if (MAILBOX_EXOSPHERE_CONFIGURATION->target_firmware < EXOSPHERE_TARGET_FIRMWARE_400) {
        set_aes_keyslot_flags(KEYSLOT_SWITCH_DEVICEKEY, 0xFF);
    } else {
        set_aes_keyslot_flags(KEYSLOT_SWITCH_4XOLDDEVICEKEY, 0xFF);
    }
    
    /* Finalize the GPU UCODE carveout. */
    mc_config_carveout_finalize();
    
    /* Lock AES keyslots. */
    for (uint32_t i = 0; i < 16; i++)
        set_aes_keyslot_flags(i, 0x15);
    
    /* Lock RSA keyslots. */
    for (uint32_t i = 0; i < 2; i++)
        set_rsa_keyslot_flags(i, 1);
    
    /* Lock the Security Engine. */
    se->_0x4 = 0;
    se->AES_KEY_READ_DISABLE_REG = 0;
    se->RSA_KEY_READ_DISABLE_REG = 0;
    se->_0x0 &= 0xFFFFFFFB;
    
    /* Boot up Exosphère. */
    MAILBOX_NX_BOOTLOADER_IS_SECMON_AWAKE = 0;
    if (MAILBOX_EXOSPHERE_CONFIGURATION->target_firmware < EXOSPHERE_TARGET_FIRMWARE_400) {
        MAILBOX_NX_BOOTLOADER_SETUP_STATE = NX_BOOTLOADER_STATE_LOADED_PACKAGE2;
    } else {
        MAILBOX_NX_BOOTLOADER_SETUP_STATE = NX_BOOTLOADER_STATE_DRAM_INITIALIZED_4X;
    }

    /* Terminate the display. */
    display_end();
    
    /* Boot CPU0. */
    cluster_boot_cpu0(boot_memaddr);
    
    /* Wait for Exosphère to wake up. */
    while (MAILBOX_NX_BOOTLOADER_IS_SECMON_AWAKE == 0) {
        udelay(1);
    }
    
    /* Signal Exosphère. */
    if (MAILBOX_EXOSPHERE_CONFIGURATION->target_firmware < EXOSPHERE_TARGET_FIRMWARE_400) {
        MAILBOX_NX_BOOTLOADER_SETUP_STATE = NX_BOOTLOADER_STATE_FINISHED;
    } else {
        MAILBOX_NX_BOOTLOADER_SETUP_STATE = NX_BOOTLOADER_STATE_FINISHED_4X;
    }

    /* Halt ourselves in waitevent state. */
    while (1) {
        FLOW_CTLR_HALT_COP_EVENTS_0 = 0x50000000;
    }
}