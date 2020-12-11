//*****************************************************************************
//
//! @file load_image_assets.c
//!
//! @brief NemaGFX example.
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

img_obj_t _speedo_needle_a8;
img_obj_t _rpm_scaled_rgba;
img_obj_t _fuel2_a8;
img_obj_t _circular_knob_rgba;

img_obj_t *image_assets[4];

#include "assets/images/speedo_needle_a8.h"
#include "assets/images/rpm_scaled_rgba.h"
#include "assets/images/fuel2_a8.h"
#include "assets/images/circular_knob_rgba.h"

void load_image_assets(void)
{
    _speedo_needle_a8.bo = LOAD_ARRAY(speedo_needle_a8);
    _speedo_needle_a8.w = 58;
    _speedo_needle_a8.h = 19;
    _speedo_needle_a8.format = NEMA_A8;
    _speedo_needle_a8.stride = -1;
    _speedo_needle_a8.sampling_mode = 1;
    _speedo_needle_a8.color = 0xff0000ff;

    image_assets[0] = &_speedo_needle_a8;

    _rpm_scaled_rgba.bo = LOAD_ARRAY(rpm_scaled_rgba);
    _rpm_scaled_rgba.w = 224;
    _rpm_scaled_rgba.h = 224;
    _rpm_scaled_rgba.format = NEMA_RGBA8888;
    _rpm_scaled_rgba.stride = -1;
    _rpm_scaled_rgba.sampling_mode = 1;
    _rpm_scaled_rgba.color = 0xff000000;

    image_assets[1] = &_rpm_scaled_rgba;

    _fuel2_a8.bo = LOAD_ARRAY(fuel2_a8);
    _fuel2_a8.w = 30;
    _fuel2_a8.h = 30;
    _fuel2_a8.format = NEMA_A8;
    _fuel2_a8.stride = -1;
    _fuel2_a8.sampling_mode = 1;
    _fuel2_a8.color = 0xffffffff;

    image_assets[2] = &_fuel2_a8;

    _circular_knob_rgba.bo = LOAD_ARRAY(circular_knob_rgba);
    _circular_knob_rgba.w = 20;
    _circular_knob_rgba.h = 20;
    _circular_knob_rgba.format = NEMA_RGBA8888;
    _circular_knob_rgba.stride = -1;
    _circular_knob_rgba.sampling_mode = 1;
    _circular_knob_rgba.color = 0xff000000;

    image_assets[3] = &_circular_knob_rgba;

}

