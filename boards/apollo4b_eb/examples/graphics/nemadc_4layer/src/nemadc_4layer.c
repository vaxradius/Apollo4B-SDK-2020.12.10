//*****************************************************************************
//
//! @file nemadc_4layer.c
//!
//! @brief NemaDC example.
//!
//! This example demonstrates DC 4 layers overlay with global alpha blending.
//!   * Layer0 - Red image
//!   * Layer1 - Green image
//!   * Layer2 - Blue image
//!   * Layer3 - Yellow image
//! Global alpha value can be changed in layer*.alpha. Blendmode can be changed
//! in layer*.blendmode.
//!
//! This example can work at two different SPI interfaces. When defined ENABLE_SPI4
//! in preprocessor defined symbols, this example drives panel through SPI4 interface.
//! When defined ENABLE_QSPI in preprocessor defined symbols, this example drives
//! panel through QSPI interface.
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
#include "nema_core.h"
#include "nema_utils.h"
#include "nema_dc.h"
#include "nema_dc_mipi.h"
#include "am_util_delay.h"
#include "am_util_stdio.h"
#include "nema_dc_regs.h"
#include "tsi_malloc.h"
#include "string.h"
#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"

#define FB_RESX 200
#define FB_RESY 200

typedef volatile struct __FB32
{
    uint32_t pixels[FB_RESY][FB_RESX];
} FB32_type;

