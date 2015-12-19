//*****************************************************************************
//
// uartstdio.c - Utility driver to provide simple UART console functions.
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

#include <stdarg.h>
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_uart.h"
#include "hw_ints.h"
#include "uart.h"
#include "debug.h"
#include "rom.h"
#include "rom_map.h"
#include "sysctl.h"
#include "interrupt.h"
#include "uartstdio.h"

//*****************************************************************************
//
//! \addtogroup utilities_api
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// This module offers two distinct modes of operation, normal mode and
// buffered mode.
//
// In normal mode, multiple instances may be opened.  Calls to UARTgets() will
// block until a complete string (terminated using a newline or ESC character)
// is received via the UART and calls to UARTprintf() will, similarly, block
// until the data has been written.
//
// In buffered mode, only a single instance of the interface may be opened.
// Calls to UARTprintf() copy data into the local transmit buffer and return
// immediately.  If the buffer does not contain sufficient space for the data,
// as much as possible is written and the remainder is discarded.  This
// behaviour is intended to support use of debug printing from interrupt
// service routines and it is, therefore, the responsibility of the caller
// to ensure that they do not pass so much data that the buffer overflows.  If
// this behaviour is not desired, UARTFlushTx() may be called to ensure that
// the transmit buffer is emptied prior to adding new data via UARTprintf().
//
// Operation of UARTgets() in buffered mode is also somewhat different.  A
// local receive buffer gathers characters from the UART under interrupt
// control and handles echo processing.  Calls to UARTgets() will return
// characters from this buffer and, as in the unbuffered case, block until a
// line termination character is received.  To allow non-blocking operation,
// function UARTPeek() can be called to check for the presence of a line
// termination character prior to calling UARTgets().
//
// Additionally, when using buffered mode, the application making use of the
// interface must make sure that function UARTStdioIntHandler is installed as
// interrupt handler for the desired UART prior to calling UARTStdioInit().
//
//*****************************************************************************

//*****************************************************************************
//
// If buffered mode is defined, set aside RX and TX buffers and read/write
// pointers to control them.
//
//*****************************************************************************
#ifdef UART_BUFFERED

//
// Output ring buffer.  Buffer is full if g_ulUartReadIndex is one ahead of
// g_ulUartWriteIndex.  Buffer is empty if the two indices are the same.
//
unsigned char g_pcUartTxBuffer[UART_TX_BUFFER_SIZE];
volatile unsigned long g_ulUartTxWriteIndex = 0;
volatile unsigned long g_ulUartTxReadIndex = 0;

//
// Input ring buffer.  Buffer is full if g_ulUartTxReadIndex is one ahead of
// g_ulUartTxWriteIndex.  Buffer is empty if the two indices are the same.
//
unsigned char g_pcUartRxBuffer[UART_TX_BUFFER_SIZE];
volatile unsigned long g_ulUartRxWriteIndex = 0;
volatile unsigned long g_ulUartRxReadIndex = 0;

//
// Macros to determine number of free and used bytes in the transmit buffer.
//
#define TX_BUFFER_USED          (GetBufferCount(&g_ulUartTxReadIndex,  \
                                                &g_ulUartTxWriteIndex, \
                                                UART_TX_BUFFER_SIZE))
#define TX_BUFFER_FREE          (UART_TX_BUFFER_SIZE - TX_BUFFER_USED)
#define TX_BUFFER_EMPTY         (IsBufferEmpty(&g_ulUartTxReadIndex,   \
                                               &g_ulUartTxWriteIndex))
#define TX_BUFFER_FULL          (IsBufferFull(&g_ulUartTxReadIndex,  \
                                              &g_ulUartTxWriteIndex, \
                                              UART_TX_BUFFER_SIZE))
#define ADVANCE_TX_BUFFER_INDEX(Index) \
                                (Index) = ((Index) + 1) % UART_TX_BUFFER_SIZE

//
// Macros to determine number of free and used bytes in the receive buffer.
//
#define RX_BUFFER_USED          (GetBufferCount(&g_ulUartRxReadIndex,  \
                                                &g_ulUartRxWriteIndex, \
                                                UART_RX_BUFFER_SIZE))
#define RX_BUFFER_FREE          (UART_RX_BUFFER_SIZE - RX_BUFFER_USED)
#define RX_BUFFER_EMPTY         (IsBufferEmpty(&g_ulUartRxReadIndex,   \
                                               &g_ulUartRxWriteIndex))
#define RX_BUFFER_FULL          (IsBufferFull(&g_ulUartRxReadIndex,  \
                                              &g_ulUartRxWriteIndex, \
                                              UART_RX_BUFFER_SIZE))
#define ADVANCE_RX_BUFFER_INDEX(Index) \
                                (Index) = ((Index) + 1) % UART_RX_BUFFER_SIZE
#endif

