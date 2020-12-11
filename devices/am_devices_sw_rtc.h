//*****************************************************************************
//
//! @file am_devices_sw_rtc.h
//!
//! @brief SW based Real Time Clock using STIMER.
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

#ifndef AM_DEVICES_SW_RTC_H
#define AM_DEVICES_SW_RTC_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Global definitions.
//
//*****************************************************************************
#if defined(AM_PART_APOLLO4)
#define STIMER_CLK_PER_HUNDREDTH        1875
#define STIMER_CLK_PER_SECOND           187500
#define STIMER_CLK_PER_MINUTE           11250000
#define STIMER_CLK_PER_HOUR             675000000
#elif defined(AM_PART_APOLLO4B)
#define STIMER_CLK_PER_HUNDREDTH        3750
#define STIMER_CLK_PER_SECOND           375000
#define STIMER_CLK_PER_MINUTE           22500000
#define STIMER_CLK_PER_HOUR             1350000000
#endif

//#define USE_SCMPR5_AS_OVERFLOW

//*****************************************************************************
//
// Global type definitions.
//
//*****************************************************************************
typedef void (*am_devices_sw_rtc_callback_t)(void);

//*****************************************************************************
//
// RTC configuration structure.
//
//*****************************************************************************
typedef struct am_devices_sw_rtc_config_t
{
    am_hal_rtc_osc_select_e eOscillator;
    bool b12Hour;
}
am_devices_sw_rtc_config_t;

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Initialize the SW RTC.
//!
//! @param pConfig      - pointer to the input configuration structure.
//!
//! This function initializes the SW RTC configuration.  It does not start
//! the SW RTC.  It does check for conflicts with the current STIMER settings.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_devices_sw_rtc_initialize(am_devices_sw_rtc_config_t *pConfig);

//*****************************************************************************
//
//! @brief Set the SW RTC Time.
//!
//! @param pTime        - pointer to the RTC time to establish.
//!
//! This function sets the current RTC time and makes sure the STIMER is running.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_devices_sw_rtc_time_set(am_hal_rtc_time_t *pTime);

//*****************************************************************************
//
//! @brief Get the SW RTC Time.
//!
//! @param pTime        - pointer to the RTC time to return.
//!
//! This function calculates and returns the current RTC time.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_devices_sw_rtc_time_get(am_hal_rtc_time_t *pTime);

//*****************************************************************************
//
//! @brief Set an SW RTC alarm.
//!
//! @param pTime        - pointer to the RTC time for the alarm callback.
//! @param pCallback    - pointer to the function to call.
//!
//! This function sets the alarm and stores the callback.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_devices_sw_rtc_alarm_set(am_hal_rtc_time_t *pTime, am_devices_sw_rtc_callback_t pCallback);

//*****************************************************************************
//
//! @brief Service the SW RTC Interrupt functions (STIMER)
//!
//! @param ui32STimerInts - pointer to the RTC time for the alarm callback.
//!
//! This function services the STIMER interrupts that may pertain to the SW RTC.
//!
//! @return none.
//
//*****************************************************************************
extern void am_devices_sw_rtc_interrupt_service(uint32_t ui32STimerInts);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_MSPI_N25Q256A_H

