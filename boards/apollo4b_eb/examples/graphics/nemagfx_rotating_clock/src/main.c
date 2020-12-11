//*****************************************************************************
//
//! @file main.c
//!
//! @brief NemaGFX example.
//! this example use two frame buffer demonstrate a digital rotating clock,
//! with Nema GPU support, the shader effect continue shows while timer passing
//! need a timer to get the accurate time past.
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
#include "nema_easing.h"
#include "am_hal_global.h"
#include "nema_programHW.h"
#include "string.h"

#ifndef DONT_USE_NEMADC
#include "nema_dc.h"
#include "nema_dc_mipi.h"
#endif

#include "am_devices_dsi_rm67162.h"
#include "am_devices_nemadc_rm67162.h"
#include "numbers_rgba.h"

// #define CUSTOM_TIME
#define RESX 384
#define RESY 192

#define PI 3.14159265f

#define DEG2RAD(a)  ((float)(a) * PI / 180.0f)
#define CHAR_ANGLE  (360.f / (PLEURES))

#define PLEURES  (10)
#define ALTITUDE (0.8f)
#define WIDTH2   (0.259936f)

static int VIEWPORT_WIDTH  = RESX;
static int VIEWPORT_HEIGHT = (RESX / 4 * 3);
#define ZNEAR           (1)
#define ZFAR            (15)

#define BG_COLOR        (0)

#define TEX_DIM       80
#define TEX_PIXSIZE   2

#define CLIP_YMIN     ((int)(0.091f * VIEWPORT_HEIGHT)     )
#define CLIP_YMAX     ((int)(0.609f * VIEWPORT_HEIGHT)     )
#define CLIP_XMIN     ((int)(0.05625f * VIEWPORT_WIDTH)    )
#define CLIP_XMAX     ((int)(VIEWPORT_WIDTH - CLIP_XMIN)  )

#define CLIP_SEC01    ((int)(0.71875f * VIEWPORT_WIDTH) + 35 )
#define CLIP_SEC10    ((int)(0.64375f * VIEWPORT_WIDTH)    )
#define CLIP_MIN01    ((int)(0.5f * VIEWPORT_WIDTH)        )
#define CLIP_MIN10    ((int)(VIEWPORT_WIDTH - CLIP_SEC10) )
#define CLIP_HOUR01   ((int)(VIEWPORT_WIDTH - CLIP_SEC01) )

static int clip_x = 0; //store current clipping area each time
static int clip_y = 0;
static int clip_w = 0;
static int clip_h = 0;

static int minx = 0; //store previous dirty region area each time
static int miny = 0;
static int maxx = 0;
static int maxy = 0;
// ------------------- Framebuffer Related ----------------------

static nema_cmdlist_t cl;
static nema_cmdlist_t cl_blit_fb;

static uintptr_t prv_fb_base_phys;
static uintptr_t cur_fb_base_phys;
static uint32_t cur_fb = 0;
static uint32_t prv_fb = 0;

#define FRAME_BUFFERS 2
static img_obj_t fb[FRAME_BUFFERS];

#ifndef DONT_USE_NEMADC
static nemadc_layer_t dc_layer[FRAME_BUFFERS];
#endif

static img_obj_t numbers_img = {{0}, TEX_DIM, TEX_DIM*10, TEX_DIM*4, 0, NEMA_RGBA8888, NEMA_FILTER_BL};

static void
load_objects(void)
{
    for (int i = 0; i < FRAME_BUFFERS; ++i)
    {
        fb[i].w = RESX;
        fb[i].h = RESY;
        fb[i].format = NEMA_RGB565;
        fb[i].stride = RESX * 2;
        fb[i].bo = nema_buffer_create(fb[i].stride * fb[i].h);
        nema_buffer_map(&fb[i].bo);

#ifndef DONT_USE_NEMADC
        dc_layer[i].sizex = dc_layer[i].resx = fb[i].w;
        dc_layer[i].sizey = dc_layer[i].resy = fb[i].h;
        dc_layer[i].stride = fb[i].stride;
        dc_layer[i].format = NEMADC_RGB565;
        dc_layer[i].blendmode = NEMADC_BL_SRC;
        dc_layer[i].baseaddr_phys = fb[i].bo.base_phys;
        dc_layer[i].baseaddr_virt = fb[i].bo.base_virt;
#endif
        printf("FB: V:%p P:0x%08x\n", (void *)fb[i].bo.base_virt, fb[i].bo.base_phys);
    }

    numbers_img.bo = nema_buffer_create(numbers_rgba_length);
    memcpy((void*)(numbers_img.bo.base_phys), numbers_rgba, numbers_rgba_length);
}

