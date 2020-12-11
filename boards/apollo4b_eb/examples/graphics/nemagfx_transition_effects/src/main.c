//*****************************************************************************
//
//! @file main.c
//!
//! @brief NemaGFX example.
//! this example use one frame buffer demonstrate two picture trasition effect,
//! with Nema GPU support, the effect include
//! NEMA_TRANS_LINEAR_H,
//! NEMA_TRANS_CUBE_H,
//! NEMA_TRANS_INNERCUBE_H,
//! NEMA_TRANS_STACK_H,
//! NEMA_TRANS_LINEAR_V,
//! NEMA_TRANS_CUBE_V,
//! NEMA_TRANS_INNERCUBE_V,
//! NEMA_TRANS_STACK_V,
//! NEMA_TRANS_FADE,
//! NEMA_TRANS_FADE_ZOOM,
//! NEMA_TRANS_MAX,
//! NEMA_TRANS_NONE
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

#include <string.h>

#include "am_bsp.h"
#include "am_util_delay.h"
#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"

#include "nema_core.h"
#include "nema_utils.h"
#include "nema_dc.h"
#include "nema_dc_mipi.h"

#include "nema_transitions.h"
#include "nema_utils.h"
#include "nemagfx_transition_effects.h"

#include "im00.rgba565.h"
#include "im01.rgba565.h"

/*
#define PANEL_RESX 360              //panel's max resolution
#define PANEL_RESY 360              //panel's max resolution
*/
#define PANEL_RESX (g_sDispCfg[g_eDispType].ui32PanelResX)
#define PANEL_RESY (g_sDispCfg[g_eDispType].ui32PanelResY)

#define RESX 256
#define RESY 256

#define PANEL_OFFSET                ((PANEL_RESX-RESX)/2)

#define SMALLFB
#define SMALLFB_STRIPES 4

static img_obj_t screen0 = {
                        {.size = im00_rgba565_len,
                         .base_virt = (void *)im00_rgba565,
                         .base_phys = (uintptr_t)im00_rgba565},
                         256, 256, -1, 0, (uint8_t)NEMA_RGB565, 0};
static img_obj_t screen1 = {
                        {.size = im01_rgba565_len,
                         .base_virt = (void *)im01_rgba565,
                         .base_phys = (uintptr_t)im01_rgba565},
                         256, 256, -1, 0, (uint8_t)NEMA_RGB565, 0};

#define FRAME_BUFFERS 1
static img_obj_t fb[FRAME_BUFFERS];
static nemadc_layer_t layer[3] = {{0}};

static void
load_objects(void)
{
    for (int i = 0; i < FRAME_BUFFERS; ++i)
    {
        fb[i].w = RESX;
        fb[i].h = RESY;
        fb[i].format = NEMA_RGB565;
        fb[i].stride = RESX * 2;

#ifdef SMALLFB
        fb[i].bo = nema_buffer_create(fb[i].stride * fb[i].h / SMALLFB_STRIPES);
#else
        fb[i].bo = nema_buffer_create(fb[i].stride * fb[i].h);
#endif

        (void)nema_buffer_map(&fb[i].bo);

        layer[i].sizex = layer[i].resx = fb[i].w;
        layer[i].sizey = layer[i].resy = fb[i].h;
        layer[i].stride = fb[i].stride;
        layer[i].format = NEMADC_RGB565;
        layer[i].blendmode = NEMADC_BL_SRC;
        layer[i].baseaddr_phys = fb[i].bo.base_phys;
        layer[i].baseaddr_virt = fb[i].bo.base_virt;
    }

    //
    // Preload the textures from MRAM to SSRAM 
    // Set the textures address in SSRAM for GPU
    //

    screen0.bo = nema_buffer_create(im00_rgba565_len);
    memcpy((void*)screen0.bo.base_phys, im00_rgba565, im00_rgba565_len);
    screen1.bo = nema_buffer_create(im01_rgba565_len);
    memcpy((void*)screen1.bo.base_phys, im01_rgba565, im01_rgba565_len); 

}

static nema_cmdlist_t cl_draw_entire_scene;
static nema_cmdlist_t cl;
static int cur_fb = 0;

static int init(void)
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


#ifdef SMALLFB
    nemadc_timing(RESX, 4, 10, 10,
                  RESY / SMALLFB_STRIPES, 10, 50, 10);
#else
    nemadc_timing(RESX, 4, 10, 10,
                  RESY, 10, 50, 10);
#endif

    load_objects();

    cl_draw_entire_scene  = nema_cl_create();
    cl  = nema_cl_create();

    return 0;
}

