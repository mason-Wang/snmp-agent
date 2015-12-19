//*****************************************************************************
//
// softeeprom.c - Example driver for software emulation of EEPROM.
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
#include "hw_flash.h"
#include "sysctl.h"
#include "flash.h"
#include "debug.h"
#include "softeeprom.h"

#ifdef SOFT_EEPROM_SELF_TEST
#include <stdio.h>
#include "uartstdio.h"
#include "softeeprom/softeeprom_wrapper.h"
#endif /* SOFT_EEPROM_SELF_TEST */

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>Software Emulation of EEPROM (softeeprom)</h1>
//!
//! This software is intended to provide example drivers for EEPROM emulation.
//
//*****************************************************************************

//*****************************************************************************
//
//! \page softeeprom_intro Introduction
//!
//! This module contains the EEPROM emulation user API along with functions
//! used by the API that the user does not have access to.
//!
//! The user API consists of the following functions:
//! - SoftEEPROMInit()  - Used to initialize the emulated EEPROM.
//! - SoftEEPROMWrite() - Used to store a variable in the emulated EEPROM.
//! - SoftEEPROMRead() - Used to read a variable from the emulated EEPROM.
//! - SoftEEPROMClear() - Used to clear the contents of the emulated EEPROM.
//!
//! SoftEEPROMInit() must be called and must return without error before
//! calling SoftEEPROMWrite(), SoftEEPROMRead(), and SoftEEPROMClear().
//!
//! The other functions are static functions used by the API.  These static
//! functions are not intended to be used by the user.
//!
//! The internal Flash of the Stellaris family of microcontrollers is used to
//! emulate real EEPROM.  The region of the Flash to be used for EEPROM
//! emulation is specified as parameters when SoftEEPROMInit() is called.  The
//! size of the region to be used must be a multiple of 4K bytes and the
//! starting address of the region must be aligned on a 4K byte boundary.
//! This is due to the Flash architecture of the Stellaris microcontrollers.
//! The EEPROM region must consist of at least two EEPROM pages.  The size of
//! of each EEPROM page is specified as a parameter when SoftEEPROMInit() is
//! called.  The internal Flash of the Stellaris microcontrollers is divided
//! into 1K byte erasable blocks.  Therefore, the size of the EEPROM pages
//! must be a multiple of 1K bytes.  In addition, the size of the EEPROM pages
//! must be evenly divisible into to total EEPROM region.
//!
//! The beginning of each EEPROM page consists of two 32-bit status words
//! used by the emulation software.  The remainder of the EEPROM page is
//! divided into EEPROM entries.  The Flash for  Stellaris microcontrollers
//! has the restriction that each 32-bit word can only be programmed one time
//! between erase operations.  Due to this, each EEPROM entry is 32-bits.
//! Each entry consists of an 16-bit identifier and 16-bits of data.
//!
//! The code for this module is contained in the <tt>softeeprom.c</tt> file,
//! with the <tt>softeeprom.h</tt> file containing definitions and functions
//! exported to the rest of the application.
//
//*****************************************************************************

//*****************************************************************************
//
// Each entry looks like the following with the identifier bits indicated by i,
// empty bits indicated as 0, data bits indicated as d, and the bit number
// below each indicator:
//
// +---------------------------------------------------------------+
// | i | i | i | i | i | i | i | i | i | i | i | i | i | i | i | i |
// +---------------------------------------------------------------+
// | 31| 30| 29| 28| 27| 26| 25| 24| 23| 22| 21| 20| 19| 18| 17| 16|
// +---------------------------------------------------------------+
//
// +---------------------------------------------------------------+
// | d | d | d | d | d | d | d | d | d | d | d | d | d | d | d | d |
// +---------------------------------------------------------------+
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// +---------------------------------------------------------------+
//
//*****************************************************************************


//*****************************************************************************
//
//! \defgroup softeeprom_api Definitions
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
//! The value that is read when the status words and entries are in the erased
//! state.  This definition is not intended to be modified.
//
//*****************************************************************************
#define ERASED_WORD          0xFFFFFFFF

//*****************************************************************************
//
//! The number of EEPROM identifiers allowed. The identifier in each EEPROM
//! entry is 8 bits.  0xFF is not allowed since it indicates an empty entry.
//! Therefore, the maximum number of identifiers allowed is 255 (0 - 254).
//
//*****************************************************************************
#define NUM_IDS                 (MAX_SOFTEEPROM_IDS)

//*****************************************************************************
//
//! The number of bits needed for the bit vector to store the status needed
//! to determine if data for a given identifier has already been copied to the
//! new page during PageSwap(). This needs to be a multiple of 8 so that we
//! can store the bit vector in a byte array.  This definition is not intended
//! to be modified.
//
//*****************************************************************************
#define NUM_VECTOR_BITS         (NUM_IDS + 8 - (NUM_IDS % 8))

//*****************************************************************************
//
//! The number of bytes needed for the bit vector to store the status needed
//! to determine if data for a given identifier has already been copied to the
//! new page during PageSwap().  This definition is not intended to be
//! modified.
//
//*****************************************************************************
#define NUM_VECTOR_BYTES        (NUM_VECTOR_BITS / 8)

//*****************************************************************************
//
//! The EEPROM pages must be aligned on a 4K boundary.  This definition is used
//! for parameter checking and is not intended to be modified.
//
//*****************************************************************************
#define EEPROM_BOUNDARY         0x1000

//*****************************************************************************
//
//! The starting address of the EEPROM emulation region.  This must be aligned
//! on a 4K boundary.
//
//*****************************************************************************
static unsigned char* g_pucEEPROMStart;

//*****************************************************************************
//
//! The ending address of the EEPROM emulation region.  This address is not
//! inclusive.  That is, it is the first address just after the EEPROM emulation
//! region.  It must be aligned on a 4K boundary. It can be the first location
//! after the end of the Flash array if the last Flash page is used for EEPROM
//! emulation.
//
//*****************************************************************************
static unsigned char* g_pucEEPROMEnd;

//*****************************************************************************
//
//! The size of the EEPROM emulation pages in bytes.  This must be evenly
//! divisible into the total EEPROM emulation region.  The size must be specified
//! such to allow for at least two EEMPROM emulation pages.
//
//*****************************************************************************
static unsigned long g_ulEEPROMPgSize;