static float prev_hour10_f ;
static float prev_hour01_f ;
static float prev_min10_f  ;
static float prev_min01_f  ;
static float prev_sec10_f  ;
static float prev_sec01_f  ;

void suite_init(void)
{
    load_objects();

    cur_fb = 0;
    cur_fb_base_phys = fb[cur_fb].bo.base_phys;

#ifndef DONT_USE_NEMADC
    //Format        | Pixclock | RESX | FP | SYNC | BP | RESY | FP | SYNC | BP
    //800x600, 60Hz | 40.000   | 800  | 40 | 128  | 88 | 600  | 1  | 4    | 23
//    nemadc_timing(800, 40, 128, 88, 600, 1, 4, 23);
//    nemadc_set_layer(0, &dc_layer[0]);
#endif

    nema_clear(0);
    //Create Command Lists
    cl  = nema_cl_create();
    cl_blit_fb = nema_cl_create();

    prev_hour10_f = -1.0f;
    prev_hour01_f = -1.0f;
    prev_min10_f  = -1.0f;
    prev_min01_f  = -1.0f;
    prev_sec10_f  = -1.0f;
}

typedef enum
{
    ALL = 0,
    HOUR10_CHANGED,
    HOUR01_CHANGED,
    MIN10_CHANGED,
    MIN01_CHANGED,
    SEC10_CHANGED,
    SEC01_CHANGED
} time_state_t ;

static time_state_t state;
static int count_frames = 0;

static int      max_number = 9;

//height2 = tan(DEG2RAD(CHAR_ANGLE)/2)*ALTITUDE;
static const float height2 = 0.3249196962*ALTITUDE;

//    gluPerspective(40, width / (float) height, 1, 15);
static float proj_matrix[4][4] = {{ 5.3365272918, 0.0000,  0.0000,  0.0000},
                           { 0.0000, 7.1153697224,  0.0000,  0.0000},
                           { 0.0000, 0.0000, -1.0833333333, -1.0000},
                           { 0.0000, 0.0000, -2.0833333333,  0.0000}};

static float modelview_matrix[4][4] = {{ 1.0000, 0.0000,  0.0000, 0.0000},
                                { 0.0000, 1.0000,  0.0000, 0.0000},
                                { 0.0000, 0.0000,  1.0000, 0.0000},
                                { 0.0000, -0.50000, -11.8980, 1.0000}};

static float mvp_matrix[4][4];

static float v[PLEURES * 2 + 2][4];

static void
obj2Window_coords(float dstvec[4], float mvp[4][4], float vec[4])
{
    ///http://www.songho.ca/opengl/gl_transform.html
    ///vec: Obj Coords

    int y = 0;
    dstvec[3] = mvp[0][3] * vec[0] + mvp[1][3] * vec[1] + mvp[2][3] * vec[2] + mvp[3][3] * vec[3];
    for (y = 0; y < 3; ++y)
    {
        ///Clip Coords
        dstvec[y] = mvp[0][y] * vec[0] + mvp[1][y] * vec[1] + mvp[2][y] * vec[2] + mvp[3][y] * vec[3];
        ///NDC Coords
        dstvec[y] /= dstvec[3];
    }

    ///Window Coords
    dstvec[0] = (VIEWPORT_WIDTH  / 2) * dstvec[0] + (VIEWPORT_WIDTH  / 2);
    dstvec[1] = (VIEWPORT_HEIGHT / 2) * dstvec[1] + (VIEWPORT_HEIGHT / 2);
    dstvec[2] = ((ZFAR-ZNEAR) * dstvec[2] + (ZFAR + ZNEAR)) / 2;
}

static void
matmult_4x4(float dstmat[4][4], float mat0[4][4], float mat1[4][4])
{
    ///GL matrices are COLUMN WISE
    int x, y;

    for (x = 0; x < 4; ++x)
    {
        for (y = 0; y < 4; ++y)
        {
            dstmat[x][y] = mat0[0][y]*mat1[x][0] + mat0[1][y]*mat1[x][1] + mat0[2][y]*mat1[x][2] + mat0[3][y]*mat1[x][3];
        }
    }
}

