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
#include <queue>
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
    ADDRINT fallthrough_address;
};

std::vector<ADDRINT> non_valid_rtn;

class BBL_info{
public:

    BBL bbl;
    string rtn_name;
    ADDRINT rtn_address;
    ADDRINT BBL_head_address;
    ADDRINT last_address;
    ADDRINT fallthrough_address;
    ADDRINT branch_target_address;
    xed_iclass_enum_t type_of_branch;
    UINT32 branch_times_not_taken;
    UINT32 branch_times_taken;
    bool is_call; //ret or uncond jmp
    int is_cond;
    bool visited;
    bool visited_through_inline;
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



std::map<ADDRINT,std::vector<int>> called_map;
std::map<ADDRINT,bool> too_hot_to_handle;
std::map<ADDRINT,bool> single_call_site;
std::vector<Invoker> inv_vec;
std::vector<Invoker> final_candidates;
std::vector<string> final_cand_rtn_name;

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
            //INS next = INS_Next(ins);
            ADDRINT fallthrough_addr = INS_NextAddress(ins);
            if(invokers_map.find(address) == invokers_map.end())
            {
                ADDRINT target_address = INS_DirectControlFlowTargetAddress(ins);
                string rtn_name = RTN_FindNameByAddress(target_address);
                Invoker curr_invoker_data = {rtn_addr,address,0,target_address,rtn_name,fallthrough_addr};
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
        INS last_ins = BBL_InsTail(bbl);
        ADDRINT head = INS_Address(BBL_InsHead(bbl));
        if(INS_Valid(last_ins))
        {
            if(BBL_map.find(head) == BBL_map.end())
            {
                //if (head == 4244574)
                //{
                //    cout << "no bugs found" << endl;
                //}
                xed_decoded_inst_t *xedd = INS_XedDec(last_ins);
                xed_iclass_enum_t type_info = xed_decoded_inst_get_iclass(xedd);
                ADDRINT rtn_addr =RTN_Address(RTN_FindByAddress(head));
                string rtn_name = RTN_FindNameByAddress(head);
                ADDRINT last_addr = INS_Address(last_ins);
                ADDRINT fallthrough ;
                if(INS_HasFallThrough(last_ins) || INS_IsCall(last_ins))
                    fallthrough = INS_NextAddress(last_ins);
                else
                    fallthrough = -1;
                ADDRINT target_addr;
                if (INS_IsDirectControlFlow(last_ins)){
                    target_addr = INS_DirectControlFlowTargetAddress(last_ins);
                }
                else{
                    target_addr = -1;
                }
                bool is_call = INS_IsCall(last_ins);
                xed_category_enum_t category_enum = xed_decoded_inst_get_category(xedd);
                bool is_cond = (category_enum == XED_CATEGORY_COND_BR);
                BBL_info BBL_inst = {bbl,rtn_name,rtn_addr,head,last_addr,fallthrough,target_addr,type_info,0,0,is_call,is_cond,false,false};
                BBL_map[head] = BBL_inst;
            }
            INS_InsertCall(last_ins, IPOINT_BEFORE, (AFUNPTR)BranchCount, IARG_BRANCH_TAKEN, IARG_PTR, &BBL_map[head], IARG_END);
        }
    }
}

class heap_element
{
    public:

    ADDRINT bbl_addr;
    UINT64 in_degree;
    bool operator<(const heap_element& other) const {
        return in_degree <= other.in_degree;
    }
};
 
void  eliminate_overlapping_bbls(std::vector<BBL_info> rtn_bbls)
{
    std::map<ADDRINT, std::vector<size_t>> overlap;

    //create a map of the indices of the bbls with the same last address
    for(size_t i = 0; i< rtn_bbls.size(); i++)
    {
        overlap[rtn_bbls[i].last_address].push_back(i); 
    }

    //iterates over last address
    for(std::map<ADDRINT,std::vector<size_t>>::iterator itr = overlap.begin(); itr!= overlap.end();itr++)
    {
        if (itr->second.size() == 1)
            continue;

        //index vector
        std::vector<size_t> bbl_overlap = itr->second;
        //sort vector by the start address
        std::sort(bbl_overlap.begin(), bbl_overlap.end(), 
              [&](const size_t bbl1, const size_t bbl2) { return rtn_bbls[bbl1].BBL_head_address < rtn_bbls[bbl2].BBL_head_address; });
        
        size_t last_indx =  bbl_overlap.size() -1;
        //make the start_ins point to the first address in the outer bbl
        for ( size_t j = 0; j < bbl_overlap.size() -1 ; j++) 
        {  
            BBL_map[rtn_bbls[bbl_overlap[last_indx]].BBL_head_address].branch_times_taken += BBL_map[rtn_bbls[bbl_overlap[j]].BBL_head_address].branch_times_taken;
            BBL_map[rtn_bbls[bbl_overlap[last_indx]].BBL_head_address].branch_times_not_taken += BBL_map[rtn_bbls[bbl_overlap[j]].BBL_head_address].branch_times_not_taken;


            // update  last address
            BBL_map[rtn_bbls[bbl_overlap[j]].BBL_head_address].last_address = 0;
            BBL_map[rtn_bbls[bbl_overlap[j]].BBL_head_address].fallthrough_address = BBL_map[rtn_bbls[bbl_overlap[j+1]].BBL_head_address].BBL_head_address;
            //branch taken = -1
            BBL_map[rtn_bbls[bbl_overlap[j]].BBL_head_address].branch_times_taken = -1;
            //target address = -1 
            BBL_map[rtn_bbls[bbl_overlap[j]].BBL_head_address].branch_target_address = -1;

        }
    }
}

