//*****************************************************************************
//
//! @file pdm_to_i2s.c
//!
//! @brief An example to show PDM to I2S operation.
//!
//! Purpose: This example enables the PDM and I2S interface to collect audio signals from an
//! external microphone, transmit to I2S1(slave), then loop back to I2S0(master). The required pin connections are:
//!
//! Printing takes place over the ITM at 1M Baud.
//!
//! GPIO 50 - PDM0 CLK
//! GPIO 51 - PDM0 DATA
//!
//! GPIO 52 - PDM1 CLK
//! GPIO 53 - PDM1 DATA
//!
//! GPIO 54 - PDM2 CLK
//! GPIO 55 - PDM2 DATA
//!
//! GPIO 56 - PDM3 CLK
//! GPIO 57 - PDM3 DATA
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
#include "SEGGER_RTT.h"
#include "am_i2s_ring_buffer.h"

//*****************************************************************************
//
// Insert compiler version at compile time.
//
//*****************************************************************************
#define STRINGIZE_VAL(n)                    STRINGIZE_VAL2(n)
#define STRINGIZE_VAL2(n)                   #n

#ifdef __GNUC__
#define COMPILER_VERSION                    ("GCC " __VERSION__)
#elif defined(__ARMCC_VERSION)
#define COMPILER_VERSION                    ("ARMCC " STRINGIZE_VAL(__ARMCC_VERSION))
#elif defined(__KEIL__)
#define COMPILER_VERSION                    "KEIL_CARM " STRINGIZE_VAL(__CA__)
#elif defined(__IAR_SYSTEMS_ICC__)
#define COMPILER_VERSION                    __VERSION__
#else
#define COMPILER_VERSION                    "Compiler unknown"
#endif

//*****************************************************************************
//
// Example parameters.
//
//*****************************************************************************
//*****************************************************************************
//
// PDM pins
//
//*****************************************************************************
#if FFT_PDM_MODULE == 0
#define PDM_DATA_GPIO_FUNC  AM_HAL_PIN_51_PDM0_DATA
#define PDM_DATA_GPIO_PIN   51
#define PDM_CLK_GPIO_FUNC   AM_HAL_PIN_50_PDM0_CLK
#define PDM_CLK_GPIO_PIN    50
#elif FFT_PDM_MODULE == 1
#define PDM_DATA_GPIO_FUNC  AM_HAL_PIN_53_PDM1_DATA
#define PDM_DATA_GPIO_PIN   53
#define PDM_CLK_GPIO_FUNC   AM_HAL_PIN_52_PDM1_CLK
#define PDM_CLK_GPIO_PIN    52
#elif FFT_PDM_MODULE == 2
#define PDM_DATA_GPIO_FUNC  AM_HAL_PIN_55_PDM2_DATA
#define PDM_DATA_GPIO_PIN   55
#define PDM_CLK_GPIO_FUNC   AM_HAL_PIN_54_PDM2_CLK
#define PDM_CLK_GPIO_PIN    54
#elif FFT_PDM_MODULE == 3
#define PDM_DATA_GPIO_FUNC  AM_HAL_PIN_57_PDM3_DATA
#define PDM_DATA_GPIO_PIN   57
#define PDM_CLK_GPIO_FUNC   AM_HAL_PIN_56_PDM3_CLK
#define PDM_CLK_GPIO_PIN    56
#endif

#define PDM_FFT_SIZE                1024//2048//4096
#define PDM_FFT_BYTES               (PDM_FFT_SIZE * 4)
#define PRINT_PDM_DATA              0
#define PRINT_FFT_DATA              0

#define  FFT_PDM_MODULE             0

#define PDM_DBG_LOG(...)    am_util_stdio_printf(__VA_ARGS__)

//! PDM interrupts.
static const IRQn_Type pdm_interrupts[] =
{
    PDM0_IRQn,
    PDM1_IRQn,
    PDM2_IRQn,
    PDM3_IRQn
};

//
// Take over the interrupt handler for whichever PDM we're using.
//
#define example_pdm_isr     am_pdm_isr1(FFT_PDM_MODULE)
#define am_pdm_isr1(n)      am_pdm_isr(n)
#define am_pdm_isr(n)       am_pdm ## n ## _isr

#define FIFO_THRESHOLD_CNT      16
#define DMA_BYTES               PDM_FFT_BYTES

//*****************************************************************************
//
// PDM configuration information.
//
//*****************************************************************************
void *PDMHandle;

