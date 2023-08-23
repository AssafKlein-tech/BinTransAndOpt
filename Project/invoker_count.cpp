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

class BBL_info{
    public:

    BBL bbl;
    ADDRINT rtn_address;
    ADDRINT BBL_head_address;
    ADDRINT branch_address;
    ADDRINT fallback_address;
    ADDRINT branch_target_address;
    xed_iclass_enum_t type_of_branch;
    UINT32 branch_times_not_taken;
    UINT32 branch_times_taken;
    bool single_branch; //ret or uncond jmp
    int position;
    bool visited;
    bool operator==(const BBL_info& other) const {
        return BBL_head_address == other.BBL_head_address;
    }
};


//struct RTNData{
//    string rtn_name;
//    UINT64 inst_count;
//    UINT64 call_count;
//};

//std::map<ADDRINT,LoopData> loop_map;
std::map<ADDRINT,Invoker> invokers_map;

std::map<ADDRINT,BBL_info> BBL_map;

std::map<ADDRINT,std::vector<BBL_info>> rtn_bbls_order;

std::map<RTN,bool> RTNList;

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
    
    if(taken)
    {
        bbl_inst->branch_times_taken++;
    }
    else{
        bbl_inst->branch_times_not_taken++;
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
    //else if( INS_IsIndirectControlFlow(ins) )
    //{
    //    if( INS_IsCall(ins) )
    //        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) inc_call_indirect, IARG_BRANCH_TAKEN,  IARG_END);
    //}

}

/**
VOID aux_reposition(BBL_info* bbl_info)
{
    if(bbl_info->visited) return;
    bbl_info->position = pos;
    bbl_info->visited =true;
    pos++;
    ADDRINT next_bbl;
    if(bbl_info->single_branch) // meaning the bbl ends in uncond jump
    {
        next_bbl = bbl_info->branch_target_address;
        aux_reposition(&BBL_map[next_bbl]);
    }
    else if(bbl_info->branch_times_taken>bbl_info->branch_times_not_taken)
    {
        next_bbl = bbl_info->branch_target_address;
        aux_reposition(&BBL_map[next_bbl] );
        if(BBL_map.find(bbl_info->fallback_address) != BBL_map.end())
        {
            next_bbl = bbl_info->fallback_address;
            aux_reposition(&BBL_map[next_bbl] );
        }
    }
    else
    {
        if(BBL_map.find(bbl_info->fallback_address) != BBL_map.end())
        {
            next_bbl = bbl_info->fallback_address;
            aux_reposition(&BBL_map[next_bbl]);
        }
        next_bbl = bbl_info->branch_target_address;
        aux_reposition(&BBL_map[next_bbl]);
    }
}


 
VOID repositionBBLS(TRACE trc, VOID *v) 
{
    BBL head = TRACE_BblHead(trc);
    ADDRINT head_addr = BBL_Address(head);
    aux_reposition(&BBL_map[head_addr]);

}
**/

VOID profBranches(TRACE trc, VOID *v)
{
    for( BBL bbl = TRACE_BblHead(trc); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        INS inst = BBL_InsTail(bbl);
        ADDRINT head = INS_Address(BBL_InsHead(bbl));
        if(INS_Valid(inst))
        {
            if(INS_IsDirectControlFlow(inst) )//|| INS_IsIndirectControlFlow(inst))
            {
                if(BBL_map.find(head) == BBL_map.end())
                {
                    xed_decoded_inst_t *xedd = INS_XedDec(inst);
		            xed_iclass_enum_t type_info = xed_decoded_inst_get_iclass(xedd);
                    ADDRINT rtn_addr =RTN_Address(RTN_FindByAddress(head));
                    ADDRINT ins_addr = INS_Address(inst);
                    ADDRINT fallback = INS_Address(INS_Next(inst));
                    ADDRINT target_addr = INS_DirectControlFlowTargetAddress(inst);
                    bool single = (type_info == XED_ICLASS_JMP );
                    BBL_info BBL_inst = {bbl,rtn_addr,head,ins_addr,fallback,target_addr,type_info,0,0,single,-1,false};
                    BBL_map[head] = BBL_inst;
                }
                INS_InsertCall(inst, IPOINT_BEFORE, (AFUNPTR)BranchCount, IARG_BRANCH_TAKEN, IARG_PTR, &BBL_map[head], IARG_END);
            }
            else if(INS_IsIndirectControlFlow(inst))
            {
                RTNList[RTN_FindByAddress(head)] =true;
            }
        }
        }
       
    }


VOID createRTNList(RTN rtn, VOID* v)
{
    if(!IMG_IsMainExecutable(IMG_FindByAddress(RTN_Address(rtn)))) 
        return;
    RTNList.push_back(rtn);
}
 