int redirection = 1;

std::map<int, ADDRINT> redirection_map;

VOID ReorderBBLs(ADDRINT curr_rtn_address)
{
    std::vector<ADDRINT> used_routines;

    std::vector<BBL_info> rtn_bbls;
    for(std::map<ADDRINT,BBL_info>::iterator itr = BBL_map.begin(); itr!= BBL_map.end();itr++)
    {
        if (itr->second.rtn_address  == curr_rtn_address)
        {
            rtn_bbls.push_back(itr->second);
        }
    }
    if(rtn_bbls.begin()->visited_through_inline)
    {
        for(std::vector<BBL_info>::iterator itr2 = rtn_bbls.begin(); itr2!= rtn_bbls.end();itr2++)
        {
            if (itr2->visited  == true)
            {
                itr2->visited = false;
            }
        }
    }

    
    std::priority_queue<heap_element> heap;
    if(rtn_bbls.empty()) return;
    eliminate_overlapping_bbls(rtn_bbls);
    heap_element first = {rtn_bbls.begin()->BBL_head_address, 0};
    

    heap.push(first);
    while (!heap.empty())
    {
        //take out
        heap_element top = heap.top();
        heap.pop();
        if(top.bbl_addr < ADDRINT(redirection))
        {
            if( redirection_map[top.bbl_addr] != 0)
            {
                BBL_info BBL_inst = {-1,BBL_map[top.bbl_addr].rtn_name,curr_rtn_address,top.bbl_addr,top.bbl_addr,0,redirection_map[top.bbl_addr],XED_ICLASS_LAST,0,0,false,false,false,false};
                BBL_map[top.bbl_addr] = BBL_inst;
                rtn_bbls_order[curr_rtn_address].push_back(BBL_map[top.bbl_addr]);
                redirection_map[top.bbl_addr] = 0;
                continue;
            }
        }
        if(BBL_map[top.bbl_addr].visited) continue;

        bool insert_target = true;
        bool insert_fallthrough = true;

        
        //insert children
        heap_element target = {BBL_map[top.bbl_addr].branch_target_address, BBL_map[top.bbl_addr].branch_times_taken};
        if(BBL_map[top.bbl_addr].branch_times_taken == 0 || BBL_map[top.bbl_addr].is_call)
        {
            insert_target = false;
            if(BBL_map[top.bbl_addr].is_cond)
            {
                redirection_map[redirection] = BBL_map[top.bbl_addr].branch_target_address;
                BBL_map[top.bbl_addr].branch_target_address= ADDRINT(redirection);
                heap_element fake_target =  {ADDRINT(redirection), BBL_map[top.bbl_addr].branch_times_taken};
                heap.push(fake_target);
                redirection++;
            }
        }
        
        heap_element fallthrough = {BBL_map[top.bbl_addr].fallthrough_address, BBL_map[top.bbl_addr].branch_times_not_taken};
        if(BBL_map[top.bbl_addr].branch_times_not_taken == 0  && BBL_map[top.bbl_addr].last_address != 0  && !BBL_map[top.bbl_addr].is_call)
        {
            insert_fallthrough = false;
            if(BBL_map[top.bbl_addr].is_cond)
            {
                redirection_map[redirection] = BBL_map[top.bbl_addr].fallthrough_address;
                BBL_map[top.bbl_addr].fallthrough_address= ADDRINT(redirection);
                heap_element fake_target =  {ADDRINT(redirection), BBL_map[top.bbl_addr].branch_times_not_taken};
                heap.push(fake_target);
                redirection++;
            }
        }

        if (BBL_map[top.bbl_addr].is_call || BBL_map[top.bbl_addr].branch_target_address == ADDRINT(-1))
        {
           fallthrough.in_degree = top.in_degree;
           if(BBL_map[top.bbl_addr].is_call && std::find(final_cand_rtn_name.begin(),final_cand_rtn_name.end(),BBL_map[target.bbl_addr].rtn_name) != final_cand_rtn_name.end())
           {
            //need to add the inlined bbls to the order

                heap.push(target);
                used_routines.push_back(BBL_map[target.bbl_addr].rtn_address);
                BBL_map[target.bbl_addr].visited_through_inline =true;
           }
        }
        
        if ((BBL_map[target.bbl_addr].rtn_address == curr_rtn_address || BBL_map[BBL_map[target.bbl_addr].rtn_address].visited_through_inline) && (insert_target  || BBL_map[top.bbl_addr].branch_target_address != ADDRINT(-1)))
        {
            heap.push(target);
        }
        if((BBL_map[fallthrough.bbl_addr].rtn_address == curr_rtn_address || BBL_map[BBL_map[fallthrough.bbl_addr].rtn_address].visited_through_inline) && insert_fallthrough)
        {
            heap.push(fallthrough);
        }  
        //make visited
        BBL_map[top.bbl_addr].visited = true;
        //insert to final vector
        rtn_bbls_order[curr_rtn_address].push_back(BBL_map[top.bbl_addr]);     
    }
}