#define MAX_ENTRY_IN_ONE_PAGE  ((g_ulEEPROMPgSize/4)-2)

//*****************************************************************************
//
//! Boolean variable that is set to true after the EEPROM emulation region has
//! been initialized.  Initially set to false at start-up.
//
//*****************************************************************************
static tBoolean g_bEEPROMInitialized = false;

//*****************************************************************************
//
//! The beginning address of the active EEPROM emulation page.
//
//*****************************************************************************
static unsigned char* g_pucActivePage;

//*****************************************************************************
//
//! The address of the next available EEPROM entry in the active page.
//
//*****************************************************************************
static unsigned char* g_pucNextAvailEntry;

//*****************************************************************************
//
//! Erases an EEPROM page. 
//!
//! This function is called to erase an EEPROM page.  It verifies that the page
//! has been erased by reading the status words of the page and confirming that
//! they read as 0xFFFFFFFF.
//!
//! \param pucPageAddr is the beginning address of the EEPROM page to erase.
//!
//! \return A value of 0 indicates that the erase was successful.  A non-zero
//! value indicates a failure.
//
//*****************************************************************************
static long
PageErase(unsigned char* pucPageAddr)
{
    unsigned char* pucAddr;

    //
    // Loop through the Flash pages within the specified EEPROM page.
    //
    for(pucAddr = pucPageAddr; pucAddr < (pucPageAddr + g_ulEEPROMPgSize);
        pucAddr += FLASH_ERASE_SIZE)
    {
        //
        // Erase the page.
        //
        if(FlashErase((unsigned long)pucAddr))
        {
            //
            // The erase failed.
            //
            return(-1);
        }
    }

    //
    // Verify that the EEPROM page is indeed erased by checking that the
    // status words are 0xFFFFFFFF (much faster than reading the entire
    // page).  It is assumed that if there was an error erasing the data
    // portion of the page, then it will be detected when verifying the
    // subsequent data writes in SoftEEPROMWrite().
    //
    if(((*(unsigned long*)pucPageAddr) != ERASED_WORD) ||
    ((*(unsigned long*)(pucPageAddr + 4)) != ERASED_WORD))
    {
        //
        // The erase-verify failed.
        //
        return(-1);
    }

    //
    // The erase and erase-verify were successful, so return 0.
    //
    return(0);
}

//*****************************************************************************
//
//! Writes a word to an EEPROM page.
//!
//! This function is called to write a word (32 bits) to an EEPROM page.  It
//! verifies that the write was successful by reading the location and comparing
//! against the value written.
//!
//! \param pulData is the pointer to the data to be written.
//!
//! \param pucPgAddr is the pointer to the target address.
//!
//! \param ulByteCount is the number of bytes to be written.
//!
//! \return A value of 0 indicates that the write was successful.  A non-zero
//! value indicates a failure.
//
//*****************************************************************************
static long
PageDataWrite(unsigned long* pulData, unsigned char* pucPgAddr, 
              unsigned long ulByteCount)
{
    //
    // Write the supplied data to the supplied address.
    //
    if(FlashProgram(pulData, (unsigned long)pucPgAddr, ulByteCount))
    {
        //
        // A error occurred.
        //
        return(-1);
    }

    //
    // Loop through and verify the program operation
    //
    for(ulByteCount = 0; ulByteCount < ulByteCount; ulByteCount += 4)
    {
        //
        // Verify that the data was programmed correctly.
        //
        if(*(unsigned long *)pucPgAddr != *pulData)
        {
            //
            // An error occurred.
            //
              return(-1);
        }

        //
        // Increment the pointers.
        //
        pucPgAddr += 4;
        pulData++;
    }
    
    //
    // The program operation was successful, so return 0.
    //    
    return(0);
}