int nemadc_4layer_test()
{
    uint16_t panel_resx = g_sDispCfg[g_eDispType].ui32PanelResX; //panel's max resolution
    uint16_t panel_resy = g_sDispCfg[g_eDispType].ui32PanelResY; //panel's max resolution
    uint32_t ui32MipiCfg = MIPICFG_8RGB888_OPT0; // default config
    uint16_t minx, miny, i, j;
    FB32_type *FB32_0;
    FB32_type *FB32_1;
    FB32_type *FB32_2;
    FB32_type *FB32_3;

    nema_sys_init();
    //Initialize NemaDC
    if ( nemadc_init() != 0 )
    {
        return -2;
    }
    if ((g_sDispCfg[g_eDispType].eInterface == IF_DSI) || (g_sDispCfg[g_eDispType].bUseDPHYPLL == true))
    {
        uint8_t ui8LanesNum = g_sDsiCfg.ui8NumLanes;
        uint8_t ui8DbiWidth = g_sDsiCfg.eDbiWidth;
        uint32_t ui32FreqTrim = g_sDsiCfg.eDsiFreq;
        pixel_format_t eFormat = FMT_RGB888;
        if ( am_hal_dsi_para_config(ui8LanesNum, ui8DbiWidth, ui32FreqTrim) != 0 )
        {
            return -3;
        }
        switch ( eFormat )
        {
            case FMT_RGB888:
                if ( ui8DbiWidth == 16 )
                {
                    ui32MipiCfg = MIPICFG_16RGB888_OPT0;
                }
                if ( ui8DbiWidth ==  8 )
                {
                    ui32MipiCfg = MIPICFG_8RGB888_OPT0;
                }
                break;

            case FMT_RGB565:
                if ( ui8DbiWidth == 16 )
                {
                    ui32MipiCfg = MIPICFG_16RGB565_OPT0;
                }
                if ( ui8DbiWidth ==  8 )
                {
                    ui32MipiCfg = MIPICFG_8RGB565_OPT0;
                }
                break;

            default:
                //
                // invalid color component index
                //
                return -3;
        }
    }

    //Set the display region to center
    if ( FB_RESX > panel_resx )
    {
        minx = 0; // set the minimum value to 0
    }
    else
    {
        minx = (panel_resx - FB_RESX) >> 1;
        minx = (minx >> 1) << 1;
    }

    if ( FB_RESY > panel_resy )
    {
        miny = 0; // set the minimum value to 0
    }
    else
    {
        miny = (panel_resy - FB_RESY) >> 1;
        miny = (miny >> 1) << 1;
    }

    //Initialize the display
    switch (g_sDispCfg[g_eDispType].eInterface)
    {
        case IF_SPI4:
            am_devices_nemadc_rm67162_init(MIPICFG_SPI4, MIPICFG_1RGB565_OPT0, FB_RESX, FB_RESY, minx, miny);
            break;
        case IF_DSPI:
            am_devices_nemadc_rm67162_init(MIPICFG_DSPI | MIPICFG_SPI4, MIPICFG_2RGB888_OPT0, FB_RESX, FB_RESY, minx, miny);
            break;
        case IF_QSPI:
            am_devices_nemadc_rm67162_init(MIPICFG_QSPI | MIPICFG_SPI4, MIPICFG_4RGB565_OPT0, FB_RESX, FB_RESY, minx, miny);
            break;
        case IF_DSI:
            am_devices_dsi_rm67162_init(ui32MipiCfg, FB_RESX, FB_RESY, minx, miny);
            break;
        default:
            ; //NOP
    }

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
    memset(layer0.baseaddr_virt, 0, layer0.resy*layer0.stride);
    FB32_0 = (volatile FB32_type*) layer0.baseaddr_virt;

    nemadc_layer_t layer1;
    layer1.resx          = FB_RESX;
    layer1.resy          = FB_RESY;
    layer1.buscfg        = 0;
    layer1.format        = NEMADC_RGBA8888;
    layer1.blendmode     = (NEMADC_BF_SRCGBLALPHA | (NEMADC_BF_INVSRCGBLALPHA << 4));
    layer1.stride        = layer1.resx*4;
    layer1.startx        = 0;
    layer1.starty        = 0;
    layer1.sizex         = layer1.resx;
    layer1.sizey         = layer1.resy;
    layer1.alpha         = 0x80;
    layer1.baseaddr_virt = tsi_malloc(layer1.resy*layer1.stride);
    layer1.baseaddr_phys = (unsigned)tsi_virt2phys(layer1.baseaddr_virt);
    memset(layer1.baseaddr_virt, 0, layer1.resy*layer1.stride);
    FB32_1 = (volatile FB32_type*) layer1.baseaddr_virt;

    nemadc_layer_t layer2;
    layer2.resx          = FB_RESX;
    layer2.resy          = FB_RESY;
    layer2.buscfg        = 0;
    layer2.format        = NEMADC_RGBA8888;
    layer2.blendmode     = (NEMADC_BF_SRCGBLALPHA | (NEMADC_BF_INVSRCGBLALPHA << 4));
    layer2.stride        = layer2.resx*4;
    layer2.startx        = 0;
    layer2.starty        = 0;
    layer2.sizex         = layer2.resx;
    layer2.sizey         = layer2.resy;
    layer2.alpha         = 0x80;
    layer2.baseaddr_virt = tsi_malloc(layer2.resy*layer2.stride);
    layer2.baseaddr_phys = (unsigned)tsi_virt2phys(layer2.baseaddr_virt);
    memset(layer2.baseaddr_virt, 0, layer2.resy*layer2.stride);
    FB32_2 = (volatile FB32_type*) layer2.baseaddr_virt;

    nemadc_layer_t layer3;
    layer3.resx          = FB_RESX;
    layer3.resy          = FB_RESY;
    layer3.buscfg        = 0;
    layer3.format        = NEMADC_RGBA8888;
    layer3.blendmode     = (NEMADC_BF_SRCGBLALPHA | (NEMADC_BF_INVSRCGBLALPHA << 4));
    layer3.stride        = layer3.resx*4;
    layer3.startx        = 0;
    layer3.starty        = 0;
    layer3.sizex         = layer3.resx;
    layer3.sizey         = layer3.resy;
    layer3.alpha         = 0x80;
    layer3.baseaddr_virt = tsi_malloc(layer3.resy*layer3.stride);
    layer3.baseaddr_phys = (unsigned)tsi_virt2phys(layer3.baseaddr_virt);
    memset(layer3.baseaddr_virt, 0, layer3.resy*layer3.stride);
    FB32_3 = (volatile FB32_type*) layer3.baseaddr_virt;

    for ( j = 0; j < (FB_RESY - 60); j++ )
    {
        for (i = 0; i < (FB_RESX - 60); i++ )
        {
            FB32_0->pixels[j +  0][i +  0] = 0xFF0000FF;    //RED
            FB32_1->pixels[j + 20][i + 20] = 0xFF00FF00;    //GREEN
            FB32_2->pixels[j + 40][i + 40] = 0xFFFF0000;    //BLUE
            FB32_3->pixels[j + 60][i + 60] = 0xFF00FFFF;    //YELLOW
        }
    }
    nemadc_set_layer(0, &layer0);
    nemadc_set_layer(1, &layer1);
    nemadc_set_layer(2, &layer2);
    nemadc_set_layer(3, &layer3);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }
    nemadc_layer_disable(0);
    nemadc_layer_disable(1);
    nemadc_layer_disable(2);
    nemadc_layer_disable(3);
    tsi_free(layer0.baseaddr_virt);
    tsi_free(layer1.baseaddr_virt);
    tsi_free(layer2.baseaddr_virt);
    tsi_free(layer3.baseaddr_virt);

    return 0;
}
