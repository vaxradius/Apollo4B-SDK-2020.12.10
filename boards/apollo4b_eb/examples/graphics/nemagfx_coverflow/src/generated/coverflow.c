//*****************************************************************************
//
//  coverflow.c
//! @file
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nema_core.h"
#include "nema_utils.h"
#include "nema_dc.h"

#include "nema_programHW.h"
#include "nema_easing.h"
#include "nema_event.h"
#include "nema_raster.h"
#include "am_util_stdio.h"

#include "nema_dc.h"
#include "nema_dc_mipi.h"

#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"

#include "im01_128x128_rgba.h"
#include "im02_128x128_rgba.h"
#include "im03_128x128_rgba.h"
#include "im04_128x128_rgba.h"


/*Porting notes
* 1)define RESX, RESY, FRAME_BUFFERS, IMG_SIZE, IMG_FORMAT, IMAGE_COUNT
* 2)MODE_INTERRACTIVE: coverflow works using mouse input, otherwise draw frames continuously
* 3)Modify "load_mult_objects" function (frame-buffer format, stride, input images path)
*/

#define COUNT_FPS
// #define MODE_INTERRACTIVE
#ifndef RESX
#define RESX 320
#endif

#ifndef RESY
#define RESY 320
#endif

#ifndef DEBUG_OVERDRAWS
#define DEBUG_OVERDRAWS 0
#endif

#define FRAME_BUFFERS 2

#define IMG_SIZE      128

#define IMAGE_COUNT   4
#define M_PI          3.14159265358979323846


#ifdef BAREMETAL

    #define FB_GPU  0
    #define FB_DC   1
    #define FB_FREE 2

    #define NUM_LAYERS 1U

    static uintptr_t triple_fbs[NUM_LAYERS][2];

    uintptr_t
    nema_init_triple_fb(int layer, uintptr_t fb0_phys, uintptr_t fb1_phys, uintptr_t fb2_phys)
    {
        //actually doing always 2 framebuffers
        //fb2_phys is ignored

        triple_fbs[layer][FB_GPU]  = fb0_phys;
        triple_fbs[layer][FB_DC]   = fb1_phys;
        // triple_fbs[layer][FB_FREE] = fb2_phys;

        return triple_fbs[layer][FB_GPU];
    }

    uintptr_t
    nema_swap_fb(int layer)
    {
        if (layer < 0)
        {
            layer = 0;
        }

        {
            uintptr_t tmp = triple_fbs[layer][FB_DC];
            triple_fbs[layer][FB_DC] = triple_fbs[layer][FB_GPU];
            triple_fbs[layer][FB_GPU]  = tmp;

            // nemadc_wait_vsync();
            nemadc_set_layer_addr(layer, triple_fbs[layer][FB_DC]);
        }
        return triple_fbs[layer][FB_GPU];
    }
#endif


typedef struct _quad_t
{
    float x0, y0, z0, w0;
    float x1, y1, z1, w1;
    float x2, y2, z2, w2;
    float x3, y3, z3, w3;
} quad_t;


static img_obj_t fb[FRAME_BUFFERS];
static img_obj_t imgs_rgba[IMAGE_COUNT];

static nemadc_layer_t layer[FRAME_BUFFERS] = {{0}};