am_hal_pdm_config_t g_sPdmConfig =
{

#ifdef AM_PART_APOLLO4B
    //
    // Example setting:
    //  1.5MHz PDM CLK OUT:
    //      AM_HAL_PDM_CLK_HFRC2ADJ_24_576MHZ, AM_HAL_PDM_MCLKDIV_1, AM_HAL_PDM_PDMA_CLKO_DIV7
    //  16.00KHz 24bit Sampling:
    //      DecimationRate = 48
    //
    .ePDMClkSpeed = AM_HAL_PDM_CLK_HFRC2ADJ_24_576MHZ,
    .eClkDivider = AM_HAL_PDM_MCLKDIV_1,
    .ePDMAClkOutDivder = AM_HAL_PDM_PDMA_CLKO_DIV7,
    .ui32DecimationRate = 48,

#else
    //
    // Example setting:
    //  1.5MHz PDM CLK OUT:
    //      AM_HAL_PDM_CLK_24MHZ, AM_HAL_PDM_MCLKDIV_1, AM_HAL_PDM_PDMA_CLKO_DIV7
    //  15.625KHz 24bit Sampling:
    //      DecimationRate = 48
    //
    .ePDMClkSpeed = AM_HAL_PDM_CLK_24MHZ,
    .eClkDivider = AM_HAL_PDM_MCLKDIV_1,
    .ePDMAClkOutDivder = AM_HAL_PDM_PDMA_CLKO_DIV7,
    .ui32DecimationRate = 48,
#endif

    .eLeftGain = AM_HAL_PDM_GAIN_P105DB,
    .eRightGain = AM_HAL_PDM_GAIN_P105DB,
    .eStepSize = AM_HAL_PDM_GAIN_STEP_0_13DB,

    .bHighPassEnable = AM_HAL_PDM_HIGH_PASS_ENABLE,
    .ui32HighPassCutoff = 0x3,
    .bDataPacking = 1,
    .ePCMChannels = AM_HAL_PDM_CHANNEL_STEREO,

    .bPDMSampleDelay = AM_HAL_PDM_CLKOUT_PHSDLY_NONE,
    .ui32GainChangeDelay = AM_HAL_PDM_CLKOUT_DELAY_NONE,

    .bSoftMute = 0,
    .bLRSwap = 0,
};

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
uint32_t g_ui32FifoOVFCount = 0;
volatile bool g_bPDMDataReady = false;
uint32_t g_ui32SampleFreq;

AM_SHARED_RW uint32_t g_ui32PDMDataBuffer[PDM_FFT_SIZE*2];

float g_fPDMTimeDomain[PDM_FFT_SIZE * 2];
float g_fPDMFrequencyDomain[PDM_FFT_SIZE * 2];
float g_fPDMMagnitudes[PDM_FFT_SIZE * 2];

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
#define config_RTT_Record 0

#if config_RTT_Record
// RTT buffer
#define RTT_BUFFER_LENGTH       (256*1024)
uint8_t g_rttRecorderBuffer[RTT_BUFFER_LENGTH];
#endif

#define PIN_DEBUG  1
#define AM_PDM0_CMPL_PIN     61
#define AM_I2S1_TX_CMPL_PIN  63

#define DATA_VERIFY 1
extern char ac20201201_sin_wave[];
//
//PDM to I2S data Ringbuff.
//
#define AUDIO_RING_BUFFER_SIZE  (16*1024)
uint8_t g_ui8AudioRingBuffer[AUDIO_RING_BUFFER_SIZE];
am_util_ring_buffer_t audioBuffer  = 
{
  .pui8Data = g_ui8AudioRingBuffer,
  .ui32Capacity  = AUDIO_RING_BUFFER_SIZE

};

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//*****************************************************************************
//
// I2S
//
//*****************************************************************************
#define     I2S_MODULE          0   // I2S master
#define     I2S_MODULE_SLAVE    1   // I2S slave
#define     USE_DMA             1

#if USE_DMA
#define BUFFER_SIZE_BYTES               PDM_FFT_BYTES  //(8*1024)

//! RX size = TX size * output sample rate /internal sample rate
//! Notice .eClock from g_sI2SConfig and g_sI2SConfig_slave.
#if DATA_VERIFY
#define BUFFER_SIZE_ASRC_RX_BYTES       PDM_FFT_BYTES//((PDM_FFT_BYTES*3)/4)//PDM_FFT_BYTES*3  //(8*1024)
#else
#define BUFFER_SIZE_ASRC_RX_BYTES       ((PDM_FFT_BYTES*3)/4)//((PDM_FFT_BYTES*3)/4)//PDM_FFT_BYTES*3  //(8*1024)
#endif
#else
#define BUFFER_SIZE_BYTES       128
#endif

//! I2S interrupts.
static const IRQn_Type i2s_interrupts[] =
{
    I2S0_IRQn,
    I2S1_IRQn
};

#if I2S_MODULE == 0
#define I2S_DATA_IN_GPIO_FUNC  AM_HAL_PIN_4_I2S0_SDIN
#define I2S_DATA_IN_GPIO_PIN   4
#define I2S_DATA_OUT_GPIO_FUNC  AM_HAL_PIN_6_RESERVED9
#define I2S_DATA_OUT_GPIO_PIN   6
#define I2S_CLK_GPIO_FUNC   AM_HAL_PIN_5_I2S0_CLK
#define I2S_CLK_GPIO_PIN    5
#define I2S_WS_GPIO_FUNC    AM_HAL_PIN_7_I2S0_WS
#define I2S_WS_GPIO_PIN     7
#elif I2S_MODULE == 1

#endif

#if I2S_MODULE_SLAVE == 1
#define I2S1_SLAVE_DATA_IN_GPIO_FUNC  AM_HAL_PIN_19_I2S1_SDIN
#define I2S1_SLAVE_DATA_IN_GPIO_PIN   19
#define I2S1_SLAVE_DATA_OUT_GPIO_FUNC  AM_HAL_PIN_17_I2S1_SDOUT
#define I2S1_SLAVE_DATA_OUT_GPIO_PIN   17
    
#define I2S1_SLAVE_CLK_GPIO_FUNC   AM_HAL_PIN_16_I2S1_CLK
#define I2S1_SLAVE_CLK_GPIO_PIN    16
#define I2S1_SLAVE_WS_GPIO_FUNC    AM_HAL_PIN_18_I2S2_DATA
#define I2S1_SLAVE_WS_GPIO_PIN     18
#endif

