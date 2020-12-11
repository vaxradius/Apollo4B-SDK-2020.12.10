//*****************************************************************************
//
//! @file main.c
//!
//! @brief NemaGFX example.
//! In computer graphics, a color gradient specifies a range of position-dependent
//! colors, usually used to fill a region. For example, many window managers
//! allow the screen background to be specified as a gradient. The colors
//! produced by a gradient vary continuously with the position, producing smooth
//! color transitions.

//!
//! AM_DEBUG_PRINTF
//! If enabled, debug messages will be sent over ITM.
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
#include "am_bsp.h"
#include "nema_core.h"
#include "nema_utils.h"
#include "am_hal_global.h"
#include "string.h"

#ifndef DONT_USE_NEMADC
#include "nema_dc.h"
#include "nema_dc_mipi.h"
#endif

#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"
#include "nema_programHW.h"

static const unsigned long OWHITE    = 0xFFFFFFFF;
static const unsigned long BLACK     = 0x00000000;

#define OPAQUE(color)         ((color) | 0xFF000000 )
#define TRANSP(alpha, color)  ((color & 0x00FFFFFF) | ((alpha & 0xff) << 24) )

#define RESX 256
#define RESY 256

AM_SHARED_RW nema_cmdlist_t cl0;
AM_SHARED_RW nema_cmdlist_t context_cl;
AM_SHARED_RW nema_cmdlist_t clearblack_cl;
AM_SHARED_RW nema_cmdlist_t clearwhite_cl;


nema_buffer_t fb_bo  = {0};
nema_buffer_t zbuffer_bo  = {0};

nemadc_layer_t dc_layer = {(void *)0, 0, RESX, RESY, RESX*2, 0, 0, RESX, RESY, 0xff, NEMADC_BL_SRC, 0, NEMADC_RGB565, 0, 0, 0, 0, 0};

void suite_init(void)
{
    fb_bo = nema_buffer_create(RESX*RESY*4);
    nema_buffer_map(&fb_bo);

    dc_layer.baseaddr_phys = fb_bo.base_phys;
    dc_layer.baseaddr_virt = fb_bo.base_virt;
    memset(dc_layer.baseaddr_virt, 0, RESX*RESY*4);

    zbuffer_bo = nema_buffer_create(RESX*RESY*4);
    nema_buffer_map(&zbuffer_bo);

    context_cl = nema_cl_create();
    nema_cl_bind(&context_cl);
    nema_set_const_reg(0, 0);
    nema_enable_gradient(0);
    nema_enable_depth(0);


    nema_bind_dst_tex(fb_bo.base_phys, RESX, RESY, NEMA_RGB565, -1);
    nema_bind_depth_buffer(zbuffer_bo.base_phys, RESX, RESY);

    nema_set_clip(0, 0, RESX, RESY);

    nema_cl_unbind();

    //Clear Black CL
    //---------------------------------------------------------------------
    clearblack_cl = nema_cl_create_sized(256);
    nema_cl_bind(&clearblack_cl);
    nema_cl_branch(&context_cl);
    nema_set_matrix_translate(0, 0);
    nema_clear(BLACK);
    nema_cl_unbind();

    //Clear White CL
    //---------------------------------------------------------------------
    clearwhite_cl = nema_cl_create_sized(256);
    nema_cl_bind(&clearwhite_cl);
    nema_cl_branch(&context_cl);
    nema_set_matrix_translate(0, 0);
    nema_clear(OWHITE);
    nema_cl_unbind();
}

int fillrect_grad()
{
    int x1, y1;
    color_var_t col0, col1, col2;

    //---------------------------------------------------------------------

    cl0 = nema_cl_create();
    //---------------------------------------------------------------------
    nema_cl_bind(&cl0);
    nema_cl_branch(&context_cl);
    //---------------------------------------------------------------------

    nema_enable_gradient(1);

    nema_set_blend_fill ( NEMA_BL_SRC );
    // Col 1
    // NEMA_P_RAST_EMULATION;
    x1 = 10; y1 = 10;
    col0.r = 0; col0.g = 0; col0.b = 0; col0.a = 0;
    col1.r = 400; col1.g = 400; col1.b = 400; col1.a = 400; // x gradient
    col2.r = 100; col2.g = 100; col2.b = 100; col2.a = 100; // y gradient
    nema_interpolate_rect_colors(x1, y1, RESX, RESY, &col0, &col1, &col2);
    nema_fill_rect(x1, y1, RESX, RESY, 0);

    nema_cl_submit(&cl0);
    nema_cl_wait(&cl0);
    nemadc_set_layer(0, &dc_layer);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }

    nema_enable_gradient(0);

    nema_cl_unbind();
    nema_cl_destroy(&cl0);

    return 0;
}

int suite_run(void)
{
    int ret;
    uint32_t ui32MipiCfg = MIPICFG_8RGB888_OPT0; // default config
    //Initialize NemaGFX
    ret = nema_init();
    if (ret != 0)
    {
        return ret;
    }
    //Initialize Nema|dc
    ret = nemadc_init();
    if (ret != 0)
    {
        return ret;
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

    uint16_t panel_resx = g_sDispCfg[g_eDispType].ui32PanelResX; //panel's max resolution
    uint16_t panel_resy = g_sDispCfg[g_eDispType].ui32PanelResY; //panel's max resolution

    uint16_t minx, miny;

    //Set the display region to center
    if ( RESX > panel_resx )
    {
        minx = 0;   // set the minimum value to 0
    }
    else
    {
        minx = (panel_resx - RESX) >> 1;
        minx = (minx >> 1) << 1;
    }

    if ( RESY > panel_resy )
    {
        miny = 0;   // set the minimum value to 0
    }
    else
    {
        miny = (panel_resy - RESY) >> 1;
        miny = (miny >> 1) << 1;
    }

    //Initialize the display
    switch (g_sDispCfg[g_eDispType].eInterface)
    {
        case IF_SPI4:
            am_devices_nemadc_rm67162_init(MIPICFG_SPI4, MIPICFG_1RGB565_OPT0, RESX, RESY, minx, miny);
            break;
        case IF_DSPI:
            am_devices_nemadc_rm67162_init(MIPICFG_DSPI | MIPICFG_SPI4, MIPICFG_2RGB888_OPT0, RESX, RESY, minx, miny);
            break;
        case IF_QSPI:
            am_devices_nemadc_rm67162_init(MIPICFG_QSPI | MIPICFG_SPI4, MIPICFG_4RGB565_OPT0, RESX, RESY, minx, miny);
            break;
        case IF_DSI:
            am_devices_dsi_rm67162_init(ui32MipiCfg, RESX, RESY, minx, miny);
            break;
        default:
            ; //NOP
    }

    suite_init();

    fillrect_grad();

    return 0;
}