static inline void
calculate_MVP(void)
{
    //mvp_matrix = modelview_matrix * proj_matrix
    matmult_4x4(mvp_matrix, proj_matrix, modelview_matrix);
}

static void
rotateX(float angle)
{
    float cos0 = nema_cos(angle);
    float sin0 = nema_sin(angle);

    modelview_matrix[1][1] =  cos0;
    modelview_matrix[1][2] =  sin0;
    modelview_matrix[2][1] = -sin0;
    modelview_matrix[2][2] =  cos0;
}

static void
calculate_vertices(float front_pleura)
{
    float vec0[4] = { WIDTH2, -height2, ALTITUDE, 1.0};
    float vec1[4] = {-WIDTH2, -height2, ALTITUDE, 1.0};
    int id = 0;

    for (id = 0; id <= max_number + 1; ++id)
    {
        rotateX((front_pleura - id)*CHAR_ANGLE);
        calculate_MVP();

        obj2Window_coords(v[id * 2    ], mvp_matrix, vec0);
        obj2Window_coords(v[id * 2 + 1], mvp_matrix, vec1);
    }
}

static void
draw_pleura(float front_pleura, int id)
{
    int tex_id = id;

    id = id % PLEURES;
    int next_id = (id + 1) % PLEURES;
    int prev_id = (id + PLEURES - 1) % PLEURES;

    float angle = (front_pleura - id) * CHAR_ANGLE;
    float cos_angle = nema_cos(angle);
    unsigned col;

    col = (int)(50.f * (cos_angle + 1.f) + 75.f);

    if ((int)front_pleura == id)
    {
        col += 80.0 * (id + 1 - front_pleura);
    }
    else if ((int)front_pleura == prev_id)
    {
        col += 80.0 * (front_pleura - prev_id);
    }
    col = ((col) << 24) | ((col) << 0) | ((col) << 8) | ((col) << 16);

    nema_set_const_color(col);
    nema_blit_subrect_quad_fit( v[     id * 2 + 1][0], v[     id * 2 + 1][1],
                                v[     id * 2    ][0], v[     id * 2    ][1],
                                v[next_id * 2    ][0], v[next_id * 2    ][1],
                                v[next_id * 2 + 1][0], v[next_id * 2 + 1][1],
                                0, tex_id * TEX_DIM, TEX_DIM, TEX_DIM);
}

static void
draw_polupleuro(float front_pleura_f)
{
    int front_pleura = ((int)front_pleura_f) % PLEURES;
    int back_pleura = (int)(front_pleura + 0.5 * PLEURES) % PLEURES;
    int pleura = back_pleura;

    calculate_vertices(front_pleura_f);

    do
    {
        if (pleura <= max_number)
        {
            draw_pleura(front_pleura_f, pleura);
        }
        pleura = (pleura + 1) % PLEURES;
    } while (pleura != front_pleura);

    pleura = back_pleura;

    do
    {
        pleura = (pleura + PLEURES - 1) % PLEURES;

        if (pleura <= max_number)
        {
            draw_pleura(front_pleura_f, pleura);
        }
    } while (pleura != front_pleura);
}

static void
position_and_draw_polupleuro(int number, float pos, float front_pleura)
{
    max_number = number;
    modelview_matrix[3][0] = pos;
    draw_polupleuro(front_pleura);
}

static void
set_cur_clip_area(int x, int y, int w, int h)
{
   clip_x = x;
   clip_y = y;
   clip_w = w;
   clip_h = h;
}

