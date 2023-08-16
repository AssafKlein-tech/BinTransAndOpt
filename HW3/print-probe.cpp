/*########################################################################################################*/
// cd /nfs/iil/ptl/bt/ghaber1/pin/pin-2.10-45467-gcc.3.4.6-ia32_intel64-linux/source/tools/SimpleExamples
// make
//  ../../../pin -t obj-intel64/print-probe.so -- ~/workdir/tst
/*########################################################################################################*/
/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2011 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/* ===================================================================== */

/* ===================================================================== */
/*! @file
 * This probe pintool prints out the disassembled instructions of a given exec file.
 */


#ifndef TARGET_WINDOWS

	#include "pin.H"
	extern "C" {
	#include "xed-interface.h"
	}
	#include <iostream>
	#include <iomanip>
	#include <fstream>
	#include <sys/mman.h>
	#include <stdio.h>
	#include <string.h>
	#include <unistd.h>
	#include <malloc.h>
	#include <stdlib.h>
	#include <errno.h>


#else

	#include "pin.H"
	extern "C" {
	#include "xed-interface.h"
	}
	namespace WIN {// for the virtual allocation and the getpagesize
	#include <Windows.h> 
	#include <MsRdc.h>
	#include <WinBase.h>
	}
	#include <assert.h>
	#include <iostream>
	#include <iomanip>
	#include <fstream>
	#include <stdio.h>
	#include <string.h>
	#include <MsRdc.h> // for get page size
	#include <stdlib.h>
	#define PAGE_LIMIT  80;

#endif // TARGET_WINDOWS

#include <iostream>
#include <iomanip>
#include <fstream>
using std::cerr;
using std::endl;
using std::hex;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
std::ofstream* out = 0;



/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
INT32 Usage()
{
    cerr << "This tool prints IA-32 and Intel(R) 64 instructions"
         << endl;
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}



/* ===================================================================== */
/* Instrumentation routine using INS_Disassemble API                     */
/* ===================================================================== */

VOID ImageLoad(IMG img, VOID *v)
{
 
	*out <<  "image file: "  << IMG_Name(img) << ":" << endl;


    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            // Open the RTN.
            RTN_Open( rtn ); 

			*out <<   RTN_Name(rtn) << ":" << endl;
						
            // Examine each instruction in the routine.
            for( INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) )
            {

                *out << hex << std::setw(8) << INS_Address (ins) << " ";
                *out << INS_Disassemble(ins) << endl;
	                    
            }
            // Close the RTN.
            RTN_Close( rtn );

        }
    }
}


/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{

    // Initialize pin & symbol manager
    out = new std::ofstream("probe-print.out");

    if( PIN_Init(argc,argv) )
        return Usage();

    PIN_InitSymbols();

    // Register ImageLoad
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Start the program, never returns
    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