//*****************************************************************************
//
// The base address of the chosen UART.
//
//*****************************************************************************
static unsigned long g_ulBase = 0;

//*****************************************************************************
//
// A mapping from an integer between 0 and 15 to its ASCII character
// equivalent.
//
//*****************************************************************************
static const char * const g_pcHex = "0123456789abcdef";

//*****************************************************************************
//
// The list of possible base addresses for the console UART.
//
//*****************************************************************************
static unsigned long g_ulUartBase[3] =
{
    UART0_BASE, UART1_BASE, UART2_BASE
};

#ifdef UART_BUFFERED
//*****************************************************************************
//
// The list of possible interrupts for the console UART.
//
//*****************************************************************************
static unsigned long g_ulUartInt[3] =
{
    INT_UART0, INT_UART1, INT_UART2
};

//*****************************************************************************
//
// The port number in use.
//
//*****************************************************************************
static unsigned long g_ulPortNum;
#endif

//*****************************************************************************
//
// The list of UART peripherals.
//
//*****************************************************************************
static unsigned long g_ulUartPeriph[3] =
{
    SYSCTL_PERIPH_UART0, SYSCTL_PERIPH_UART1, SYSCTL_PERIPH_UART2
};

//*****************************************************************************
//
//! \internal
//!
//! Determines whether the ring buffer whose pointers and size are provided
//! is full or not.
//!
//! \param pulRead points to the read index for the buffer.
//! \param pulWrite points to the write index for the buffer.
//! \param ulSize is the size of the buffer in bytes.
//!
//! This function is used to determine whether or not a given ring buffer is
//! full.  The structure is specifically to ensure that we do not see
//! warnings from the compiler related to the order of volatile accesses
//! being undefined.
//!
//! \return Returns \b true if the buffer is full or \b false otherwise.
//
//*****************************************************************************
#ifdef UART_BUFFERED
static tBoolean
IsBufferFull(unsigned long volatile *pulRead,
             unsigned long volatile *pulWrite, unsigned long ulSize)
{
    unsigned long ulWrite;
    unsigned long ulRead;

    ulWrite = *pulWrite;
    ulRead = *pulRead;

    return((((ulWrite + 1) % ulSize) ==  ulRead) ? true : false);
}
#endif

//*****************************************************************************
//
//! \internal
//!
//! Determines whether the ring buffer whose pointers and size are provided
//! is empty or not.
//!
//! \param pulRead points to the read index for the buffer.
//! \param pulWrite points to the write index for the buffer.
//!
//! This function is used to determine whether or not a given ring buffer is
//! empty.  The structure is specifically to ensure that we do not see
//! warnings from the compiler related to the order of volatile accesses
//! being undefined.
//!
//! \return Returns \b true if the buffer is empty or \b false otherwise.
//
//*****************************************************************************
#ifdef UART_BUFFERED
static tBoolean
IsBufferEmpty(unsigned long volatile *pulRead,
              unsigned long volatile *pulWrite)
{
    unsigned long ulWrite;
    unsigned long ulRead;

    ulWrite = *pulWrite;
    ulRead = *pulRead;

    return((ulWrite  == ulRead) ? true : false);
}
#endif

//*****************************************************************************
//
//! \internal
//!
//! Determines the number of bytes of data contained in a ring buffer.
//!
//! \param pulRead points to the read index for the buffer.
//! \param pulWrite points to the write index for the buffer.
//! \param ulSize is the size of the buffer in bytes.
//!
//! This function is used to determine how many bytes of data a given ring
//! buffer currently contains.  The structure is specifically to ensure that
//! we do not see warnings from the compiler related to the order of volatile
//! accesses being undefined.
//!
//! \return Returns the number of bytes of data currently in the buffer.
//
//*****************************************************************************
#ifdef UART_BUFFERED
static unsigned long
GetBufferCount(unsigned long volatile *pulRead,
               unsigned long volatile *pulWrite,  unsigned long ulSize)
{
    unsigned long ulWrite;
    unsigned long ulRead;

    ulWrite = *pulWrite;
    ulRead = *pulRead;

    return((ulWrite >= ulRead) ? (ulWrite - ulRead) :
                                 (ulSize - (ulRead - ulWrite)));
}
#endif

//*****************************************************************************
//
// Take as many bytes from the transmit buffer as we have space for and move
// them into the UART transmit FIFO.
//
//*****************************************************************************
#ifdef UART_BUFFERED
static void
UARTPrimeTransmit(unsigned long ulBase)
{
    //
    // Do we have any data to transmit?
    //
    if(!TX_BUFFER_EMPTY)
    {
        //
        // Disable the UART interrupt. If we don't do this there is a race
        // condition which can cause the read index to be corrupted.
        //
        MAP_IntDisable(g_ulUartInt[g_ulPortNum]);

        //
        // Yes - take some characters out of the transmit buffer and feed
        // them to the UART transmit FIFO.
        //
        while(MAP_UARTSpaceAvail(ulBase) && !TX_BUFFER_EMPTY)
        {
            MAP_UARTCharPutNonBlocking(ulBase,
                                       g_pcUartTxBuffer[g_ulUartTxReadIndex]);
            ADVANCE_TX_BUFFER_INDEX(g_ulUartTxReadIndex);
        }

        //
        // Reenable the UART interrupt.
        //
        MAP_IntEnable(g_ulUartInt[g_ulPortNum]);
    }
}
#endif

