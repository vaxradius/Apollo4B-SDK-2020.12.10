Name:
=====
 pdm_to_i2s


Description:
============
 An example to show PDM to I2S operation.


Purpose:
========
This example enables the PDM and I2S interface to collect audio signals from an
external microphone, transmit to I2S1(slave), then loop back to I2S0(master). The required pin connections are:

Printing takes place over the ITM at 1M Baud.

GPIO 50 - PDM0 CLK
GPIO 51 - PDM0 DATA

GPIO 52 - PDM1 CLK
GPIO 53 - PDM1 DATA

GPIO 54 - PDM2 CLK
GPIO 55 - PDM2 DATA

GPIO 56 - PDM3 CLK
GPIO 57 - PDM3 DATA


******************************************************************************


