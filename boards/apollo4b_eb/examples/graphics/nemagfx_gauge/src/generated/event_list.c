//*****************************************************************************
//
//! @file event_list.c
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

#ifndef __EVENT_LIST_C__
#define __EVENT_LIST_C__

const int event_list_size   = 2;

#include "predefined_callbacks.c"
#include "custom_callbacks.c"

event_t event_list[2] =
{
//source_gitem | target_tree_node | Trigger | Action | Next | Callback | Easing | Pending | Progress | Type | Start Time | Duration | Period | Re-trigger| last pause| Effect
    {&_1269Horizontal_Slider1, &node_1244Gauge1, 5, 8, 0, callback_set_value, nema_ez_linear, 0, 0.f, EVENT_ONESHOT, 0.f, 0.5, 1, EVENT_RESET, 0.f, EV_EFFECT_NONE},
    {&_1269Horizontal_Slider1, &node_1251Digital_Meter2, 5, 8, 0, callback_set_value, nema_ez_linear, 0, 0.f, EVENT_ONESHOT, 0.f, 0.5, 1, EVENT_RESET, 0.f, EV_EFFECT_NONE},
};

#endif // __EVENT_LIST_C__