VOID ReorderBBLs(RTN rtn)
{
    if(!IMG_IsMainExecutable(IMG_FindByAddress(RTN_Address(rtn)))) 
        return;
    RTN_Open(rtn);
    ADDRINT curr_rtn_address = RTN_Address(rtn);
    for(std::map<ADDRINT,BBL_info>::iterator itr = BBL_map.begin(); itr!= BBL_map.end();itr++)
    {
        if (itr->second.rtn_address  == curr_rtn_address)
        {
            rtn_bbls_order[curr_rtn_address].push_back(itr->second);
        }
    }


    
    std::vector<BBL_info> temp_for_reorder;
    int pos =1;
    
    for(std::vector<BBL_info>::iterator itr2 = rtn_bbls_order[curr_rtn_address].begin();
                                    itr2!= rtn_bbls_order[curr_rtn_address].end();itr2++)
    {
        if(itr2->visited) 
        {
            continue;
        }
        
        itr2->position =pos;
        pos++;
        itr2->visited =true;
        temp_for_reorder.push_back(*itr2);
        std::vector<BBL_info>::iterator itr3;
        if(itr2->single_branch)
        {
            continue;
        }
        
        else if(itr2->branch_times_taken > itr2->branch_times_not_taken)
            {

                itr3 = std::find(rtn_bbls_order[curr_rtn_address].begin(), rtn_bbls_order[curr_rtn_address].end(),
                 BBL_map[itr2->branch_target_address]);
            }
        else{
            itr3 = std::find(rtn_bbls_order[curr_rtn_address].begin(), rtn_bbls_order[curr_rtn_address].end(),
                 BBL_map[itr2->fallback_address]);
        }
        
    

        while(!(itr3->visited))
        {
            if(itr3->single_branch)
            {
                break;
            }
            else if(itr3->branch_times_taken > itr3->branch_times_not_taken)
            {
                 itr3->visited =true;
                 itr3->position =pos;
                 temp_for_reorder.push_back(*itr3);
                 itr3 = std::find(rtn_bbls_order[curr_rtn_address].begin(), rtn_bbls_order[curr_rtn_address].end(),
                 BBL_map[itr3->branch_target_address]);
            }
            else{
                 itr3->visited =true;
                 itr3->position =pos;
                 temp_for_reorder.push_back(*itr3);
                 itr3 = std::find(rtn_bbls_order[curr_rtn_address].begin(), rtn_bbls_order[curr_rtn_address].end(),
                 BBL_map[itr3->fallback_address]);
            }
            pos++;
        }
        pos++;
    }
    rtn_bbls_order[curr_rtn_address] = temp_for_reorder;
    RTN_Close(rtn);
    
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
            too_hot_to_handle[itr2->first] = ( sorted_vec[0] > 20* sorted_vec[1]);
        }     
    }


    std::vector<BBL_info> bbl_vec;
    for(std::map<ADDRINT,BBL_info>::iterator itr2 = BBL_map.begin(); itr2!= BBL_map.end();itr2++)
    {
        bbl_vec.push_back(itr2->second);
    }
    std::sort(bbl_vec.begin(), bbl_vec.end(), 
              [](const BBL_info bbl1, const BBL_info bbl2) { return bbl1.position < bbl2.position; });

    std::ofstream results("Count.csv");
    for (Invoker inv_entry: inv_vec)
    {
        if( inv_entry.num_invokes != 0 && too_hot_to_handle[inv_entry.target_addr] && (inv_entry.rtn_name.length() < 3 || inv_entry.rtn_name.substr(inv_entry.rtn_name.length() - 3) != "plt"))
        {
            results << "0x" << std::hex << inv_entry.invoker_rtn_address << ",";
            results << "0x"  << inv_entry.invoker_address << ",";
            results <<  std::dec << inv_entry.num_invokes << ",";
            results << "0x" << std::hex << inv_entry.target_addr << ",";
            results << inv_entry.rtn_name << endl;
            
        }
    }
    results.close();

    std::ofstream resultsbranches("CountBranches.csv"); 
    for (BBL_info bbl_entry: bbl_vec)
    {
            resultsbranches << "0x" << std::hex << bbl_entry.rtn_address << ",";
            resultsbranches << "0x" <<  bbl_entry.BBL_head_address << ",";
            resultsbranches << "0x"  << bbl_entry.branch_address << ",";
            resultsbranches << "0x"  << bbl_entry.branch_target_address << ",";
             resultsbranches << GetEnumAsString(bbl_entry.type_of_branch) << ",";
            resultsbranches <<  std::dec << bbl_entry.branch_times_taken << ","; 
            resultsbranches << bbl_entry.branch_times_not_taken << ",";
            if(bbl_entry.single_branch)
            {
                resultsbranches << "TRUE" << ",";
            }
            else
            {
                resultsbranches << "FALSE" << ",";
            }
            resultsbranches << bbl_entry.position << endl;
        
    }
    resultsbranches.close();

    for(std::vector<RTN>::iterator iter = RTNList.begin(); iter != RTNList.end(); iter++;)
    {
            ReorderBBLs(*iter);
    }

    std::ofstream resultsRTNBBLOrder("RTNBBLOrder.csv"); 
    for(std::map<ADDRINT,std::vector<BBL_info>>::iterator itr_rtn = rtn_bbls_order.begin(); 
            itr_rtn!= rtn_bbls_order.end(); itr_rtn++)
    {
        resultsRTNBBLOrder << "0x" << std::hex << (rtn_bbls_order.begin()->second.begin())->rtn_address << ",";
        for (BBL_info bbl_entry: itr_rtn->second)
        {
                //resultsRTNBBLOrder << "0x" << std::hex << bbl_entry.rtn_address << ",";
                resultsRTNBBLOrder << "0x" <<  bbl_entry.BBL_head_address << ",";
                resultsRTNBBLOrder << "0x"  << bbl_entry.branch_address << ",";
                resultsRTNBBLOrder << "0x"  << bbl_entry.branch_target_address << ",";
                resultsRTNBBLOrder << "0x"  << bbl_entry.fallback_address << ",";
                resultsRTNBBLOrder << GetEnumAsString(bbl_entry.type_of_branch) << ",";
                resultsRTNBBLOrder <<  std::dec << bbl_entry.branch_times_taken << ","; 
                resultsRTNBBLOrder << bbl_entry.branch_times_not_taken << ",";
                if(bbl_entry.single_branch)
                {
                    resultsRTNBBLOrder << "TRUE" << ",";
                }
                else
                {
                    resultsRTNBBLOrder << "FALSE" << ",";
                }
                resultsRTNBBLOrder << bbl_entry.position;
            
        }
        resultsRTNBBLOrder << endl;
    }
    resultsbranches.close();
    
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