//*****************************************************************************
//
//! Copies the most recent data from the full page to the new page.
//!
//! This function is called when an EEPROM page is full and is responsible for
//! copying the most recent data for each ID to the next page.  The global
//! variables containing the beginning address of the active page and the next
//! available entry are updated in this function.
//!
//! Order of operations:
//! -# Erase the next page.
//! -# Move the current data for each ID to the next page.
//! -# Mark the next page as the active page (increment the counter).
//! -# Mark the full page as used.
//!
//! It is possible that a power loss or reset could occur during the swap
//! process.  SoftEEPROMInit() is called upon the subsequent boot up and will
//! attempt to detect and take the appropriate corrective actions if necessary.
//!
//! \param pucFullPageAddr is a pointer to beginning of the full page.
//!
//! \return A value of 0 indicates that the page swap was successful.  A
//! non-zero value indicates a failure.
//
//*****************************************************************************
static long
PageSwap(unsigned char* pucFullPageAddr)
{
    unsigned short usCnt;
    unsigned short usSize;
    unsigned long  ulEntry;
    unsigned short usEntryID;
    unsigned char* pucNewEntry;
    unsigned char* pucUsedEntry;
    unsigned char* pucNewPageAddr;
    //
    // An array of bytes used as a bit vector to determine if the entry for the
    // given ID has already been copied over to the new page.
    //
    unsigned char ucIDSwapped[NUM_VECTOR_BYTES];
    //
    // An array to store the entries to be moved to the next page.
    // Declared as static so that the memory is not allocated on the stack.
    //
    static unsigned long ulDataBuffer[32];
    unsigned long ulNumWordsInBuffer;
    unsigned long ulByteCount;

    //
    // Initially set all the bits of the entire ucAddrSwapped array to 0.
    //
    for(usCnt = 0; usCnt < NUM_VECTOR_BYTES; usCnt++)
    {
        //
        // Set all bits of the bit vector to 0.
        //
        ucIDSwapped[usCnt] = 0;
    }

    //
    // Get the new page pointer.
    //
    pucNewPageAddr = ((pucFullPageAddr + g_ulEEPROMPgSize) < g_pucEEPROMEnd) ?
                     (pucFullPageAddr + g_ulEEPROMPgSize) :
                     g_pucEEPROMStart;

    //
    // Step 1) Erase the new page.
    //
    if(PageErase(pucNewPageAddr))
    {
        //
        // Return error.
        //
        return(ERR_SWAP | ERR_PG_ERASE);
    }

    //
    // Initially set the used entry pointer to the last entry of the full page.
    //
    pucUsedEntry = pucFullPageAddr + g_ulEEPROMPgSize - 4;

    //
    // Initially set the new entry pointer to the first entry of the new page.
    //
    pucNewEntry = pucNewPageAddr + 8;

    //
    // Get the size of the variable array.  The entries are 4 bytes each and the
    // first 2 words are used for status.
    //
     usSize = (g_ulEEPROMPgSize / 4) - 2;

    //
    // Calculate the number of bytes that can be written with the first
    // programming operation if using a part with Flash write buffers.
    // Initialize the number of words in the storage buffer.
    //
    ulByteCount = 32 - ((unsigned long)pucNewEntry & 0x7F);  
    ulNumWordsInBuffer = 0;   
    
    //
    // Step 2) Now copy the most recent data.  Read from the end of the
    // currently active page first.  Only copy the data for a given ID the
    // first time it is encountered since this is the most recent value for
    // that ID.
    //
    for(usCnt = 0;usCnt < usSize; usCnt++)
    {
        //
        // Read the entry.
        //
        ulEntry = *(unsigned long*)pucUsedEntry;

        //
        // Decrement the pointer to the entry in the full page.
        //
        pucUsedEntry -= 4;
        
        //
        // Get the ID for the entry.
        //
        usEntryID = ulEntry >> 16;

        //
        // If this id has not been copied yet and the id is not equal to 0xFF
        // then copy it.  Under normal conditions, we should never encounter
        // an entry with an id of 0xFF since PageSwap() should only be called
        // when the currently active page is full, but check just in case since
        // we don't want to copy empty entries.
        //
        if(((ucIDSwapped[usEntryID / 8] & (0x1 << (usEntryID % 8))) == 0)
           && (usEntryID != 0xFF))
        {
            //
            // Put the data in the buffer and increment the counter.
            //
            ulDataBuffer[ulNumWordsInBuffer++] = ulEntry;
            
            //
            // Set the appropriate bit in the bit vector to indicate that the ID
            // has already been copied.
            //
            ucIDSwapped[usEntryID / 8] |= (0x1 << (usEntryID % 8));
            
            //
            // Decrement the ulByteCount remaining in the Flash write buffers.
            //
            ulByteCount -= 4;
            
            //
            // Check to see if all of the Flash write buffer space will be used
            // up in the next programming operation.
            //
            if(ulByteCount == 0)
            {
                //
                // Program the buffer to memory.
                //
                if(PageDataWrite(&ulDataBuffer[0], pucNewEntry, ulNumWordsInBuffer * 4))
                {
                    //
                    // Return the error.
                    //
                    return(ERR_SWAP | ERR_PG_WRITE);                   
                }
                
                //
                // Increment the address to program the next entry in the new page.
                //
                pucNewEntry += (ulNumWordsInBuffer * 4);

                //
                // Reset the byte count remaining the write buffer space to the
                // equivalent of 32 words.  Reset the number of words already in
                // the local buffer to 0.
                //
                ulByteCount = 32 * 4;
                ulNumWordsInBuffer = 0;
            }
        }
	}

	//
	//	Are there any words remaining in the data buffer?
	//
	if(ulNumWordsInBuffer != 0)
	{
        //
        // Program the buffer to memory.
        //
        if(PageDataWrite(&ulDataBuffer[0], pucNewEntry, ulNumWordsInBuffer * 4))
        {
            //
            // Return the error.
            //
            return(ERR_SWAP | ERR_PG_WRITE);                   
        }

        //
	    // Increment the address to program the next entry in the new page.
	    //
	    pucNewEntry += (ulNumWordsInBuffer * 4);
	}


    //
    // Step 3) Mark the new page as active. Increment the active status
    // counter by 1 from the previous page.  First just store in local buffer.
    //
    ulDataBuffer[0] = (*(unsigned long*)pucFullPageAddr) + 1;
	
	//
    // Now program the status word to Flash.
    //
    if(PageDataWrite(&ulDataBuffer[0], pucNewPageAddr, 4))
    {
        //
        // Return the error.
        //
        return(ERR_SWAP | ERR_PG_WRITE);
    }

    //
    // Step 4) Mark the full page as used. This is indicated by marking the
    // second status word in the page.  First just store in local buffer.
    //
    ulDataBuffer[0] = ~(ERASED_WORD);
    
    //
    // Now program the status word to Flash.
    //
    if(PageDataWrite(&ulDataBuffer[0], pucFullPageAddr + 4, 4))
    {
        //
        // Return the error.
        //
        return(ERR_SWAP | ERR_PG_WRITE);
    }

    //
    // Now save the pointer to the beginning of the new active page and return.
    //
    g_pucActivePage = pucNewPageAddr;

    //
    // The next available entry location in the new page is pucNewEntry.
    //
    g_pucNextAvailEntry = pucNewEntry;

	//
	// Check that the next available entry is within the the new page.  If a
	// page size of 1K is used (1024/4 - 2 = 254 entries) and all 255 IDs are
	// used (0-254) then it is possible to swap to a new page and not have
	// any available entries in the new page.  This should be avoided by
    // the user by configuring the EEPROM appropriately for the given
    // application.
	//
	if(!(g_pucNextAvailEntry < (g_pucActivePage + g_ulEEPROMPgSize)))
	{
		//
        // Return the error.
        //
        return(ERR_SWAP | ERR_AVAIL_ENTRY);
	}

    //
    // Return success.
    //
     return(0);
}

//*****************************************************************************
//
//! Checks if the page is active.
//!
//! This function is called to determine if the given EEPROM page is marked as
//! active.
//!
//! \param pucPageAddr is the starting address of the EEPROM page to check for
//! the active status.
//!
//! \return A value of true indicates that the page is marked as active.  A
//! return value of false indicates that the page is not marked as active.
//
//*****************************************************************************
static tBoolean
PageIsActive(unsigned char* pucPageAddr)
{
    //
    // Check the two status words of the page for the active status.
    //
    if((*(unsigned long*)pucPageAddr != ERASED_WORD) &&
       (*(unsigned long*)(pucPageAddr + 4) == ERASED_WORD))
    {
        //
        // The page is active.  Return true.
        //
        return(true);
    }
    else
    {
        //
        // The page is not active.  Return false.
        //
        return(false);
    }
}