/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{ 
    
    //sort invoker map by the time of invokes on each entry and crate called_map
    for(std::map<ADDRINT,Invoker>::iterator itr = invokers_map.begin(); itr!= invokers_map.end();itr++)
    {
        if (itr->second.num_invokes > 1)
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
        if(sorted_vec.size() == 1 )
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

    
    for (Invoker inv_entry: inv_vec)
    {
        if ( counter == NUM_INLINED_FUNC)
            break;
        if((inv_entry.num_invokes > 400 ) &&(too_hot_to_handle[inv_entry.target_addr] || single_call_site[inv_entry.target_addr])
          && (inv_entry.rtn_name.length() < 3 || inv_entry.rtn_name.substr(inv_entry.rtn_name.length() - 3) != "plt") 
          && inv_entry.target_addr != inv_entry.invoker_rtn_address && std::find(non_valid_rtn.begin(), non_valid_rtn.end(), inv_entry.target_addr) == non_valid_rtn.end()
          && std::find(final_cand_rtn_name.begin(),final_cand_rtn_name.end(), inv_entry.rtn_name) == final_cand_rtn_name.end())
        {
            final_candidates.push_back(inv_entry);
            final_cand_rtn_name.push_back(inv_entry.rtn_name);
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
            RTN_map[inv_entry.invoker_rtn_address].num_inst += RTN_map[inv_entry.target_addr].num_inst;
            if (RTN_map.find(inv_entry.target_addr) != RTN_map.end())
                RTN_map[inv_entry.target_addr].num_inst *=  0.1;
        }
       
    }

    // reorder RTN's for translation
    std::vector<RTN_reorder_info> rtn_vec;
    for(std::map<ADDRINT,RTN_reorder_info>::iterator itr3 = RTN_map.begin(); itr3!= RTN_map.end();itr3++)
    {
        if(std::find(non_valid_rtn.begin(), non_valid_rtn.end(), itr3->first) == non_valid_rtn.end() && itr3->second.num_inst > 100)
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
        results <<  "0x" << std::hex << inv_entry.fallthrough_address << ",";
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
        resultsRTNBBLOrder << "0x" << std::hex << itr_rtn->first << endl;
        bool first = true;
        for (BBL_info bbl_entry: itr_rtn->second)
        {
            if (first)
            {
                resultsRTNBBLOrder << "0x"  << bbl_entry.BBL_head_address << ",";
                first = false;
            }
            else
                resultsRTNBBLOrder << ",0x"  << bbl_entry.BBL_head_address << ",";
            //cout << " bbl head: " << bbl_entry.BBL_head_address << endl;
            resultsRTNBBLOrder << "0x"  << bbl_entry.branch_target_address << ",";
            resultsRTNBBLOrder << "0x"  << bbl_entry.fallthrough_address << ",";
            resultsRTNBBLOrder << "0x"  << bbl_entry.last_address;
        }
        resultsRTNBBLOrder << endl; 
        
    }
    resultsRTNBBLOrder.close();
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
