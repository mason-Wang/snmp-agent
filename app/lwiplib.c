//*****************************************************************************
//
// lwiplib.c - lwIP TCP/IP Library Abstraction Layer.
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

//*****************************************************************************
//
// Ensure that the lwIP Compile Time Options are included first.
//
//*****************************************************************************
#include "lwiplib.h"

//*****************************************************************************
//
// Include lwIP high-level API code.
//
//*****************************************************************************
#include "../lwip-1.3.0/src/api/api_lib.c"
#include "../lwip-1.3.0/src/api/api_msg.c"
#include "../lwip-1.3.0/src/api/err.c"
#include "../lwip-1.3.0/src/api/netbuf.c"
#include "../lwip-1.3.0/src/api/netdb.c"
#include "../lwip-1.3.0/src/api/netifapi.c"
#include "../lwip-1.3.0/src/api/sockets.c"
#include "../lwip-1.3.0/src/api/tcpip.c"

//*****************************************************************************
//
// Include the core lwIP TCP/IP stack code.
//
//*****************************************************************************
#include "../lwip-1.3.0/src/core/dhcp.c"
#include "../lwip-1.3.0/src/core/dns.c"
#include "../lwip-1.3.0/src/core/init.c"
#include "../lwip-1.3.0/src/core/mem.c"
#include "../lwip-1.3.0/src/core/memp.c"
#include "../lwip-1.3.0/src/core/netif.c"
#include "../lwip-1.3.0/src/core/pbuf.c"
#include "../lwip-1.3.0/src/core/raw.c"
#include "../lwip-1.3.0/src/core/stats.c"
#include "../lwip-1.3.0/src/core/sys.c"
#include "../lwip-1.3.0/src/core/tcp.c"
#include "../lwip-1.3.0/src/core/tcp_in.c"
#include "../lwip-1.3.0/src/core/tcp_out.c"
#include "../lwip-1.3.0/src/core/udp.c"

//*****************************************************************************
//
// Include the IPV4 code.
//
//*****************************************************************************
#include "../lwip-1.3.0/src/core/ipv4/autoip.c"
#include "../lwip-1.3.0/src/core/ipv4/icmp.c"
#include "../lwip-1.3.0/src/core/ipv4/igmp.c"
#include "../lwip-1.3.0/src/core/ipv4/inet.c"
#include "../lwip-1.3.0/src/core/ipv4/inet_chksum.c"
#include "../lwip-1.3.0/src/core/ipv4/ip.c"
#include "../lwip-1.3.0/src/core/ipv4/ip_addr.c"
#include "../lwip-1.3.0/src/core/ipv4/ip_frag.c"

//*****************************************************************************
//
// Include the lwIP SNMP code.
//
//*****************************************************************************
#include "../lwip-1.3.0/src/core/snmp/asn1_dec.c"
#include "../lwip-1.3.0/src/core/snmp/asn1_enc.c"
#include "../lwip-1.3.0/src/core/snmp/mib2.c"
#include "../lwip-1.3.0/src/core/snmp/mib_structs.c"
#include "../lwip-1.3.0/src/core/snmp/msg_in.c"
#include "../lwip-1.3.0/src/core/snmp/msg_out.c"

//*****************************************************************************
//
// Include the Network Interface code.
//
//*****************************************************************************
#include "../lwip-1.3.0/src/netif/etharp.c"
#include "../lwip-1.3.0/src/netif/loopif.c"

//*****************************************************************************
//
// Include the Network Interface PPP code.
//
//*****************************************************************************
#include "../lwip-1.3.0/src/netif/ppp/auth.c"
#include "../lwip-1.3.0/src/netif/ppp/chap.c"
#include "../lwip-1.3.0/src/netif/ppp/chpms.c"
#include "../lwip-1.3.0/src/netif/ppp/fsm.c"
#include "../lwip-1.3.0/src/netif/ppp/ipcp.c"
#include "../lwip-1.3.0/src/netif/ppp/lcp.c"
#include "../lwip-1.3.0/src/netif/ppp/magic.c"
#include "../lwip-1.3.0/src/netif/ppp/md5.c"
#include "../lwip-1.3.0/src/netif/ppp/pap.c"
#include "../lwip-1.3.0/src/netif/ppp/ppp.c"
#include "../lwip-1.3.0/src/netif/ppp/ppp_oe.c"
#include "../lwip-1.3.0/src/netif/ppp/randm.c"
#include "../lwip-1.3.0/src/netif/ppp/vj.c"

//*****************************************************************************
//
// Include Stellaris-specific lwIP interface/porting layer code.
//
//*****************************************************************************
#include "../lwip-1.3.0/ports/stellaris/perf.c"
#include "../lwip-1.3.0/ports/stellaris/sys_arch.c"
#include "../lwip-1.3.0/ports/stellaris/netif/stellarisif.c"

//*****************************************************************************
//
//! \addtogroup utilities_api
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// The lwIP Library abstration layer provides for a host callback function
// to be called periodically in the lwIP context.  This is the timer
// interval, in ms, for this periodic callback.  If the timer interval is
// defined to 0 (the default value), then no periodic host callback is
// performed.
//
//*****************************************************************************
#ifndef HOST_TMR_INTERVAL
#define HOST_TMR_INTERVAL       0
#else
extern void lwIPHostTimerHandler(void);
#endif

//*****************************************************************************
//
// Driverlib headers needed for this library module.
//
//*****************************************************************************
#include "../inc/hw_ethernet.h"
#include "../inc/hw_ints.h"
#include "../inc/hw_memmap.h"
#include "../inc/hw_nvic.h"
#include "../inc/hw_types.h"
#include "../inc/ethernet.h"
#include "../inc/gpio.h"
#include "../inc/sysctl.h"
#include "../inc/debug.h"

//*****************************************************************************
//
// The lwIP network interface structure for the Stellaris Ethernet MAC.
//
//*****************************************************************************
static struct netif lwip_netif;

//*****************************************************************************
//
// The local time for the lwIP Library Abstraction layer, used to support the
// Host and lwIP periodic callback functions.
//
//*****************************************************************************
static unsigned long g_ulLocalTimer = 0;

//*****************************************************************************
//
// The local time when the TCP timer was last serviced.
//
//*****************************************************************************
static unsigned long g_ulTCPTimer = 0;

//*****************************************************************************
//
// The local time when the HOST timer was last serviced.
//
//*****************************************************************************
#if HOST_TMR_INTERVAL
static unsigned long g_ulHostTimer = 0;
#endif

//*****************************************************************************
//
// The local time when the ARP timer was last serviced.
//
//*****************************************************************************
#if LWIP_ARP
static unsigned long g_ulARPTimer = 0;
#endif

//*****************************************************************************
//
// The local time when the AutoIP timer was last serviced.
//
//*****************************************************************************
#if LWIP_AUTOIP
static unsigned long g_ulAutoIPTimer = 0;
#endif

//*****************************************************************************
//
// The local time when the DHCP Coarse timer was last serviced.
//
//*****************************************************************************
#if LWIP_DHCP
static unsigned long g_ulDHCPCoarseTimer = 0;
#endif

//*****************************************************************************
//
// The local time when the DHCP Fine timer was last serviced.
//
//*****************************************************************************
#if LWIP_DHCP
static unsigned long g_ulDHCPFineTimer = 0;
#endif

//*****************************************************************************
//
// The default IP address aquisition mode.
//
//*****************************************************************************
static unsigned long g_ulIPMode = IPADDR_USE_STATIC;

//*****************************************************************************
//
//! \internal
//!
//! Service the lwIP timers.
//!
//! This function services all of the lwIP periodic timers, including TCP and
//! Host timers.  This should be called from the lwIP context, which may be
//! the Ethernet interrupt (in the case of a non-RTOS system) or the lwIP
//! thread, in the event that an RTOS is used.
//!
//! \return None.
//
//*****************************************************************************
static void
lwIPServiceTimers(void)
{
    //
    // Service the host timer.
    //
#if HOST_TMR_INTERVAL
    if((g_ulLocalTimer - g_ulHostTimer) >= HOST_TMR_INTERVAL)
    {
        g_ulHostTimer = g_ulLocalTimer;
        lwIPHostTimerHandler();
    }
#endif

    //
    // Service the ARP timer.
    //
#if LWIP_ARP
    if((g_ulLocalTimer - g_ulARPTimer) >= ARP_TMR_INTERVAL)
    {
        g_ulARPTimer = g_ulLocalTimer;
        etharp_tmr();
    }
#endif

    //
    // Service the TCP timer.
    //
    if((g_ulLocalTimer - g_ulTCPTimer) >= TCP_TMR_INTERVAL)
    {
        g_ulTCPTimer = g_ulLocalTimer;
        tcp_tmr();
    }

    //
    // Service the AutoIP timer.
    //
#if LWIP_AUTOIP
    if((g_ulLocalTimer - g_ulAutoIPTimer) >= AUTOIP_TMR_INTERVAL)
    {
        g_ulAutoIPTimer = g_ulLocalTimer;
        autoip_tmr();
    }
#endif

    //
    // Service the DCHP Coarse Timer.
    //
#if LWIP_DHCP
    if((g_ulLocalTimer - g_ulDHCPCoarseTimer) >= DHCP_COARSE_TIMER_MSECS)
    {
        g_ulDHCPCoarseTimer = g_ulLocalTimer;
        dhcp_coarse_tmr();
    }
#endif

    //
    // Service the DCHP Fine Timer.
    //
#if LWIP_DHCP
    if((g_ulLocalTimer - g_ulDHCPFineTimer) >= DHCP_FINE_TIMER_MSECS)
    {
        g_ulDHCPFineTimer = g_ulLocalTimer;
        dhcp_fine_tmr();
    }
#endif
}