//*****************************************************************************
//
//! Initializes the UART console.
//!
//! \param ulPortNum is the number of UART port to use for the serial console
//! (0-2)
//!
//! This function will initialize the specified serial port to be used as a
//! serial console.  The serial parameters will be set to 115200, 8-N-1.
//!
//! This function must be called prior to using any of the other UART console
//! functions: UARTprintf() or UARTgets().  In order for this function to work
//! correctly, SysCtlClockSet() must be called prior to calling this function.
//!
//! It is assumed that the caller has previously configured the relevant UART
//! pins for operation as a UART rather than as GPIOs.
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return None.
//
//*****************************************************************************
void
UARTStdioInit(unsigned long ulPortNum)
{
    //
    // Check the arguments.
    //
    ASSERT((ulPortNum == 0) || (ulPortNum == 1) ||
           (ulPortNum == 2));

#ifdef UART_BUFFERED
    //
    // In buffered mode, we only allow a single instance to be opened.
    //
    ASSERT(g_ulBase == 0);
#endif

    //
    // Check to make sure the UART peripheral is present.
    //
    if(!MAP_SysCtlPeripheralPresent(g_ulUartPeriph[ulPortNum]))
    {
        return;
    }

    //
    // Select the base address of the UART.
    //
    g_ulBase = g_ulUartBase[ulPortNum];

    //
    // Enable the UART peripheral for use.
    //
    MAP_SysCtlPeripheralEnable(g_ulUartPeriph[ulPortNum]);

    //
    // Configure the UART for 115200, n, 8, 1
    //
    MAP_UARTConfigSetExpClk(g_ulBase, MAP_SysCtlClockGet(), 115200,
                            (UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_WLEN_8));

#ifdef UART_BUFFERED
    //
    // Set the UART to interrupt whenever the TX FIFO is almost empty or
    // when any character is received.
    //
    MAP_UARTFIFOLevelSet(g_ulBase, UART_FIFO_TX1_8, UART_FIFO_RX1_8);

    //
    // Flush both the buffers.
    //
    UARTFlushRx();
    UARTFlushTx(true);

    //
    // Remember which interrupt we are dealing with.
    //
    g_ulPortNum = ulPortNum;

    //
    // We are configured for buffered output so enable the master interrupt
    // for this UART and the receive interrupts.  We don't actually enable the
    // transmit interrupt in the UART itself until some data has been placed
    // in the transmit buffer.
    //
    MAP_UARTIntDisable(g_ulBase, 0xFFFFFFFF);
    MAP_UARTIntEnable(g_ulBase, UART_INT_RX | UART_INT_RT);
    MAP_IntEnable(g_ulUartInt[ulPortNum]);
#endif

    //
    // Enable the UART operation.
    //
    MAP_UARTEnable(g_ulBase);
}

