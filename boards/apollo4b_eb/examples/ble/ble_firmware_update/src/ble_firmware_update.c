//*****************************************************************************
//
//! @file ble_firmware_update.c
//!
//! @brief This is the application just for updating built-in BLE firmware into Cooper.
//!
//!
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2020, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision b0-release-20201110-399-g4728b5a52 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "am_devices_cooper.h"
#include "ble_fw_image.h"

//*****************************************************************************
//
// Configuration options
//
//*****************************************************************************

//*****************************************************************************
//
// SPI configuration.
//
//*****************************************************************************
#if defined(COOPER_QFN)
#define SPI_MODULE           0
//
// we need to slow down SPI clock for fly-wire case in between
// Apollo3/3p/4 EB/EVB and Cooper QFN part. 8MHz is chosen conservatively.
//
#define COOPER_IOM_FREQ         AM_HAL_IOM_8MHZ
#else
#define SPI_MODULE           4
#define COOPER_IOM_FREQ         AM_HAL_IOM_24MHZ
#endif

//*****************************************************************************
//
// Global Variables
//
//*****************************************************************************
uint32_t DMATCBBuffer[2048];

void *g_IomDevHdl;
void *g_pvHciSpiHandle;

static am_devices_cooper_sbl_update_data_t     g_sBLEFwImage =
{
    (uint8_t*)& ble_fw_image_bin,
    sizeof(ble_fw_image_bin),
    AM_DEVICES_COOPER_SBL_UPDATE_IMAGE_TYPE_FW,
    AM_DEVICES_COOPER_SBL_FW_VERSION
};

#define COOPER_IOM_IRQn           ((IRQn_Type)(IOMSTR0_IRQn + SPI_MODULE))

#if defined(AM_PART_APOLLO4B)
uint8_t hci_vsc_write_fw_signature_cmd[1 + 2 + 1 + 4] = {0x01, 0x75, 0xFC, 0x04, 0x6A, 0x87, 0x93, 0xC5};
static uint32_t reset_with_sbl_check(am_devices_cooper_config_t* pDevConfig)
{
    uint32_t sbl_status = 0;
    am_devices_cooper_reset();
    am_devices_cooper_image_update_init(g_IomDevHdl, pDevConfig->pNBTxnBuf);
    sbl_status = AM_DEVICES_COOPER_SBL_STATUS_INIT;
    sbl_status = am_devices_cooper_update_image();
    while( (sbl_status != AM_DEVICES_COOPER_SBL_STATUS_OK) && ( sbl_status != AM_DEVICES_COOPER_SBL_STATUS_FAIL) )
    {
        while (am_devices_cooper_irq_read(g_IomDevHdl) == 0)
        {
            am_util_delay_us(50);
        }
        sbl_status = am_devices_cooper_update_image();
    }
    //
    // Return the status.
    //
    if (sbl_status == AM_DEVICES_COOPER_SBL_STATUS_OK)
    {
        // need to wait a bit to jump from SBL to Cooper application firmware
        am_util_delay_ms(10);
        am_util_stdio_printf("SBL Done\r\n");
        return AM_DEVICES_COOPER_STATUS_SUCCESS;
    }
    else
    {
        // free up resource that won't be used.
        am_devices_cooper_term(g_IomDevHdl);
        g_IomDevHdl = NULL;
        g_pvHciSpiHandle = NULL;
        am_util_stdio_printf("SBL Cooper Error 0x%x\r\n", sbl_status);
        return AM_DEVICES_COOPER_STATUS_ERROR;
    }
}
#endif

//*****************************************************************************
//
// Main
//
//*****************************************************************************
int
main(void)
{
    uint32_t ui32Status;

    //
    // Default setup.
    //
    am_bsp_low_power_init();

    //
    // Enable the ITM
    //
    am_bsp_itm_printf_enable();

    //
    // Print the banner.
    //
    am_util_stdio_terminal_clear();

    am_util_stdio_printf("BLE Firmware Update Application\n");

    am_devices_cooper_config_t stCooperConfig;
    stCooperConfig.ui32ClockFreq = COOPER_IOM_FREQ;
    stCooperConfig.pNBTxnBuf = DMATCBBuffer;
    stCooperConfig.ui32NBTxnBufLength = sizeof(DMATCBBuffer) / 4;

    // Update firmware image information to SBL.
    am_devices_cooper_get_FwImage(&g_sBLEFwImage);

    //
    // Initialize the SPI module.
    //
    ui32Status = am_devices_cooper_init(SPI_MODULE, &stCooperConfig, &g_IomDevHdl, &g_pvHciSpiHandle);
    if ( ui32Status )
    {
        am_util_stdio_printf("BLE firmware update fails!\n");
    }
    else
    {
        am_util_stdio_printf("BLE firmware update Done!\n");
    }

#if defined(AM_PART_APOLLO4B)
    // For fw patching
    // write HCI command to trigger Cooper to reboot for SBL to do download.
    am_devices_cooper_blocking_write(g_IomDevHdl, AM_DEVICES_COOPER_RAW,
                        (uint32_t*)&hci_vsc_write_fw_signature_cmd,
                        sizeof(hci_vsc_write_fw_signature_cmd));

    // wait for finish writing
    am_util_delay_ms(100);

    // reset Cooper to get SBL to update info1
    // am_util_stdio_printf("Reset Cooper for info1 updating\r\n");
    ui32Status = reset_with_sbl_check(&stCooperConfig);
    if ( ui32Status )
    {
        am_util_stdio_printf("am_devices_cooper_init fails on update\r\n");
        return -1;
    }
#endif

    while(1);
}
