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

class BBL_info{
    public:

    BBL bbl;
    ADDRINT rtn_address;
    ADDRINT BBL_head_address;
    ADDRINT branch_address;
    ADDRINT fallthrough_address;
    ADDRINT branch_target_address;
    xed_iclass_enum_t type_of_branch;
    UINT32 branch_times_not_taken;
    UINT32 branch_times_taken;
    bool is_call; //ret or uncond jmp
    int position;
    bool visited;
    bool operator==(const BBL_info& other) const {
        return BBL_head_address == other.BBL_head_address;
    }
};

struct RTN_reorder_info {
    ADDRINT rtn_addr;
    UINT64 num_inst;

};


//std::map<ADDRINT,LoopData> loop_map;
std::map<ADDRINT,Invoker> invokers_map;

std::map<ADDRINT,BBL_info> BBL_map;

std::map<ADDRINT,std::vector<BBL_info>> rtn_bbls_order;

std::map<ADDRINT, RTN_reorder_info> RTN_map;


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
    RTN rtn = RTN_FindByAddress(INS_Address(ins));
    if(RTN_Valid(rtn) == false) return;
    IMG img = IMG_FindByAddress(RTN_Address(rtn));
    if(!IMG_IsMainExecutable(img))
    {
        return;
    }
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


VOID INS_count(RTN_reorder_info * rtn_info)
{
    rtn_info->num_inst++;
	
}

VOID countRtnsForReorder(RTN rtn, VOID *v)
{

    if(RTN_Valid(rtn) == false) return;
    
    RTN_Open(rtn);
    ADDRINT rtn_addr = RTN_Address(rtn);
    IMG img = IMG_FindByAddress(rtn_addr);
    if(!IMG_IsMainExecutable(img))
    {
        RTN_Close(rtn);
        return;
    }
    RTN_map[rtn_addr] = {rtn_addr,0};
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        // Insert a call to docount to increment the instruction counter for this rtn
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)INS_count, IARG_PTR,&RTN_map[rtn_addr], IARG_END);
    }
    RTN_Close(rtn);
}


VOID checkvalidrtn(INS ins, VOID *v)
{
    RTN rtn = RTN_FindByAddress(INS_Address(ins));
    if(RTN_Valid(rtn) == false) return;
    
    RTN_Open(rtn);
    ADDRINT rtn_addr = RTN_Address(rtn);
    IMG img = IMG_FindByAddress(rtn_addr);
    if(!IMG_IsMainExecutable(img))
    {
        RTN_Close(rtn);
        return;
    }
    RTN_Close(rtn);
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
            if(INS_IsDirectControlFlow(inst) ||  INS_IsRet(inst))
            {
                if(BBL_map.find(head) == BBL_map.end())
                {
                    xed_decoded_inst_t *xedd = INS_XedDec(inst);
		            xed_iclass_enum_t type_info = xed_decoded_inst_get_iclass(xedd);
                    ADDRINT rtn_addr =RTN_Address(RTN_FindByAddress(head));
                    ADDRINT ins_addr = INS_Address(inst);
                    ADDRINT fallthrough = INS_NextAddress(inst);
                    if(fallthrough == 0) cout<< "shit";
                    ADDRINT target_addr;
                    if(INS_IsRet(inst)) 
                    {
                        target_addr =-1;
                    }
                    else{
                         target_addr = INS_DirectControlFlowTargetAddress(inst);
                    }
                    bool is_call = (type_info == XED_ICLASS_CALL_NEAR );
                    BBL_info BBL_inst = {bbl,rtn_addr,head,ins_addr,fallthrough,target_addr,type_info,0,0,is_call,-1,false};
                    BBL_map[head] = BBL_inst;
                }
                INS_InsertCall(inst, IPOINT_BEFORE, (AFUNPTR)BranchCount, IARG_BRANCH_TAKEN, IARG_PTR, &BBL_map[head], IARG_END);
            }
        }

       
    }
}

class heap_element
{
    public:

    ADDRINT bbl_addr;
    UINT64 in_degree;
    bool operator<(const heap_element& other) const {
        return in_degree < other.in_degree;
    }
};
 

