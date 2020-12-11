//*****************************************************************************
//
//! @file rtc_print.c
//!
//! @brief Example using the internal RTC.
//!
//! This example demonstrates how to interface with the RTC and prints the
//! time over SWO.
//!
//! The example works by configuring a timer interrupt which will periodically
//! wake the core from deep sleep. After every interrupt, it prints the current
//! RTC time.
//!
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


#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_devices_sw_rtc.h"
#include "am_util.h"


#define TIMER_NUM       0

//
// String arrays to index Days and Months with the values returned by the RTC.
//
char *pcWeekday[] =
{
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Invalid day"
};
char *pcMonth[] =
{
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
    "Invalid month"
};

am_hal_rtc_time_t hal_time;
am_hal_rtc_time_t alarm_time;

uint32_t    g_LastSecond = 0;
uint32_t    g_TestCount = 0;

volatile bool g_bAlarmReceived;

am_devices_sw_rtc_config_t g_SwRtcConfig = 
{
  .eOscillator = AM_HAL_RTC_OSC_XT,
  .b12Hour = false,
};

#define SW_RTC_XTAL_TIMER       1

const am_hal_timer_config_t g_sXtalTimerCfg = 
{
  .eInputClock          = AM_HAL_TIMER_CLOCK_XT,
  .eFunction            = AM_HAL_TIMER_FN_CONTINUOUS,
  .bInvertOutput0       = false,
  .bInvertOutput1       = false,
  .eTriggerType         = AM_HAL_TIMER_TRIGGER_DIS,
  .bLowJitter           = false,
  .ui32PatternLimit     = 0,
  .ui32Compare0         = 0xFFFFFFFF,
  .ui32Compare1         = 0xFFFFFFFF
};

//*****************************************************************************
//
// Init function for Timer #0.
//
//*****************************************************************************
void 
timer_init(void)
{
    am_hal_timer_config_t       TimerConfig;


    //
    // Enable the LFRC.
    //
    am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_LFRC_START, 0);

    //
    // Setup Timer #0
    //
    am_hal_timer_default_config_set(&TimerConfig);
    TimerConfig.eInputClock = AM_HAL_TIMER_CLOCK_HFRC_DIV4;
    TimerConfig.eFunction = AM_HAL_TIMER_FN_CONTINUOUS;
    TimerConfig.ui32Compare0 = AM_HAL_CLKGEN_FREQ_MAX_HZ / 4;
    am_hal_timer_config(TIMER_NUM, &TimerConfig);
    am_hal_timer_clear(TIMER_NUM);
    am_hal_timer_interrupt_enable(AM_HAL_TIMER_MASK(TIMER_NUM, AM_HAL_TIMER_COMPARE0));

}

//*****************************************************************************
//
// Timer Interrupt Service Routine (ISR)
//
//*****************************************************************************
void
am_ctimer_isr(void)
{
    //
    // Clear TimerA0 Interrupt (write to clear).
    //
    am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(TIMER_NUM, AM_HAL_TIMER_COMPARE0));

    am_devices_sw_rtc_time_get(&hal_time);

    am_hal_timer_clear(TIMER_NUM);


}

//*****************************************************************************
//
// Check XTAL and calibrate HFRC vs. XTAL.
//
//*****************************************************************************
void 
sw_rtc_hfadj(void)
{
  uint32_t      ui32XtalTimerVal;
  am_hal_gpio_pincfg_t gpio33 = AM_HAL_GPIO_PINCFG_DEFAULT;
  am_hal_gpio_pincfg_t gpio37 = AM_HAL_GPIO_PINCFG_DEFAULT;
  
  // Set GPIO33 to CLKOUT for HFRC.
  CLKGEN->CLKOUT_b.CKSEL = CLKGEN_CLKOUT_CKSEL_HFRC_DIV256;  // Nominal 375KHz output.
  CLKGEN->CLKOUT_b.CKEN = CLKGEN_CLKOUT_CKEN_EN;
  gpio33.GP.cfg_b.uFuncSel = AM_HAL_PIN_33_CLKOUT;
  am_hal_gpio_pinconfig(33, gpio33);
  
  // Put GPIO37 into XTAL 32K output.
  gpio37.GP.cfg_b.uFuncSel = AM_HAL_PIN_37_32KHzXT;
  am_hal_gpio_pinconfig(37, gpio37);
  
  // Set the trigger at the start of HFADJ.
  am_hal_gpio_output_set(85);
  
  // Enable RTC to use XTAL.
  if ( AM_HAL_STATUS_SUCCESS != am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_RTC_SEL_XTAL, NULL))
  {
    am_util_stdio_printf("Failed to start and select XTAL and RTC source\n");
    return;
  }       
  
  //
  // Wait for 1 second for the 32KHz XTAL to startup and stabilize.
  //
  am_util_delay_ms(1000);
  
  // Set up Timer with XTAL input.
  am_hal_timer_config(SW_RTC_XTAL_TIMER, (am_hal_timer_config_t *)&g_sXtalTimerCfg);
  am_hal_timer_enable(SW_RTC_XTAL_TIMER);
  am_hal_timer_start(SW_RTC_XTAL_TIMER);
  
  // Read the time, wait for it to update and make sure the XTAL is actually running.
  ui32XtalTimerVal = am_hal_timer_read(SW_RTC_XTAL_TIMER);
  am_util_delay_us(300);
  if (am_hal_timer_read(SW_RTC_XTAL_TIMER) == ui32XtalTimerVal)
  {
    am_util_stdio_printf("XTAL is not running\n"); 
    return;
  }
  
  // Turn on HFADJ
  // return;
  if (am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_HFADJ_ENABLE, NULL))
  {
    am_util_stdio_printf("Failed to enable HFADJ\n");
    return;
  }
  
  am_util_stdio_printf("Enabled HFRC Adjustment.\n");
  
  // Delay for a second to make sure HFRC adjustment is complete and stable.
  am_util_delay_ms(1000);
  
}

