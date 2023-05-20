/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2013 Intel Corporation. All rights reserved.
 
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
/*
  @ORIGINAL_AUTHOR: Robert Muth
*/

/* ===================================================================== */
/*! @file
 *  This file contains an ISA-portable PIN tool for counting dynamic instructions
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
using std::cout;
using std::endl;
using std::cerr;
using std::string;


const UINT32 NUMOFRTN = 7000;
/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
struct RtnData{
    string img_name;
    ADDRINT image_address;
    string rtn_name;
    ADDRINT rtn_address;
    UINT64 inst_count;
    UINT64 call_count;
};

std::vector<RtnData> rtn_data(NUMOFRTN);

const char* StripPath(const char* path)
{
    const char* file = strrchr(path, '/');
    if (file)
        return file + 1;
    else
        return path;
}
/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr <<
        "This tool prints out the number of dynamic instructions executed to stderr.\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

/* ===================================================================== */

VOID RTN_count(UINT32 id)
{
    rtn_data[id].call_count++;	
}

VOID INS_count(UINT32 id)
{
    rtn_data[id].inst_count++;
	
}

/* ===================================================================== */

VOID PrintRTNnum(RTN rtn, VOID *v)
{
    RTN_Open(rtn);
    UINT32 id = RTN_Id(rtn);
    rtn_data[id].img_name = IMG_Name(SEC_Img(RTN_Sec(rtn)));
    rtn_data[id].image_address = IMG_LowAddress(SEC_Img(RTN_Sec(rtn)));
    rtn_data[id].rtn_name = RTN_Name(rtn);
    rtn_data[id].rtn_address = RTN_Address(rtn);
    rtn_data[id].inst_count = 0;
    rtn_data[id].call_count = 0;
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)RTN_count, IARG_UINT32, id, IARG_END);
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        // Insert a call to docount to increment the instruction counter for this rtn
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)INS_count, IARG_UINT32 ,id, IARG_END);
    }
 
    RTN_Close(rtn);
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{ 
    std::sort(rtn_data.begin(), rtn_data.end(), 
              [](const RtnData rtn1, const RtnData rtn2) { return rtn1.inst_count > rtn2.inst_count; });

    std::ofstream results("rtn-output.csv");
    for (RtnData rtn_entry: rtn_data)
    {
        if( rtn_entry.call_count != 0)
        {
            results << rtn_entry.img_name << ", ";
            results << "0x" << std::hex <<rtn_entry.image_address << ", ";
            results << rtn_entry.rtn_name << ", ";
            results << "0x" << std::hex <<rtn_entry.rtn_address << ", ";
            results << std::dec << rtn_entry.inst_count << ", ";
            results << rtn_entry.call_count << endl;
        }
    }
    results.close();
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }
    PIN_InitSymbols();

    RTN_AddInstrumentFunction(PrintRTNnum, 0);
    PIN_AddFiniFunction(Fini, 0);

    

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */