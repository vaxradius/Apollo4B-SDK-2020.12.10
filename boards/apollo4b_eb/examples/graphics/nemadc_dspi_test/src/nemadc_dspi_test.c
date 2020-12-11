//*****************************************************************************
//
//! @file nemadc_dspi_test.c
//!
//! @brief NemaDC example.
//!
//! This example demonstrates how to drive display panel with 1P1T 2-wire
//! Dual-SPI interface.
//!
//! 1P1T 2-wire Dual-SPI interface includes 4 signals,
//!   * Chip select (CSX)
//!   * SPI clock (CLK)
//!   * Data interface 0 (DATA0)
//!   * Data interface 1 (DATA1).
//!
//! During the write sequence the display controller writes one or more bytes of
//! information to the display module via the interface. The write sequence is
//! initiated when CSX is driven from high to low and ends when CSX is pulled high.
//! Dual-SPI reuses SPI4 DCX as the second DATA signal (DATA1), and sends 4 data
//! bits per clock cycle. In this example, when send commands, SPI interface works
//! at SPI4 mode. When send frame data, SPI interface works at Dual-SPI mode. Panel
//! must be set to Dual-SPI mode through writing register in panel driver IC before
//! sending frame data.
//!
//! When define TESTMODE_EN to 1 in nemadc_dspi_test.c, this example runs at test pattern mode.
//! When define TESTMODE_EN to 0, this example runs at image display mode.
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

// -----------------------------------------------------------------------------
// Copyright (c) 2019 Think Silicon S.A.
// Think Silicon S.A. Confidential Proprietary
// -----------------------------------------------------------------------------
//     All Rights reserved - Unpublished -rights reserved under
//         the Copyright laws of the European Union
//
//  This file includes the Confidential information of Think Silicon S.A.
//  The receiver of this Confidential Information shall not disclose
//  it to any third party and shall protect its confidentiality by
//  using the same degree of care, but not less than a reasonable
//  degree of care, as the receiver uses to protect receiver's own
//  Confidential Information. The entire notice must be reproduced on all
//  authorised copies and copies may only be made to the extent permitted
//  by a licensing agreement from Think Silicon S.A..
//
//  The software is provided 'as is', without warranty of any kind, express or
//  implied, including but not limited to the warranties of merchantability,
//  fitness for a particular purpose and noninfringement. In no event shall
//  Think Silicon S.A. be liable for any claim, damages or other liability, whether
//  in an action of contract, tort or otherwise, arising from, out of or in
//  connection with the software or the use or other dealings in the software.
//
//
//                    Think Silicon S.A.
//                    http://www.think-silicon.com
//                    Patras Science Park
//                    Rion Achaias 26504
//                    Greece
// -----------------------------------------------------------------------------

#include "am_bsp.h"
#include "nema_hal.h"
#include "nema_dc.h"
#include "nema_dc_mipi.h"
#include "nema_dc_regs.h"
#include "am_util_delay.h"
#include "am_util_stdio.h"
#include "tsi_malloc.h"
#include "nema_dc.h"
#include "oli_200x200_rgba.h"
#include "string.h"
#include "am_devices_nemadc_rm67162.h"

#define TESTMODE_EN 1
#if TESTMODE_EN
    #define FB_RESX 360
    #define FB_RESY 360
#else
    #define FB_RESX 200
    #define FB_RESY 200
#endif

//------------------------------------------------------------------------------
/**
 * Test DSPI interface
 * @param pixel_format Panel pixel format
 */