void *I2SHandle;
void *I2SSlaveHandle;

I2S0_Type* g_I2S0;
I2S0_Type* g_I2S1;

AM_SHARED_RW uint32_t g_ui32I2SRxDataBuffer[BUFFER_SIZE_ASRC_RX_BYTES / 4];
AM_SHARED_RW uint32_t g_ui32I2STxDataBuffer[BUFFER_SIZE_BYTES / 4] ;
AM_SHARED_RW uint32_t g_ui32I2SRxDataBuffer_slave[BUFFER_SIZE_BYTES / 4] ;
AM_SHARED_RW uint32_t g_ui32I2STxDataBuffer_slave[BUFFER_SIZE_BYTES / 4] ;

// Programmer Reference setting.
static am_hal_i2s_io_signal_t g_sI2SIOConfig =
{
  .eFyncCpol = AM_HAL_I2S_IO_FSYNC_CPOL_HIGH,
  
  .eTxCpol = AM_HAL_I2S_IO_TX_CPOL_FALLING,
  .eRxCpol = AM_HAL_I2S_IO_RX_CPOL_RISING,
};

static am_hal_i2s_io_signal_t g_sI2SIOConfig_slve =
{
  .eFyncCpol = AM_HAL_I2S_IO_FSYNC_CPOL_HIGH,
  
  .eTxCpol = AM_HAL_I2S_IO_TX_CPOL_FALLING,
  .eRxCpol = AM_HAL_I2S_IO_RX_CPOL_RISING,
};


static am_hal_i2s_data_format_t g_sI2SDataConfig =
{
  .ePhase = AM_HAL_I2S_DATA_PHASE_SINGLE,
  .eDataDelay = 0x0,
  .eDataJust = AM_HAL_I2S_DATA_JUSTIFIED_LEFT,
    
  .eChannelLenPhase1 = AM_HAL_I2S_FRAME_32BITS_WDLEN, //32bits
  .eChannelLenPhase2 = AM_HAL_I2S_FRAME_32BITS_WDLEN, //32bits
  .eSampleLenPhase1 = AM_HAL_I2S_FRAME_24BITS_WDLEN,
  .eSampleLenPhase2 = AM_HAL_I2S_FRAME_24BITS_WDLEN
};

//*****************************************************************************
//
// I2S configuration information.
//
//*****************************************************************************
static am_hal_i2s_config_t g_sI2SConfig =
{
  //.ui32AudioFreq        = AM_HAL_I2S_SAMPLE_RATE_16KHZ,
#if DATA_VERIFY  
  .eClock               = eAM_HAL_I2S_CLKSEL_HFRC_3MHz, //eAM_HAL_I2S_CLKSEL_HFRC_3MHz
  .eDiv3                = 1,
#else
  .eClock               = eAM_HAL_I2S_CLKSEL_HFRC_750KHz,
  .eDiv3                = 0,
#endif  
  .eMode                = AM_HAL_I2S_IO_MODE_MASTER,
  .eXfer                = AM_HAL_I2S_XFER_RX,
  .ui32ChnNumber        = 2,
  
  .eData                = &g_sI2SDataConfig,
  .eIO                  = &g_sI2SIOConfig
};

static am_hal_i2s_config_t g_sI2SConfig_slave =
{
  //.ui32AudioFreq        = AM_HAL_I2S_SAMPLE_RATE_16KHZ,
  .eClock               = eAM_HAL_I2S_CLKSEL_XTHS_1MHz, //eAM_HAL_I2S_CLKSEL_HFRC_6MHz,
  .eDiv3                = 0,
#if DATA_VERIFY    
  .eASRC                = 0,
#else
  .eASRC                = 1,
#endif  
  .eMode                = AM_HAL_I2S_IO_MODE_SLAVE,
  .eXfer                = AM_HAL_I2S_XFER_TX,
  .ui32ChnNumber        = 2,

  .eData                = &g_sI2SDataConfig,
  .eIO                  = &g_sI2SIOConfig_slve
};

// Transfer setting.
static am_hal_i2s_transfer_t sTransfer = 
{
  .ui32RxTotalCount = BUFFER_SIZE_ASRC_RX_BYTES/4, //!!! Must less than 16K bytes.
  .ui32RxTargetAddr = (uint32_t)g_ui32I2SRxDataBuffer,
  
  .ui32TxTotalCount = BUFFER_SIZE_BYTES/4,
  .ui32TxTargetAddr = (uint32_t)g_ui32I2STxDataBuffer
    
};

static am_hal_i2s_transfer_t sTransfer_slave = 
{
  .ui32RxTotalCount = BUFFER_SIZE_BYTES/4,
  .ui32RxTargetAddr = (uint32_t)g_ui32I2SRxDataBuffer_slave,
  
  .ui32TxTotalCount = BUFFER_SIZE_BYTES/4,
  .ui32TxTargetAddr = (uint32_t)g_ui32I2STxDataBuffer_slave,
  
};

volatile uint8_t g_bI2S0_Cmpl = 0x1;
volatile uint8_t g_bI2S1_Cmpl = 0x1;