//*****************************************************************************
//
//! Writes a string of characters to the UART output.
//!
//! \param pcBuf points to a buffer containing the string to transmit.
//! \param ulLen is the length of the string to transmit.
//!
//! This function will transmit the string to the UART output.  The number of
//! characters transmitted is determined by the \e ulLen parameter.  This
//! function does no interpretation or translation of any characters.  Since
//! the output is sent to a UART, any LF (/n) characters encountered will be
//! replaced with a CRLF pair.
//!
//! Besides using the \e ulLen parameter to stop transmitting the string, if a
//! null character (0) is encountered, then no more characters will be
//! transmitted and the function will return.
//!
//! In non-buffered mode, this function is blocking and will not return until
//! all the characters have been written to the output FIFO.  In buffered mode,
//! the characters are written to the UART transmit buffer and the call returns
//! immediately.  If insufficient space remains in the transmit buffer,
//! additional characters are discarded.
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return Returns the count of characters written.
//
//*****************************************************************************
static int
UARTwrite(const char *pcBuf, unsigned long ulLen)
{
#ifdef UART_BUFFERED
    unsigned int uIdx;

    //
    // Check for valid arguments.
    //
    ASSERT(pcBuf != 0);
    ASSERT(g_ulBase != 0);

    //
    // Send the characters
    //
    for(uIdx = 0; uIdx < ulLen; uIdx++)
    {
        //
        // If the character to the UART is \n, then add a \r before it so that
        // \n is translated to \n\r in the output.
        //
        if(pcBuf[uIdx] == '\n')
        {
            if(!TX_BUFFER_FULL)
            {
                g_pcUartTxBuffer[g_ulUartTxWriteIndex] = '\r';
                ADVANCE_TX_BUFFER_INDEX(g_ulUartTxWriteIndex);
            }
            else
            {
                //
                // Buffer is full - discard remaining characters and return.
                //
                break;
            }
        }

        //
        // Send the character to the UART output.
        //
        if(!TX_BUFFER_FULL)
        {
            g_pcUartTxBuffer[g_ulUartTxWriteIndex] = pcBuf[uIdx];
            ADVANCE_TX_BUFFER_INDEX(g_ulUartTxWriteIndex);
        }
        else
        {
            //
            // Buffer is full - discard remaining characters and return.
            //
            break;
        }
    }

    //
    // If we have anything in the buffer, make sure that the UART is set
    // up to transmit it.
    //
    if(!TX_BUFFER_EMPTY)
    {
        UARTPrimeTransmit(g_ulBase);
        MAP_UARTIntEnable(g_ulBase, UART_INT_TX);
    }

    //
    // Return the number of characters written.
    //
    return(uIdx);
#else
    unsigned int uIdx;

    //
    // Check for valid UART base address, and valid arguments.
    //
    ASSERT(g_ulBase != 0);
    ASSERT(pcBuf != 0);

    //
    // Send the characters
    //
    for(uIdx = 0; uIdx < ulLen; uIdx++)
    {
        //
        // If the character to the UART is \n, then add a \r before it so that
        // \n is translated to \n\r in the output.
        //
        if(pcBuf[uIdx] == '\n')
        {
            MAP_UARTCharPut(g_ulBase, '\r');
        }
		
        //
        // Send the character to the UART output.
        //
		MAP_UARTCharPut(g_ulBase, pcBuf[uIdx]);
    }

    //
    // Return the number of characters written.
    //
    return(uIdx);
#endif
}

//*****************************************************************************
//
//! A simple UART based get string function, with some line processing.
//!
//! \param pcBuf points to a buffer for the incoming string from the UART.
//! \param ulLen is the length of the buffer for storage of the string,
//! including the trailing 0.
//!
//! This function will receive a string from the UART input and store the
//! characters in the buffer pointed to by \e pcBuf.  The characters will
//! continue to be stored until a termination character is received.  The
//! termination characters are CR, LF, or ESC.  A CRLF pair is treated as a
//! single termination character.  The termination characters are not stored in
//! the string.  The string will be terminated with a 0 and the function will
//! return.
//!
//! In both buffered and unbuffered modes, this function will block until
//! a termination character is received.  If non-blocking operation is required
//! in buffered mode, a call to UARTPeek() may be made to determine whether
//! a termination character already exists in the receive buffer prior to
//! calling UARTgets().
//!
//! Since the string will be null terminated, the user must ensure that the
//! buffer is sized to allow for the additional null character.
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return Returns the count of characters that were stored, not including
//! the trailing 0.
//
//*****************************************************************************
int
UARTgets(char *pcBuf, unsigned long ulLen)
{
#ifdef UART_BUFFERED
    unsigned long ulCount = 0;
    char cChar;

    //
    // Check the arguments.
    //
    ASSERT(pcBuf != 0);
    ASSERT(ulLen != 0);
    ASSERT(g_ulBase != 0);

    //
    // Adjust the length back by 1 to leave space for the trailing
    // null terminator.
    //
    ulLen--;

    //
    // Process characters until a newline is received.
    //
    while(1)
    {
        //
        // Read the next character from the receive buffer.
        //
        if(!RX_BUFFER_EMPTY)
        {
            cChar = g_pcUartRxBuffer[g_ulUartRxReadIndex];
            ADVANCE_RX_BUFFER_INDEX(g_ulUartRxReadIndex);

            //
            // See if a newline or escape character was received.
            //
            if((cChar == '\r') || (cChar == '\n') || (cChar == 0x1b))
            {
                //
                // Stop processing the input and end the line.
                //
                break;
            }

            //
            // Process the received character as long as we are not at the end
            // of the buffer.  If the end of the buffer has been reached then
            // all additional characters are ignored until a newline is
            // received.
            //
            if(ulCount < ulLen)
            {
                //
                // Store the character in the caller supplied buffer.
                //
                pcBuf[ulCount] = cChar;

                //
                // Increment the count of characters received.
                //
                ulCount++;
            }
        }
    }

    //
    // Add a null termination to the string.
    //
    pcBuf[ulCount] = 0;

    //
    // Return the count of chars in the buffer, not counting the trailing 0.
    //
    return(ulCount);
#else
    unsigned long ulCount = 0;
    char cChar;
    static char bLastWasCR = 0;

    //
    // Check the arguments.
    //
    ASSERT(pcBuf != 0);
    ASSERT(ulLen != 0);
    ASSERT(g_ulBase != 0);

    //
    // Adjust the length back by 1 to leave space for the trailing
    // null terminator.
    //
    ulLen--;

    //
    // Process characters until a newline is received.
    //
    while(1)
    {
        //
        // Read the next character from the console.
        //
        cChar = MAP_UARTCharGet(g_ulBase);

        //
        // See if the backspace key was pressed.
        //
        if(cChar == '\b')
        {
            //
            // If there are any characters already in the buffer, then delete
            // the last.
            //
            if(ulCount)
            {
                //
                // Rub out the previous character.
                //
                UARTwrite("\b \b", 3);

                //
                // Decrement the number of characters in the buffer.
                //
                ulCount--;
            }

            //
            // Skip ahead to read the next character.
            //
            continue;
        }

        //
        // If this character is LF and last was CR, then just gobble up the
        // character because the EOL processing was taken care of with the CR.
        //
        if((cChar == '\n') && bLastWasCR)
        {
            bLastWasCR = 0;
            continue;
        }

        //
        // See if a newline or escape character was received.
        //
        if((cChar == '\r') || (cChar == '\n') || (cChar == 0x1b))
        {
            //
            // If the character is a CR, then it may be followed by a LF which
            // should be paired with the CR.  So remember that a CR was
            // received.
            //
            if(cChar == '\r')
            {
                bLastWasCR = 1;
            }

            //
            // Stop processing the input and end the line.
            //
            break;
        }

        //
        // Process the received character as long as we are not at the end of
        // the buffer.  If the end of the buffer has been reached then all
        // additional characters are ignored until a newline is received.
        //
        if(ulCount < ulLen)
        {
            //
            // Store the character in the caller supplied buffer.
            //
            pcBuf[ulCount] = cChar;

            //
            // Increment the count of characters received.
            //
            ulCount++;

            //
            // Reflect the character back to the user.
            //
            MAP_UARTCharPut(g_ulBase, cChar);
        }
    }

    //
    // Add a null termination to the string.
    //
    pcBuf[ulCount] = 0;

    //
    // Send a CRLF pair to the terminal to end the line.
    //
    UARTwrite("\r\n", 2);

    //
    // Return the count of chars in the buffer, not counting the trailing 0.
    //
    return(ulCount);
#endif
}