//*****************************************************************************
//
//! Initializes the lwIP TCP/IP stack.
//!
//! \param pucMAC is a pointer to a six byte array containing the MAC
//! address to be used for the interface.
//! \param ulIPAddr is the IP address to be used (static).
//! \param ulNetMask is the network mask to be used (static).
//! \param ulGWAddr is the Gateway address to be used (static).
//! \param ulIPMode is the IP Address Mode.  0 will force static IP addressing
//! to be used, 1 will force DHCP with fallback to Link Local (Auto IP), while
//! 2 will force Link Local only.
//!
//! This function performs initialization of the lwIP TCP/IP stack for the
//! Stellaris Ethernet MAC, including DHCP and/or AutoIP, as configured.
//!
//! \return None.
//
//*****************************************************************************
void
lwIPInit(const unsigned char *pucMAC, unsigned long ulIPAddr,
         unsigned long ulNetMask, unsigned long ulGWAddr,
         unsigned long ulIPMode)
{
    struct ip_addr ip_addr;
    struct ip_addr net_mask;
    struct ip_addr gw_addr;

    //
    // Check the parameters.
    //
#if LWIP_DHCP && LWIP_AUTOIP
    ASSERT((ulIPMode == IPADDR_USE_STATIC) ||
           (ulIPMode == IPADDR_USE_DHCP) ||
           (ulIPMode == IPADDR_USE_AUTOIP))
#elif LWIP_DHCP
    ASSERT((ulIPMode == IPADDR_USE_STATIC) ||
           (ulIPMode == IPADDR_USE_DHCP))
#elif LWIP_AUTOIP
    ASSERT((ulIPMode == IPADDR_USE_STATIC) ||
           (ulIPMode == IPADDR_USE_AUTOIP))
#else
    ASSERT(ulIPMode == IPADDR_USE_STATIC)
#endif

    //
    // Enable the ethernet peripheral.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);

    //
    // Program the MAC address into the Ethernet controller.
    //
    EthernetMACAddrSet(ETH_BASE, (unsigned char *)pucMAC);

    //
    // Initialize lwIP library modules
    //
    lwip_init();

    //
    // Setup the network address values.
    //
    if(ulIPMode == IPADDR_USE_STATIC)
    {
        ip_addr.addr = htonl(ulIPAddr);
        net_mask.addr = htonl(ulNetMask);
        gw_addr.addr = htonl(ulGWAddr);
    }
#if LWIP_DHCP || LWIP_AUTOIP
    else
    {
        ip_addr.addr = 0;
        net_mask.addr = 0;
        gw_addr.addr = 0;
    }
#endif

    //
    // Create, configure and add the Ethernet controller interface with
    // default settings.
    //
    netif_add(&lwip_netif, &ip_addr, &net_mask, &gw_addr, NULL,
              stellarisif_init, ip_input);
    netif_set_default(&lwip_netif);

    //
    // Indicate that we are running in static IP mode.
    //
    g_ulIPMode = ulIPMode;

    //
    // Start DHCP, if enabled.
    //
#if LWIP_DHCP
    if(g_ulIPMode == IPADDR_USE_DHCP)
    {
        dhcp_start(&lwip_netif);
    }
#endif

    //
    // Start AutoIP, if enabled and DHCP is not.
    //
#if LWIP_AUTOIP
    if(g_ulIPMode == IPADDR_USE_AUTOIP)
    {
        autoip_start(&lwip_netif);
    }
#endif

    //
    // Bring the interface up.
    //
    netif_set_up(&lwip_netif);
}

//*****************************************************************************
//
//! Handles periodic timer events for the lwIP TCP/IP stack.
//!
//! \param ulTimeMS is the incremental time for this periodic interrupt.
//!
//! This function will update the local timer by the value in \e ulTimeMS.
//! If the system is configured for use without an RTOS, an Ethernet interrupt
//! will be triggered to allow the lwIP periodic timers to be serviced in the
//! Ethernet interrupt.
//!
//! \return None.
//
//*****************************************************************************
void
lwIPTimer(unsigned long ulTimeMS)
{
    //
    // Increment the lwIP Ethernet timer.
    //
    g_ulLocalTimer += ulTimeMS;

#if NO_SYS
    //
    // Generate an Ethernet interrupt.  This will perform the actual work
    // of checking the lwIP timers and taking the appropriate actions.  This is
    // needed since lwIP is not re-entrant, and this allows all lwIP calls to
    // be placed inside the Ethernet interrupt handler ensuring that all calls
    // into lwIP are coming from the same context, preventing any reentrancy
    // issues.  Putting all the lwIP calls in the Ethernet interrupt handler
    // avoids the use of mutexes to avoid re-entering lwIP.
    //
    HWREG(NVIC_SW_TRIG) |= INT_ETH - 16;

#else
    //
    // If running in an RTOS/SYSTEM environment, what should happen is is that
    // an event or semaphore should be posted that will wake up the main
    // processing thread.  The exact mechanism for this needs to be determined.
    // Until that time, generate an error if this module is compiled for an
    // RTOS based system.
    //
    #error "lwIPTimer does not yet support an RTOS!"
#endif
}

