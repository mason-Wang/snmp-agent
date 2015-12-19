//*****************************************************************************
//
// lwiplib.h - Prototypes for the lwIP library wrapper API.
//
// Copyright (c) 2008 Luminary Micro, Inc.  All rights reserved.
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

#ifndef __LWIPLIB_H__
#define __LWIPLIB_H__

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// lwIP Options
//
//*****************************************************************************
#include "../lwip-1.3.0/src/include/lwip/opt.h"

//*****************************************************************************
//
// Ensure that AUTOIP COOP option is configured correctly.
//
//*****************************************************************************
#undef LWIP_DHCP_AUTOIP_COOP
#define LWIP_DHCP_AUTOIP_COOP   ((LWIP_DHCP) && (LWIP_AUTOIP))

//*****************************************************************************
//
// lwIP API Header Files
//
//*****************************************************************************
#include "../lwip-1.3.0/src/include/lwip/api.h"
#include "../lwip-1.3.0/src/include/lwip/netifapi.h"
#include "../lwip-1.3.0/src/include/lwip/tcp.h"
#include "../lwip-1.3.0/src/include/lwip/udp.h"
#include "../lwip-1.3.0/src/include/lwip/tcpip.h"
#include "../lwip-1.3.0/src/include/lwip/sockets.h"
#include "../lwip-1.3.0/src/include/lwip/mem.h"

//*****************************************************************************
//
// IP Address Acquisition Modes
//
//*****************************************************************************
#define IPADDR_USE_STATIC       0
#define IPADDR_USE_DHCP         1
#define IPADDR_USE_AUTOIP       2

//*****************************************************************************
//
// lwIP Abstraction Layer API
//
//*****************************************************************************
extern void lwIPInit(const unsigned char *pucMac, unsigned long ulIPAddr,
                     unsigned long ulNetMask, unsigned long ulGWAddr,
                     unsigned long ulIPMode);
extern void lwIPTimer(unsigned long ulTimeMS);
extern void lwIPEthernetIntHandler(void);
extern unsigned long lwIPLocalIPAddrGet(void);
extern unsigned long lwIPLocalNetMaskGet(void);
extern unsigned long lwIPLocalGWAddrGet(void);
extern void lwIPLocalMACGet(unsigned char *pucMac);
extern void lwIPNetworkConfigChange(unsigned long ulIPAddr,
                                    unsigned long ulNetMask,
                                    unsigned long ulGWAddr,
                                    unsigned long ulIPMode);

#ifdef __cplusplus
}
#endif

#endif // __LWIPLIB_H__
