//*****************************************************************************
//
// enet_lwip.c - Sample WebServer Application using lwIP.
//
// Copyright (c) 2007-2008 Luminary Micro, Inc.  All rights reserved.
// 
// Software License Agreement
// 
// Luminary Micro, Inc. (LMI) is supplying this software for use solely and
// exclusively on LMI's microcontroller products.
// 
// The software is owned by LMI and/or its suppliers, and is protected under
// applicable copyright laws.  All rights are reserved.  You may not combine
// this software with "viral" open-source software in order to form a larger
// program.  Any use in violation of the foregoing restrictions may subject
// the user to criminal sanctions under applicable laws, as well as to civil
// liability for the breach of the terms and conditions of this license.
// 
// THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
// OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
// LMI SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR
// CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 3416 of the Stellaris Peripheral Driver Library.
//
//*****************************************************************************

#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_nvic.h"
#include "hw_types.h"
#include "ethernet.h"
#include "flash.h"
#include "gpio.h"
#include "interrupt.h"
#include "sysctl.h"
#include "systick.h"
#include "ustdlib.h"
#include "uartstdio.h"
#include "lwiplib.h"
#include "../lwip-1.3.0/apps/httpserver_raw/httpd.h"
#include "../lwip-1.3.0/src/include/lwip/snmp.h"
#include "softeeprom.h"
#include "softeeprom_wrapper.h"
#include "storage_config.h"

//*****************************************************************************
//
//! \addtogroup ek_lm3s6965_list
//! <h1>Ethernet with lwIP (enet_lwip)</h1>
//!
//! This example application demonstrates the operation of the Stellaris
//! Ethernet controller using the lwIP TCP/IP Stack.  DHCP is used to obtain
//! an Ethernet address.  If DHCP times out without obtaining an address,
//! AUTOIP will be used to obtain a link-local address.  The address that is
//! selected will be shown on the OLED display.
//!
//! The file system code will first check to see if an SD card has been plugged
//! into the microSD slot.  If so, all file requests from the web server will
//! be directed to the SD card.  Otherwise, a default set of pages served up
//! by an internal file system will be used.
//!
//! For additional details on lwIP, refer to the lwIP web page at:
//! http://www.sics.se/~adam/lwip/
//
//*****************************************************************************

//*****************************************************************************
//
// Defines for setting up the system clock.
//
//*****************************************************************************
#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)
#define SYSTICKUS               (1000000 / SYSTICKHZ)
#define SYSTICKNS               (1000000000 / SYSTICKHZ)

//*****************************************************************************
//
// Start address of internal flash memory used for EEPROM emulation
//
//*****************************************************************************
#define EEPROM_START_ADDR  (0x3D000)

//*****************************************************************************
//
// EEPROM page size to be emulated on internal flash memory, size must be
// at least:
//  EEPROM_PAGE_SIZE >= (MAX_SOFTEEPROM_IDS*8)
//
//*****************************************************************************
#define EEPROM_PAGE_SIZE (0x400)

// sanity check
#if (EEPROM_PAGE_SIZE < (MAX_SOFTEEPROM_IDS*8))
#error EEPROM_PAGE_SIZE must be at least 8 times MAX_SOFTEEPROM_IDS!
#endif

//*****************************************************************************
//
// End address (+1) of internal flash memory used for EEPROM emulation
//
//*****************************************************************************
#define EEPROM_END_ADDR  (EEPROM_START_ADDR + 4*EEPROM_PAGE_SIZE)

// sanity check
#if (((EEPROM_END_ADDR - EEPROM_START_ADDR)/EEPROM_PAGE_SIZE) < 2)
#error There must be at least two EEPROM pages inside the memory range between EEPROM_START_ADDR and EEPROM_END_ADDR
#endif


//*****************************************************************************
//
// External Application references.
//
//*****************************************************************************
extern void parseCmd(char *cmd, unsigned long len);

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

//*****************************************************************************
//
// Display an lwIP type IP Address.
//
//*****************************************************************************
void
DisplayIPAddress(unsigned long ipaddr, char *type)
{
    unsigned char *pucTemp = (unsigned char *)&ipaddr;

		//
    // print the string.
    //
	  UARTprintf("%s:   %d.%d.%d.%d\n", type, pucTemp[0], pucTemp[1],
								pucTemp[2], pucTemp[3]);
}

//*****************************************************************************
//
// Required by lwIP library to support any host-related timer functions.
//
//*****************************************************************************
void
lwIPHostTimerHandler(void)
{
    static unsigned long ulLastIPAddress = 0;
    unsigned long ulIPAddress;

    ulIPAddress = lwIPLocalIPAddrGet();

    //
    // If IP Address has not yet been assigned, update the display accordingly
    //
    if(ulIPAddress == 0)
    {
    }

    //
    // Check if IP address has changed, and display if it has.
    //
    else if(ulLastIPAddress != ulIPAddress)
    {
        ulLastIPAddress = ulIPAddress;
        DisplayIPAddress(ulIPAddress, "IP");
        ulIPAddress = lwIPLocalNetMaskGet();
        DisplayIPAddress(ulIPAddress, "MASK");
        ulIPAddress = lwIPLocalGWAddrGet();
        DisplayIPAddress(ulIPAddress, "GW");
    }
}

//*****************************************************************************
//
// The interrupt handler for the SysTick interrupt.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
    unsigned char rx_los, fiber;
    
    //
    // Call the lwIP timer handler.
    //
    lwIPTimer(SYSTICKMS);
	
	//
	// update SNMP uptime timestamp
	//
	snmp_inc_sysuptime();
    
    // The FIBER pin keep track with RX_LOS
    rx_los = GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2) >> 2;
    fiber = GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_3) >> 3;
    if (rx_los != fiber) {
        UARTprintf("Write fiber pin to %d\n", rx_los);
        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3, rx_los << 3);
    }
}


