//*****************************************************************************
//
// softeeprom_wrapper.c - Example driver for software emulation of EEPROM wrapper.
//
// Copyright (c) 2005-2013 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//   Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
//*****************************************************************************

#include "hw_types.h"
#include "softeeprom.h"
#include "softeeprom_wrapper.h"
#include "uartstdio.h"

//*****************************************************************************
//
//! Writes an amount of data to specific EEPROM address.
//!
//! This function writes the specified data to the specified address
//! in the EEPROM page.
//!
//! \param usAddress is destination address where the data will be written into
//                      the EEPROM (0 - MAX_SOFTEEPROM_IDS*2).
//!
//! \param usSize is the data size to be written.
//!
//! \param pucData is the pointer to the buffer containing the data to be
//!                 written into the EEPROM.
//!
//! \return A value of 0 indicates that the write was successful.  A non-zero
//! value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROM_WrapperWrite(unsigned short usAddress, unsigned short usSize,
						const unsigned char *pucData)
{
	long lReturn = 0;
	tBoolean bFound;
	unsigned short usData;

	// check if address range is valid
	if(((usAddress + usSize)/2) > MAX_SOFTEEPROM_IDS)
	{
		return ERR_PAGE_RANGE;
	}

	while(usSize>0)
	{
		// odd address
		if(((usAddress % 2) == 1) || (usSize == 1))
		{
			// give initial value to data
			usData = 0xFFFF;

			// first read the word from the address
			lReturn = SoftEEPROMRead(usAddress/2, &usData, &bFound);

			// form the new data
			if((usAddress % 2) == 1)
			{
				usData = (usData & 0x00FF) | (((unsigned short)(*(pucData++))) << 8);
			}
			else
			{
				usData = (usData & 0xFF00) | ((unsigned short)(*(pucData++)));
			}

			// write the data
			lReturn = SoftEEPROMWrite(usAddress/2, usData);
			if(lReturn != 0)
			{
				// abort and return error code
				break;
			}

			// decrement size to read and increment address
			usAddress += 1;
			usSize -= 1;
		}
		else // even address
		{
			// form the data
			usData = ((unsigned short) pucData[0]) | (((unsigned short) pucData[1]) << 8);

			// write the data
			lReturn = SoftEEPROMWrite(usAddress/2, usData);
			if(lReturn != 0)
			{
				// abort and return error code
				break;
			}

			// decrement size to read and increment address
			pucData += 2;
			usAddress += 2;
			usSize -= 2;
		}
	}
	
	if(lReturn != 0)
    {
        //
        // Indicate that an error occurred during a write operation.
        //
        UARTprintf("\rAn error occurred during a soft EEPROM write "          \
                   "operation");
        //
        // Output the error to the UART and quit.
        //
        OutputErrorAndQuit(lReturn);
    }

    return(lReturn);

}