//*****************************************************************************
//
//! Handles Ethernet interrupts for the lwIP TCP/IP stack.
//!
//! This function handles Ethernet interrupts for the lwIP TCP/IP stack.  At
//! the lowest level, all receive packets are placed into a packet queue for
//! processing at a higher level.  Also, the transmit packet queue is checked
//! and packets are drained and transmitted through the Ethernet MAC as needed.
//! If the system is configured without an RTOS, additional processing is
//! performed at the interrupt level.  The packet queues are processed by the
//! lwIP TCP/IP code, and lwIP periodic timers are serviced (as needed).
//!
//! \return None.
//
//*****************************************************************************
void
lwIPEthernetIntHandler(void)
{
    unsigned long ulStatus;

    //
    // Read and Clear the interrupt.
    //
    ulStatus = EthernetIntStatus(ETH_BASE, false);
    EthernetIntClear(ETH_BASE, ulStatus);

    //
    // If a transmit/rx interrupt was active, run the low-level interrupt
    // handler.
    //
    if(ulStatus)
    {
        stellarisif_interrupt(&lwip_netif);
    }

#if NO_SYS
    //
    // If no RTOS is enabled, then we do all lwIP procesing in the interrupt.
    //

    //
    // Service any packets on the receive queue.
    //
    stellarisif_input(&lwip_netif);

    //
    // Service the lwIP timers.
    //
    lwIPServiceTimers();
#endif
}

//*****************************************************************************
//
//! Returns the IP address for this interface.
//!
//! This function will read and return the currently assigned IP address for
//! the Stellaris Ethernet interface.
//!
//! \return Returns the assigned IP address for this interface.
//
//*****************************************************************************
unsigned long
lwIPLocalIPAddrGet(void)
{
    return((unsigned long)lwip_netif.ip_addr.addr);
}

//*****************************************************************************
//
//! Returns the network mask for this interface.
//!
//! This function will read and return the currently assigned network mask for
//! the Stellaris Ethernet interface.
//!
//! \return the assigned network mask for this interface.
//
//*****************************************************************************
unsigned long
lwIPLocalNetMaskGet(void)
{
    return((unsigned long)lwip_netif.netmask.addr);
}

//*****************************************************************************
//
//! Returns the gateway address for this interface.
//!
//! This function will read and return the currently assigned gateway address
//! for the Stellaris Ethernet interface.
//!
//! \return the assigned gateway address for this interface.
//
//*****************************************************************************
unsigned long
lwIPLocalGWAddrGet(void)
{
    return((unsigned long)lwip_netif.gw.addr);
}

//*****************************************************************************
//
//! Returns the local MAC/HW address for this interface.
//!
//! \param pucMAC is a pointer to an array of bytes used to store the MAC
//! address.
//!
//! This function will read the currently assigned MAC address into the array
//! pased in \e pucMAC.
//!
//! \return None.
//
//*****************************************************************************
void
lwIPLocalMACGet(unsigned char *pucMAC)
{
    EthernetMACAddrGet(ETH_BASE, pucMAC);
}