//*****************************************************************************
//
// PDM initialization.
//
//*****************************************************************************
void
pdm_init(void)
{
    //
    // Initialize, power-up, and configure the PDM.
    //
    am_hal_pdm_initialize(FFT_PDM_MODULE, &PDMHandle);
    am_hal_pdm_power_control(PDMHandle, AM_HAL_PDM_POWER_ON, false);

#ifdef AM_PART_APOLLO4B
    // use external XTHS, not reference clock
    am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_EXTCLK32M_KICK_START, false);

    // Enable HFRC2
    CLKGEN->MISC |= 0x1 << 5U;  // set FRCHFRC2 bit
    am_util_delay_us(200);      // wait for FLL to lock

    // set HF2ADJ for 24.576MHz output
    CLKGEN->HF2ADJ1_b.HF2ADJTRIMEN          = 7;
    CLKGEN->HF2ADJ2_b.HF2ADJXTALDIVRATIO    = 0;
    CLKGEN->HF2ADJ2_b.HF2ADJRATIO           = 0x624DD;  // 12.288*(2^15)
    CLKGEN->HF2ADJ0_b.HF2ADJEN              = 1;

    am_util_delay_us(500);      // wait for adj to apply
#endif

    am_hal_pdm_configure(PDMHandle, &g_sPdmConfig);

    //
    // Configure the necessary pins.
    //
    am_hal_gpio_pincfg_t sPinCfg = {0};

    sPinCfg.GP.cfg_b.uFuncSel = PDM_DATA_GPIO_FUNC;
    am_hal_gpio_pinconfig(PDM_DATA_GPIO_PIN, sPinCfg);
    sPinCfg.GP.cfg_b.uFuncSel = PDM_CLK_GPIO_FUNC;
    am_hal_gpio_pinconfig(PDM_CLK_GPIO_PIN, sPinCfg);

    am_hal_pdm_fifo_threshold_setup(PDMHandle, FIFO_THRESHOLD_CNT);

    //
    // Configure and enable PDM interrupts (set up to trigger on DMA
    // completion).
    //
    am_hal_pdm_interrupt_enable(PDMHandle, (AM_HAL_PDM_INT_DERR
                                            | AM_HAL_PDM_INT_DCMP
                                            | AM_HAL_PDM_INT_UNDFL
                                            | AM_HAL_PDM_INT_OVF));

    NVIC_EnableIRQ(pdm_interrupts[FFT_PDM_MODULE]);

}

//*****************************************************************************
//
// Print PDM configuration data.
//
//*****************************************************************************
void
pdm_config_print(void)
{
    uint32_t ui32PDMClk;
    uint32_t ui32MClkDiv;
    uint32_t ui32DivClkQ;
    float fFrequencyUnits;

    //
    // Read the config structure to figure out what our internal clock is set
    // to.
    //
    switch (g_sPdmConfig.eClkDivider)
    {
        case AM_HAL_PDM_MCLKDIV_3: ui32MClkDiv = 3; break;
        case AM_HAL_PDM_MCLKDIV_2: ui32MClkDiv = 2; break;
        case AM_HAL_PDM_MCLKDIV_1: ui32MClkDiv = 1; break;

        default:
            ui32MClkDiv = 1;
    }

    switch (g_sPdmConfig.ePDMAClkOutDivder)
    {
        case AM_HAL_PDM_PDMA_CLKO_DIV15: ui32DivClkQ = 15; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV14: ui32DivClkQ = 14; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV13: ui32DivClkQ = 13; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV12: ui32DivClkQ = 12; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV11: ui32DivClkQ = 11; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV10: ui32DivClkQ = 10; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV9: ui32DivClkQ = 9; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV8: ui32DivClkQ = 8; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV7: ui32DivClkQ = 7; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV6: ui32DivClkQ = 6; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV5: ui32DivClkQ = 5; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV4: ui32DivClkQ = 4; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV3: ui32DivClkQ = 3; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV2: ui32DivClkQ = 2; break;
        case AM_HAL_PDM_PDMA_CLKO_DIV1: ui32DivClkQ = 1; break;
        default:
            ui32DivClkQ = 1;
    }

#ifdef AM_PART_APOLLO4B
    switch (g_sPdmConfig.ePDMClkSpeed)
    {
        case AM_HAL_PDM_CLK_HFRC2ADJ_24_576MHZ:     ui32PDMClk = 24576000; break;
        case AM_HAL_PDM_CLK_HFXTAL:                 ui32PDMClk = 32000000; break;
        case AM_HAL_PDM_CLK_HFRC_24MHZ:             ui32PDMClk = 24000000; break;

        default:
            ui32PDMClk = 24000000;
    }
#else
    switch (g_sPdmConfig.ePDMClkSpeed)
    {
        case AM_HAL_PDM_CLK_48MHZ:      ui32PDMClk = 48000000; break;
        case AM_HAL_PDM_CLK_24MHZ:      ui32PDMClk = 24000000; break;
        case AM_HAL_PDM_CLK_12MHZ:      ui32PDMClk = 12000000; break;
        case AM_HAL_PDM_CLK_6MHZ:       ui32PDMClk =  6000000; break;
        case AM_HAL_PDM_CLK_3MHZ:       ui32PDMClk =  3000000; break;
        case AM_HAL_PDM_CLK_1_5MHZ:     ui32PDMClk =  1500000; break;
        case AM_HAL_PDM_CLK_750KHZ:     ui32PDMClk =   750000; break;
        case AM_HAL_PDM_CLK_HS_CRYSTAL: ui32PDMClk =   0; break;

        default:
            ui32PDMClk = 24000000;
    }
#endif
    //
    // Record the effective sample frequency. We'll need it later to print the
    // loudest frequency from the sample.
    //
    g_ui32SampleFreq = (ui32PDMClk /
                        ((ui32MClkDiv + 1) * (ui32DivClkQ + 1) * 2 * g_sPdmConfig.ui32DecimationRate));

    ui32PDMClk =  (ui32PDMClk /
                        ((ui32MClkDiv + 1) * (ui32DivClkQ + 1)));   // actual PDM CLK output

    fFrequencyUnits = (float) g_ui32SampleFreq / (float) PDM_FFT_SIZE;

    am_util_stdio_printf("PDM Settings:\n");
    am_util_stdio_printf("PDM Clock (Hz):         %12d\n", ui32PDMClk);
    am_util_stdio_printf("Decimation Rate:        %12d\n", g_sPdmConfig.ui32DecimationRate);
    am_util_stdio_printf("Effective Sample Freq.: %12d\n", g_ui32SampleFreq);
    am_util_stdio_printf("FFT Length:             %12d\n\n", PDM_FFT_SIZE);
    am_util_stdio_printf("FFT Resolution: %15.3f Hz\n", fFrequencyUnits);
}