void RtcAlarmCallback (void)
{
     g_bAlarmReceived = true;
}

//*****************************************************************************
//
// Main
//
//*****************************************************************************
int
main(void)
{

    //
    // Set the default cache configuration
    //
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();

    //
    // Configure the board for low power operation.
    //
    am_bsp_low_power_init();

    //
    // Enable the XT for the RTC.
    //
    am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_XTAL_START, 0);

    //
    // Initialize the printf interface for ITM/SWO output.
    //
    am_bsp_itm_printf_enable();

    //
    // Enable the ITM.
    //
    am_hal_itm_enable();
    am_util_delay_ms(100);
    am_util_stdio_printf("SW RTC Printf Example\n");
    
    //
    // Perform HFRC adjustment vs. XTAL.
    //
    sw_rtc_hfadj();

    //
    // Set the RTC time for this example.
    // The RTC is initialized from an arbitrary date/time.
    //
    hal_time.ui32Hour = 14;
    hal_time.ui32Minute = 24;
    hal_time.ui32Second = 33;
    hal_time.ui32Hundredths = 50;
    hal_time.ui32Weekday = 3;
    hal_time.ui32DayOfMonth = 15;
    hal_time.ui32Month = 4;
    hal_time.ui32Year = 20;
    hal_time.ui32Century = 0;

    //
    // Stop the STIMER.
    //
#if defined(AM_PART_APOLLO4)
    am_hal_stimer_config(STIMER_STCFG_CLKSEL_HFRC_187KHZ | 
                         AM_HAL_STIMER_CFG_FREEZE |
                           AM_HAL_STIMER_CFG_CLEAR);
#elif defined(AM_PART_APOLLO4B)
    am_hal_stimer_config(STIMER_STCFG_CLKSEL_HFRC_375KHZ | 
                         AM_HAL_STIMER_CFG_FREEZE |
                           AM_HAL_STIMER_CFG_CLEAR);
#endif
    //
    // Initialize the SW RTC
    //
    am_devices_sw_rtc_initialize(&g_SwRtcConfig);
    
    //
    // Set the start time
    //
    am_devices_sw_rtc_time_set(&hal_time);
    
    //
    //  Set an alarm for 5 seconds in the future with callback.
    //
    am_util_stdio_printf("\n\nSetting an Alarm for 5 seconds in the future\n");
    alarm_time = hal_time;
    am_util_stdio_printf("Time is now: %02d:%02d:%02d\n",
                         hal_time.ui32Hour,
                         hal_time.ui32Minute,
                         hal_time.ui32Second);
    alarm_time.ui32Second += 5;
    g_bAlarmReceived = false;
    am_devices_sw_rtc_alarm_set(&alarm_time, &RtcAlarmCallback);
    while(!g_bAlarmReceived);
    am_devices_sw_rtc_time_get(&hal_time);
    am_util_stdio_printf("Alarm received at: %02d:%02d:%02d\n",
                         hal_time.ui32Hour,
                         hal_time.ui32Minute,
                         hal_time.ui32Second);
    
    //
    // TimerA0 init.
    //
    timer_init();

    //
    // Enable the timer interrupt in the NVIC.
    //
    NVIC_EnableIRQ(TIMER_IRQn);
    am_hal_interrupt_master_enable();

    //
    // Enable the timer.
    //
    am_hal_timer_start(TIMER_NUM);


    //
    // Loop forever, printing when awakened by the timer.
    //
    while (1)
    {
        //
        // Enable debug printf messages using ITM on SWO pin
        //
        am_bsp_debug_printf_enable();

        //
        // Clear the terminal.
        //
        //am_util_stdio_terminal_clear();

        //
        // Print the banner.
        //
        am_util_stdio_printf("\n\n\n\n\n\nRTC Print Example\n");
        //
        // Print RTC time.
        //
        am_devices_sw_rtc_time_get(&hal_time);
        am_util_stdio_printf("\tIt is now ");
        am_util_stdio_printf("%d : ", hal_time.ui32Hour);
        am_util_stdio_printf("%02d : ", hal_time.ui32Minute);
        am_util_stdio_printf("%02d.", hal_time.ui32Second);
        am_util_stdio_printf("%02d ", hal_time.ui32Hundredths);
        am_util_stdio_printf(pcWeekday[hal_time.ui32Weekday]);
        am_util_stdio_printf(" ");
        am_util_stdio_printf(pcMonth[hal_time.ui32Month]);
        am_util_stdio_printf(" ");
        am_util_stdio_printf("%d, ", hal_time.ui32DayOfMonth);
        am_util_stdio_printf("20%02d", hal_time.ui32Year);

        //
        // We are done printing. Disable debug printf messages on ITM.
        //
        am_bsp_debug_printf_disable();

        //
        // Go to Deep Sleep and wait for a wake up.
        //
        am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
    }
}

void
am_stimer_cmpr7_isr(void)
{
  uint32_t      StimerInts = am_hal_stimer_int_status_get(true);
  am_devices_sw_rtc_interrupt_service(StimerInts);
}


void
am_stimerof_isr(void)
{
  uint32_t      StimerInts = am_hal_stimer_int_status_get(true);
  am_devices_sw_rtc_interrupt_service(StimerInts);
}