static void
load_mult_objects(void)
{
    //Allocate Framebuffers
    for (int i = 0; i < FRAME_BUFFERS; ++i)
    {
        fb[i].w      = RESX;
        fb[i].h      = RESY;
        fb[i].format = NEMA_RGB565;
        fb[i].stride = nema_stride_size(fb[i].format, 0, fb[i].w);
        fb[i].bo     = nema_buffer_create( nema_texture_size(fb[i].format, 0, fb[i].w, fb[i].h) );
        nema_buffer_map(&fb[i].bo);


        layer[i].format        = NEMADC_RGB565;
        layer[i].sizex         = layer[i].resx = fb[i].w;
        layer[i].sizey         = layer[i].resy = fb[i].h;
        layer[i].stride        = fb[i].stride;
        layer[i].blendmode     = NEMADC_BL_SRC;
        layer[i].baseaddr_phys = fb[i].bo.base_phys;
        layer[i].baseaddr_virt = fb[i].bo.base_virt;

        am_util_stdio_printf("FB: V:%p P:0x%08x\n", (void *)fb[i].bo.base_virt, fb[i].bo.base_phys);
    }

    nemadc_set_bgcolor(0x40404040);
    uint32_t color = 0x50505050;


    // Texture is already loaded to memory, use it directly
    // No need to used nema_buffer_create()
    // nema_buffer_create() - like malloc() - only allocates an empty buffer
    // When using nema_buffer_create, make sure to memcpy the original texture to the
    //     allocated buffer
    imgs_rgba[0].bo.base_phys = (uintptr_t)&im01_128x128_rgba[0];
    imgs_rgba[0].format = NEMA_RGBA8888;
    imgs_rgba[0].w      = IMG_SIZE;
    imgs_rgba[0].h      = IMG_SIZE;
    imgs_rgba[0].stride = nema_stride_size(imgs_rgba[0].format, 0, imgs_rgba[0].w);
    imgs_rgba[0].sampling_mode =  0;
    nemadc_set_bgcolor(color);
    color += 0x10101010;

    imgs_rgba[1].bo.base_phys = (uintptr_t)&im02_128x128_rgba[0];
    imgs_rgba[1].format = NEMA_RGBA8888;
    imgs_rgba[1].w      = IMG_SIZE;
    imgs_rgba[1].h      = IMG_SIZE;
    imgs_rgba[1].stride = nema_stride_size(imgs_rgba[0].format, 0, imgs_rgba[0].w);
    imgs_rgba[1].sampling_mode =  0;
    nemadc_set_bgcolor(color);
    color += 0x10101010;

    imgs_rgba[2].bo.base_phys = (uintptr_t)&im03_128x128_rgba[0];
    imgs_rgba[2].format = NEMA_RGBA8888;
    imgs_rgba[2].w      = IMG_SIZE;
    imgs_rgba[2].h      = IMG_SIZE;
    imgs_rgba[2].stride = nema_stride_size(imgs_rgba[0].format, 0, imgs_rgba[0].w);
    imgs_rgba[2].sampling_mode =  0;
    nemadc_set_bgcolor(color);
    color += 0x10101010;

    imgs_rgba[3].bo.base_phys = (uintptr_t)&im04_128x128_rgba[0];
    imgs_rgba[3].format = NEMA_RGBA8888;
    imgs_rgba[3].w      = IMG_SIZE;
    imgs_rgba[3].h      = IMG_SIZE;
    imgs_rgba[3].stride = nema_stride_size(imgs_rgba[0].format, 0, imgs_rgba[0].w);
    imgs_rgba[3].sampling_mode =  0;
    nemadc_set_bgcolor(color);
    color += 0x10101010;
}

static inline void
bind_img(img_obj_t *img)
{
    nema_set_tex_color(0);
    nema_bind_src_tex(img->bo.base_phys, img->w, img->h, img->format, img->stride, NEMA_FILTER_BL | NEMA_TEX_BORDER);
}

static float FoV    = 30.f;
static float y_orig = 0;
static float x_orig = 0;

static nema_matrix4x4_t proj_matrix;
//Transform coordinates from [-1,1] to [0, RESX] and [0, RESY]
static void transform_quad_coords(quad_t *q)
{
    nema_mat4x4_obj_to_win_coords(proj_matrix, x_orig, y_orig, RESX, RESY, -1, 1, &q->x0, &q->y0, &q->z0, &q->w0);
    nema_mat4x4_obj_to_win_coords(proj_matrix, x_orig, y_orig, RESX, RESY, -1, 1, &q->x1, &q->y1, &q->z1, &q->w1);
    nema_mat4x4_obj_to_win_coords(proj_matrix, x_orig, y_orig, RESX, RESY, -1, 1, &q->x2, &q->y2, &q->z2, &q->w2);
    nema_mat4x4_obj_to_win_coords(proj_matrix, x_orig, y_orig, RESX, RESY, -1, 1, &q->x3, &q->y3, &q->z3, &q->w3);
}