//*****************************************************************************
//
// This example demonstrates the use of the Ethernet Controller.
//
//*****************************************************************************
int
main(void)
{
    unsigned char pucMACArray[6];
	char cmd[128];
	unsigned long cmdlen = 0;
	unsigned long ulIpAddr, ulNetMask, ulGateWay;
   
    //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_16 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_6MHZ);

    //
    // Initialize the UART for debug output.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioInit(0);

	UARTprintf("\nSNMP with lwIP, build time:%s %s\n", __DATE__, __TIME__);
	UARTprintf("System clock:%dHZ\n", SysCtlClockGet());

    //
    // Enable and Reset the Ethernet Controller.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);
   
    // GPIO PA2 PA3 for STATUS1 STATUS2
    GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
                     
    // GPIO PA3-PA7 for BAUD2_4-BAUD2_1
    GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, 
                   GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
     
    // GPIO PB0 for BAUD1_1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    
    // GPIO PB1-PB3 for Far_TP_Link1 RX_LOS Far_TP_Link2
    GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 
                   GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
                     
    // GPIO PC4-PC7 for TP_Link4-TP_Link1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    GPIODirModeSet(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, 
                   GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
                     
    // GPIO PD0-PD3 for BAUD1_4_R-BAUD1_1_R
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 
                   GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
                     
    // GPIO PD4-PD7 for RXD2_MON TXD2_MON RXD1_MON TXD1_MON
    GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, 
                   GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
                     
    // GPIO PE0 PE1 for Far_TP_Link3 Far_TP_Link4
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1, 
                   GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
                     
    // GPIO PE3-7 for FIBER and BAUD2_1_R-BAUD2_4_R
    GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, 
                   GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, 
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
                     
    // GPIO PF1-PF3 for BAUD1_2-BAUD1_4
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 
                   GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
/*
    //
    // Enable Port F for Ethernet LEDs.
    //  LED0        Bit 3   Output
    //  LED1        Bit 2   Output
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
					 
	// GPIO PD6 PD7 for LED2 and LED3				 
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_6 | GPIO_PIN_7, GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_6 | GPIO_PIN_7, 1 << 6 | 1 << 7);
	GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_6 | GPIO_PIN_7,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
					 
	// GPIO PB0 for beep
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_DIR_MODE_OUT);
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
	
	// GPIO PE0 PE1 for key
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_DIR_MODE_IN);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
*/
    //
    // Configure SysTick for a periodic interrupt.
    //
    SysTickPeriodSet(SysCtlClockGet() / SYSTICKHZ);
    SysTickEnable();
    SysTickIntEnable();

    //
    // Enable processor interrupts.
    //
    IntMasterEnable();

	// Initialize the EEPROM emulation region.
    //
    SoftEEPROM_WrapperInit(EEPROM_START_ADDR, EEPROM_END_ADDR, EEPROM_PAGE_SIZE);

	// get the MAC address from the eeprom
	SoftEEPROM_WrapperRead(EEPROM_MAC_ADDR, 6, pucMACArray);
	if (pucMACArray[0] == 0xff && pucMACArray[1] == 0xff && pucMACArray[2] == 0xff &&
			pucMACArray[3] == 0xff && pucMACArray[4] == 0xff && pucMACArray[5] == 0xff)
	{
		UARTprintf("MAC Address Not Programmed!Use ac-de-48-33-56-12 default!\n");
		pucMACArray[0] = 0xac;
		pucMACArray[1] = 0xde;
		pucMACArray[2] = 0x48;
		pucMACArray[3] = 0x33;
		pucMACArray[4] = 0x56;
		pucMACArray[5] = 0x12;
		//SoftEEPROM_WrapperWrite(EEPROM_MAC_ADDR, 6, pucMACArray);
	}
		
	UARTprintf("MAC addr:%02x-%02x-%02x-%02x-%02x-%02x\n", pucMACArray[0], pucMACArray[1], \
				pucMACArray[2], pucMACArray[3], pucMACArray[4], pucMACArray[5]);

	// get the ip address
	SoftEEPROM_WrapperRead(EEPROM_IP_ADDR, sizeof(unsigned long), (unsigned char *)&ulIpAddr);
	if (ulIpAddr == 0xFFFFFFFF)
		ulIpAddr = 0xC0A80010;

	// get the netmask
	SoftEEPROM_WrapperRead(EEPROM_NETMASK_ADDR, sizeof(unsigned long), (unsigned char *)&ulNetMask);
	if (ulNetMask == 0xFFFFFFFF)
		ulNetMask = 0xFFFFFF00;
	
	// get the gateway
	SoftEEPROM_WrapperRead(EEPROM_GATEWAY_ADDR, sizeof(unsigned long), (unsigned char *)&ulGateWay);
	if (ulGateWay == 0xFFFFFFFF)
		ulGateWay = 0;
	
    //
    // Initialze the lwIP library, using DHCP.
    //
    lwIPInit(pucMACArray, ulIpAddr, ulNetMask, ulGateWay, IPADDR_USE_STATIC);

    //
    // Indicate that DHCP has started.
    //
	UARTprintf("Waiting for IP...\n");

    //
    // Loop forever.  All the work is done in interrupt handlers.
    //
    while(1)
    {
		cmdlen = UARTgets(cmd, sizeof(cmd));
		parseCmd(cmd, cmdlen);
    }
}
