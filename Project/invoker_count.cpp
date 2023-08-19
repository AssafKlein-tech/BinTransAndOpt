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

extern "C" {
#include "xed-interface.h"
}

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
struct Invoker{
    ADDRINT invoker_rtn_address;
    ADDRINT invoker_address;
    UINT32 num_invokes;
    ADDRINT target_addr;
    string rtn_name;
};

std::vector<ADDRINT> non_valid_rtn;

struct BBL_info{
    ADDRINT rtn_address;
    ADDRINT BBL_head_address;
    ADDRINT branch_address;
    ADDRINT branch_target_address;
    xed_iclass_enum_t type_of_branch;
    UINT32 branch_times_seen;
    UINT32 branch_times_taken;
    bool need_reorder;
};


//struct RTNData{
//    string rtn_name;
//    UINT64 inst_count;
//    UINT64 call_count;
//};

//std::map<ADDRINT,LoopData> loop_map;
std::map<ADDRINT,Invoker> invokers_map;

std::map<ADDRINT,BBL_info> BBL_map;

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

string GetEnumAsString(xed_iclass_enum_t type)
{
    string enum_str;

    switch(type)
    {
        case XED_ICLASS_JB:
				enum_str = "XED_ICLASS_JB";		
				break;

			case XED_ICLASS_JBE:
				enum_str = "XED_ICLASS_JBE";
				break;

			case XED_ICLASS_JL:
				enum_str = "XED_ICLASS_JL";
				break;
		
			case XED_ICLASS_JLE:
				enum_str = "XED_ICLASS_JLE";
				break;

			case XED_ICLASS_JNB: 
			    enum_str = "XED_ICLASS_JNB";
				break;

			case XED_ICLASS_JNBE: 
				enum_str = "XED_ICLASS_JNBE";
				break;

			case XED_ICLASS_JNL:
			    enum_str = "XED_ICLASS_JNL";
				break;

			case XED_ICLASS_JNLE:
				enum_str = "XED_ICLASS_JNLE";
				break;

			case XED_ICLASS_JNO:
				enum_str = "XED_ICLASS_JNO";
				break;

			case XED_ICLASS_JNP: 
				enum_str = "XED_ICLASS_JNP";
				break;

			case XED_ICLASS_JNS: 
				enum_str = "XED_ICLASS_JNS";
				break;

			case XED_ICLASS_JNZ:
				enum_str = "XED_ICLASS_JNZ";
				break;

			case XED_ICLASS_JO:
				enum_str = "XED_ICLASS_JO";
				break;

			case XED_ICLASS_JP: 
			    enum_str = "XED_ICLASS_JP";
				break;

			case XED_ICLASS_JS: 
				enum_str = "XED_ICLASS_JS";
				break;

			case XED_ICLASS_JZ:
				enum_str = "XED_ICLASS_JZ";
                break;

            case XED_ICLASS_JMP:
                enum_str = "XED_ICLASS_JMP";
                break;

            default:
                enum_str = "NONE";
                break;

				
    }
    return enum_str;
}

/* ===================================================================== */
/* ===================================================================== */


//INC(_call)
VOID inc_call(Invoker * inv) {
    inv->num_invokes++;
}

VOID BranchCount(UINT32 taken ,BBL_info * bbl_inst)
{
    bbl_inst->branch_times_seen++;
    if(taken)
    {
        bbl_inst->branch_times_taken++;
    }
}

/* ===================================================================== */
/* ===================================================================== */



VOID countInvokers(INS ins, VOID *v)
{
    if (INS_IsDirectControlFlow(ins))
    {
        if( INS_IsCall(ins) )
        {
            ADDRINT address = INS_Address(ins);
            RTN rtn = RTN_FindByAddress(address);
            ADDRINT rtn_addr = RTN_Address(rtn);
            if(invokers_map.find(address) == invokers_map.end())
            {
                ADDRINT target_address = INS_DirectControlFlowTargetAddress(ins);
                string rtn_name = RTN_FindNameByAddress(target_address);
                Invoker curr_invoker_data = {rtn_addr,address,0,target_address,rtn_name};
                invokers_map[address] = curr_invoker_data;
            }
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) inc_call,IARG_PTR, &invokers_map[address], IARG_END);
        }
    }       
}

VOID checkvalidrtn(INS ins, VOID *v)
{
    bool invalid = false;
    if (INS_IsControlFlow(ins))
    {
        if( (INS_IsIndirectControlFlow(ins) && ! INS_IsRet(ins)) || INS_RepPrefix(ins )|| INS_RepnePrefix (ins))
        {
            invalid = true;
        }
        else if (INS_IsDirectBranch(ins))
        {
            ADDRINT target_address = INS_DirectControlFlowTargetAddress(ins);
            RTN target_rtn = RTN_FindByAddress(target_address);
            ADDRINT address = INS_Address(ins);
            RTN rtn = RTN_FindByAddress(address);

            //ADDRINT target_addr = RTN_Address(target_rtn);
            if ( RTN_Address(rtn) != RTN_Address(target_rtn))
            {
                //cout<< "the rtn  " << std::hex<< target_addr << " has a jump outside" << endl;
                invalid = true;
            }
        }
        else { return;}
        ADDRINT address = INS_Address(ins);
        RTN rtn = RTN_FindByAddress(address);
        ADDRINT rtn_addr = RTN_Address(rtn);
        if (invalid &&  std::find(non_valid_rtn.begin(), non_valid_rtn.end(), rtn_addr) == non_valid_rtn.end())
            non_valid_rtn.push_back(rtn_addr);     
    }  
}