static void calc_circle(float *x_off, float *z_off, float angle, float r)
{
    *x_off = nema_cos(angle)*r;
    *z_off = nema_sin(angle)*r;
}

const float img_width     = 0.2f;
const float img_angle_deg = 90.f;
const float back_rail_z   = 0.98f;
const float front_img_z   = 0.7f;

static float front_rail_z;
static float shift_par0, shift_par3;
static float r_big, r_small;
static float centre_right_x, img_x_proj;

static void cover_flow_init(void)
{
    nema_mat4x4_load_perspective(proj_matrix, FoV, (float)RESX / RESY, 1, 100);

    img_x_proj = nema_cos(img_angle_deg) * img_width;
    float img_z_proj = nema_sin(img_angle_deg) * img_width;

    front_rail_z = back_rail_z - img_z_proj;

    r_big   = (back_rail_z  - front_img_z) / nema_sin(img_angle_deg);
    r_small = (front_rail_z - front_img_z) / nema_sin(img_angle_deg);

    float x0_front_x = -img_width / 2;

    centre_right_x = x0_front_x + r_big;

    float right_critical_x0 = centre_right_x - nema_cos(img_angle_deg) * r_big;

    shift_par0 = -(right_critical_x0 + img_x_proj - 1) * 0.5f;
    shift_par3 = 1.f - shift_par0;

#if 0
    float centre_right_z = front_img_z;

    float centre_left_x  = x0_front_x - r_small;
    float centre_left_z  = front_img_z;
    float right_critical_z0 = back_rail_z;
    float right_critical_x1 = centre_right_x-nema_cos(img_angle_deg)*r_small;
    float right_critical_z1 = front_rail_z;

    float x0_front_z = front_img_z;

    float x1_front_x = img_width/2;
    float x1_front_z = front_img_z;
    printf("%f\n", img_width    );
    printf("%f\n", img_angle_deg);
    printf("%f\n", img_x_proj);
    printf("%f\n", img_z_proj);
    printf("%f\n", back_rail_z  );
    printf("%f\n", front_img_z  );
    printf("%f\n", front_rail_z);
    printf("%f\n", r_big  );
    printf("%f\n", r_small);

    printf("\n");

    printf("%f\t%f\n", x0_front_x       , x0_front_z       );
    printf("%f\t%f\n", x1_front_x       , x1_front_z       );
    printf("%f\t%f\n", centre_right_x   , centre_right_z   );
    printf("%f\t%f\n", centre_left_x    , centre_left_z    );
    printf("%f\t%f\n", right_critical_x0, right_critical_z0);
    printf("%f\t%f\n", right_critical_x1, right_critical_z1);

    printf("\n");

    printf("%f\n", shift_par0);
    printf("%f\n", shift_par3);
#endif
}

static void map_quad_points(quad_t *q, float shift_par)
{
    if (shift_par < shift_par0)
    {
        //0 - 0.3
        q->x0 = -2 * shift_par + 1 - img_x_proj;
        q->z0 = back_rail_z;

        q->x1 = q->x0 + img_x_proj;
        q->z1 = front_rail_z;
    }
    else if (shift_par < 0.5f)
    {
        //shift_par0 - 0.5
        //angle 71.6 - 0
        float angle = nema_ez( img_angle_deg, 0.f, 0.5f-shift_par0, shift_par-shift_par0, &nema_ez_linear);
        calc_circle(&q->x0, &q->z0, angle, r_big);

        q->x0 = centre_right_x - q->x0;
        q->z0 = front_img_z + q->z0;

        calc_circle(&q->x1, &q->z1, angle, r_small);

        q->x1 = centre_right_x - q->x1;
        q->z1 = front_img_z + q->z1;
    }
    else if (shift_par < shift_par3)
    {
        //0.5 - shift_par3
        //angle 0 - 71.6
        float angle = nema_ez( 0.f, img_angle_deg, shift_par3-0.5f, shift_par-0.5f, &nema_ez_linear);
        calc_circle(&q->x0, &q->z0, angle, r_small);

        q->x0 = -centre_right_x + q->x0;
        q->z0 = front_img_z + q->z0;

        calc_circle(&q->x1, &q->z1, angle, r_big);

        q->x1 = -centre_right_x + q->x1;
        q->z1 = front_img_z + q->z1;
    }
    else
    {
        //0.7 - 1.0
        q->x0 = -2 * shift_par + 1;
        q->z0 = front_rail_z;


        q->x1 = q->x0 + img_x_proj;
        q->z1 = back_rail_z;
    }


    ///X
    q->x2 = q->x1;
    q->x3 = q->x0;

    ///Y
    q->y0 = q->y1 = -0.5f*img_width;
    q->y2 = q->y3 =  0.5f*img_width;

    ///Z
    q->z2 = q->z1;
    q->z3 = q->z0;

    ///W
    q->w0 = q->w1 = q->w2 = q->w3 = 1.f;
}