//*****************************************************************************
//
//! Checks if the page is used.
//!
//! This function is called to determine if the given EEPROM page is marked as
//! used.
//!
//! \param pucPageAddr is the starting address of the EEPROM page to check for
//! the used status.
//!
//! \return A value of true indicates that the page is marked as used.  A
//! return value of false indicates that the page is not marked as used.
//
//*****************************************************************************
static tBoolean
PageIsUsed(unsigned char* pucPageAddr)
{
    //
    // Check the two status words of the page for the used status.
    //
    if((*(unsigned long*)pucPageAddr != ERASED_WORD) &&
       (*(unsigned long*)(pucPageAddr + 4) != ERASED_WORD))
    {
        //
        // The page is used.  Return true.
        //
        return(true);
    }
    else
    {
        //
        // The page is not used.  Return false.
        //
        return(false);
    }
}

//*****************************************************************************
//
//! Gets the number of active pages.
//! 
//! This function searches the EEPROM pages counting the number of pages marked
//! as active.
//!
//! \return The number of EEPROM pages marked as active.
//
//*****************************************************************************
static unsigned char
GetActivePageCount(void)
{
    unsigned char ucCnt;
    unsigned char* pucPageAddr;

    //
    // Initially set ucCnt to 0
    //
    ucCnt = 0;

    //
    // Loop through the EEPROM pages.
    //
    for(pucPageAddr = g_pucEEPROMStart; pucPageAddr < g_pucEEPROMEnd;
        pucPageAddr += g_ulEEPROMPgSize)
    {
        //
        // Is the page marked as active?
        //
        if(PageIsActive(pucPageAddr))
        {
            //
            // The page is marked as active so increment the counter.
            //
            ucCnt++;
        }
    }

    //
    // return the active page count
    //
    return ucCnt;
}

//*****************************************************************************
//
//! Gets the number of used pages.
//!
//! This function searches the EEPROM pages counting the number of pages marked
//! as used.
//!
//! \return The number of EEPROM pages marked as used.
//
//*****************************************************************************
static unsigned char
GetUsedPageCount(void)
{
    unsigned char ucCnt;
    unsigned char* pucPageAddr;

    //
    // Initially set ucCnt to 0
    //
    ucCnt = 0;

    //
    // Loop through the EEPROM pages.
    //
    for(pucPageAddr = g_pucEEPROMStart; pucPageAddr < g_pucEEPROMEnd;
        pucPageAddr += g_ulEEPROMPgSize)
    {
        //
        // Is the page marked as used?
        //
        if(PageIsUsed(pucPageAddr))
        {
            //
            // The page is marked as used so increment the counter.
            //
            ucCnt++;
        }
    }

    //
    // return the used page count
    //
    return ucCnt;
}

//*****************************************************************************
//
//! Gets the next available entry.
//!
//! The function finds the next available EEPROM entry in the currently active
//! page.  If the active page is full, then it returns the address just outside
//! of the active EEPROM page.  This will be detected by the next write
//! operation and PageSwap() will be called.  The g_pucActivePage variable
//! must be initialized prior to calling this function.
//!
//! \return The address of the next available EEPROM entry.
//
//*****************************************************************************
static unsigned char*
GetNextAvailEntry(void)
{
    unsigned char* pucPageAddr;
    unsigned short usIdx;
    unsigned short usSize;

    //
    // Get the number of entries in the EEPROM page.  Each entry is 4 bytes and
    // the first 2 word are uses for status.
    //
    usSize = (g_ulEEPROMPgSize / 4) - 2;

    //
    // Initially set the entry pointer to the first entry in the active
    // page.
    //
    pucPageAddr = g_pucActivePage + 8;

    //
    // Loop through the page.
    //
    for(usIdx = 0; usIdx < usSize; usIdx++)
    {
        //
        // Is the entry available?
        //
        if(*(unsigned long*)(pucPageAddr) == ERASED_WORD)
        {
            //
            // An empty entry was found.  Break out of the loop.
            //
            break;
        }

        //
        // Increment the entry pointer.
        //
        pucPageAddr += 4;
    }

    //
    // Return the address of the next available entry.
    //
    return(pucPageAddr);
}

//*****************************************************************************
//
//! Gets the most recently used page.
//!
//! This function searches the EEPROM pages to find the most recently used
//! page.
//!
//! \return A pointer to the beginning address of the page that is most
//! recently used.  If no used pages are found, the pointer points to
//! 0xFFFFFFFF.
//
//*****************************************************************************
static unsigned char*
GetMostRecentlyUsedPage(void)
{
    unsigned char* pucMRUPage;
    unsigned char* pucPageAddr;
    unsigned long ulActiveStatusCnt;
    
    //
    // Set the page pointer to the first page.
    //
    pucPageAddr = g_pucEEPROMStart;
    
    //
    // Initially set ulActiveStatusCnt to 0 and pucMRUPage to 0xFFFFFFFF.
    //
    ulActiveStatusCnt = 0;
    pucMRUPage = (unsigned char*)0xFFFFFFFF;
    
    //
    // Loop through all of the pages.
    //
    for(pucPageAddr = g_pucEEPROMStart; pucPageAddr < g_pucEEPROMEnd;
        pucPageAddr += g_ulEEPROMPgSize)
    {
        //
        // Is the page used.
        //
        if(PageIsUsed(pucPageAddr))
        {
            //
            // Is this active status count higher?
            //
            if(*(unsigned long *)pucPageAddr > ulActiveStatusCnt)
            {
                //
                // Save the new high active status count and the page address.
                //
                ulActiveStatusCnt = *(unsigned long *)pucPageAddr;
                pucMRUPage = pucPageAddr;
            }
            
        }
    }
    
    //
    // Return the beginning address of the most recently used page.
    //
    return(pucMRUPage);
}

