//*****************************************************************************
//
//! @file main.c
//!
//! brief Example that demonstrates blend feature
//! Blending requires a series of calculations between the source (foreground)
//! and destination (background)color fragments for producing the final color,
//! which will be written in memory.This example use a constent table inside
//! most of the supported blending mode.demonstrates each more every 1 second.
//! the dst color is nema_rgba(0xff, 0, 0, 0x80), which is red color with 50%
//! alpha blending, the src color is nema_rgba(0, 0, 0xff, 0x80), which is blue
//! color with 50% alpha blending.
//!
//! Printing takes place over the ITM at 1M Baud.

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
#include "nema_dc_hal.h"
#include "nema_dc_regs.h"
#include "am_util_delay.h"
#include "string.h"


#ifndef DONT_USE_NEMADC
#include "nema_dc.h"
#include "nema_dc_mipi.h"
#endif

#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"

#include "homer_rgba.h"
#include "greekisland_200x133_rgba.h"
#include "pic_48x48_rgba.h"

#define RESX (g_sDispCfg[g_eDispType].ui32PanelResX)
#define RESY (g_sDispCfg[g_eDispType].ui32PanelResY)


static const unsigned long RED       = 0x000000FF;

#define OPAQUE(color)         ((color) | 0xFF000000 )

static img_obj_t fb;
nemadc_layer_t dc_layer;

/*
#ifndef DONT_USE_NEMADC
nemadc_layer_t dc_layer = {(void *)0, 0, RESX, RESY, -1, 0, 0, RESX, RESY, 0xff, NEMADC_BL_SRC, 0, NEMADC_RGBA8888, 0, 0, 0, 0, 0};
#endif
*/

void fb_reload_rgba8888(void)
{
    fb.bo = nema_buffer_create(RESX*RESY*4);
    memset((void*)(fb.bo.base_phys), 0, RESX*RESY*4);
    fb.w = RESX;
    fb.h = RESY;
    fb.stride = RESX*4;
    fb.color = 0;
    fb.format = NEMA_RGBA8888;
    fb.sampling_mode = 0;
    printf("FB: V:%p P:0x%08x\n", (void *)fb.bo.base_virt, fb.bo.base_phys);

    dc_layer.format = NEMADC_RGBA8888;
    dc_layer.baseaddr_virt = (void*)fb.bo.base_virt;
    dc_layer.baseaddr_phys = (uintptr_t)fb.bo.base_phys;
    dc_layer.resx = RESX;
    dc_layer.resy = RESY;
    dc_layer.stride = RESX*4;
    dc_layer.startx = 0;
    dc_layer.starty = 0;
    dc_layer.sizex = RESX;
    dc_layer.sizey = RESY;
    dc_layer.alpha = 0xFF;
    dc_layer.blendmode = NEMADC_BL_SRC;
    dc_layer.buscfg = 0;
    dc_layer.format = NEMADC_RGBA8888;
    dc_layer.mode = 0;
    dc_layer.u_base = 0;
    dc_layer.v_base = 0;
    dc_layer.u_stride = 0;
    dc_layer.v_stride = 0;
    printf("FB: V:%p P:0x%08x\n", (void *)fb.bo.base_virt, fb.bo.base_phys);
}

void fb_release(void)
{
    nema_buffer_destroy(&fb.bo);
}

const uint32_t blend_mode[13] =
{
    NEMA_BL_SIMPLE,
    NEMA_BL_CLEAR,
    NEMA_BL_SRC,
    NEMA_BL_SRC_OVER,
    NEMA_BL_DST_OVER,
    NEMA_BL_SRC_IN,
    NEMA_BL_DST_IN,
    NEMA_BL_SRC_OUT,
    NEMA_BL_DST_OUT,
    NEMA_BL_SRC_ATOP,
    NEMA_BL_DST_ATOP,
    NEMA_BL_ADD,
    NEMA_BL_XOR
};