static float map_to_ratio(float val, float break_point, float laska)
{
    float ret_val = 0.f;

    if (val <= break_point)
    {
        return val / (1.f + laska);
    }

    ret_val += break_point;
    val     -= break_point;

    float before_laska = (0.5f - break_point) * 2.f;
    float after_laska  = (0.5f - break_point) * 2.f + laska;

    if (val < before_laska)
    {
        ret_val += val * after_laska / before_laska;
        return ret_val / (1.f + laska);
    }

    ret_val += after_laska;

    val     -= before_laska;
    ret_val += val;

    return ret_val / (1.f + laska);
}

#ifdef STANDALONE
int main()
#else
int cover_flow()
#endif
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

#ifndef BAREMETAL
    nema_event_init(1, 0, 0, RESX, RESY);
#endif

    load_mult_objects();

    nemadc_layer_disable(1);
    nemadc_layer_disable(2);
    nemadc_cursor_enable(0);
    nemadc_set_layer(0, &layer[1]);

    uintptr_t cur_fb_base_phys = nema_init_triple_fb(0, layer[0].baseaddr_phys,
                                                  layer[1].baseaddr_phys,
                                                  0);
    cover_flow_init();

    nema_cmdlist_t cl0  = nema_cl_create();
    nema_cmdlist_t cl1  = nema_cl_create();

    nema_cmdlist_t* cls[2] = {&cl0, &cl1};
    nema_cmdlist_t* cur_cl = cls[0];

    nema_cl_bind(&cl0);
    nema_set_clip(0, 0, RESX, RESY);
//****************************************************************
//**************    Background    ********************************

    for (int i = 0; i < FRAME_BUFFERS; ++i)
    {
        nema_set_blend_fill(NEMA_BL_SRC);
        nema_bind_dst_tex(fb[i].bo.base_phys, fb[i].w, fb[i].h, fb[i].format, fb[i].stride);
        nema_fill_rect(0, 0, RESX, RESY, 0);

/*
        #if RESX == 800 && RESY == 600 && !defined(PLATFORM_SYNOPSYS)
            nema_set_blend_blit(NEMA_BL_SRC);
            nema_bind_src_tex(TSi_logo.bo.base_phys, TSi_logo.w, TSi_logo.h, TSi_logo.format, TSi_logo.stride, NEMA_FILTER_BL);
            nema_blit_rect_fit(RESX/2 - TSi_logo.w/4, 30, TSi_logo.w/2, TSi_logo.h/2);
        #endif
*/
    }
    nema_cl_submit(&cl0);
    nema_cl_wait(&cl0);
//****************************************************************
//**************** Coverflow *************************************

    float shift_par;
    #ifdef COUNT_FPS
    float sec0 = nema_get_time();
    #endif

    int frames  = 0;
    float secs  = 0.5; //TODO =1.0
    int cur_img = 0;

    nema_cmdlist_t cl_bg = nema_cl_create();
    nema_cl_bind(&cl_bg);

#if (DEBUG_OVERDRAWS == 0)
    nema_enable_gradient(0);