static nema_transition_t effect = NEMA_TRANS_LINEAR_H;

static void display(float step)
{
    nema_cl_bind(&cl_draw_entire_scene);
    nema_cl_rewind(&cl_draw_entire_scene);

    nema_bind_tex(NEMA_TEX1, screen0.bo.base_phys,  \
        screen0.w, screen0.h, screen0.format, screen0.stride, NEMA_FILTER_BL | NEMA_TEX_BORDER);
    nema_bind_tex(NEMA_TEX2, screen1.bo.base_phys,  \
        screen1.w, screen1.h, screen1.format, screen1.stride, NEMA_FILTER_BL | NEMA_TEX_BORDER);

    nema_set_tex_color(0);

    nema_transition(effect, NEMA_TEX1, NEMA_TEX2, NEMA_BL_SRC, step, fb[0].w, fb[0].h);

#ifdef SMALLFB
    for (int stripe = 0; stripe < SMALLFB_STRIPES; ++stripe)
    {
        nema_cl_bind(&cl);
        nema_cl_rewind(&cl);

        //Set Clipping Rectangle
        nema_set_clip(0, stripe * RESY / SMALLFB_STRIPES, RESX, RESY / SMALLFB_STRIPES);
        //Bind Framebuffer
        nema_bind_dst_tex(layer[cur_fb].baseaddr_phys - stripe * RESY / SMALLFB_STRIPES * fb[0].stride, fb[0].w, fb[0].h, fb[0].format, fb[0].stride);

        nema_cl_jump(&cl_draw_entire_scene);
        nema_cl_submit(&cl);
        (void)nema_cl_wait(&cl);
        
        if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
        {
            am_devices_dsi_rm67162_set_region(RESX, RESY / SMALLFB_STRIPES, PANEL_OFFSET, PANEL_OFFSET + (RESY / SMALLFB_STRIPES)*stripe);
        }
        else if (g_sDispCfg[g_eDispType].eInterface == IF_QSPI)
        {
            am_devices_nemadc_rm67162_set_region(MIPICFG_QSPI, RESX, RESY / SMALLFB_STRIPES, PANEL_OFFSET, PANEL_OFFSET + (RESY / SMALLFB_STRIPES) * stripe);
        }
        else if (g_sDispCfg[g_eDispType].eInterface == IF_SPI4)
        {
            am_devices_nemadc_rm67162_set_region(MIPICFG_SPI4, RESX, RESY / SMALLFB_STRIPES, PANEL_OFFSET, PANEL_OFFSET + (RESY / SMALLFB_STRIPES) * stripe);
        }
        else if (g_sDispCfg[g_eDispType].eInterface == IF_DSPI)
        {
            am_devices_nemadc_rm67162_set_region(MIPICFG_DSPI, RESX, RESY / SMALLFB_STRIPES, PANEL_OFFSET, PANEL_OFFSET + (RESY / SMALLFB_STRIPES) * stripe);
        }
        
        
        nemadc_set_layer(0, &layer[0]);
        if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
        {
            dsi_send_frame_single(NEMADC_OUTP_OFF);
        }
        else
        {
            nemadc_send_frame_single();
        }

    }
#else
    nema_cl_bind(&cl);
    nema_cl_rewind(&cl);

    //Set Clipping Rectangle
    nema_set_clip(0, 0, RESX, RESY);
    //Bind Framebuffer
    nema_bind_dst_tex(layer[cur_fb].baseaddr_phys, fb[0].w, fb[0].h, fb[0].format, fb[0].stride);

    nema_cl_jump(&cl_draw_entire_scene);

    nema_cl_submit(&cl);
    nema_cl_wait(&cl);


    nemadc_set_layer(0, &layer[0]);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }
#endif
}

#define ANIMATION_STEP_0_1 0.02f

static void next_effect(void)
{
    effect = (nema_transition_t)(((int)effect + 1) % NEMA_TRANS_MAX);
}

static void loop(void)
{
#define MIN_STEP          0.f //0.18 //0.f       
#define MAX_STEP          1.f //0.24 //1.f
    float step = MIN_STEP;
    float step_step = ANIMATION_STEP_0_1;

    while (1)
    {
        display(step);

        if (step <= MIN_STEP)
        {
            step = MIN_STEP;
            step_step = ANIMATION_STEP_0_1;
            next_effect();
        }
        else if (step >= MAX_STEP)
        {
            step = MAX_STEP;
            step_step = -ANIMATION_STEP_0_1;
            next_effect();
        }

        step += step_step;

        nema_calculate_fps();
    }
}


int transition_effects(void)
{
    init();

    loop();

    return 0;
}