//*****************************************************************************
//
//! Initializes the emulated EEPROM.
//!
//! This function initializes the EEPROM emulation area within the Flash. This
//! function must be called prior to using any of the other functions in the
//! API. It is expected that SysCtlClockSet() is called prior to calling this
//! function due to SysCtlClockGet() being used by this function.
//!
//! \param ulStart is the start address for the EEPROM region. This address
//! must be aligned on a 4K boundary.
//!
//! \param ulEnd is the end address for the EEPROM region.  This address is
//! not inclusive.  That is, it is the first address just after the EEPROM 
//! emulation region.  It must be aligned on a 4K boundary. It can be the first
//! location after the end of the Flash array if the last Flash page is used
//! for EEPROM emulation.
//!
//! \param ulSize is the size of each EEPROM page. This must be evenly
//! divisible into the total EEPROM emulation region.  The size must be
//! specified such to allow for at least two EEMPROM emulation pages.
//!
//! \return A value of 0 indicates that the initialization was successful.  A
//! non-zero value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROMInit(unsigned long ulStart, unsigned long ulEnd,
               unsigned long ulSize)
{
    unsigned long ulActiveStatusCnt;
    unsigned char ucActivePgCnt;
    unsigned char* pucPageAddr;
    unsigned char* pucActivePg;
	tBoolean bFullPgFound;

    ASSERT(ulEnd > ulStart);
    ASSERT((ulStart % EEPROM_BOUNDARY) == 0);
    ASSERT((ulEnd % EEPROM_BOUNDARY) == 0);
    ASSERT((ulSize % FLASH_ERASE_SIZE) == 0);
    ASSERT((ulSize/4) >= (MAX_SOFTEEPROM_IDS*2) );
    ASSERT(((ulEnd - ulStart) / ulSize) >= 2);
    
    //
    // Check that the EEPROM region is within the Flash.
    //
    if(ulEnd > SysCtlFlashSizeGet())
    {
        //
        // Return the proper error.
        //
        return(ERR_RANGE);   
    }

    //
    // Save the characteristics of the EEPROM Emulation area. Mask off the 
    // lower bits of the addresses to ensure that they are 4K aligned.
    //
    g_pucEEPROMStart = (unsigned char *)(ulStart & ~(EEPROM_BOUNDARY - 1));
    g_pucEEPROMEnd = (unsigned char *)(ulEnd & ~(EEPROM_BOUNDARY - 1));
    g_ulEEPROMPgSize = ulSize;

    //
    // Set the number of clocks per microsecond to enable the Flash controller
    // to properly program the Flash.
    //
    FlashUsecSet(SysCtlClockGet() / 1000000);

    //
    // Get the active page count.
    //
    ucActivePgCnt = GetActivePageCount();
    
    //
    // If there are no active pages, execute the following.  This will be true
    // for a fresh start and can also be true if a reset or power-down occurs
    // during a clear operation.
    //
    if(ucActivePgCnt == 0)
    {
        //
        // If there are not any used pages, then this is a fresh start.
        //
        if(GetUsedPageCount() == 0)
        {
            //
            // Erase the first page.
            //
            if(PageErase(g_pucEEPROMStart))
            {
                //
                // Return the proper error.
                //
                return(ERR_PG_ERASE);
            }

			//
			// The active status count will be 0.
			//
			ulActiveStatusCnt = 0;

            //
            // Mark the new page as active. Since this is a fresh start
            // start the counter at 0.
            //
            if(PageDataWrite(&ulActiveStatusCnt, g_pucEEPROMStart, 4))
            {
                //
                // Return the proper error.
                //
                return(ERR_PG_WRITE);
            }

            //
            // Save the active page pointer.
            //
            g_pucActivePage = g_pucEEPROMStart;
    
            //
            // Save the next available entry.
            //
            g_pucNextAvailEntry = g_pucEEPROMStart + 8;
        }
        
        //
        // Else, a reset must have occurred before a clear operation could
        // complete.  This is known since there are used pages but no active
        // pages.
        //
        else
        {
            //
            // Get the beginning address of the most recently used page.
            //
            pucPageAddr = GetMostRecentlyUsedPage();
            
            //
            // Get the active status counter for the most recently used
            // page.  Then add one to it for the next page.
            //
            ulActiveStatusCnt = *(unsigned long *)pucPageAddr + 1;
            
            //
            // Calculate the address of the page just after the most
            // recently used.
            //
            pucPageAddr = ((pucPageAddr + g_ulEEPROMPgSize) < g_pucEEPROMEnd) ?
                          (pucPageAddr + g_ulEEPROMPgSize) :
                          g_pucEEPROMStart;
            
            //
            // Erase this page.
            //
            if(PageErase(pucPageAddr))
            {
                //
                // Return the proper error.
                //
                return(ERR_PG_ERASE);
            }

            //
            // Mark this page as active.
            //
            if(PageDataWrite(&ulActiveStatusCnt, pucPageAddr, 4))
            {
                //
                // Return the proper error.
                //
                return(ERR_PG_WRITE);
            }

            //
            // Save the active page pointer.
            //
            g_pucActivePage = pucPageAddr;
    
            //
            // Save the next available entry.
            //
            g_pucNextAvailEntry = pucPageAddr + 8;
        }
    }
    
    //
    // Else, if there is 1 active page, execute the following.  This will be
    // true for a normal start where the EEPROM has been previously
    // initialized and can also be true if a reset or power-down occurs during
    // a clear operation.
    //
    else if(ucActivePgCnt == 1)
    {
        //
        // Loop through the pages.
        //	
        for(pucActivePg = g_pucEEPROMStart; pucActivePg < g_pucEEPROMEnd;
            pucActivePg += g_ulEEPROMPgSize)
        {
            //
            // Is this the active page?
            //
            if(PageIsActive(pucActivePg))
            {
                //
                // Break out of the loop.
                //
                break;
            }
        }
        
        //
        // Now calculate the address of the page before the active page.
        //
        pucPageAddr = (pucActivePg == g_pucEEPROMStart) ?
                      (g_pucEEPROMEnd - g_ulEEPROMPgSize) :
                      (pucActivePg - g_ulEEPROMPgSize);
                    
        //
        // Check to see if the page before has been used.
        //
        if(PageIsUsed(pucPageAddr))
        {
            //
            // Check to see that the used page counter is one less than the 
			// active page counter.
            //
            if(*(unsigned long*)pucPageAddr == 
			   (*(unsigned long*)pucActivePg - 1))
            {
                //
                // This is a normal start. Save the active page pointer.
                //
                g_pucActivePage = pucActivePg;
        
                //
                // Save the next available entry.
                //
                g_pucNextAvailEntry = GetNextAvailEntry();					
            }
            
            //
            // Else, a reset must have occurred during the page erase or
			// programming the the active status counter of a 
			// clear operation to leave the EEPROM in this state.
            //
            else
            {
                //
                // Erase the page that was marked active.  It is incorrectly
                // marked active due to the counter being off.
                //
                if(PageErase(pucActivePg))
                {
                    //
                    // Return the proper error.
                    //
                    return(ERR_PG_ERASE);
                }
                
                //
                // Get the active status counter for the most recently used
                // page.  Then add one to it for the next page.
                //
                ulActiveStatusCnt = *(unsigned long *)pucPageAddr + 1;
                
                //
                // Mark this page as active.
                //
                if(PageDataWrite(&ulActiveStatusCnt, pucActivePg, 4))
                {
                    //
                    // Return the proper error.
                    //
                    return(ERR_PG_WRITE);
                }
                
                //
                // Save the active page pointer.
                //
                g_pucActivePage = pucActivePg;
        
                //
                // Save the next available entry.
                //
                g_pucNextAvailEntry = pucActivePg + 8;
            }
        }
        
        //
        // Else, the page before the active one has not been used yet.
        //
        else
        {
            //
            // This is a normal start. Save the active page pointer.
            //
            g_pucActivePage = pucActivePg;
            
            //
            // Save the next available entry.
            //
            g_pucNextAvailEntry = GetNextAvailEntry();					
        }
    }

    //
    // Else, if there are 2 active pages, execute the following.  This should
    // only occur if a reset or power-down occurs during a page swap operation.
    // In this case, one of the active pages must be full or else PageSwap()
    // would not have been called.
    //
    else if(ucActivePgCnt == 2)
    {
		//
		// Initially set bFullPgFound to false;
		//
		bFullPgFound = false;

        //
        // Loop through the pages.
        //
        for(pucActivePg = g_pucEEPROMStart; pucActivePg < g_pucEEPROMEnd;
            pucActivePg += g_ulEEPROMPgSize)
        {
            //
            // Is this the active page?
            //
            if(PageIsActive(pucActivePg))
            {
                //
                // Is the page full?
                //
                if(*(unsigned long*)(pucActivePg + g_ulEEPROMPgSize - 4)
                   != 0xFFFFFFFF)
                {
                    //
					// Set the status to true
					//
					bFullPgFound = true;

					//
                    // Then the page is full.  Break out of the loop.
                    //
                    break;
                }
            }
        }
        
        //
		// Was a full page found?
		//
		if(bFullPgFound == true)
		{
			//
	        // Now, the full page is pointed to by pucActivePg.  Save this as
            // the active page.  PageSwap() will be called again on the next
            // write.
	        //
	        g_pucActivePage = pucActivePg;
	        
	        //
	        // Save the next available entry. It is the location just after
	        // the end of the page since the page is full.  This will cause
	        // PageSwap() to be called on the next write.
	        //
	        g_pucNextAvailEntry = pucActivePg + g_ulEEPROMPgSize;
		}
		
		//
		// Else, this is not an expected case.  Report the error.
		//
		else
		{
			//
            // Return the proper error.
            //
            return(ERR_TWO_ACTIVE_NO_FULL);	
		}
    }

    //
    // Else there are more than 2 active pages.  This should never happen.
    //
    else
    {
        //
        // Return the proper error.
        //
        return(ERR_ACTIVE_PG_CNT);
    }

    //
    // The EEPROM has been initialized.
    //
    g_bEEPROMInitialized = true;

    //
    // Return indicating that no error occurred.
    //
    return(0);
}

//*****************************************************************************
//
//! Writes an EEPROM entry. 
//!
//! This function writes the specified ID and data to the next entry available
//! in the active EEPROM page.  If the page is full, PageSwap() will be called.
//!
//! \param usID is the identifier associated with the data.
//!
//! \param usData is the data to be saved for the given ID.
//!
//! \return A value of 0 indicates that the write was successful.  A non-zero
//! value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROMWrite(unsigned short usID, unsigned short usData)
{
    unsigned long ulEntry;
    long lReturnCode;

    //
    // Has the EEPROM been initialized?  If not, return the error.
    //
    if(!g_bEEPROMInitialized)
    {
        //
        // Return the proper error.
        //
        return(ERR_NOT_INIT);
    }

    //
    // Is the ID specified a valid ID?  If not, return the error.
    //
    if(usID >= NUM_IDS)
    {
        //
        // Return the proper error.
        //
        return(ERR_ILLEGAL_ID);
    }

    //
    // If the next available entry location is outside of the currently active
    // page, then we need to call PageSwap.
    //
    if(g_pucNextAvailEntry >= (g_pucActivePage + g_ulEEPROMPgSize))
    {
        //
        // Call PageSwap.
        //
        lReturnCode = PageSwap(g_pucActivePage);
        
        //
        // Was there a failure?  If so, return the failure code.
        //
        if(lReturnCode != 0)
        {
            return(lReturnCode);
        }
    }
    
    //
    // Calculate the entry.
    //
    ulEntry = (0UL | (((unsigned long)usID) << 16) | ((unsigned long)usData));

    //
    // Write the entry to the next available spot in the active page.
    //
    if(PageDataWrite(&ulEntry, g_pucNextAvailEntry, 4))
    {
        //
        // Return the proper error.
        //
        return(ERR_PG_WRITE);
    }
    
    //
    // Increment the next available index variable
    //
    g_pucNextAvailEntry += 4;

    //
    // Return indicating that no error occurred.
    //
    return(0);
}