//*****************************************************************************
//
//! Change the configuration of the lwIP network interface.
//!
//! \param ulIPAddr is the new IP address to be used (static).
//! \param ulNetMask is the new network mask to be used (static).
//! \param ulGWAddr is the new Gateway address to be used (static).
//! \param ulIPMode is the IP Address Mode.  0 will force static IP addressing
//! to be used, 1 will force DHCP with fallback to Link Local (Auto IP), while
//! 2 will force Link Local only.
//!
//! This function will evaluate the new configuration data.  If necessary, the
//! interface will be brought down, reconfigured, and then brought back up
//! with the new configuration.
//!
//! \return None.
//
//*****************************************************************************
void
lwIPNetworkConfigChange(unsigned long ulIPAddr, unsigned long ulNetMask,
                        unsigned long ulGWAddr, unsigned long ulIPMode)
{
    struct ip_addr ip_addr;
    struct ip_addr net_mask;
    struct ip_addr gw_addr;

    //
    // Check the parameters.
    //
#if LWIP_DHCP && LWIP_AUTOIP
    ASSERT((ulIPMode == IPADDR_USE_STATIC) ||
           (ulIPMode == IPADDR_USE_DHCP) ||
           (ulIPMode == IPADDR_USE_AUTOIP))
#elif LWIP_DHCP
    ASSERT((ulIPMode == IPADDR_USE_STATIC) ||
           (ulIPMode == IPADDR_USE_DHCP))
#elif LWIP_AUTOIP
    ASSERT((ulIPMode == IPADDR_USE_STATIC) ||
           (ulIPMode == IPADDR_USE_AUTOIP))
#else
    ASSERT(ulIPMode == IPADDR_USE_STATIC)
#endif

    //
    // Setup the network address values.
    //
    if(ulIPMode == IPADDR_USE_STATIC)
    {
        ip_addr.addr = htonl(ulIPAddr);
        net_mask.addr = htonl(ulNetMask);
        gw_addr.addr = htonl(ulGWAddr);
    }
#if LWIP_DHCP || LWIP_AUTOIP
    else
    {
        ip_addr.addr = 0;
        net_mask.addr = 0;
        gw_addr.addr = 0;
    }
#endif

    //
    // Switch on the current IP Address Aquisition mode.
    //
    switch(g_ulIPMode)
    {
        //
        // Static IP
        //
        case IPADDR_USE_STATIC:
        {
            //
            // Set the new address parameters.  This will change the address
            // configuration in lwIP, and if necessary, will reset any links
            // that are active.  This is valid for all three modes.
            //
            netif_set_addr(&lwip_netif, &ip_addr, &net_mask, &gw_addr);

            //
            // If we are going to DHCP mode, then start the DHCP server now.
            //
#if LWIP_DHCP
            if(ulIPMode == IPADDR_USE_DHCP)
            {
                dhcp_start(&lwip_netif);
            }
#endif

            //
            // If we are going to AutoIP mode, then start the AutoIP process
            // now.
            //
#if LWIP_AUTOIP
            if(ulIPMode == IPADDR_USE_AUTOIP)
            {
                autoip_start(&lwip_netif);
            }
#endif
            //
            // Save the new mode.
            //
            g_ulIPMode = ulIPMode;

            //
            // And we're done.
            //
            break;
        }

        //
        // DHCP (with AutoIP fallback).
        //
#if LWIP_DHCP
        case IPADDR_USE_DHCP:
        {
            //
            // If we are going to static IP addressing, then disable DHCP and
            // force the new static IP address.
            //
            if(ulIPMode == IPADDR_USE_STATIC)
            {
                dhcp_stop(&lwip_netif);
                netif_set_addr(&lwip_netif, &ip_addr, &net_mask, &gw_addr);
            }

            //
            // If we are going to AUTO IP addressing, then disable DHCP, set
            // the default addresses, and start AutoIP.
            //
#if LWIP_AUTOIP
            else if(ulIPMode == IPADDR_USE_AUTOIP)
            {
                dhcp_stop(&lwip_netif);
                netif_set_addr(&lwip_netif, &ip_addr, &net_mask, &gw_addr);
                autoip_start(&lwip_netif);
            }
#endif
            break;
        }
#endif

        //
        // AUTOIP
        //
#if LWIP_AUTOIP
        case IPADDR_USE_AUTOIP:
        {
            //
            // If we are going to static IP addressing, then disable AutoIP and
            // force the new static IP address.
            //
            if(ulIPMode == IPADDR_USE_STATIC)
            {
                autoip_stop(&lwip_netif);
                netif_set_addr(&lwip_netif, &ip_addr, &net_mask, &gw_addr);
            }

            //
            // If we are going to DHCP addressing, then disable AutoIP, set the
            // default addresses, and start dhcp.
            //
#if LWIP_DHCP
            else if(ulIPMode == IPADDR_USE_AUTOIP)
            {
                autoip_stop(&lwip_netif);
                netif_set_addr(&lwip_netif, &ip_addr, &net_mask, &gw_addr);
                dhcp_start(&lwip_netif);
            }
#endif
            break;
        }
#endif
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
