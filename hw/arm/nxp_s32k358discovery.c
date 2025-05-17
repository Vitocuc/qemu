/*
 * ST STM32VLDISCOVERY machine
 *
 * Copyright (c) 2021 Alexandre Iooss <erdnaxe@crans.org>
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 #include "qemu/osdep.h"
 #include "qapi/error.h"
 #include "hw/boards.h"
 #include "hw/qdev-properties.h"
 #include "hw/qdev-clock.h"
 #include "qemu/error-report.h"
 #include "hw/arm/nxps32k358_soc.h"
 #include "hw/arm/boot.h"
 
 /* nxp_s32k358discovery implementation is derived from stm32vldiscovery */
 
 /* Main SYSCLK frequency in Hz (24MHz) */
 #define SYSCLK_FRQ 24000000ULL // unsigned long long
 
 static void nxp_s32k358discovery_init(MachineState *machine)
 {
     DeviceState *dev;
     Clock *sysclk;
     /* This clock doesn't need migration because it is fixed-frequency */
     sysclk = clock_new(OBJECT(machine), "SYSCLK");
     clock_set_hz(sysclk, SYSCLK_FRQ);
 
     dev = qdev_new(TYPE_NXPS32K358_SOC);
     object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
     qdev_connect_clock_in(dev, "sysclk", sysclk);
     sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
 
     armv7m_load_kernel(NXPS32K358_SOC(dev)->armv7m.cpu,
                        machine->kernel_filename,
                        CODE_FLASH_BASE_ADDRESS, CODE_FLASH_BLOCK_SIZE * 4); // da capire come lui le ha ridefinite
 }
 
 static void nxp_s32k358discovery_machine_init(MachineClass *mc)
 {
    static const char *const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m7"), NULL
    }; 
    
    mc->desc = "NXP NXPS32K358 (Cortex-M7)";
    mc->init = nxp_s32k358discovery_init;
    mc->valid_cpu_types = valid_cpu_types;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1; 
 }
 
 DEFINE_MACHINE("nxps32k358discovery", nxp_s32k358discovery_machine_init)
 