//*****************************************************************************
//
//! @file main.c
//!
//! @brief NemaGFX example.

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
#include "stdlib.h"

#ifndef DONT_USE_NEMADC
#include "nema_dc.h"
#include "nema_dc_mipi.h"
#endif

#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"


#define RESX (g_sDispCfg[g_eDispType].ui32PanelResX / 64 * 64)
#define RESY (g_sDispCfg[g_eDispType].ui32PanelResY / 64 * 64)


#define FRAME_BUFFERS  2
static img_obj_t fbs[FRAME_BUFFERS];
static nemadc_layer_t layer[FRAME_BUFFERS];

static void
fb_reload_rgb565_2(void)
{
    for (int i = 0; i < FRAME_BUFFERS; ++i)
    {
        fbs[i].w = RESX;
        fbs[i].h = RESY;
        fbs[i].format = NEMA_RGB565;
        fbs[i].stride = RESX*2;
        fbs[i].bo = nema_buffer_create(fbs[i].stride * fbs[i].h);
        (void)nema_buffer_map(&fbs[i].bo);

        layer[i].startx = layer[i].starty = 0;
        layer[i].sizex = layer[i].resx = fbs[i].w;
        layer[i].sizey = layer[i].resy = fbs[i].h;
        layer[i].stride = fbs[i].stride;
        layer[i].format = NEMADC_RGB565 | (nemadc_format_t)NEMADC_MODULATE_A;

        layer[i].blendmode = NEMADC_BL_SIMPLE;
        layer[i].alpha     = 0x80;
        layer[i].baseaddr_phys = fbs[i].bo.base_phys;
        layer[i].baseaddr_virt = fbs[i].bo.base_virt;
    }
}

static int cur_fb = 0;
static int last_fb = -1;

static void
swap_fb(void)
{
    last_fb = cur_fb;
    cur_fb = (cur_fb + 1) % FRAME_BUFFERS;
}

#define MAX_CIRCLE_NUM               (15)

typedef struct
{
    int last_x, last_y;
    int center_x, center_y;
    int r;
    int speed_x, speed_y;
    unsigned char red, green, blue, aplha;
    unsigned char isFilled;
    char direction;
}circle_t;

circle_t ga_circle[MAX_CIRCLE_NUM];

void update_circle(void)
{
    uint16_t i;
    int16_t current_y;
    int16_t current_x;

    for (i = 0; i < MAX_CIRCLE_NUM; i++)
    {
        current_y = ga_circle[i].center_y + ga_circle[i].speed_y;
        if (current_y > RESY - ga_circle[i].r || current_y < ga_circle[i].r)
        {
            if (ga_circle[i].speed_y > 0)
            {
                ga_circle[i].center_y = RESY - ga_circle[i].r;
                ga_circle[i].speed_y = -ga_circle[i].speed_y;
            }
            else
            {
                ga_circle[i].center_y = ga_circle[i].r;
                ga_circle[i].speed_y = abs(ga_circle[i].speed_y);
            }
        }
        else
        {
            ga_circle[i].center_y = current_y;
        }
    }

    for (i = 0; i < MAX_CIRCLE_NUM; i++)
    {
        current_x = ga_circle[i].center_x + ga_circle[i].speed_x;
        if (current_x > RESX - ga_circle[i].r || current_x < ga_circle[i].r)
        {
            if (ga_circle[i].speed_x > 0)
            {
                ga_circle[i].center_x = RESX - ga_circle[i].r;
                ga_circle[i].speed_x = -ga_circle[i].speed_x;
            }
            else
            {
                ga_circle[i].center_x = ga_circle[i].r;
                ga_circle[i].speed_x = abs(ga_circle[i].speed_x);
            }
        }
        else
        {
            ga_circle[i].center_x = current_x;
        }
    }

    for (i = 0; i < MAX_CIRCLE_NUM; i++)
    {
        nema_fill_circle(ga_circle[i].center_x, ga_circle[i].center_y, ga_circle[i].r,
                         nema_rgba(ga_circle[i].red, ga_circle[i].green, ga_circle[i].blue, ga_circle[i].aplha));
    }
}

void test_blit_balls(void)
{
    nema_cmdlist_t cl_circles;
    nema_cmdlist_t cl_clear;
    uint16_t i;
    uint8_t flag = 0;

    srand(1);

    for (i = 0; i < MAX_CIRCLE_NUM; i++)
    {
        ga_circle[i].r = rand() % 100;
        ga_circle[i].center_x = rand() % (RESX - ga_circle[i].r * 2) + ga_circle[i].r;
        ga_circle[i].center_y = rand() % (RESY - ga_circle[i].r * 2) + ga_circle[i].r;
/*
        am_util_stdio_printf("ga_circle[%d].r = %d, ga_circle[%d].center_x = %d, ga_circle[%d].center_y = %d\n",
                              i,ga_circle[i].r, i, ga_circle[i].center_x, i, ga_circle[i].center_y);
*/
        ga_circle[i].red = rand() % 256;
        ga_circle[i].green = rand() % 256;
        ga_circle[i].blue = rand() % 256;
        ga_circle[i].aplha = rand() % 256;

        ga_circle[i].speed_x = rand() % 5 + 1;
        ga_circle[i].speed_y = rand() % 5 + 1;
    }

    cl_circles = nema_cl_create();
    cl_clear = nema_cl_create();

    while(1)
    {
        nema_cl_bind(&cl_clear);
        nema_bind_dst_tex(fbs[cur_fb].bo.base_phys, fbs[cur_fb].w, fbs[cur_fb].h, fbs[cur_fb].format, fbs[cur_fb].stride);
        nema_set_clip(0, 0, RESX, RESY);
        if (flag == 0)
        {
            nema_clear(nema_rgba(0x00, 0x00, 0x00, 0xff));
        }
        nema_cl_submit(&cl_clear);

        nema_cl_bind(&cl_circles);
        nema_set_blend_fill(NEMA_BL_SIMPLE);

        update_circle();

        nema_cl_submit(&cl_circles);

        if (last_fb >= 0)
        {
            nemadc_set_layer(0, &layer[last_fb]);

            if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
            {
                dsi_send_frame_single(NEMADC_OUTP_OFF);
            }
            else
            {
                nemadc_send_frame_single();
            }

            nema_calculate_fps();
        }

        nema_cl_wait(&cl_circles);
        nema_cl_rewind(&cl_circles);
        nema_cl_rewind(&cl_clear);

        swap_fb();
    }
}

int balls_bench()
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


    fb_reload_rgb565_2();
    test_blit_balls();

    return 0;
}