//*****************************************************************************
//
//! Reads the most recent EEPROM entry for the given ID.
//!
//! This function reads the data for the specified ID.  It will search the
//! active EEPROM page from the end to get the most recent data for the given
//! ID.  0xFF is not a valid ID as it is equivalent to being erased.
//!
//! \param usID is the ID to search for.
//!
//! \param pusData is the address to store the data for the specified ID.  
//! The address is written with 0xFFFF if the ID is not found, thereby
//! emulating a real EEPROM being erased at that address.
//!
//! \param pbFound is the address to store the boolean found value.  If the
//! ID is found, this address will be set to true.  If the ID is not found,
//! this address will be set to false.
//!
//! \return A value of 0 indicates that the read was successful.  A non-zero
//! value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROMRead(unsigned short usID, unsigned short *pusData,
               tBoolean *pbFound)
{
    unsigned char* pucEntry;

    //
    // Has the EEPROM been initialized?  If not, return the error.
    //
    if(!g_bEEPROMInitialized)
    {
        //
        // Return the proper error.
        //
        return(ERR_NOT_INIT);
    }

    //
    // Is the ID specified a valid ID?  If not, return the error.
    //
    if(usID > (NUM_IDS - 1))
    {
        //
        // Return the proper error.
        //
        return(ERR_ILLEGAL_ID);
    }
    
    //
    // Initially set pbFound to false and pusData to 0xFFFF
    //
    *pbFound = false;
    *pusData = 0xFFFF;
    
    //
    // Loop from the last entry in the EEPROM page to the beginning.
    //
    for(pucEntry = g_pucNextAvailEntry - 4; pucEntry >= g_pucActivePage + 8;
        pucEntry -= 4)
    {
        //
        // Does the ID of this entry match the specified ID?
        //
        if(((*(unsigned long*)pucEntry) >> 16) == usID)
        {
            //
            // Get the data and set bFound to true.
            //
            *pusData = (*(unsigned long*)pucEntry) & 0xFFFF;
            *pbFound = true;
            
            //
            // Break out of the loop.
            //
            break;
        }
    }

    //
    // Return indicating that no error occurred.
    //
    return(0);
}


//*****************************************************************************
//
//! Clears the EEPROM contents.
//!
//! This function clears the EEPROM contents. 
//! Order of operations:
//! -# Mark the current page as used.
//! -# Erase the next page.
//! -# Mark the erased page as the active page (increment the counter).
//!
//! It is possible that a power loss or reset could occur during the clear
//! process.  SoftEEPROMInit() is called upon the subsequent boot up and will
//! attempt to detect and take the appropriate corrective actions if necessary.
//!
//! \return A value of 0 indicates that the clear operation was successful.  A
//! non-zero value indicates a failure.
//
//*****************************************************************************
long
SoftEEPROMClear(void)
{
    unsigned long ulStatus;
	unsigned char* pucNewPageAddr;

    //
    // Has the EEPROM been initialized?  If not, return the error.
    //
    if(!g_bEEPROMInitialized)
    {
        //
        // Return the proper error.
        //
        return(ERR_NOT_INIT);
    }

    //
    // Step 1) Mark the current page as used.
    //
	ulStatus = 	~(ERASED_WORD);
    if(PageDataWrite(&ulStatus, g_pucActivePage + 4, 4))
    {
        //
        // Return the proper error.
        //
        return(ERR_PG_WRITE);
    }

    //
    // Get new page pointer.
    //
    pucNewPageAddr = ((g_pucActivePage + g_ulEEPROMPgSize) < g_pucEEPROMEnd) ?
                     (g_pucActivePage + g_ulEEPROMPgSize) :
                     g_pucEEPROMStart;
                    

    //
    // Step 2) Erase the new page.
    //
    if(PageErase(pucNewPageAddr))
    {
        //
        // Return the proper error.
        //
        return(ERR_PG_ERASE);
    }


    //
    // Step 3) Mark the new page as active.  Increment the active count by one
    // from the previous page.
    //
	ulStatus = (*(unsigned long*)g_pucActivePage) + 1;
    if(PageDataWrite(&ulStatus, pucNewPageAddr, 4))
    {
        //
        // Return the proper error.
        //
        return(ERR_PG_WRITE);
    }

    //
    // Now save the pointer to the beginning of the new active page and return.
    //
    g_pucActivePage = pucNewPageAddr;

    //
    // The next available entry location is the first entry in the new page.
    //
    g_pucNextAvailEntry = pucNewPageAddr + 8;

    //
    // Return indicating that no error occurred.
    //
    return(0);
}