//*****************************************************************************
//
//! A simple UART based printf function supporting \%c, \%d, \%p, \%s, \%u,
//! \%x, and \%X.
//!
//! \param pcString is the format string.
//! \param ... are the optional arguments, which depend on the contents of the
//! format string.
//!
//! This function is very similar to the C library <tt>fprintf()</tt> function.
//! All of its output will be sent to the UART.  Only the following formatting
//! characters are supported:
//!
//! - \%c to print a character
//! - \%d to print a decimal value
//! - \%s to print a string
//! - \%u to print an unsigned decimal value
//! - \%x to print a hexadecimal value using lower case letters
//! - \%X to print a hexadecimal value using lower case letters (not upper case
//! letters as would typically be used)
//! - \%p to print a pointer as a hexadecimal value
//! - \%\% to print out a \% character
//!
//! For \%s, \%d, \%u, \%p, \%x, and \%X, an optional number may reside
//! between the \% and the format character, which specifies the minimum number
//! of characters to use for that value; if preceeded by a 0 then the extra
//! characters will be filled with zeros instead of spaces.  For example,
//! ``\%8d'' will use eight characters to print the decimal value with spaces
//! added to reach eight; ``\%08d'' will use eight characters as well but will
//! add zeroes instead of spaces.
//!
//! The type of the arguments after \e pcString must match the requirements of
//! the format string.  For example, if an integer was passed where a string
//! was expected, an error of some kind will most likely occur.
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return None.
//
//*****************************************************************************
void
UARTprintf(const char *pcString, ...)
{
    unsigned long ulIdx, ulValue, ulPos, ulCount, ulBase, ulNeg;
    char *pcStr, pcBuf[16], cFill;
    va_list vaArgP;

    //
    // Check the arguments.
    //
    ASSERT(pcString != 0);

    //
    // Start the varargs processing.
    //
    va_start(vaArgP, pcString);

    //
    // Loop while there are more characters in the string.
    //
    while(*pcString)
    {
        //
        // Find the first non-% character, or the end of the string.
        //
        for(ulIdx = 0; (pcString[ulIdx] != '%') && (pcString[ulIdx] != '\0');
            ulIdx++)
        {
        }

        //
        // Write this portion of the string.
        //
        UARTwrite(pcString, ulIdx);

        //
        // Skip the portion of the string that was written.
        //
        pcString += ulIdx;

        //
        // See if the next character is a %.
        //
        if(*pcString == '%')
        {
            //
            // Skip the %.
            //
            pcString++;

            //
            // Set the digit count to zero, and the fill character to space
            // (i.e. to the defaults).
            //
            ulCount = 0;
            cFill = ' ';

            //
            // It may be necessary to get back here to process more characters.
            // Goto's aren't pretty, but effective.  I feel extremely dirty for
            // using not one but two of the beasts.
            //
again:

            //
            // Determine how to handle the next character.
            //
            switch(*pcString++)
            {
                //
                // Handle the digit characters.
                //
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    //
                    // If this is a zero, and it is the first digit, then the
                    // fill character is a zero instead of a space.
                    //
                    if((pcString[-1] == '0') && (ulCount == 0))
                    {
                        cFill = '0';
                    }

                    //
                    // Update the digit count.
                    //
                    ulCount *= 10;
                    ulCount += pcString[-1] - '0';

                    //
                    // Get the next character.
                    //
                    goto again;
                }

                //
                // Handle the %c command.
                //
                case 'c':
                {
                    //
                    // Get the value from the varargs.
                    //
                    ulValue = va_arg(vaArgP, unsigned long);

                    //
                    // Print out the character.
                    //
                    UARTwrite((char *)&ulValue, 1);

                    //
                    // This command has been handled.
                    //
                    break;
                }

                //
                // Handle the %d command.
                //
                case 'd':
                {
                    //
                    // Get the value from the varargs.
                    //
                    ulValue = va_arg(vaArgP, unsigned long);

                    //
                    // Reset the buffer position.
                    //
                    ulPos = 0;

                    //
                    // If the value is negative, make it positive and indicate
                    // that a minus sign is needed.
                    //
                    if((long)ulValue < 0)
                    {
                        //
                        // Make the value positive.
                        //
                        ulValue = -(long)ulValue;

                        //
                        // Indicate that the value is negative.
                        //
                        ulNeg = 1;
                    }
                    else
                    {
                        //
                        // Indicate that the value is positive so that a minus
                        // sign isn't inserted.
                        //
                        ulNeg = 0;
                    }

                    //
                    // Set the base to 10.
                    //
                    ulBase = 10;

                    //
                    // Convert the value to ASCII.
                    //
                    goto convert;
                }

                //
                // Handle the %s command.
                //
                case 's':
                {
                    //
                    // Get the string pointer from the varargs.
                    //
                    pcStr = va_arg(vaArgP, char *);

                    //
                    // Determine the length of the string.
                    //
                    for(ulIdx = 0; pcStr[ulIdx] != '\0'; ulIdx++)
                    {
                    }

                    //
                    // Write the string.
                    //
                    UARTwrite(pcStr, ulIdx);

                    //
                    // Write any required padding spaces
                    //
                    if(ulCount > ulIdx)
                    {
                        ulCount -= ulIdx;
                        while(ulCount--)
                        {
                            UARTwrite(" ", 1);
                        }
                    }
                    //
                    // This command has been handled.
                    //
                    break;
                }

                //
                // Handle the %u command.
                //
                case 'u':
                {
                    //
                    // Get the value from the varargs.
                    //
                    ulValue = va_arg(vaArgP, unsigned long);

                    //
                    // Reset the buffer position.
                    //
                    ulPos = 0;

                    //
                    // Set the base to 10.
                    //
                    ulBase = 10;

                    //
                    // Indicate that the value is positive so that a minus sign
                    // isn't inserted.
                    //
                    ulNeg = 0;

                    //
                    // Convert the value to ASCII.
                    //
                    goto convert;
                }

                //
                // Handle the %x and %X commands.  Note that they are treated
                // identically; i.e. %X will use lower case letters for a-f
                // instead of the upper case letters is should use.  We also
                // alias %p to %x.
                //
                case 'x':
                case 'X':
                case 'p':
                {
                    //
                    // Get the value from the varargs.
                    //
                    ulValue = va_arg(vaArgP, unsigned long);

                    //
                    // Reset the buffer position.
                    //
                    ulPos = 0;

                    //
                    // Set the base to 16.
                    //
                    ulBase = 16;

                    //
                    // Indicate that the value is positive so that a minus sign
                    // isn't inserted.
                    //
                    ulNeg = 0;

                    //
                    // Determine the number of digits in the string version of
                    // the value.
                    //
convert:
                    for(ulIdx = 1;
                        (((ulIdx * ulBase) <= ulValue) &&
                         (((ulIdx * ulBase) / ulBase) == ulIdx));
                        ulIdx *= ulBase, ulCount--)
                    {
                    }

                    //
                    // If the value is negative, reduce the count of padding
                    // characters needed.
                    //
                    if(ulNeg)
                    {
                        ulCount--;
                    }

                    //
                    // If the value is negative and the value is padded with
                    // zeros, then place the minus sign before the padding.
                    //
                    if(ulNeg && (cFill == '0'))
                    {
                        //
                        // Place the minus sign in the output buffer.
                        //
                        pcBuf[ulPos++] = '-';

                        //
                        // The minus sign has been placed, so turn off the
                        // negative flag.
                        //
                        ulNeg = 0;
                    }

                    //
                    // Provide additional padding at the beginning of the
                    // string conversion if needed.
                    //
                    if((ulCount > 1) && (ulCount < 16))
                    {
                        for(ulCount--; ulCount; ulCount--)
                        {
                            pcBuf[ulPos++] = cFill;
                        }
                    }

                    //
                    // If the value is negative, then place the minus sign
                    // before the number.
                    //
                    if(ulNeg)
                    {
                        //
                        // Place the minus sign in the output buffer.
                        //
                        pcBuf[ulPos++] = '-';
                    }

                    //
                    // Convert the value into a string.
                    //
                    for(; ulIdx; ulIdx /= ulBase)
                    {
                        pcBuf[ulPos++] = g_pcHex[(ulValue / ulIdx) % ulBase];
                    }

                    //
                    // Write the string.
                    //
                    UARTwrite(pcBuf, ulPos);

                    //
                    // This command has been handled.
                    //
                    break;
                }

                //
                // Handle the %% command.
                //
                case '%':
                {
                    //
                    // Simply write a single %.
                    //
                    UARTwrite(pcString - 1, 1);

                    //
                    // This command has been handled.
                    //
                    break;
                }

                //
                // Handle all other commands.
                //
                default:
                {
                    //
                    // Indicate an error.
                    //
                    UARTwrite("ERROR", 5);

                    //
                    // This command has been handled.
                    //
                    break;
                }
            }
        }
    }

    //
    // End the varargs processing.
    //
    va_end(vaArgP);
}

