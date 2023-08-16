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
#include <map>
#include <vector>
#include <algorithm>
using std::cout;
using std::endl;
using std::cerr;
using std::string;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
struct LoopData{
    //bool in_loop;
    UINT32 count_seen;
    UINT32 inv_count;
    UINT32 round_iter_count;
    UINT32 last_round_iter;
    UINT32 diff_count;
    ADDRINT rtn_address;
    ADDRINT target_addr;
};

struct RTNData{
    string rtn_name;
    UINT64 inst_count;
    UINT64 call_count;
};

std::map<ADDRINT,LoopData> loop_map;
std::map<ADDRINT,RTNData> rtn_map;


const char* StripPath(const char* path)
{
    const char* file = strrchr(path, '/');
    if (file)
        return file + 1;
    else
        return path;
}

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
VOID RTN_count(RTNData * rtn)
{
    rtn->call_count++;	
}


VOID INS_count(RTNData * rtn)
{
    rtn->inst_count++;
	
}

/* ===================================================================== */

VOID LOOP1_count (UINT32 taken ,LoopData * loop )
{
    loop->round_iter_count++;
    if(!taken)
    {
        loop->inv_count++;
        if(loop->last_round_iter != loop->round_iter_count && loop->last_round_iter != 0)
        {
            loop->diff_count++;
        }
        loop->last_round_iter = loop->round_iter_count;
        loop->count_seen += loop->round_iter_count;
        loop->round_iter_count = 0;
    }
}

VOID LOOP2_count(UINT32 taken ,LoopData * loop)
{
    if(taken)
    {
        loop->inv_count++;
        if(loop->last_round_iter != loop->round_iter_count && loop->last_round_iter != 0)
        {
            loop->diff_count++;
        }
        loop->last_round_iter = loop->round_iter_count;
        loop->count_seen += loop->round_iter_count;
        loop->round_iter_count = 0;
    }
}
/* ===================================================================== */


VOID countRTN(RTN rtn, VOID *v)
{
    if(RTN_Valid(rtn) == false) return;
    
    RTN_Open(rtn);
    ADDRINT rtn_addr = RTN_Address(rtn);
    string rtn_name = RTN_Name(rtn);

    rtn_map[rtn_addr] = {rtn_name,0,0};
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)RTN_count, IARG_PTR, &rtn_map[rtn_addr], IARG_END);
    //count instruction in the routine
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        // Insert a call to docount to increment the instruction counter for this rtn
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)INS_count, IARG_PTR,&rtn_map[rtn_addr], IARG_END);
    }
    RTN_Close(rtn);
}


VOID loopsData(TRACE trc, VOID* v)
{
    ADDRINT target_addr;
    for( BBL bbl = TRACE_BblHead(trc); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        INS inst = BBL_InsTail(bbl);
        if(INS_Valid(inst))
        {
            RTN rtn = RTN_FindByAddress(INS_Address(inst));
            ADDRINT rtn_addr = RTN_Address(rtn);
            if (INS_IsDirectControlFlow(inst) && !INS_IsCall(inst))
            {
                target_addr = INS_DirectControlFlowTargetAddress(inst);
                if (target_addr < INS_Address(inst)) // found a loop
                {
                    if(loop_map.find(target_addr) == loop_map.end())
                    {
                        LoopData curr_loop_data = {0,0,0,0,0,rtn_addr,target_addr};
                        loop_map[target_addr] = curr_loop_data;
                    }
                    INS_InsertCall(inst, IPOINT_BEFORE, (AFUNPTR)LOOP1_count, IARG_BRANCH_TAKEN, IARG_PTR, &loop_map[target_addr], IARG_END);

                    for(INS ins_iter = inst; INS_Address(ins_iter) >= target_addr; ins_iter = INS_Prev(ins_iter))
                    {
                        if(INS_IsDirectControlFlow(ins_iter) && !INS_IsCall(ins_iter) && INS_DirectControlFlowTargetAddress(ins_iter) > INS_Address(inst))
                        {
                            INS_InsertCall(inst, IPOINT_BEFORE, (AFUNPTR)LOOP2_count, IARG_BRANCH_TAKEN, IARG_PTR, &loop_map[target_addr], IARG_END);
                        }
                    }
                }
            }
        }

    }
}


/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{ 
    std::vector<LoopData> loop_vec;
    for(std::map<ADDRINT,LoopData>::iterator itr = loop_map.begin(); itr!= loop_map.end();itr++)
    {
        if (itr->second.inv_count > 0)
            loop_vec.push_back(itr->second);
    }
    std::sort(loop_vec.begin(), loop_vec.end(), 
              [](const LoopData loop1, const LoopData loop2) { return loop1.count_seen > loop2.count_seen; });
    std::ofstream results("loop_count.csv");
    for (LoopData loop_entry: loop_vec)
    {
        if( loop_entry.count_seen != 0)
        {
            results << "0x" << std::hex << loop_entry.target_addr << ",";
            results <<  std::dec << loop_entry.count_seen << ",";
            results << loop_entry.inv_count << ",";
            results << (double)loop_entry.count_seen/(double)loop_entry.inv_count << ","; // mean_taken
            results << loop_entry.diff_count << ",";
            results << (rtn_map[loop_entry.rtn_address]).rtn_name << ",";
            results << "0x" << std::hex << loop_entry.rtn_address <<std::dec <<",";
            results << (rtn_map[loop_entry.rtn_address]).inst_count << ",";
            results << (rtn_map[loop_entry.rtn_address]).call_count << endl;
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

    TRACE_AddInstrumentFunction(loopsData,0);
    RTN_AddInstrumentFunction(countRTN, 0);
    PIN_AddFiniFunction(Fini, 0);


    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */