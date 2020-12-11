//*****************************************************************************
//
//! @file am_hal_timer.c
//!
//! @brief
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

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"

//
// Configure a TIMER
//
uint32_t
am_hal_timer_config(uint32_t ui32TimerNumber,
                    am_hal_timer_config_t *psTimerConfig)
{
    uint32_t ui32ConfigVal1, ui32ConfigVal2;

    //
    // Build up a value in SRAM before we start writing to the timer control
    // registers.
    //
    ui32ConfigVal1 = _VAL2FLD(TIMER_CTRL0_TMR0CLK,     psTimerConfig->eInputClock);
    ui32ConfigVal1 |= _VAL2FLD(TIMER_CTRL0_TMR0FN,      psTimerConfig->eFunction);
    ui32ConfigVal1 |= _VAL2FLD(TIMER_CTRL0_TMR0POL1,    psTimerConfig->bInvertOutput1);
    ui32ConfigVal1 |= _VAL2FLD(TIMER_CTRL0_TMR0POL0,    psTimerConfig->bInvertOutput0);
    ui32ConfigVal1 |= _VAL2FLD(TIMER_CTRL0_TMR0TMODE,   psTimerConfig->eTriggerType);
    ui32ConfigVal1 |= _VAL2FLD(TIMER_CTRL0_TMR0LMT,     psTimerConfig->ui32PatternLimit);
    ui32ConfigVal1 |= _VAL2FLD(TIMER_CTRL0_TMR0EN, 0);

    ui32ConfigVal2 = _VAL2FLD(TIMER_MODE0_TMR0TRIGSEL, psTimerConfig->eTriggerSource);

    //
    // Disable the timer.
    //
    TIMERn(ui32TimerNumber)->CTRL0_b.TMR0EN = 0;

    //
    // Apply the settings from the configuration structure.
    //
    TIMERn(ui32TimerNumber)->CTRL0 = ui32ConfigVal1;
    TIMERn(ui32TimerNumber)->MODE0 = ui32ConfigVal2;
    TIMERn(ui32TimerNumber)->TMR0CMP0 = psTimerConfig->ui32Compare0;
    TIMERn(ui32TimerNumber)->TMR0CMP1 = psTimerConfig->ui32Compare1;

    //
    // Clear the timer to make sure it has the appropriate starting value.
    //
    TIMERn(ui32TimerNumber)->CTRL0_b.TMR0CLR = 1;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Initialize a timer configuration structure with default values.
//
uint32_t
am_hal_timer_default_config_set(am_hal_timer_config_t *psTimerConfig)
{

    psTimerConfig->eInputClock = AM_HAL_TIMER_CLOCK_HFRC_DIV4;
    psTimerConfig->eFunction = AM_HAL_TIMER_FN_EDGE;
    psTimerConfig->ui32Compare0 = 0xFFFFFFFF;
    psTimerConfig->ui32Compare1 = 0xFFFFFFFF;
    psTimerConfig->bInvertOutput0 = false;
    psTimerConfig->bInvertOutput1 = false;
    psTimerConfig->eTriggerType = AM_HAL_TIMER_TRIGGER_DIS;
    psTimerConfig->eTriggerSource = AM_HAL_TIMER_TRIGGER_TMR0_CMP0;
    psTimerConfig->ui32PatternLimit = 0;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Reset the timer to the default state.
//
uint32_t
am_hal_timer_reset_config(uint32_t ui32TimerNumber)
{
    //
    // Disable Interrupts.
    //
    am_hal_timer_interrupt_disable(3 << (ui32TimerNumber * 2));

    //
    // Enable the Global enable.
    //
    am_hal_timer_enable_sync(1 << ui32TimerNumber);

    //
    // Reset the Timer specific registers.
    //
    TIMERn(ui32TimerNumber)->CTRL0 = 0;
    TIMERn(ui32TimerNumber)->TIMER0 = 0;
    TIMERn(ui32TimerNumber)->TMR0CMP0 = 0;
    TIMERn(ui32TimerNumber)->TMR0CMP1 = 0;
    TIMERn(ui32TimerNumber)->MODE0 = 0;

    am_hal_timer_interrupt_clear(3 << (ui32TimerNumber *2));

    return AM_HAL_STATUS_SUCCESS;
}

//
// Enable a single TIMER
//
uint32_t
am_hal_timer_enable(uint32_t ui32TimerNumber)
{
    //
    // Enable the timer in both the individual enable register and the global
    // sync register.
    //
    TIMER->GLOBEN |= 1 << ui32TimerNumber;
    TIMERn(ui32TimerNumber)->CTRL0_b.TMR0EN = 1;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Disable a single TIMER
//
uint32_t
am_hal_timer_disable(uint32_t ui32TimerNumber)
{
    //
    // Disable the timer in its individual register.
    //
    TIMERn(ui32TimerNumber)->CTRL0_b.TMR0EN = 0;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Enable a group of TIMERS all at once
//
uint32_t
am_hal_timer_enable_sync(uint32_t ui32TimerMask)
{
    //
    // Disable the timers in the global sync register, make sure they are all
    // individually enabled, and then re-enable them in the global sync
    // register.
    //
    AM_CRITICAL_BEGIN;

    TIMER->GLOBEN &= ~(ui32TimerMask);

    for (uint32_t i = 0; i < AM_REG_NUM_TIMERS; i++)
    {
        if ((1 << i) & ui32TimerMask)
        {
            TIMERn(i)->CTRL0_b.TMR0EN = 1;
        }
    }

    TIMER->GLOBEN |= ui32TimerMask;

    AM_CRITICAL_END;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Disable a group of TIMERS all at once
//
uint32_t
am_hal_timer_disable_sync(uint32_t ui32TimerMask)
{
    //
    // Disable the timers in the global sync register, make sure they are all
    // individually disabled, and then re-enable them in the global sync
    // register.
    //
    AM_CRITICAL_BEGIN;

    TIMER->GLOBEN &= ~(ui32TimerMask);

    for (uint32_t i = 0; i < AM_REG_NUM_TIMERS; i++)
    {
        if ((1 << i) & ui32TimerMask)
        {
            TIMERn(i)->CTRL0_b.TMR0EN = 0;
        }
    }

    AM_CRITICAL_END;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Start a single TIMER
//
uint32_t
am_hal_timer_start(uint32_t ui32TimerNum)
{
    AM_CRITICAL_BEGIN;

    // Enable the timer.
    TIMERn(ui32TimerNum)->CTRL0_b.TMR0EN = 1;

    AM_CRITICAL_END;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Disable a single TIMER
//
uint32_t
am_hal_timer_stop(uint32_t ui32TimerNum)
{
    AM_CRITICAL_BEGIN;

    // Disable the timer.
    TIMERn(ui32TimerNum)->CTRL0_b.TMR0EN = 0;

    AM_CRITICAL_END;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Clear a single TIMER
//
uint32_t
am_hal_timer_clear(uint32_t ui32TimerNum)
{
    AM_CRITICAL_BEGIN;

    // Clear the timer.
    TIMERn(ui32TimerNum)->CTRL0_b.TMR0CLR = 1;  // FIXME - Do we need to set this back to 0?

    AM_CRITICAL_END;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Read the current value of a timer.
//
uint32_t
am_hal_timer_read(uint32_t ui32TimerNumber)
{
    // Read the current timer value.
    return TIMERn(ui32TimerNumber)->TIMER0;
}

//
// Write the current value of a timer.
//
uint32_t
am_hal_timer_write(uint32_t ui32TimerNumber, uint32_t ui32Value)
{
    // Write the current timer value.
    TIMERn(ui32TimerNumber)->TIMER0 = ui32Value;
    return AM_HAL_STATUS_SUCCESS;
}

//
// Set the COMPARE0 value for a single timer.
//
uint32_t
am_hal_timer_compare0_set(uint32_t ui32TimerNumber,
                          uint32_t ui32CompareValue)
{
    //
    // Apply the Compare0 value without disabling the timer.
    //
    TIMERn(ui32TimerNumber)->TMR0CMP0 = ui32CompareValue;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Set the COMPARE1 value for a single timer.
//
uint32_t
am_hal_timer_compare1_set(uint32_t ui32TimerNumber,
                          uint32_t ui32CompareValue)
{
    //
    // Apply the Compare1 value without disabling the timer.
    //
    TIMERn(ui32TimerNumber)->TMR0CMP1 = ui32CompareValue;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Configure a TIMER output.
//
uint32_t
am_hal_timer_output_config(uint32_t ui32GPIONum,
                           am_hal_timer_output_e eOutputType)
{
    // Need to implement.  Return an error for now.
    return AM_HAL_STATUS_FAIL;
}

//
// Enable timer interrupts.
//
uint32_t
am_hal_timer_interrupt_enable(uint32_t ui32InterruptMask)
{
    TIMER->INTEN |= ui32InterruptMask;

    return AM_HAL_STATUS_SUCCESS;
}

//
// Disable timer interrupts.
//
uint32_t
am_hal_timer_interrupt_disable(uint32_t ui32InterruptMask)
{
    TIMER->INTEN &= ~(ui32InterruptMask);

    return AM_HAL_STATUS_SUCCESS;
}

//
// Get the timer interrupt status.
//
uint32_t
am_hal_timer_interrupt_status_get(bool bEnabledOnly, uint32_t *pui32IntStatus)
{
    DIAG_SUPPRESS_VOLATILE_ORDER()

    if (bEnabledOnly)
    {
        *pui32IntStatus = TIMER->INTSTAT & TIMER->INTEN;
    }
    else
    {
        *pui32IntStatus = TIMER->INTSTAT;
    }

    return AM_HAL_STATUS_SUCCESS;

    DIAG_DEFAULT_VOLATILE_ORDER()
}

//
// Clear timer interrupts.
//
uint32_t
am_hal_timer_interrupt_clear(uint32_t ui32InterruptMask)
{
    TIMER->INTCLR = ui32InterruptMask;
    return AM_HAL_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