VOID ReorderBBLs(ADDRINT curr_rtn_address)
{
    cout<<"Here"<< endl;
    std::vector<BBL_info> rtn_bbls;
    for(std::map<ADDRINT,BBL_info>::iterator itr = BBL_map.begin(); itr!= BBL_map.end();itr++)
    {
        if (itr->second.rtn_address  == curr_rtn_address)
        {
            rtn_bbls.push_back(itr->second);
        }
    }
    
    std::vector<heap_element> heap;
    cout<<"Here2"<< endl;
    cout<< "0x" << std::hex << curr_rtn_address <<endl;
    if(rtn_bbls.empty()) return;
    heap_element first = {rtn_bbls.begin()->BBL_head_address, 0};
    
    std::make_heap(heap.begin(),heap.end(), [](const heap_element el1, const heap_element el2) { return el1.in_degree > el2.in_degree; });
    heap.push_back(first);
    std::push_heap(heap.begin(),heap.end());
    

    while (!heap.empty())
    {
        cout<<"in the while";
        //take out
        heap_element top = heap.front();
        std::pop_heap(heap.begin(),heap.end());
        heap.pop_back();
        if(BBL_map[top.bbl_addr].visited)
            continue;
        //insert final vector
        rtn_bbls_order[curr_rtn_address].push_back(BBL_map[top.bbl_addr]);
        //make visited
        BBL_map[top.bbl_addr].visited = true;
        //insert children
        heap_element target = {BBL_map[top.bbl_addr].branch_target_address, BBL_map[top.bbl_addr].branch_times_taken};
        heap_element fallthrough = {BBL_map[top.bbl_addr].fallthrough_address, BBL_map[top.bbl_addr].branch_times_not_taken};
        if (BBL_map[top.bbl_addr].is_call)
        {
           fallthrough.in_degree = BBL_map[top.bbl_addr].branch_times_taken;
        }
        if (RTN_Address(RTN_FindByAddress(target.bbl_addr)) == curr_rtn_address)
        {
            cout << "insert target bbl" << endl;
            heap.push_back(target);
            std::push_heap(heap.begin(),heap.end());
        }
        if(RTN_Address(RTN_FindByAddress(fallthrough.bbl_addr)) == curr_rtn_address)
        {
            cout << "insert next bbl" << endl;
            heap.push_back(fallthrough);
            std::push_heap(heap.begin(),heap.end());
        }
        else
        {
           
        }
        
    }
    if(!rtn_bbls_order[curr_rtn_address].empty())
    {
        if(rtn_bbls_order[curr_rtn_address].size() == rtn_bbls.size())
        {
            cout<< "OK"<<endl;
        }
        else 
        {
            cout<< "NOT OK"<<endl;
        }
    }
}






/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{ 
    std::map<ADDRINT,std::vector<int>> called_map;
    std::map<ADDRINT,bool> too_hot_to_handle;
    std::map<ADDRINT,bool> single_call_site;
    std::vector<Invoker> inv_vec;
    //sort invoker map by the time of invokes on each entry and crate called_map
    for(std::map<ADDRINT,Invoker>::iterator itr = invokers_map.begin(); itr!= invokers_map.end();itr++)
    {
        if (itr->second.num_invokes > 0)
            inv_vec.push_back(itr->second);
        called_map[itr->second.target_addr].push_back(itr->second.num_invokes);
    }
    std::sort(inv_vec.begin(), inv_vec.end(), 
              [](const Invoker inv1, const Invoker inv2) { return inv1.num_invokes > inv2.num_invokes; });

    //sort the called map and find rtn that have hot call sites
    for(std::map<ADDRINT,std::vector<int>>::iterator itr2 = called_map.begin(); itr2!= called_map.end();itr2++)
    {
        std::vector<int> sorted_vec = itr2->second;
        std::sort(sorted_vec.begin(), sorted_vec.end(), []( int a, int b ){return a > b;});
        if(sorted_vec.size() == 1)
             single_call_site[itr2->first] = true;
        else
        {
            int sum_of_elems = 0;
            std::for_each(sorted_vec.begin() ++ , sorted_vec.end(), [&] (int n) {sum_of_elems += n;});
            too_hot_to_handle[itr2->first] = ( sorted_vec[0] > 10* sum_of_elems);
        }     
    }

    int NUM_INLINED_FUNC = 10;
    int counter = 0;

    std::vector<Invoker> final_candidates;
    for (Invoker inv_entry: inv_vec)
    {
        if ( counter == NUM_INLINED_FUNC)
            break;
        if( inv_entry.num_invokes != 0 && (too_hot_to_handle[inv_entry.target_addr] || single_call_site[inv_entry.target_addr])  && (inv_entry.rtn_name.length() < 3 || inv_entry.rtn_name.substr(inv_entry.rtn_name.length() - 3) != "plt") && inv_entry.target_addr != inv_entry.invoker_rtn_address && std::find(non_valid_rtn.begin(), non_valid_rtn.end(), inv_entry.target_addr) == non_valid_rtn.end())
        {
            final_candidates.push_back(inv_entry);
            counter++;
        }
    }


    //add inline candidate invokation number to original count
    for (Invoker inv_entry: final_candidates)
    {
        if(single_call_site[inv_entry.target_addr])
        {
            if(RTN_map.find(inv_entry.target_addr) != RTN_map.end())
            {
                RTN_map.erase(inv_entry.target_addr);
                continue;
            }
        }
        if(RTN_map.find(inv_entry.invoker_rtn_address) != RTN_map.end())
        {
              RTN_map[inv_entry.invoker_rtn_address].num_inst += RTN_map[inv_entry.target_addr].num_inst/inv_entry.num_invokes;
              RTN_map[inv_entry.target_addr].num_inst -= RTN_map[inv_entry.target_addr].num_inst/inv_entry.num_invokes;
        }
       
    }

    // reorder RTN's for translation
    std::vector<RTN_reorder_info> rtn_vec;
    for(std::map<ADDRINT,RTN_reorder_info>::iterator itr3 = RTN_map.begin(); itr3!= RTN_map.end();itr3++)
    {
        if(std::find(non_valid_rtn.begin(), non_valid_rtn.end(), itr3->first) == non_valid_rtn.end() )
            rtn_vec.push_back(itr3->second);
    }
    std::sort(rtn_vec.begin(), rtn_vec.end(), 
              [](const RTN_reorder_info rtn1, const RTN_reorder_info rtn2) { return rtn1.num_inst > rtn2.num_inst; });

    //print RTN for translation and RTN for inlining
    std::ofstream results("Count.csv");
    int vec_size = rtn_vec.size();
    results << vec_size;
    for (RTN_reorder_info rtn_entry: rtn_vec)
    {
        results << ",0x" << std::hex << rtn_entry.rtn_addr;
    }
    results <<endl;

    for (Invoker inv_entry: final_candidates)
    {
        results << "0x" << std::hex << inv_entry.invoker_rtn_address << ",";
        results << "0x"  << inv_entry.invoker_address << ",";
        results <<  std::dec << inv_entry.num_invokes << ",";
        results << "0x" << std::hex << inv_entry.target_addr << ",";
        results << inv_entry.rtn_name << endl;

    }
    results << endl ;

    results.close();

    

    for(std::vector<RTN_reorder_info>::iterator iterator = rtn_vec.begin(); iterator != rtn_vec.end(); iterator++)
    {
        ReorderBBLs(iterator->rtn_addr);
    }

    std::ofstream resultsRTNBBLOrder("RTNBBLOrder.csv"); 
    for(std::map<ADDRINT,std::vector<BBL_info>>::iterator itr_rtn = rtn_bbls_order.begin(); 
            itr_rtn!= rtn_bbls_order.end(); itr_rtn++)
    {
        resultsRTNBBLOrder << "0x" << std::hex << itr_rtn->first;
        for (BBL_info bbl_entry: itr_rtn->second)
        {
                resultsRTNBBLOrder << "0x" <<  bbl_entry.BBL_head_address << ",";
                //resultsRTNBBLOrder << "0x"  << bbl_entry.branch_address << ",";
                resultsRTNBBLOrder << "0x"  << bbl_entry.branch_target_address << ",";
                resultsRTNBBLOrder << "0x"  << bbl_entry.fallthrough_address << ",";
                resultsRTNBBLOrder << bbl_entry.type_of_branch << ",";
                //resultsRTNBBLOrder <<  std::dec << bbl_entry.branch_times_taken << ","; 
                //resultsRTNBBLOrder << bbl_entry.branch_times_not_taken << ",";
                //if(bbl_entry.single_branch)
                //{
                //    resultsRTNBBLOrder << "TRUE" << ",";
                //}
                //else
                //{
                    //resultsRTNBBLOrder << "FALSE" << ",";
                //}
                //resultsRTNBBLOrder << bbl_entry.position;
        }
        resultsRTNBBLOrder << endl; 
        
    }
    resultsRTNBBLOrder.close();
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