//*****************************************************************************
//
//! Reads an amount of data from a specific EEPROM address.
//!
//! \param usAddress is destination address where the data will be read from
//                      the EEPROM (0 - MAX_SOFTEEPROM_IDS*2).
//!
//! \param usSize is the data size to be read.
//!
//! \param pucData is the pointer to the buffer containing the data to be
//!                 read from the EEPROM.
//!
//! \return A value of 0 indicates that the read was successful.  A non-zero
//! value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROM_WrapperRead(unsigned short usAddress, unsigned short usSize,
						unsigned char *pucData)
{
	long lReturn = 0;
	tBoolean bFound;
	unsigned short usData = 0;

	// check if address range is valid
	if(((usAddress + usSize)/2) > MAX_SOFTEEPROM_IDS)
	{
		return ERR_PAGE_RANGE;
	}

	while(usSize>0)
	{
		// read two bytes at a time
		lReturn = SoftEEPROMRead(usAddress/2, &usData, &bFound);
		//if((lReturn == 0) && (bFound == true))
		{
			// odd address
			if((usAddress % 2) == 1)
			{
				// read only the high byte
				*(pucData++) = (unsigned char) (usData >> 8);

				// decrement size to read and increment address
				usAddress += 1;
				usSize -= 1;
			}
			else // even address
			{
				if(usSize > 1)
				{
					// write both bytes to buffer
					*(pucData++) = (unsigned char) (usData & 0x00FF);
					*(pucData++) = (unsigned char) (usData >> 8);

					// decrement size to read and increment address
					usAddress += 2;
					usSize -= 2;
				}
				else
				{
					// this is the last byte
					*(pucData) = (unsigned char) (usData & 0x00FF);
					// decrement size
					usSize--;
				}
			}
		}
		/*
		else
		{
			// abort and return the error code from SoftEEPROM Library
			break;
		}
		*/
	}

	if(lReturn != 0)
    {
		//
        // Indicate that an error occurred during a read operation.
        //
        UARTprintf("\rAn error occurred during a soft EEPROM read "          \
                   "operation");
		//
        // Output the error to the UART and quit.
        //
        OutputErrorAndQuit(lReturn);
    }
	
    return(lReturn);
}


//*****************************************************************************
//
//! Clears the EEPROM contents.
//!
//! \return A value of 0 indicates that the clear operation was successful.  A
//! non-zero value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROM_WrapperClear(void)
{
	return(SoftEEPROMClear());
}

long SoftEEPROM_WrapperInit(unsigned long ulStart, unsigned long ulEnd,
                           unsigned long ulSize)
{
	long lReturn = 0;
	
	lReturn = SoftEEPROMInit(ulStart, ulEnd, ulSize);
	
	//
    // Check to see if an error occurred.
    //
    if(lReturn != 0)
    {
        //
        // Indicate that an error occurred during initialization.
        //
        UARTprintf("\rAn error occurred during Soft EEPROM initialization!");

        //
        // Output the error to the UART and quit.
        //
        OutputErrorAndQuit(lReturn);
    }
	
	return lReturn;
}

//*****************************************************************************
//
//! Outputs the error and quits.
//!
//! This function will output an error message for the given error code and
//! enter an infinite loop.  The message is sent out via UART0.
//!
//! \param ulError is the error code returned by the soft EEPROM drivers.
//!
//! \return None.
//
//*****************************************************************************
void
OutputErrorAndQuit(unsigned long ulError)
{
    //
    // Switch and output based on the error code.
    //
    switch(ulError & 0x7FFF)
    {
        case ERR_NOT_INIT:
            UARTprintf("\r\nERROR: Soft EEPROM not initialized!");
            break;
        case ERR_ILLEGAL_ID:
            UARTprintf("\r\nERROR: Illegal ID used!");
            break;
        case ERR_PG_ERASE:
            UARTprintf("\r\nERROR: Soft EEPROM page erase error!");
            break;
        case ERR_PG_WRITE:
            UARTprintf("\r\nERROR: Soft EEPROM page write error!");
            break;
        case ERR_ACTIVE_PG_CNT:
            UARTprintf("\r\nERROR: Active soft EEPROM page count error!");
            break;
        case ERR_RANGE:
            UARTprintf("\r\nERROR: Soft EEPROM specified out of range!");
            break;
        case ERR_AVAIL_ENTRY:
            UARTprintf("\r\nERROR: Next available entry error!");
            break;
        case ERR_TWO_ACTIVE_NO_FULL:
            UARTprintf("\r\nERROR: Two active pages found but not full!");
            break;
        default:
            UARTprintf("\r\nERROR: Unidentified Error");
            break;
    }

    //
    // Did the error occur during the swap operation?
    //
    if(ulError & ERR_SWAP)
    {
        //
        // Indicate that the error occurred during the swap operation.
        //
        UARTprintf("\r\nOccurred during the swap operation.");
    }

    //
    // Enter an infinite loop
    //
    while(1)
    {
        //
        // Do nothing.
        //
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