#if config_RTT_Record
void pcm_rtt_record(void* pBuffer, uint32_t NumBytes)
{
      uint32_t bytes_stored = SEGGER_RTT_Write(1, (uint8_t*)pBuffer, NumBytes);
      
      if(bytes_stored != NumBytes)
      {
        //while(1);
      }
}
#endif

//*****************************************************************************
//
// Start a transaction to get some number of bytes from the PDM interface.
//
//*****************************************************************************
void
pdm_data_get(void)
{
    //
    // Configure DMA and target address.
    //
    am_hal_pdm_transfer_t sTransfer;
    sTransfer.ui32TargetAddr = (uint32_t ) g_ui32PDMDataBuffer;
    sTransfer.ui32TotalCount = PDM_FFT_BYTES;

    //
    // Start the data transfer.
    //

    am_hal_pdm_dma_start(PDMHandle, &sTransfer);

    am_hal_pdm_fifo_flush(PDMHandle);
}

//*****************************************************************************
//
// PDM interrupt handler.
//
//*****************************************************************************
void
example_pdm_isr(void)
{
    uint32_t ui32Status;

    //
    // Read the interrupt status.
    //
    am_hal_pdm_interrupt_status_get(PDMHandle, &ui32Status, true);
    am_hal_pdm_interrupt_clear(PDMHandle, ui32Status);
    //
    // Once our DMA transaction completes, we will disable the PDM and send a
    // flag back down to the main routine. Disabling the PDM is only necessary
    // because this example only implemented a single buffer for storing FFT
    // data. More complex programs could use a system of multiple buffers to
    // allow the CPU to run the FFT in one buffer while the DMA pulls PCM data
    // into another buffer.
    //
    if (ui32Status & AM_HAL_PDM_INT_DCMP)
    {
        //
        // Re-arrange data
        //
//        uint8_t* temp1 = (uint8_t*)g_ui32PDMDataBuffer;
//        for ( uint32_t i = 0; i < (PDM_FFT_BYTES / 4); i++ )
//        {
//            temp1[3*i]     = g_ui32PDMDataBuffer[i] & 0xFF;
//            temp1[3*i + 1] = (g_ui32PDMDataBuffer[i] & 0xFF00) >> 8U;
//            temp1[3*i + 2] = (g_ui32PDMDataBuffer[i] & 0xFF0000) >> 16U;
//        }
        //pcm_rtt_record(g_ui32PDMDataBuffer, PDM_FFT_BYTES * 3 / 4);
        
        // am_util_stdio_printf("pdm isr.\n");
        g_bPDMDataReady = true;
     #if PIN_DEBUG
        am_hal_gpio_output_toggle(AM_PDM0_CMPL_PIN);
     #endif

    #if DATA_VERIFY
        am_util_ring_buffer_write(&audioBuffer, (void*)g_ui32I2STxDataBuffer_slave, PDM_FFT_BYTES);
    #else    
        am_util_ring_buffer_write(&audioBuffer, (void*)g_ui32PDMDataBuffer, PDM_FFT_BYTES);
    #endif
        pdm_data_get();
    }

     if (ui32Status & AM_HAL_PDM_INT_OVF)
     {
        uint32_t count = am_hal_pdm_fifo_count_get(PDMHandle);
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        volatile uint32_t ui32FifoDummy;
        for ( uint8_t i = 0; i < count; i++ )
        {
            ui32FifoDummy = PDMn(FFT_PDM_MODULE)->FIFOREAD;
        }
#pragma GCC diagnostic pop
#endif
        am_hal_pdm_fifo_flush(PDMHandle);

        g_ui32FifoOVFCount++;
     }
}