//*****************************************************************************
//
//! Looks ahead in the receive buffer for a particular character.
//!
//! \param ucChar is the character that is to be searched for.
//!
//! This function, available only when the module is built to operate in
//! buffered mode using \b UART_BUFFERED, may be used to look ahead in the
//! receive buffer for a particular character and report its position if found.
//! It is typically used to determine whether a complete line of user input is
//! available, in which case ucChar should be set to CR ('\\r') which is used
//! as the line end marker in the receive buffer.
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return Returns -1 to indicate that the requested character does not exist
//! in the receive buffer.  Returns a non-negative number if the character was
//! found in which case the value represents the position of the first instance
//! of \e ucChar relative to the receive buffer read pointer.
//
//*****************************************************************************
#if defined(UART_BUFFERED) || defined(DOXYGEN)
int
UARTPeek(unsigned char ucChar)
{
    int iCount;
    int iAvail;
    unsigned long ulReadIndex;

    //
    // How many characters are there in the receive buffer?
    //
    iAvail = (int)RX_BUFFER_USED;
    ulReadIndex = g_ulUartRxReadIndex;

    //
    // Check all the unread characters looking for the one passed.
    //
    for(iCount = 0; iCount < iAvail; iCount++)
    {
        if(g_pcUartRxBuffer[ulReadIndex] == ucChar)
        {
            //
            // We found it so return the index
            //
            return(iCount);
        }
        else
        {
            //
            // This one didn't match so move on to the next character.
            //
            ADVANCE_RX_BUFFER_INDEX(ulReadIndex);
        }
    }

    //
    // If we drop out of the loop, we didn't find the character in the receive
    // buffer.
    //
    return(-1);
}
#endif