#ifdef SOFT_EEPROM_SELF_TEST
//*****************************************************************************
//
// Self test function
//
//*****************************************************************************
void
SoftEEPROMSelfTest(void)
{
	unsigned char ucData[MAX_SOFTEEPROM_IDS*2];
	char cErrText[80];
	unsigned short usData;
	tBoolean bFlag;
	unsigned char ucActvPgCnt, ucInitUsedPgCnt, ucNewUsedPgCnt, ucTotalPgCnt;
	unsigned char *pucActvPg;
    unsigned long ulIdx;
    long lReturn;
    unsigned short usId;

    UARTprintf("\n\n");
    UARTprintf("------------------------------------------------\n");
    UARTprintf("Soft EEPROM Self Test\n");
    UARTprintf("------------------------------------------------\n");

    // check if EEPROM has been initialized
    if(g_bEEPROMInitialized == false)
    {
        UARTprintf("SoftEEPROMIni() needs to be executed before launching self test!\n");
        while(1);
    }

    //
    // test page swap
    //

	// clear EEPROM to make sure we are starting from one clean page
	SoftEEPROMClear();

    // get initial active, used, and total page counts
    ucActvPgCnt = GetActivePageCount();
	if(ucActvPgCnt != 1)
	{
        // Indicate error
		sprintf(cErrText, "Invalid active page counts - %u\n", ucActvPgCnt);
        UARTprintf(cErrText);
        while(1);
	}
    ucInitUsedPgCnt = GetUsedPageCount();
    ucTotalPgCnt = (unsigned char) (((unsigned long)(g_pucEEPROMEnd - g_pucEEPROMStart)) /
    		g_ulEEPROMPgSize);
    if((ucTotalPgCnt == 2) && (ucInitUsedPgCnt != 1))
    {
		sprintf(cErrText, "Unexpected initial used page counter, total page=%u, used page=%u\n",
				ucTotalPgCnt, ucInitUsedPgCnt);
        UARTprintf(cErrText);
        while(1);
    }
    pucActvPg = g_pucActivePage;

    // fill the EEPROM page (with the for loop) with two IDs, then try to write
    // new entries with the same two IDs
    for(ulIdx = 0 ; ulIdx < MAX_ENTRY_IN_ONE_PAGE ; ulIdx++)
    {
    	if(ulIdx < MAX_ENTRY_IN_ONE_PAGE/2)
    	{
    		usId = 0x123;
    	}
    	else
    	{
    		usId = 0x321;
    	}

    	// write
    	lReturn = SoftEEPROMWrite(usId, (unsigned short) ulIdx);
    	if(lReturn != 0)
    	{
            // Indicate error
    		sprintf(cErrText, "Error writing ID %u at index %u\n", usId, ulIdx);
            UARTprintf(cErrText);
            while(1);
    	}

    	// read and verify
    	lReturn = SoftEEPROMRead(usId, &usData, &bFlag);
    	if((lReturn != 0) || (bFlag != true))
    	{
            // Indicate error
    		sprintf(cErrText, "Error reading ID %u at index %u - read %u\n", usId, ulIdx, usData);
            UARTprintf(cErrText);
            while(1);
    	}
    }

    // after this point writing will cause page swap
    SoftEEPROMWrite(0x123, (unsigned short) 0xDEAD);
	// read and verify
	lReturn = SoftEEPROMRead(0x123, &usData, &bFlag);
	if((lReturn != 0) || (bFlag != true) || (usData != 0xDEAD))
	{
        // Indicate error
		sprintf(cErrText, "Error reading ID %u at index %u - read %u\n", usId, ulIdx, usData);
        UARTprintf(cErrText);
        while(1);
	}

	// check active page pointer
	if(pucActvPg == g_pucActivePage)
	{
        // Indicate error
        UARTprintf("Active page pointer is not updated on page swap!\n");
        while(1);
	}

	// check number of used page
	ucNewUsedPgCnt = GetUsedPageCount();
	if(ucTotalPgCnt > 2)
	{
		if((ucInitUsedPgCnt < (ucTotalPgCnt - 1)) &&
		   (ucNewUsedPgCnt <= ucInitUsedPgCnt))
		{
	        // Indicate error
	        UARTprintf("Used page counter is not incrementing: - init=%u, new=%u!\n",
	        		ucInitUsedPgCnt, ucNewUsedPgCnt);
	        while(1);
		}
	}
	else
    {
		if(ucNewUsedPgCnt != 1)
		{
			sprintf(cErrText, "Unexpected new used page counter, total page=%u, used page=%u\n",
					ucTotalPgCnt, ucNewUsedPgCnt);
			UARTprintf(cErrText);
			while(1);
		}
    }

	// try to write another entry in the new page
	SoftEEPROMWrite(0x321, (unsigned short) 0xBEEF);
	// read and verify
	lReturn = SoftEEPROMRead(0x321, &usData, &bFlag);
	if((lReturn != 0) || (bFlag != true) || (usData != 0xBEEF))
	{
        // Indicate error
		sprintf(cErrText, "Error reading ID %u at index %u - read %u\n", usId, ulIdx, usData);
        UARTprintf(cErrText);
        while(1);
	}

	// clear EEPROM
	SoftEEPROMClear();

	// print success
    UARTprintf("Test Page Swap OK!\n");

	//
	// Test address read/write wrapper function
	//
    for(ulIdx = 0 ; ulIdx < MAX_SOFTEEPROM_IDS*2 ; ulIdx++)
    {
    	// fill in data buffer
    	ucData[ulIdx] = (unsigned char) ulIdx;
    }

    // execute address write
    lReturn = SoftEEPROM_WrapperWrite(0, MAX_SOFTEEPROM_IDS*2, ucData);
	if(lReturn != 0)
	{
        // Indicate error
		UARTprintf("Error address writing at address 0\n");
        while(1);
	}

    // execute address read
    lReturn = SoftEEPROM_WrapperRead(0, MAX_SOFTEEPROM_IDS*2, ucData);
	if(lReturn != 0)
	{
        // Indicate error
		UARTprintf("Error address reading at address 0\n");
        while(1);
	}

	// verify the read data
    for(ulIdx = 0 ; ulIdx < MAX_SOFTEEPROM_IDS*2 ; ulIdx++)
    {
    	// fill in data buffer
    	if(ucData[ulIdx] != (unsigned char) ulIdx)
    	{
            // Indicate error
    		sprintf(cErrText, "Error data read at index %u - expected=%u read=%u\n",
    				ulIdx, (unsigned char)ulIdx, ucData[ulIdx]);
            UARTprintf(cErrText);
            while(1);
    	}
    }

    // execute address write
    lReturn = SoftEEPROM_WrapperWrite(1, (MAX_SOFTEEPROM_IDS*2)-1, ucData);
	if(lReturn != 0)
	{
        // Indicate error
		UARTprintf("Error address writing at address 1\n");
        while(1);
	}

    // execute address read
    lReturn = SoftEEPROM_WrapperRead(1, (MAX_SOFTEEPROM_IDS*2)-1, &ucData[1]);
	if(lReturn != 0)
	{
        // Indicate error
		UARTprintf("Error address reading at address 1\n");
        while(1);
	}

	// verify the read data
	if(ucData[0] != 0x00)
	{
        // Indicate error
		sprintf(cErrText, "Error data read at index 0 - expected=0 read=%u\n",
				ucData[0]);
        UARTprintf(cErrText);
        while(1);
	}
    for(ulIdx = 0 ; ulIdx < (MAX_SOFTEEPROM_IDS*2)-1 ; ulIdx++)
    {
    	// fill in data buffer
    	if(ucData[ulIdx+1] != (unsigned char) ulIdx)
    	{
            // Indicate error
    		sprintf(cErrText, "Error data read at index %u - expected=%u read=%u\n",
    				ulIdx+1, (unsigned char)ulIdx, ucData[ulIdx+1]);
            UARTprintf(cErrText);
            while(1);
    	}
    }

	// print success
    UARTprintf("Test Addressing write/read OK!\n\n");

	// clear EEPROM before start
	SoftEEPROMClear();
}
#endif

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