//*****************************************************************************
//
// Analyze and print frequency data.
//
//*****************************************************************************
#if 0
uint32_t ui32LoudestFrequency;
void
pcm_fft_print(void)
{
    float fMaxValue;
    uint32_t ui32MaxIndex;
    int32_t *pi32PDMData = (int32_t *) g_ui32PDMDataBuffer;
    //
    // Convert the PDM samples to floats, and arrange them in the format
    // required by the FFT function.
    //
    for (uint32_t i = 0; i < PDM_FFT_SIZE; i++)
    {
        if (PRINT_PDM_DATA)
        {
            am_util_stdio_printf("%d\n", pi32PDMData[i]);
        }

        g_fPDMTimeDomain[2 * i] = (pi32PDMData[i] << 8) / 65536.0;
        g_fPDMTimeDomain[2 * i + 1] = 0.0;
    }

    if (PRINT_PDM_DATA)
    {
        am_util_stdio_printf("END\n");
    }

    //
    // Perform the FFT.
    //
    //arm_cfft_radix4_instance_f32 S;
    //arm_cfft_radix4_init_f32(&S, PDM_FFT_SIZE, 0, 1);
    //arm_cfft_radix4_f32(&S, g_fPDMTimeDomain);
    //arm_cmplx_mag_f32(g_fPDMTimeDomain, g_fPDMMagnitudes, PDM_FFT_SIZE);

    if (PRINT_FFT_DATA)
    {
        for (uint32_t i = 0; i < PDM_FFT_SIZE / 2; i++)
        {
            am_util_stdio_printf("%f\n", g_fPDMMagnitudes[i]);
        }

        am_util_stdio_printf("END\n");
    }

    //
    // Find the frequency bin with the largest magnitude.
    //
    arm_max_f32(g_fPDMMagnitudes, PDM_FFT_SIZE / 2, &fMaxValue, &ui32MaxIndex);

    ui32LoudestFrequency = (g_ui32SampleFreq * ui32MaxIndex) / PDM_FFT_SIZE;

    if (PRINT_FFT_DATA)
    {
        am_util_stdio_printf("Loudest frequency bin: %d\n", ui32MaxIndex);
    }
    am_util_stdio_printf("Loudest frequency: %d\n", ui32LoudestFrequency);

}
#endif
    
//*****************************************************************************
// i2s_init
//*****************************************************************************
void
i2s_init(void)
{
    g_I2S0 = (I2S0_Type*)0x40208000;
    g_I2S1 = (I2S0_Type*)0x40209000;
    
    //
    // Configure the necessary pins.
    //
    am_hal_gpio_pincfg_t sPinCfg = 
    {
      .GP.cfg_b.eGPOutCfg = 1,
      .GP.cfg_b.ePullup   = 0
    };

    sPinCfg.GP.cfg_b.uFuncSel = I2S_DATA_OUT_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S_DATA_OUT_GPIO_PIN, sPinCfg);
    sPinCfg.GP.cfg_b.uFuncSel = I2S_DATA_IN_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S_DATA_IN_GPIO_PIN, sPinCfg);
    
    sPinCfg.GP.cfg_b.uFuncSel = I2S_CLK_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S_CLK_GPIO_PIN, sPinCfg);
    sPinCfg.GP.cfg_b.uFuncSel = I2S_WS_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S_WS_GPIO_PIN, sPinCfg);  
    
    am_hal_i2s_initialize(I2S_MODULE, &I2SHandle);
    am_hal_i2s_power_control(I2SHandle, AM_HAL_I2S_POWER_ON, false);
    am_hal_i2s_configure(I2SHandle, &g_sI2SConfig);
    am_hal_i2s_enable(I2SHandle);
    
    // I2S1 pins
    sPinCfg.GP.cfg_b.uFuncSel = I2S1_SLAVE_DATA_OUT_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S1_SLAVE_DATA_OUT_GPIO_PIN, sPinCfg);
    sPinCfg.GP.cfg_b.uFuncSel = I2S1_SLAVE_DATA_IN_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S1_SLAVE_DATA_IN_GPIO_PIN, sPinCfg);
    
    sPinCfg.GP.cfg_b.uFuncSel = I2S1_SLAVE_CLK_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S1_SLAVE_CLK_GPIO_PIN, sPinCfg);
    sPinCfg.GP.cfg_b.uFuncSel = I2S1_SLAVE_WS_GPIO_FUNC;
    am_hal_gpio_pinconfig(I2S1_SLAVE_WS_GPIO_PIN, sPinCfg);
    
    am_hal_i2s_initialize(I2S_MODULE_SLAVE, &I2SSlaveHandle);
    am_hal_i2s_power_control(I2SSlaveHandle, AM_HAL_I2S_POWER_ON, false);
    am_hal_i2s_configure(I2SSlaveHandle, &g_sI2SConfig_slave);
    am_hal_i2s_enable(I2SSlaveHandle);
    
    //
    // When changing the I2S clock source, no matter whether HFRC2 is being used or not,
    // HFRC2 has to be enabled first to make clock source changing effective.
    //
    CLKGEN->MISC |= 0x20; //bit 5.
    am_util_delay_ms(200);
   
    if((eAM_HAL_I2S_CLKSEL_XTHS_EXTREF_CLK <= g_sI2SConfig.eClock  && g_sI2SConfig.eClock <= eAM_HAL_I2S_CLKSEL_XTHS_500KHz) || (eAM_HAL_I2S_CLKSEL_XTHS_EXTREF_CLK <= g_sI2SConfig_slave.eClock  && g_sI2SConfig_slave.eClock <= eAM_HAL_I2S_CLKSEL_XTHS_500KHz)) //enable EXTCLK32M
    {
        am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_EXTCLK32M_NORMAL, 0);
        am_util_delay_ms(200);
    }
}