//*****************************************************************************
//
//! Flushes the receive buffer.
//!
//! This function, available only when the module is built to operate in
//! buffered mode using \b UART_BUFFERED, may be used to discard any data
//! received from the UART but not yet read using UARTgets().
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return None.
//
//*****************************************************************************
#if defined(UART_BUFFERED) || defined(DOXYGEN)
void
UARTFlushRx(void)
{
    unsigned long ulInt;

    //
    // Temporarily turn off interrupts.
    //
    ulInt = IntMasterDisable();

    //
    // Flush the receive buffer.
    //
    g_ulUartRxReadIndex = 0;
    g_ulUartRxWriteIndex = 0;

    //
    // If interrupts were enabled when we turned them off, turn them
    // back on again.
    //
    if(!ulInt)
    {
        IntMasterEnable();
    }
}
#endif

//*****************************************************************************
//
//! Flushes the transmit buffer.
//!
//! \param bDiscard indicates whether any remaining data in the buffer should
//! be discarded (\b true) or transmitted (\b false).
//!
//! This function, available only when the module is built to operate in
//! buffered mode using \b UART_BUFFERED, may be used to flush the transmit
//! buffer, either discarding or transmitting any data received via calls to
//! UARTprintf() that remains untransmitted.  On return, the transmit buffer
//! will be empty.
//!
//! This function is contained in <tt>utils/uartstdio.c</tt>, with
//! <tt>utils/uartstdio.h</tt> containing the API definition for use by
//! applications.
//!
//! \return None.
//
//*****************************************************************************
#if defined(UART_BUFFERED) || defined(DOXYGEN)
void
UARTFlushTx(tBoolean bDiscard)
{
    unsigned long ulInt;

    //
    // Should the remaining data be discarded or transmitted?
    //
    if(bDiscard)
    {
        //
        // The remaining data should be discarded, so temporarily turn off
        // interrupts.
        //
        ulInt = IntMasterDisable();

        //
        // Flush the transmit buffer.
        //
        g_ulUartTxReadIndex = 0;
        g_ulUartTxWriteIndex = 0;

        //
        // If interrupts were enabled when we turned them off, turn them
        // back on again.
        //
        if(!ulInt)
        {
            IntMasterEnable();
        }
    }
    else
    {
        //
        // Wait for all remaining data to be transmitted before returning.
        //
        while(!TX_BUFFER_EMPTY)
        {
        }
    }
}
#endif