VOID profBranches(TRACE trc, VOID *v)
{
    for( BBL bbl = TRACE_BblHead(trc); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        INS inst = BBL_InsTail(bbl);
        ADDRINT head = INS_Address(BBL_InsHead(bbl));
        if(INS_Valid(inst))
        {
            if(INS_IsDirectControlFlow(inst) && !INS_IsCall(inst) && (INS_DirectControlFlowTargetAddress(inst)>INS_Address(inst)))
            {
                if(BBL_map.find(head) == BBL_map.end())
                {
                    xed_decoded_inst_t *xedd = INS_XedDec(inst);
		            xed_iclass_enum_t type_info = xed_decoded_inst_get_iclass(xedd);
                    ADDRINT rtn_addr =RTN_Address(RTN_FindByAddress(head));
                    ADDRINT ins_addr = INS_Address(inst);
                    ADDRINT target_addr = INS_DirectControlFlowTargetAddress(inst);
                    BBL_info BBL_inst = {rtn_addr,head,ins_addr,target_addr,type_info,0,0,false};
                    BBL_map[head] = BBL_inst;
                }
                INS_InsertCall(inst, IPOINT_BEFORE, (AFUNPTR)BranchCount, IARG_BRANCH_TAKEN, IARG_PTR, &BBL_map[head], IARG_END);
            }
        }

}
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{ 
    std::map<ADDRINT,std::vector<int>> called_map;
    std::map<ADDRINT,bool> too_hot_to_handle;
    std::vector<Invoker> inv_vec;
    for(std::map<ADDRINT,Invoker>::iterator itr = invokers_map.begin(); itr!= invokers_map.end();itr++)
    {
        if (itr->second.num_invokes > 0)
            inv_vec.push_back(itr->second);
        called_map[itr->second.target_addr].push_back(itr->second.num_invokes);
    }
    std::sort(inv_vec.begin(), inv_vec.end(), 
              [](const Invoker inv1, const Invoker inv2) { return inv1.num_invokes > inv2.num_invokes; });
              
    for(std::map<ADDRINT,std::vector<int>>::iterator itr2 = called_map.begin(); itr2!= called_map.end();itr2++)
    {
        std::vector<int> sorted_vec = itr2->second;
        std::sort(sorted_vec.begin(), sorted_vec.end(), []( int a, int b ){return a > b;});
        if(sorted_vec.size() == 1)
             too_hot_to_handle[itr2->first] = true;
        else
        {
            int sum_of_elems = 0;
            std::for_each(sorted_vec.begin() ++ , sorted_vec.end(), [&] (int n) {sum_of_elems += n;});
            too_hot_to_handle[itr2->first] = ( sorted_vec[0] > 10* sum_of_elems);
        }     
    }


    std::vector<BBL_info> bbl_vec;
    for(std::map<ADDRINT,BBL_info>::iterator itr2 = BBL_map.begin(); itr2!= BBL_map.end();itr2++)
    {
        if((double((itr2->second).branch_times_taken))/(double((itr2->second).branch_times_seen)) > 0.5)
        {
            itr2->second.need_reorder = true;
        }
        bbl_vec.push_back(itr2->second);
    }

    std::ofstream results("Count.csv");
    int count = 0;
    for (Invoker inv_entry: inv_vec)
    {
        if (count < 29)
        {
            results << "0x" << std::hex << inv_entry.target_addr << ",";
            count++;
        }
        else 
        {
            results << "0x" << std::hex << inv_entry.target_addr << endl;
            break;
        }

    }
    for (Invoker inv_entry: inv_vec)
    {
        if( inv_entry.num_invokes != 0 && too_hot_to_handle[inv_entry.target_addr] && (inv_entry.rtn_name.length() < 3 || inv_entry.rtn_name.substr(inv_entry.rtn_name.length() - 3) != "plt") && inv_entry.target_addr != inv_entry.invoker_rtn_address && std::find(non_valid_rtn.begin(), non_valid_rtn.end(), inv_entry.target_addr) == non_valid_rtn.end())
        {
            results << "0x" << std::hex << inv_entry.invoker_rtn_address << ",";
            results << "0x"  << inv_entry.invoker_address << ",";
            results <<  std::dec << inv_entry.num_invokes << ",";
            results << "0x" << std::hex << inv_entry.target_addr << ",";
            results << inv_entry.rtn_name << endl;
            
        }
    }
    results << endl << endl << endl << endl << endl;

    for (BBL_info bbl_entry: bbl_vec)
    {
            results << "0x" << std::hex << bbl_entry.rtn_address << ",";
            results << "0x" <<  bbl_entry.BBL_head_address << ",";
            results << "0x"  << bbl_entry.branch_address << ",";
            results << "0x"  << bbl_entry.branch_target_address << ",";
             results << GetEnumAsString(bbl_entry.type_of_branch) << ",";
            results <<  std::dec << bbl_entry.branch_times_seen << ",";
            results << bbl_entry.branch_times_taken << ",";
            if(bbl_entry.need_reorder)
            {
                results << "TRUE" << endl;
            }
            else
            {
                results << "FALSE" << endl;
            }
            
        
    }
    results.close();
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