void
am_dspi2s0_isr()
{
    uint32_t ui32Status;

    am_hal_i2s_interrupt_status_get(I2SHandle, &ui32Status, true);
    am_hal_i2s_interrupt_clear(I2SHandle, ui32Status);

    //am_hal_i2s_transfer_complete(I2SHandle);
    
    if (ui32Status & AM_HAL_I2S_INT_RXDMACPL)
    {
        uint32_t ui32DMAStatus;
        am_hal_i2s_dma_status_get(I2SHandle, &ui32DMAStatus, AM_HAL_I2S_XFER_RX);

        //
        // The RX/TX DMACPL interrupt asserts when the programmed DMA completes, 
        // or end with an errorcondition.
        //
        if(ui32DMAStatus & AM_HAL_I2S_STAT_DMA_RX_ERR)
        {
          // cleare DMAERR bit
          am_hal_i2s_dma_error(I2SHandle, AM_HAL_I2S_XFER_RX);
        }

        g_bI2S0_Cmpl = 1;
    }

    if (ui32Status & AM_HAL_I2S_INT_TXDMACPL)	
    {
        uint32_t ui32DMAStatus;
        am_hal_i2s_dma_status_get(I2SHandle, &ui32DMAStatus, AM_HAL_I2S_XFER_TX);

        if(ui32DMAStatus & AM_HAL_I2S_STAT_DMA_RX_ERR)
        {
          // cleare DMAERR bit
          am_hal_i2s_dma_error(I2SHandle, AM_HAL_I2S_XFER_TX);
        }

        //am_util_stdio_printf("i2s0 txcmpt.\n");
        
        g_bI2S0_Cmpl = 1;
    }


    //
    if (ui32Status & AM_HAL_I2S_INT_IPB)
    {
      am_hal_i2s_ipb_interrupt_service(I2SHandle);
    }
}