//*****************************************************************************
//
// UART transmit interrupt handler used during buffered operation.
//
//*****************************************************************************
#if defined(UART_BUFFERED) || defined(DOXYGEN)
void
UARTStdioIntHandler(void)
{
    unsigned long ulInts;
    char cChar;
    long lChar;
    static tBoolean bLastWasCR = false;

    //
    // Get and clear the current interrupt source(s)
    //
    ulInts = MAP_UARTIntStatus(g_ulBase, true);
    MAP_UARTIntClear(g_ulBase, ulInts);

    //
    // Are we being interrupted because the TX FIFO has space available?
    //
    if(ulInts & UART_INT_TX)
    {
        //
        // Move as many bytes as we can into the transmit FIFO.
        //
        UARTPrimeTransmit(g_ulBase);

        //
        // If the output buffer is empty, turn off the transmit interrupt.
        //
        if(TX_BUFFER_EMPTY)
        {
            MAP_UARTIntDisable(g_ulBase, UART_INT_TX);
        }
    }

    //
    // Are we being interrupted due to a received character?
    //
    if(ulInts & (UART_INT_RX | UART_INT_RT))
    {
        //
        // Get all the available characters from the UART.
        //
        while(MAP_UARTCharsAvail(g_ulBase))
        {
            //
            // Read a character
            //
            lChar = MAP_UARTCharGetNonBlocking(g_ulBase);
            cChar = (unsigned char)(lChar & 0xFF);

            //
            // Handle backspace by erasing the last character in the buffer.
            //
            if(cChar == '\b')
            {
                //
                // If there are any characters already in the buffer, then
                // delete the last.
                //
                if(!RX_BUFFER_EMPTY)
                {
                    //
                    // Rub out the previous character on the users terminal.
                    //
                    UARTwrite("\b \b", 3);

                    //
                    // Decrement the number of characters in the buffer.
                    //
                    if(g_ulUartRxWriteIndex == 0)
                    {
                        g_ulUartRxWriteIndex = UART_RX_BUFFER_SIZE - 1;
                    }
                    else
                    {
                        g_ulUartRxWriteIndex--;
                    }
                }

                //
                // Skip ahead to read the next character.
                //
                continue;
            }

            //
            // If this character is LF and last was CR, then just gobble up the
            // character since we already echoed the previous CR and we don't
            // want to store 2 characters in the buffer if we don't need to.
            //
            if((cChar == '\n') && bLastWasCR)
            {
                bLastWasCR = false;
                continue;
            }

            //
            // See if a newline or escape character was received.
            //
            if((cChar == '\r') || (cChar == '\n') || (cChar == 0x1b))
            {
                //
                // If the character is a CR, then it may be followed by an LF
                // which should be paired with the CR.  So remember that a CR
                // was received.
                //
                if(cChar == '\r')
                {
                    bLastWasCR = 1;
                }

                //
                // Regardless of the line termination character received,
                // put a CR in the receive buffer as a marker telling
                // UARTgets() where the line ends.  We also send an additional
                // LF to ensure that the local terminal echo receives both CR
                // and LF.
                //
                cChar = '\r';
                UARTwrite("\n", 1);
            }

            //
            // If there is space in the receive buffer, put the character
            // there, otherwise throw it away.
            //
            if(!RX_BUFFER_FULL)
            {
                //
                // Store the new character in the receive buffer
                //
                g_pcUartRxBuffer[g_ulUartRxWriteIndex] =
                    (unsigned char)(lChar & 0xFF);
                ADVANCE_RX_BUFFER_INDEX(g_ulUartRxWriteIndex);

                //
                // Echo it to the transmit buffer so that the user gets
                // some immediate feedback.
                //
                UARTwrite(&cChar, 1);
            }
        }

        //
        // If we wrote anything to the transmit buffer, make sure it actually
        // gets transmitted.
        //
        UARTPrimeTransmit(g_ulBase);
        MAP_UARTIntEnable(g_ulBase, UART_INT_TX);
    }
}
#endif

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
