//*****************************************************************************
//
// uartstdio.h - Prototypes for the UART console functions.
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

#ifndef __UARTSTDIO_H__
#define __UARTSTDIO_H__

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Buffered vs. Unbuffered Operation.
// ----------------------------------
//
// If the label UART_BUFFERED is undefined, this module will build to use
// unbuffered I/O.  In this mode, UARTprintf() will return once all the
// string characters have been written to the UART transmit FIFO and will
// block until this is possible.  Similarly, UARTgets will block until the
// user enters a string terminated by either a CR, LF or ESC character.
//
// If the label UART_BUFFERED is defined, this module will build to use
// buffered I/O.  Traffic to and from the UART is managed under interrupt and
// the data buffered in RAM.  Calls to UARTprintf() will output as many
// characters as are possible to fit into the transmit buffer.  If no space
// is available, the call will discard remaining characters and return
// immediately.  Function UARTgets() will block until a CR, LF or ESC
// character is read from the receive buffer.  Applications may, however,
// call UARTPeek() to determine whether a line end is present prior to calling
// UARTgets() if non-blocking operation is required.  In cases where the
// buffer supplied on UARTgets() fills before a line termination character is
// received, the call will return with a full buffer.
//
// In buffered mode, function UARTFlushTx() may be called to block until all
// previously buffered transmit data has been written to the UART transmit
// FIFO.  UARTFlushRx() may also be used to flush any data in the receive
// buffer.
//
//*****************************************************************************
//#define UART_BUFFERED

//*****************************************************************************
//
// If built for buffered operation, the following labels define the sizes of
// the transmit and receive buffers respectively.
//
//*****************************************************************************

#ifdef UART_BUFFERED
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE     128
#endif
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE     1024
#endif
#endif

//*****************************************************************************
//
// Prototypes for the APIs.
//
//*****************************************************************************
extern void UARTStdioInit(unsigned long ulPort);
extern int  UARTgets(char *pcBuf, unsigned long ulLen);
extern void UARTprintf(const char *pcString, ...);
#ifdef UART_BUFFERED
extern int UARTPeek(unsigned char ucChar);
extern void UARTFlushTx(tBoolean bDiscard);
extern void UARTFlushRx(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // __UARTSTDIO_H__