//*****************************************************************************
//
// I2S1 interrupt handler.
//
//*****************************************************************************
void
am_dspi2s1_isr()
{
    uint32_t ui32Status;

    am_hal_i2s_interrupt_status_get(I2SSlaveHandle, &ui32Status, true);
    am_hal_i2s_interrupt_clear(I2SSlaveHandle, ui32Status);


    //am_hal_i2s_transfer_complete(I2SSlaveHandle);

    if (ui32Status & AM_HAL_I2S_INT_RXDMACPL)
    {
        uint32_t ui32DMAStatus;
        am_hal_i2s_dma_status_get(I2SSlaveHandle, &ui32DMAStatus, AM_HAL_I2S_XFER_RX);

        if(ui32DMAStatus & AM_HAL_I2S_STAT_DMA_RX_ERR)
        {
          // cleare DMAERR bit
          am_hal_i2s_dma_error(I2SSlaveHandle, AM_HAL_I2S_XFER_RX);
        }

        //am_util_stdio_printf("i2s1 rxcmpt.\n");

    #if 0
        uint8_t* temp1 = (uint8_t*)g_ui32I2SRxDataBuffer_slave;
        for ( uint32_t i = 0; i < (PDM_FFT_BYTES / 4); i++ )
        {
            temp1[3*i]     = g_ui32I2SRxDataBuffer_slave[i] & 0xFF;
            temp1[3*i + 1] = (g_ui32I2SRxDataBuffer_slave[i] & 0xFF00) >> 8U;
            temp1[3*i + 2] = (g_ui32I2SRxDataBuffer_slave[i] & 0xFF0000) >> 16U;
        }

        pcm_rtt_record(g_ui32I2SRxDataBuffer_slave, PDM_FFT_BYTES * 3 / 4);
    #endif

        g_bI2S1_Cmpl = 0x1;
    }

    if (ui32Status & AM_HAL_I2S_INT_TXDMACPL)
    {
        uint32_t ui32DMAStatus;
        am_hal_i2s_dma_status_get(I2SSlaveHandle, &ui32DMAStatus, AM_HAL_I2S_XFER_TX);

    #if PIN_DEBUG
        am_hal_gpio_output_set(AM_I2S1_TX_CMPL_PIN);
    #endif
        if(ui32DMAStatus & AM_HAL_I2S_STAT_DMA_RX_ERR)
        {
          // clear DMAERR bit.
          am_hal_i2s_dma_error(I2SSlaveHandle, AM_HAL_I2S_XFER_TX);
        }

        g_bI2S1_Cmpl = 0x1;
    }

    //ipb
    if (ui32Status & AM_HAL_I2S_INT_IPB)
    {
      am_hal_i2s_ipb_interrupt_service(I2SSlaveHandle);
    }
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
    // Initialize the printf interface for ITM output
    //
    am_bsp_itm_printf_enable();

    //
    // Print the banner.
    //
    am_util_stdio_terminal_clear();
    am_util_stdio_printf("==============================================\n");
    am_util_stdio_printf("PDM to I2S example.\n\n");

    //
    // Initialize RTT
    //
#if config_RTT_Record
    SEGGER_RTT_Init();
    SEGGER_RTT_ConfigUpBuffer(1, "DataLogger", g_rttRecorderBuffer, RTT_BUFFER_LENGTH, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif

    // init data.
    for (int i = 0; i < BUFFER_SIZE_BYTES / 4; i++)
    {
        g_ui32I2STxDataBuffer[i]       = (i & 0xFF) | 0xAB0000;
        g_ui32I2STxDataBuffer_slave[i] = (i & 0xFF) | 0xAB0000;
    }

#if 0
    //fill up sin wave to i2s1 slave tx buffer. (high 8 bits filled with 0x0)
    memset(g_ui32I2STxDataBuffer_slave, 0x0, sizeof(g_ui32I2STxDataBuffer_slave));
    for(int i = 0; i<BUFFER_SIZE_BYTES/4; i++)
    {
        uint8_t* pBuf = (uint8_t*)(&g_ui32I2STxDataBuffer_slave[i]);
        pBuf[0] = ac20201201_sin_wave[3*i];
        pBuf[1] = ac20201201_sin_wave[3*i+1];
        pBuf[2] = ac20201201_sin_wave[3*i+2]; 
    }
#endif

    am_util_ring_buffer_init(&audioBuffer, g_ui8AudioRingBuffer, AUDIO_RING_BUFFER_SIZE);

#if PIN_DEBUG
    am_hal_gpio_pinconfig(AM_I2S1_TX_CMPL_PIN, am_hal_gpio_pincfg_output);
    am_hal_gpio_state_write(AM_I2S1_TX_CMPL_PIN, AM_HAL_GPIO_OUTPUT_CLEAR);
    
    am_hal_gpio_pinconfig(AM_PDM0_CMPL_PIN, am_hal_gpio_pincfg_output);
    am_hal_gpio_state_write(AM_PDM0_CMPL_PIN, AM_HAL_GPIO_OUTPUT_CLEAR);
#endif    
    //
    // Initialize PDM-to-PCM module
    //
    pdm_init();

    //
    // Print compiler version and rtt control block address
    //
    am_util_stdio_printf("App Compiler:    %s\n", COMPILER_VERSION);

    //
    // Print the PDM configuration
    //
    pdm_config_print();

    // Enable the pdm module.
    //
    am_hal_pdm_enable(PDMHandle);

    //
    // Initialize I2S.
    //
    i2s_init();
    NVIC_EnableIRQ(i2s_interrupts[I2S_MODULE_SLAVE]);
    NVIC_EnableIRQ(i2s_interrupts[I2S_MODULE]);
    am_hal_interrupt_master_enable();

    //
    //
    // Start data conversion
    //
    pdm_data_get();

    //
    am_hal_i2s_dma_configure(I2SSlaveHandle, &g_sI2SConfig_slave, &sTransfer_slave);
    am_hal_i2s_dma_configure(I2SHandle, &g_sI2SConfig, &sTransfer);

    am_hal_i2s_dma_transfer(I2SHandle, &g_sI2SConfig);      
    am_hal_i2s_dma_transfer(I2SSlaveHandle, &g_sI2SConfig_slave);

    //
    // Loop forever while sleeping.
    //
    while (1)
    {
        if(1) //(g_bPDMDataReady)
        {
            g_bPDMDataReady = false;
            //memset(g_ui32I2SRxDataBuffer_slave, 0, sizeof(g_ui32I2SRxDataBuffer_slave));
            
            //I2S1(Slave)
            if(g_bI2S1_Cmpl == 0x1)  
            {
                //memcpy(g_ui32I2STxDataBuffer_slave, g_ui32PDMDataBuffer, PDM_FFT_BYTES);
                uint32_t length = 0;
                AM_CRITICAL_BEGIN
                if(audioBuffer.ui32Length)
                {
                  length = audioBuffer.ui32Length;
                  am_util_ring_buffer_read(&audioBuffer, (void*)g_ui32I2STxDataBuffer_slave, audioBuffer.ui32Length);
                }

                while(!am_hal_i2s_tx_fifo_empty(I2SSlaveHandle));

                g_bI2S1_Cmpl = 0x0;                
                //am_util_stdio_printf("i2s1 cmpl. DMA size =%d\n", length);

                am_hal_i2s_transfer_complete(I2SSlaveHandle);

                am_hal_i2s_dma_configure(I2SSlaveHandle, &g_sI2SConfig_slave, &sTransfer_slave);
                am_hal_i2s_dma_transfer(I2SSlaveHandle, &g_sI2SConfig_slave);
                AM_CRITICAL_END

            #if PIN_DEBUG
                am_hal_gpio_output_clear(AM_I2S1_TX_CMPL_PIN);
            #endif
            }

            //I2S0(Master)
            if(g_bI2S0_Cmpl == 1)
            {
                g_bI2S0_Cmpl = 0x0;

                //am_util_stdio_printf("i2s0 cmpl.\n");

                //32bits convert to 24bits.(high 8 bits are zero)
              #if config_RTT_Record
                AM_CRITICAL_BEGIN
                uint8_t* temp1 = (uint8_t*)g_ui32I2SRxDataBuffer;
                for ( uint32_t i = 0; i < (BUFFER_SIZE_ASRC_RX_BYTES / 4); i++ )
                {
                    temp1[3*i]     = g_ui32I2SRxDataBuffer[i] & 0xFF;
                    temp1[3*i + 1] = (g_ui32I2SRxDataBuffer[i] & 0xFF00) >> 8U;
                    temp1[3*i + 2] = (g_ui32I2SRxDataBuffer[i] & 0xFF0000) >> 16U;
                }
                pcm_rtt_record(g_ui32I2SRxDataBuffer, BUFFER_SIZE_ASRC_RX_BYTES * 3 / 4);
                AM_CRITICAL_END
              #endif

                am_hal_i2s_transfer_complete(I2SHandle);

                am_hal_i2s_dma_configure(I2SHandle, &g_sI2SConfig, &sTransfer);
                am_hal_i2s_dma_transfer(I2SHandle, &g_sI2SConfig);

                //I2Sn(0)->CLKCFG = 0;
            }
        }

        //
        // Go to Deep Sleep.
        //
        //am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);

    } 
}