void test_blend_mode(void)
{
    nema_cmdlist_t cl;
    //Create Command Lists
    cl = nema_cl_create();

    //Bind Command List
    nema_cl_bind(&cl);

    //Bind Framebuffer
    nema_bind_dst_tex(fb.bo.base_phys, fb.w, fb.h, fb.format, fb.stride);
    //Set Clipping Rectangle
    nema_set_clip(0, 0, RESX, RESY);

    for ( int i = 0; i < 13; i++ )
    {
        nema_set_blend_fill(NEMA_BL_SIMPLE);
        nema_fill_rect(0, 0, RESX / 2, RESY / 2, nema_rgba(0xff, 0, 0, 0x80));
        nema_set_blend_fill(blend_mode[i]);
        nema_fill_rect(RESX / 4, RESY / 4, RESX / 2 , RESY / 2, nema_rgba(0, 0, 0xff, 0x80));

        nema_cl_submit(&cl);
        nema_cl_wait(&cl);
        nema_cl_rewind(&cl);
        nemadc_set_layer(0, &dc_layer);
        if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
        {
            dsi_send_frame_single(NEMADC_OUTP_OFF);
        }
        else
        {
            nemadc_send_frame_single();
        }
        nema_clear(0x00000000);
        am_util_delay_ms(1000);
    }

    nema_cl_destroy(&cl);

}

void tsuite2d_srcdstkey(void)
{
    nema_cmdlist_t cl = nema_cl_create();
    nema_cl_bind(&cl);
    nema_bind_dst_tex(fb.bo.base_phys, fb.w, fb.h, fb.format, fb.stride);
    nema_clear(0x00000000);
    img_obj_t obj_homer_rgba = {{0}, 32, 72, 32*4, 0, NEMA_RGBA8888, 0};
    obj_homer_rgba.bo.base_phys = (uintptr_t)homer_rgba;
    obj_homer_rgba.bo.base_virt = (void*)obj_homer_rgba.bo.base_phys;
    nema_bind_src_tex(obj_homer_rgba.bo.base_phys,
                  obj_homer_rgba.w,
                  obj_homer_rgba.h,
                  obj_homer_rgba.format,
                  obj_homer_rgba.stride,
                  NEMA_FILTER_PS);
    nema_set_clip(0, 0, 360, 360);
    nema_set_blend_blit(NEMA_BL_SRC);
    nema_blit(50, 50);

    nema_set_blend_fill ( NEMA_BL_SRC );
    nema_fill_rect      (200, 200, 32, 72, OPAQUE(RED));
    nema_set_src_color_key(0xff00e100);
    nema_set_dst_color_key(RED);
    nema_set_blend_blit(NEMA_BL_SRC | NEMA_BLOP_DST_CKEY | NEMA_BLOP_SRC_CKEY);
    nema_blit(200, 200);

    nema_cl_unbind();
    nema_cl_submit(&cl);
    nema_cl_wait(&cl);
    nemadc_set_layer(0, &dc_layer);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }
    nema_cl_destroy(&cl);
}