static void
blit_previous_fb()
{
    if ( count_frames >= 2 )
    {
        // new drawing bbox
        int new_minx = clip_x;
        int new_miny = clip_y;
        int new_maxx = clip_x + clip_w - 1;
        int new_maxy = clip_y + clip_h - 1;

        // check if new bbox is included prev bbox
        bool a = new_minx > minx;
        bool b = new_miny > miny;
        bool c = new_maxx < maxx;
        bool d = new_maxy < maxy;

        if ( !a || !b || !c || !d)
        {
            int x = a ? minx : new_minx;
            int y = b ? miny : new_miny;
            int w = (c ? maxx : new_maxx) -
                    (a ? minx : new_minx) ;
            int h = (d ? maxy : new_maxy) -
                    (b ? miny : new_miny) ;

            nema_cl_bind(&cl_blit_fb);
            nema_cl_rewind(&cl_blit_fb);

            nema_bind_dst_tex(cur_fb_base_phys, fb[0].w, fb[0].h, fb[0].format, fb[0].stride);
            nema_bind_src_tex(prv_fb_base_phys, fb[prv_fb].w, fb[prv_fb].h, fb[prv_fb].format, fb[prv_fb].stride, NEMA_FILTER_BL);
            nema_set_clip(0, 0, RESX, RESY);
            nema_set_blend_blit(NEMA_BL_SRC);
            nema_blit_subrect(x, y, w, h, x, y);

            nema_cl_submit(&cl_blit_fb);
            nema_cl_wait(&cl_blit_fb);
        }
    }
}

#define EASE nema_ez_bounce_out

static void
draw_frame(unsigned hour, unsigned min, unsigned sec, float msec)
{
    unsigned sec01  = sec % 10;
    unsigned sec10  = sec / 10;
    unsigned min01  = min % 10;
    unsigned min10  = min / 10;
    unsigned hour01 = hour % 10;
    unsigned hour10 = hour / 10;

    //calulate time in float format
    float sec01_f  = (msec + sec01);
    float sec10_f  = sec01 != 9  ? sec10 :
                     sec10 != 5  ? (EASE(msec)     + (float)sec10):
                                   (EASE(msec)*5.f + (float)sec10);

    float min01_f  = sec   != 59 ? min01 :
                                  (EASE(msec) + min01);
    float min10_f  = min01 != 9 || sec != 59
                           ? min10 :
                             min10 != 5  ? (EASE(msec)     + min10):
                                           (EASE(msec)*5.f + min10);

    float hour01_f = sec   != 59 || min != 59
                          ? hour01 :
                            hour == 23 ? (EASE(msec)*7.f + hour01) :
                            (EASE(msec) + hour01);
    float hour10_f = sec   != 59 || min != 59 || (hour01 != 9 && hour != 23)
                          ? hour10 :
                            hour01 == 9 ? (EASE(msec)     + hour10):
                                          (EASE(msec)*8.f + hour10);

   // find current state and clipping area

    if (count_frames >= 0) // for the first 2 cycles clear the whole screen (2 buffers)
    {
        set_cur_clip_area(0, 0, RESX, RESY);
        count_frames++;
        state = ALL;
    }
    else
    {
        if ( prev_hour10_f != hour10_f)
        {
            set_cur_clip_area(CLIP_XMIN, CLIP_YMIN, CLIP_XMAX-CLIP_XMIN, CLIP_YMAX);
            state = HOUR10_CHANGED;
        }
        else if ( prev_hour01_f != hour01_f)
        {
            set_cur_clip_area(CLIP_HOUR01, CLIP_YMIN, CLIP_XMAX-CLIP_HOUR01, CLIP_YMAX);
            state = HOUR01_CHANGED;
        }
        else if ( prev_min10_f != min10_f)
        {
            set_cur_clip_area(CLIP_MIN10, CLIP_YMIN, CLIP_XMAX - CLIP_MIN10, CLIP_YMAX);
            state = MIN10_CHANGED;
        }
        else if ( prev_min01_f != min01_f)
        {
            set_cur_clip_area(CLIP_MIN01, CLIP_YMIN, CLIP_XMAX - CLIP_MIN01, CLIP_YMAX);
            state = MIN01_CHANGED;
        }
        else if ( prev_sec10_f != sec10_f)
        {
            set_cur_clip_area(CLIP_SEC10, CLIP_YMIN, CLIP_XMAX - CLIP_SEC10, CLIP_YMAX);
            state = SEC10_CHANGED;
        }
        else if ( prev_sec01_f != sec01_f)
        {
            set_cur_clip_area(CLIP_SEC01, CLIP_YMIN, CLIP_XMAX- CLIP_SEC01, CLIP_YMAX);
            state = SEC01_CHANGED;
        }
    }

    //Blit previous clipping area union with current clipping area
#if (FRAME_BUFFERS > 1)
    blit_previous_fb();
#endif
    //Draw current clipping area
    nema_cl_bind(&cl);
    nema_cl_rewind(&cl);

    nema_bind_dst_tex(cur_fb_base_phys, fb[0].w, fb[0].h, fb[0].format, fb[0].stride);
    nema_bind_src_tex(numbers_img.bo.base_phys, numbers_img.w, numbers_img.h, numbers_img.format, numbers_img.stride, numbers_img.sampling_mode);
    nema_set_clip(clip_x, clip_y, clip_w, clip_h);

    nema_clear(0);
    nema_set_blend_blit(NEMA_BL_SIMPLE | NEMA_BLOP_MODULATE_RGB);
    nema_clear_dirty_region();

    // draw and position each polupleuro only if it changes state
    switch(state)
    {
        case ALL:
        case HOUR10_CHANGED:
        case HOUR01_CHANGED:
            position_and_draw_polupleuro(2, -1.56, hour10_f);
            position_and_draw_polupleuro(9, -1.00, hour01_f);
        case MIN10_CHANGED:
            position_and_draw_polupleuro(5, -0.28, min10_f);
        case MIN01_CHANGED:
            position_and_draw_polupleuro(9, 0.28, min01_f);
        case SEC10_CHANGED:
        case SEC01_CHANGED:
        default:
            position_and_draw_polupleuro(9, 1.56, sec01_f);
            position_and_draw_polupleuro(5, 1.00, sec10_f);
            break;
    }

    //Submit Command List
#if (FRAME_BUFFERS == 1)
    // If we have only 1 FB
    // DC and NemaP are operating on the same FB
    // make sure previous display update has finished
    // and then submit CL
    nemadc_wait_vsync();
#endif
    nema_cl_submit(&cl);

#if (FRAME_BUFFERS > 1)
    // If we have >= 2 FBs
    // DC and NemaP are operating on different FBs
    // So we can push "wait_vsync" closer to "send_frame_single"
    nemadc_wait_vsync();
#endif

    //Wait for submitted Command List to finish
    nema_cl_wait(&cl);

    nemadc_set_layer(0, &dc_layer[cur_fb]);
    if (g_sDispCfg[g_eDispType].eInterface == IF_DSI)
    {
        dsi_send_frame_single(NEMADC_OUTP_OFF);
    }
    else
    {
        nemadc_send_frame_single();
    }


    nema_get_dirty_region(  //store current dirty region (In next call it will be previous dirty region)
        &minx,
        &miny,
        &maxx,
        &maxy
        );

    prev_hour10_f = hour10_f;
    prev_hour01_f = hour01_f;
    prev_min10_f = min10_f;
    prev_min01_f = min01_f;
    prev_sec10_f = sec10_f;
    prev_sec01_f = sec01_f;
}

