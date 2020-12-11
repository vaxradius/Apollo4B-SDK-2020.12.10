//*****************************************************************************
//
//! @file hp_mode_192mhz.c
//!
//! @brief Example demonstrates the usage of High Performance Mode(192MHz) HAL.
//!
//! Purpose: This example sets the Apollo4 into Low Power Mode(96MHz), then
//! times a calculation of prime numbers, displaying the elapsed time. Next,
//! it switches the Apollo4 into High Performance Mode(192MHz), performs the
//! the same calculation, then displays the elapsed time, which should be
//! roughly 50% of Low Power Mode.
//!
//! Printing takes place over the ITM at 1M Baud.
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

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
#define NUM_OF_PRIMES_IN 10000
#define EXP_PRIMES 9592

//*****************************************************************************
//
// Init function for the STimer.
//
//*****************************************************************************
void
stimer_init(void)
{
    //
    // Configure the STIMER and run
    //
    am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
    am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
}

//*****************************************************************************
//
//  Purpose:
//
//    prime_number() returns the number of primes between 1 and N.
//
//  Discussion:
//
//    A naive algorithm is used.
//
//    Mathematica can return the number of primes less than or equal to N
//    by the command PrimePi[N].
//
//                N  PRIME_NUMBER
//
//                1           0
//               10           4
//              100          25
//            1,000         168
//           10,000       1,229
//          100,000       9,592
//        1,000,000      78,498
//       10,000,000     664,579
//      100,000,000   5,761,455
//    1,000,000,000  50,847,534
//
//  Licensing:
//
//    This code is distributed under the GNU LGPL license.
//
//  Modified:
//
//    23 April 2009
//
//  Author:
//
//    John Burkardt
//
//  Parameters:
//
//    Input, int N, the maximum number to check.
//
//    Output, int PRIME_NUMBER, the number of prime numbers up to N.
//
//*****************************************************************************
uint32_t
prime_number(int32_t i32n)
{
    uint32_t ui32Total, ui32Prime;
    int32_t ix, jx;

    ui32Total = 0;

    for ( ix = 2; ix <= i32n; ix++ )
    {
        ui32Prime = 1;
        for ( jx = 2; jx < ix; jx++ )
        {
            if ( (ix % jx) == 0 )
            {
                ui32Prime = 0;
                break;
            }
        }
        ui32Total += ui32Prime;
    }

    return ui32Total;
}

//*****************************************************************************
//
// Main function.
//
//*****************************************************************************
int
main(void)
{
    uint32_t                      ui32StartTime, ui32StopTime;
    uint32_t                      ui32HPModeDelta, ui32LPModeDelta;
    
    am_util_delay_ms(5000);

    //
    // Set the default cache configuration
    //
    // FIXME - Caching does not seem to work with 192MHz workaround.
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();

    //
    // Configure the board for low power operation.
    //
    am_bsp_low_power_init();

    //
    // Initialize the printf interface for ITM/SWO output.
    //
    am_bsp_itm_printf_enable();

    //
    // Clear the terminal and print the banner.
    //
    am_util_stdio_terminal_clear();
    am_util_stdio_printf("Apollo4 192MHz High Performance Mode Example\n");

    //
    // STIMER init.
    //
    stimer_init();
    
#if defined(AM_PART_APOLLO4B)   
    //Turn on simobuck
    MCUCTRL->SIMOBUCK0 = 7;
    PWRCTRL->VRCTRL_b.SIMOBUCKEN = 1;
    
    // Wait for SIMOBuck to stabilize.
    am_hal_delay_us(10000); //delay 10ms
#endif

    //
    // Initialize for Low Power Mode
    //
    if ( am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER) == AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("\nOperating in Low Power Mode (96MHz)\n");
        //
        // Execute the example algorithm.
        //
        am_util_stdio_printf("\nStarted calculating primes in Low Power Mode\n");

        //
        // Capture the start time.
        //
        ui32StartTime = am_hal_stimer_counter_get();

        am_util_stdio_printf("\nNumber of Primes: %d\n", prime_number(NUM_OF_PRIMES_IN));

        //
        // Stop the timer and calculate the elapsed time.
        //
        ui32StopTime = am_hal_stimer_counter_get();

        //
        // Calculate the Low Power mode delta time.
        //
        ui32LPModeDelta = ui32StopTime - ui32StartTime;
        am_util_stdio_printf("Low Power Mode Delta: %d\n", ui32LPModeDelta);
    }
    else
    {
        am_util_stdio_printf("\nFailed to Initialize for Low Power Mode operation\n");
    }

    //
    // Initialize for High Performance Mode
    //
    if ( am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE) == AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("\nOperating in High Performance Mode (192MHz)\n");
        //
        // Execute the example algorithm.
        //
        am_util_stdio_printf("\nStarted calculating primes in High Performance Mode\n");

        //
        // Capture the start time.
        //
        ui32StartTime = am_hal_stimer_counter_get();

        am_util_stdio_printf("\nNumber of Primes: %d\n", prime_number(NUM_OF_PRIMES_IN));

        //
        // Stop the timer and calculate the elapsed time.
        //
        ui32StopTime = am_hal_stimer_counter_get();

        //
        // Calculate the High Performance mode delta time.
        //
        ui32HPModeDelta = ui32StopTime - ui32StartTime;
        am_util_stdio_printf("High Performance Mode Delta: %d\n", ui32HPModeDelta);
    }
    else
    {
        am_util_stdio_printf("\nFailed to Initialize for High Performance Mode operation\n");
    }

    //
    // Return to Low-Power mode (96MHz).
    //
    if ( am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER) == AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("\nReturned to operating in Low Power Mode (96MHz)\n");
    }
    else
    {
        am_util_stdio_printf("\nFailed to return to Low Power Mode operation\n");
    }
    
    //
    // End of example.
    //
    am_util_stdio_printf("\n");
    am_util_stdio_printf("192MHz High Performance Mode Example Complete.");

    //
    // Loop forever while sleeping.
    //
    while (1)
    {
        //
        // Go to Deep Sleep.
        //
        am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
    }
}
