#ifndef HW_ARM_NXPS32K358_SOC_H
#define HW_ARM_NXPS32K358_SOC_H

//#include "hw/misc/stm32f2xx_syscfg.h"
#include "hw/char/nxps32k358_lpuart.h"
#include "hw/or-irq.h"
#include "hw/ssi/nxps32k358_spi.h"
#include "hw/arm/armv7m.h"
#include "hw/clock.h"
#include "qom/object.h"

#define TYPE_NXPS32K358_SOC "nxps32k358-soc"
OBJECT_DECLARE_SIMPLE_TYPE(NXPS32K358State, NXPS32K358_SOC)

#define NXP_NUM_LPUARTS 8
// #define STM_NUM_TIMERS 4
// #define STM_NUM_ADCS 3
#define NXP_NUM_SPIS 4

#define FLASH_BASE_ADDRESS 0x08000000
#define FLASH_SIZE (1024 * 1024)
#define SRAM_BASE_ADDRESS 0x20000000
#define SRAM_SIZE (128 * 1024)

struct NXPS32K358State {
    SysBusDevice parent_obj;

    ARMv7MState armv7m;

    NXPS32K358SyscfgState syscfg;
    NXPS32K358LpuartState lpuarts[NXP_NUM_LPUARTS];
    //STM32F2XXTimerState timer[STM_NUM_TIMERS];
    //STM32F2XXADCState adc[STM_NUM_ADCS];
    NXPS32K358SPIState spi[NXP_NUM_SPIS];

    OrIRQState *adc_irqs;

    MemoryRegion sram;
    MemoryRegion flash;
    MemoryRegion flash_alias;

    Clock *sysclk;
    Clock *refclk;

    Clock *aips_plat_clk; 
    Clock *aips_slow_clk; 
};

#endif