void tsuite2d_dst_ckey(void)
{
    //---------------------------------------------------------------------
    nema_cmdlist_t cl = nema_cl_create();
    nema_cl_bind(&cl);
    nema_bind_dst_tex(fb.bo.base_phys, fb.w, fb.h, fb.format, fb.stride);
    nema_clear(0x00000000);
    img_obj_t obj_homer_rgba = {{0}, 32, 72, 32 * 4, 0, NEMA_RGBA8888, 0};
    obj_homer_rgba.bo.base_phys = (uintptr_t)homer_rgba;
    obj_homer_rgba.bo.base_virt = (void*)obj_homer_rgba.bo.base_phys;
    nema_bind_src_tex(obj_homer_rgba.bo.base_phys,
                  obj_homer_rgba.w,
                  obj_homer_rgba.h,
                  obj_homer_rgba.format,
                  obj_homer_rgba.stride,
                  NEMA_FILTER_PS);

    nema_set_blend_blit(NEMA_BL_SRC);
    nema_blit( 50 - 40, 50 + 100);
    nema_blit(100 - 40, 50 + 100);
    //---------------------------------------------------------------------
    nema_set_dst_color_key(0xff00e100);
    nema_set_blend_fill(NEMA_BL_SRC | NEMA_BLOP_DST_CKEY);
    nema_fill_rect(40 - 40, 40 + 100, 100, 100, OPAQUE(RED));
    //---------------------------------------------------------------------
    nema_set_blend_fill ( NEMA_BL_SRC );
    nema_fill_rect( 200 - 80, 40 + 100, 202, 135, 0xff00e100);
    nema_set_blend_blit(NEMA_BL_SRC);
    nema_blit(250 - 80, 50 + 100);
    nema_blit(300 - 80, 50 + 100);
    //---------------------------------------------------------------------
    img_obj_t obj_greekisland_200x133_rgba = {{0}, 200, 133, 200*4, 0, NEMA_RGBA8888, 0};
    obj_greekisland_200x133_rgba.bo.base_phys = (uintptr_t)greekisland_200x133_rgba;
    obj_greekisland_200x133_rgba.bo.base_virt = (void*)obj_greekisland_200x133_rgba.bo.base_phys;
    nema_bind_src_tex(obj_greekisland_200x133_rgba.bo.base_phys,
                  obj_greekisland_200x133_rgba.w,
                  obj_greekisland_200x133_rgba.h,
                  obj_greekisland_200x133_rgba.format,
                  obj_greekisland_200x133_rgba.stride,
                  NEMA_FILTER_PS);

    nema_set_dst_color_key(0xff00e100);
    nema_set_blend_blit(NEMA_BL_SRC | NEMA_BLOP_DST_CKEY);
    nema_blit(201 - 80, 41 + 100);

    nema_cl_unbind();
    nema_cl_submit(&cl);
    nema_cl_wait(&cl);
    nemadc_set_layer(0, &dc_layer);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }
    nema_cl_destroy(&cl);
}

void blit_texture_scale(void)
{
    nema_cmdlist_t cl;
    img_obj_t obj_pic_48x48_rgba = {{0}, 48, 48, -1, 0, NEMA_RGBA8888, 0};

    cl = nema_cl_create();
    nema_cl_bind(&cl);
    nema_bind_dst_tex(fb.bo.base_phys, fb.w, fb.h, fb.format, fb.stride);

    obj_pic_48x48_rgba.bo.base_phys = (uintptr_t)pic_48x48_rgba;
    obj_pic_48x48_rgba.bo.base_virt = (void*)obj_pic_48x48_rgba.bo.base_phys;
    nema_bind_src_tex(obj_pic_48x48_rgba.bo.base_phys,
                      obj_pic_48x48_rgba.w,
                      obj_pic_48x48_rgba.h,
                      obj_pic_48x48_rgba.format,
                      obj_pic_48x48_rgba.stride,
                      NEMA_FILTER_PS);
    nema_set_clip(0, 0, RESX, RESY);
    nema_set_blend_blit(NEMA_BL_SRC);
    nema_blit((RESX - 48) / 2, 20);

    nema_bind_src_tex(obj_pic_48x48_rgba.bo.base_phys,
                      obj_pic_48x48_rgba.w,
                      obj_pic_48x48_rgba.h,
                      obj_pic_48x48_rgba.format,
                      obj_pic_48x48_rgba.stride,
                      NEMA_FILTER_PS);
    nema_blit_rect_fit(0, 100, 48*4, 48*4);

    nema_bind_src_tex(obj_pic_48x48_rgba.bo.base_phys,
                      obj_pic_48x48_rgba.w,
                      obj_pic_48x48_rgba.h,
                      obj_pic_48x48_rgba.format,
                      obj_pic_48x48_rgba.stride,
                      NEMA_FILTER_BL);
    nema_blit_rect_fit(200, 100, 48*4, 48*4);

    nema_cl_submit(&cl);
    nema_cl_wait(&cl);
    nemadc_set_layer(0, &dc_layer);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }

    nema_cl_destroy(&cl);
}

int test_blend()
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


    fb_reload_rgba8888();
    test_blend_mode();
    am_util_delay_ms(1000);
    tsuite2d_srcdstkey();
    fb_release();

    fb_reload_rgba8888();
    am_util_delay_ms(1000);
    tsuite2d_dst_ckey();
    fb_release();

    fb_reload_rgba8888();
    am_util_delay_ms(1000);
    blit_texture_scale();
    fb_release();

    return 0;
}