void test_MIPI_dspi(int pixel_format)
{
    uint16_t panel_resx = g_sDispCfg[g_eDispType].ui32PanelResX; //panel's max resolution
    uint16_t panel_resy = g_sDispCfg[g_eDispType].ui32PanelResY; //panel's max resolution
    uint16_t minx, miny;

    //Set the display region to center of panel
    if ( FB_RESX > panel_resx )
    {
        minx = 0;   // set the minimum value to 0
    }
    else
    {
        minx = (panel_resx - FB_RESX) >> 1;
        minx = (minx >> 1) << 1;
    }

    if ( FB_RESY > panel_resy )
    {
        miny = 0;   // set the minimum value to 0
    }
    else
    {
        miny = (panel_resy - FB_RESY) >> 1;
        miny = (miny >> 1) << 1;
    }

    am_devices_nemadc_rm67162_init(MIPICFG_DSPI | MIPICFG_SPI4, pixel_format, FB_RESX, FB_RESY, minx, miny);

#if TESTMODE_EN
    nemadc_layer_enable(0);
    uint32_t dbi_cfg = nemadc_reg_read( NEMADC_REG_DBIB_CFG);
    nemadc_MIPI_CFG_out( dbi_cfg & (~MIPICFG_DSPI) & (~MIPICFG_QSPI));
    // Start MIPI Panel Memory Write
    nemadc_MIPI_out(MIPI_DBIB_CMD | MIPI_write_memory_start);
    nemadc_MIPI_CFG_out(((dbi_cfg & (~MIPICFG_SPI4)) | MIPICFG_SPI3)
                       | MIPICFG_SPIDC_DQSPI | MIPICFG_SPI_HOLD);
    // Send One Frame
    nemadc_set_mode(NEMADC_ONE_FRAME | NEMADC_TESTMODE);
    // Wait for transfer to be completed
    nemadc_wait_vsync();
    nemadc_MIPI_CFG_out(dbi_cfg);
#else
    // send layer 0 to display via NemaDC
    nemadc_layer_t layer0;
    layer0.resx          = FB_RESX;
    layer0.resy          = FB_RESY;
    layer0.buscfg        = 0;
    layer0.format        = NEMADC_RGBA8888;
    layer0.blendmode     = NEMADC_BL_SRC;
    layer0.stride        = layer0.resx*4;
    layer0.startx        = 0;
    layer0.starty        = 0;
    layer0.sizex         = layer0.resx;
    layer0.sizey         = layer0.resy;
    layer0.alpha         = 0xff;
    layer0.baseaddr_virt = tsi_malloc(layer0.resy*layer0.stride);
    layer0.baseaddr_phys = (unsigned)tsi_virt2phys(layer0.baseaddr_virt);

    memcpy((char*)layer0.baseaddr_virt, oli_200x200_rgba, sizeof(oli_200x200_rgba));

    // Program NemaDC Layer0
    nemadc_set_layer(0, &layer0); //This function includes layer enable.
    nemadc_send_frame_single();
    tsi_free(layer0.baseaddr_virt);
#endif
}


//-------------------------------------------
int nemadc_dspi_test()
{
    nema_sys_init();

    //Initialize NemaDC
    if ( nemadc_init() != 0 )
    {
        return -2;
    }
    if (g_sDispCfg[g_eDispType].bUseDPHYPLL == true)
    {
        uint8_t ui8LanesNum = g_sDsiCfg.ui8NumLanes;
        uint8_t ui8DbiWidth = g_sDsiCfg.eDbiWidth;
        uint32_t ui32FreqTrim = g_sDsiCfg.eDsiFreq;
        if ( am_hal_dsi_para_config(ui8LanesNum, ui8DbiWidth, ui32FreqTrim) != 0 )
        {
            return -3;
        }
    }

//-------------------------------------------
    unsigned int config;

    config = nemadc_get_config();
    if ((config&NEMADC_CFG_DBIB))
    {
        //---------------------------------------------------------------------------------------------------------------
        test_MIPI_dspi(MIPICFG_2RGB888_OPT0);
        am_util_delay_ms(1000);
        test_MIPI_dspi(MIPICFG_2RGB666_OPT0);
        am_util_delay_ms(1000);
        test_MIPI_dspi(MIPICFG_2RGB565_OPT0);
        am_util_delay_ms(1000);
        //test_MIPI_dspi(MIPICFG_2RGB444_OPT0);
        //---------------------------------------------------------------------------------------------------------------
    }

    return 1;
}
//-------------------------------------------
