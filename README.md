# SD-PCB-Bring-Up

USB-C PD sink bring-up for the STM32G474RE and U17 TCPP03-M20.
Requests a fixed **20 V, 1 A** contract when the source advertises it.
Otherwise requests the first (5 V) PDO, capped at 1 A and the source's advertised current.
This requests available power; it does not force the board to draw 1 A or set a hardware current limiter.

Build with `platformio run`. The active PlatformIO project contains the same firmware.
The corrected firmware was built and flashed through the connected ST-LINK.

Connections: I2C3 SDA PC11, SCL PA8, TCPP EN PD2, UCPD CC1 PB6, CC2 PB4.
The TCPP uses 7-bit I2C address 0x34. Its normal-mode control value is 0x10:
consumer gate closed, provider gate off, VCONN and discharge off.
UCPD uses DMA1 channels 1/2 and TIM1. The clock override uses HSI16 and a
170 MHz system clock so PD timing does not depend on the Nucleo's HSE configuration.

The original PC7 blink is retained (1000 ms high, 100 ms low), without blocking PD.
There is no serial output, ADC, or voltage-monitor application code.
The PD stack uses the TCPP VBUS_OK flag for presence detection. With R9/R10
unpopulated it cannot independently verify 20 V or vSafe0V. Reset recovery uses
the TCPP's coarse VBUS presence comparator, and the policy engine polls it because
FLGn is unconnected. This is a bring-up configuration, not a
USB-PD compliance-validated implementation. The TCPP's own VSENSE protection
network (R64/R65) must be present as designed.

ST dependencies and licenses are in `lib/UsbPdST`:

- USB-PD core: https://github.com/STMicroelectronics/stm32-mw-usbpd-core/tree/66dab10c5f426c600aae1bac3908b9f4e279c0a4
- STM32G4 device layer: https://github.com/STMicroelectronics/x-cube-tcpp/tree/main/Middlewares/ST/STM32_USBPD_Library/Devices/STM32G4XX
- TCPP register definitions: https://github.com/STMicroelectronics/stm32-tcpp0203/tree/9918655bff176ac3046ccf378b5c7bbbc6a38d15

Verified on the connected PCB through ST-LINK: initialization and attachment succeed,
six source PDOs are received, the 20 V PDO is selected, the RDO is 0x51019064
(PDO position 5, operating/max current 1 A), and PE_Power is EXPLICITCONTRACT.
The physical output voltage has not been independently measured.