#endif
    nema_set_blend_fill(NEMA_BL_SRC);

    uint32_t col_step = 0x20000;
    uint32_t col_init = 0x080000;
    nema_set_gradient_fx(col_init, col_init, col_init, 0xff0000,
                         0       , col_step,
                         0       , col_step,
                         0       , col_step,
                         0       , 0 );

    nema_fill_rect(0, RESY>>1, RESX, RESY>>1, 0x0);
    nema_enable_gradient(0);
    nema_fill_rect(0, 0, RESX, RESY>>1, 0x0);


    float sec_init     = 0.f;
    float cur_sec      = 0.f;

    #ifdef MODE_INTERRACTIVE
    nema_event_t event = {0};
    #endif

    while(1)
    {

        cur_cl = cls[frames & 0x1];

        nema_cl_bind(cur_cl);
        nema_cl_rewind(cur_cl);

        nema_enable_aa(1, 1, 1, 1);

        //Optimize clip for known resolutions
        if ( RESX == 800 && RESY == 600 )
        {
            nema_set_clip(0, 140, RESX, RESY-140);
        }
        else
        {
            nema_set_clip(0, 0, RESX, RESY);
        }

        //Bind Framebuffer
        nema_bind_dst_tex(cur_fb_base_phys, fb[0].w, fb[0].h, fb[0].format, fb[0].stride);

        //Clear screen before printing items on updated positions
        nema_cl_branch(&cl_bg);

#if (DEBUG_OVERDRAWS != 0)
    nema_debug_overdraws(1U);
#endif

        //Redraw gradient on Synopsys, when using the gyroscope
        #if defined(MODE_INTERRACTIVE) && defined(PLATFORM_SYNOPSYS)
        nema_set_blend_fill(NEMA_BL_SRC);
        nema_enable_gradient(0);
        nema_set_gradient_fx(col_init, col_init, col_init, 0xff0000,
                             0, col_step,
                             0, col_step,
                             0, col_step,
                             0, 0 );

        nema_fill_rect(0, (RESY >> 1) - (int)roll, RESX, (RESY >> 1) + (int)roll, 0x0);
        nema_enable_gradient(0);
        nema_fill_rect(0, 0, RESX, (RESY>>1)-(int)roll, 0x0);
        #endif

        nema_set_const_color( nema_rgba(50, 50, 50, 0x80) );

        //STOP_COUNT: number of images inside frustum
        #define STOP_COUNT (40)

        const float break_point = 0.5f - 1.f / STOP_COUNT;
        float clip_left  = RESX;
        float clip_right = 0;

        for ( int  stop = STOP_COUNT-1; stop >= 0; --stop )
        {
            int stop_idx = (stop & 0x1) ? STOP_COUNT - ((stop + 1) >> 1) : stop >> 1 ;
            int img_idx  = (cur_img + stop_idx + IMAGE_COUNT * 10 - (STOP_COUNT >> 1)) % IMAGE_COUNT;

            if ( stop_idx > STOP_COUNT >> 1)
            {
                if (clip_right >= RESX)
                {
                    continue;
                }
                //right side pictures
                nema_set_clip((int32_t)clip_right, 0, (uint32_t)(RESX - clip_right), RESY);
            }
            else
            { //if ( stop_idx > STOP_COUNT>>1)
                if (clip_left < 0)
                {
                    continue;
                }
                //left side pictures
                nema_set_clip(0, 0, (uint32_t)clip_left, RESY);
            }

            float shift_target, shift_prev;

            shift_target = nema_ez(.5f, 0.f, STOP_COUNT >> 1, stop_idx,     nema_ez_linear) + 0.5f;
            shift_prev   = nema_ez(.5f, 0.f, STOP_COUNT >> 1, stop_idx + 1, nema_ez_linear) + 0.5f;

            shift_target = map_to_ratio(shift_target, break_point, 0.19f);

            shift_prev   = map_to_ratio(shift_prev, break_point, 0.19f);

            shift_par    = nema_ez( shift_prev, shift_target, 1.0, cur_sec, &nema_ez_linear);

            quad_t q1;
            map_quad_points(&q1, shift_par);


            transform_quad_coords(&q1);

            //make sides of quads integers
            //so that clipping is done right
            q1.x0 = q1.x3 = (int) q1.x0;
            q1.x1 = q1.x2 = (int) q1.x1;

            ///Reflections
            nema_set_blend_blit(NEMA_BL_SIMPLE | NEMA_BLOP_MODULATE_A);
            bind_img(&imgs_rgba[(img_idx) % IMAGE_COUNT]);

            float screen_img_height_0 = q1.y3 - q1.y0;
            float screen_img_height_1 = q1.y2 - q1.y1;

            nema_blit_quad_fit(q1.x0, q1.y0 + 2.f * screen_img_height_0,
                               q1.x1, q1.y1 + 2.f * screen_img_height_1,
                               q1.x2, q1.y2,
                               q1.x3, q1.y3);

            ///Images
            nema_set_blend_blit(NEMA_BL_SIMPLE);
            nema_blit_quad_fit(q1.x0, q1.y0,
                               q1.x1, q1.y1,
                               q1.x2, q1.y2,
                               q1.x3, q1.y3);

            if ( stop_idx > STOP_COUNT >> 1)
            {
                //right side pictures
                clip_right = q1.x1;
            }
            else if ( stop_idx < STOP_COUNT >> 1)
            {
                //left side pictures
                clip_left = q1.x0;
            }
            else
            {
                clip_left  = q1.x0;
                clip_right = q1.x1;
            }

            if (clip_right >= RESX && clip_left < 0)
            {
                break;
            }

        }

        nema_cl_submit(cur_cl);


#ifdef COUNT_FPS
         ++frames;
        if (frames % 100 == 0)
        {
            float sec1 = nema_get_time();
            am_util_stdio_printf("sec1 = %f, fps: %.2f\n", sec1, (100.f / (sec1 - sec0)));
            sec0 = nema_get_time();
        }
#endif

        #ifdef MODE_INTERRACTIVE
        static float d_cur_sec = 0.f;
        static float d_FoV     = 0.f;

        nema_event_wait(&event, 0);

        //Fixed-numders tuned for Synopsys platform
        if ( event.mouse_event == MOUSE_EVENT_NONE && (event.mouse_state == MOUSE_STATE_LEFT_CLICKED))
        {
            d_cur_sec -= event.mouse_dx / 90.f;
            if (d_cur_sec > 0.9f)
            {
                d_cur_sec = 0.9f;
            }
            else if (d_cur_sec < -0.9f)
            {
                d_cur_sec = -0.9f;
            }

        }
        else if (event.mouse_event == MOUSE_EVENT_SCROLL_UP)
        {
            d_FoV -= 1.f;
        }
        else if (event.mouse_event == MOUSE_EVENT_SCROLL_DOWN)
        {
             d_FoV += 1.f;
        }

        cur_sec   += d_cur_sec;
        d_cur_sec *= 0.8f;

        FoV   += d_FoV;

        //Swipe momentum
        d_FoV *= 0.8f;

        if (FoV < 20.f)
        {
            FoV   = 20.f;
            d_FoV = 0.f;
        }
        else if (FoV > 35.f)
        {
            FoV   = 35.f;
            d_FoV = 0.f;
        }

        nema_mat4x4_load_perspective(proj_matrix, FoV, (float)RESX / RESY, 1, 100);

        //Continuous mode (not interractive)
        #else
        //Small cur_sec => slow transition (does NOT affect the actual fps)
        cur_sec += 0.05;
        #endif


        if (cur_sec > secs + 0.5)
        {
            cur_sec  -= secs + 0.5;
            sec_init += secs + 0.5;
            cur_img   = (cur_img + 1) % IMAGE_COUNT;
        }
        else if (cur_sec < 0.f)
        {
            cur_sec  += secs + 0.5;
            sec_init -= secs + 0.5;
            cur_img   = (cur_img + IMAGE_COUNT - 1) % IMAGE_COUNT;
        }

        //nema_cl_wait(cur_cl);

        if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
        {
            dsi_send_frame_single(NEMADC_OUTP_OFF);
        }
        else
        {
            nemadc_send_frame_single();
        }

        nema_cl_wait(cur_cl);

        cur_fb_base_phys = nema_swap_fb(0);
    }
    // #endif
}