static inline uintptr_t
nema_swap_fb(int layer)
{
    cur_fb = (cur_fb + 1) % FRAME_BUFFERS;
    return fb[cur_fb].bo.base_phys;
}

#ifdef STANDALONE
int main()
#else
int rotating_clock()
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

    suite_init();

#ifdef CUSTOM_TIME
    static float time0 = 23.f*60.f*60.f + 59.f*60.f + 30.f;
#endif
#if ENABLE_QSPI
    uint32_t dbi_cfg = nemadc_reg_read( NEMADC_REG_DBIB_CFG);   //FIXME
    nemadc_MIPI_CFG_out( dbi_cfg | MIPICFG_SPI_HOLD  );         //FIXME
#endif

    while(1)
    {

#ifdef CUSTOM_TIME  //calculate current time
        float time = time0;
        time0 += 2.f / 60.f;

        if (time0 >= 24.f * 60.f * 60.f)
        {
            time0 = 0.f;
        }
#else
        float time = nema_get_time();
#endif

        int sec  = (int)time % 60;
        int hour = (int)(time / 60 / 60);
        int min  = ((int)(time / 60)) % 60;
        float msec = (time - (float)(int)time);

        draw_frame(hour, min, sec, msec);

        prv_fb = cur_fb; //store current buffer to previous
        prv_fb_base_phys = cur_fb_base_phys;
        cur_fb_base_phys = nema_swap_fb(0);
        nema_calculate_fps();
    }
}
