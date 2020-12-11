//*****************************************************************************
//
//! @file am_hal_dsi.h
//!
//! @brief Hardware abstraction for the DSI
//!
//! @addtogroup
//! @ingroup
//! @{
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

#ifndef AM_HAL_DSI_H
#define AM_HAL_DSI_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// DSI error codes.
//
//*****************************************************************************
/*typedef enum
{
    AM_HAL_DSI_INT_STATUS_RX_START_TRANS_ERROR = AM_HAL_STATUS_MODULE_SPECIFIC_START,
    AM_HAL_DSI_INT_STATUS_RX_START_TRANS_SYNC_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_END_TRANS_SYNC_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_ESCAPE_ENTRY_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_LP_TX_SYNC_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_PERIPH_TIMEOUT_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_FALSE_CONTROL_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_ECC_SINGLE_BIT_ERROR,
    AM_HAL_DSI_INT_STATUS_RX_ECC_MULTI_BIT_ERROR,
    AM_HAL_DSI_INT_STATUS_START_TRANS_SYNC_ERROR,

}
am_hal_dsi_interrupt_status_t;*/

//*****************************************************************************
//
// DSI clock lane frequency.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_DSI_FREQ_TRIM_X1  = 0x40,
    AM_HAL_DSI_FREQ_TRIM_X2  = 0x01,
    AM_HAL_DSI_FREQ_TRIM_X3  = 0x41,
    AM_HAL_DSI_FREQ_TRIM_X4  = 0x02,
    AM_HAL_DSI_FREQ_TRIM_X5  = 0x42,
    AM_HAL_DSI_FREQ_TRIM_X6  = 0x03,
    AM_HAL_DSI_FREQ_TRIM_X7  = 0x43,
    AM_HAL_DSI_FREQ_TRIM_X8  = 0x04,
    AM_HAL_DSI_FREQ_TRIM_X9  = 0x44,
    AM_HAL_DSI_FREQ_TRIM_X10 = 0x05,
    AM_HAL_DSI_FREQ_TRIM_X11 = 0x45,
    AM_HAL_DSI_FREQ_TRIM_X12 = 0x06,
    AM_HAL_DSI_FREQ_TRIM_X13 = 0x46,
    AM_HAL_DSI_FREQ_TRIM_X14 = 0x07,
    AM_HAL_DSI_FREQ_TRIM_X15 = 0x47,
    AM_HAL_DSI_FREQ_TRIM_X16 = 0x08,
    AM_HAL_DSI_FREQ_TRIM_X17 = 0x48,
    AM_HAL_DSI_FREQ_TRIM_X18 = 0x09,
    AM_HAL_DSI_FREQ_TRIM_X19 = 0x49,
    AM_HAL_DSI_FREQ_TRIM_X20 = 0x0A,
    AM_HAL_DSI_FREQ_TRIM_X21 = 0x4A,
    AM_HAL_DSI_FREQ_TRIM_X22 = 0x0B,
    AM_HAL_DSI_FREQ_TRIM_X23 = 0x4B,
    AM_HAL_DSI_FREQ_TRIM_X24 = 0x0C,
    AM_HAL_DSI_FREQ_TRIM_X25 = 0x4C,
    AM_HAL_DSI_FREQ_TRIM_X26 = 0x0D,
    AM_HAL_DSI_FREQ_TRIM_X27 = 0x4D,
    AM_HAL_DSI_FREQ_TRIM_X28 = 0x0E,
    AM_HAL_DSI_FREQ_TRIM_X29 = 0x4E,
    AM_HAL_DSI_FREQ_TRIM_X30 = 0x0F,
    AM_HAL_DSI_FREQ_TRIM_X31 = 0x4F,
} am_hal_dsi_freq_trim_e;


//*****************************************************************************
//
// DBI to DSI: DBI data width.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_DSI_DBI_WIDTH_8  = 8,
    AM_HAL_DSI_DBI_WIDTH_16 = 16,
} am_hal_dsi_dbi_width_e;

//! @}

//*****************************************************************************
//
// External functions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief DSI configuration
//!
//! @param ui8LanesNum is number of lanes.
//!
//! @param ui8DBIBusWidth is width of DBI bus.
//!
//! This function should be called after DSI power is enabled.
//!
//! @return AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t
am_hal_dsi_para_config(uint8_t ui8LanesNum, uint8_t ui8DBIBusWidth, uint32_t ui32FreqTrim);

//*****************************************************************************
//
//! @brief DSI initialization
//!
//! Configure power and clock of DSI.
//!
//! @return AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t
am_hal_dsi_init(void);

//*****************************************************************************
//
//! @brief DSI deinit
//!
//! Turn off power and clock of DSI.
//!
//! @return AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t
am_hal_dsi_deinit(void);

//*****************************************************************************
//
//! @brief DSI configuration
//!
//! Configure DSI frequency and timing
//!
//! @return AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t
am_hal_dsi_timing(uint32_t ui32FreqTrim);

//*****************************************************************************
//
//! @brief DSI state
//!
//! Enter ULPS mode
//!
//! @return AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t
am_hal_dsi_ulps_enter(void);

//*****************************************************************************
//
//! @brief DSI state
//!
//! Exit ULPS mode
//!
//! @return AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t
am_hal_dsi_ulps_exit(void);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_DSI_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
