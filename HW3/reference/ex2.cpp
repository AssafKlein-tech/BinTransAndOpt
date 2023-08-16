#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <iostream>
#include <iomanip>
#include <set>
#include <algorithm>
//#include <map>
#include <unistd.h>
#include <fstream>
#include "pin.H"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

/*namespaces:*/
using std::setw;
using std::cerr;
using std::string;
using std::endl;
using std::vector;
using std::map;
using std::pair;

using namespace std;

/*Global variables:*/

/*typedef*/
typedef struct {
    UINT64 CountSeen;
    ADDRINT targetAddress;
    UINT64 CountLoopInvoked;
    UINT64 DiffCount;
    string RTN_Name;
    ADDRINT RTN_Address;
    UINT64 PREVIOUS_iterations_counter;
    UINT64 CURRENT_iterations_counter;
    bool previously_taken;
} LOOP_INFO;

/*Global variables:*/
//REMARK FOR TAL: we don't need to sort this map! just take the counter of the pair which its key is routine address.
map<ADDRINT,UINT64> RTN_MAP; //Key is RTN_Address and Value is RTN_COUNTER*/
/*This map we need to sort :( :*/
map<ADDRINT,LOOP_INFO> LOOP_MAP;;//KEY is ins address, VAL=LOOP_INFO[KEY]
map<ADDRINT,UINT64> MAP_FOR_SORTING;

/*this function inserts a new loop to the map and updates its parameters in case it has already been added*/
void inc_loop(INT32 taken,ADDRINT ins_addr) {
    LOOP_MAP[ins_addr].CountSeen++;
    MAP_FOR_SORTING[ins_addr]++; //I think this is going to work!
    if(taken){
        if(LOOP_MAP[ins_addr].previously_taken==false){
            // LOOP_MAP[ins_addr].CountLoopInvoked++; TRY DOING THIS EVERY TIMEM THE LOOP IS NOT TAKEN.
        }
        LOOP_MAP[ins_addr].CURRENT_iterations_counter++; //starts counting from zero.
        LOOP_MAP[ins_addr].previously_taken=true; //updates previously_taken
    }
    else{
        LOOP_MAP[ins_addr].CountLoopInvoked++; //DELETE IF THIS IS NOT CORRECT!
        LOOP_MAP[ins_addr].previously_taken=false; //updates previously_taken
        /*updates DiffCount*/
        if( LOOP_MAP[ins_addr].CountLoopInvoked >0 &&
            (LOOP_MAP[ins_addr].PREVIOUS_iterations_counter!=LOOP_MAP[ins_addr].CURRENT_iterations_counter)){
            LOOP_MAP[ins_addr].DiffCount++;
        }
        LOOP_MAP[ins_addr].PREVIOUS_iterations_counter=LOOP_MAP[ins_addr].CURRENT_iterations_counter;
        LOOP_MAP[ins_addr].CURRENT_iterations_counter=0;
    }
}


VOID docount (UINT64* counter){ //TAKEN FROM EX1
    (*counter)++;
}

VOID Trace (TRACE trace, void *v)
{
    /*Validation check:*/

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        INS ins = BBL_InsTail(bbl);
        if (INS_Valid(ins)==false)
            continue;
        ADDRINT ins_addr = INS_Address(ins);
        RTN ins_rtn = RTN_FindByAddress(ins_addr);
        if (RTN_Valid(ins_rtn)==false)
            continue;
        ADDRINT rtn_addr = RTN_Address(ins_rtn);
        string rtn_name = RTN_FindNameByAddress(ins_addr);//changed from RTN_NAME(ins_rtn) to RTN_FindNameByAddress(). If it doesn't work, try with rtn_address
        /*checks if current ins is a branch command:*/
        if (INS_IsDirectBranch(ins) && INS_IsControlFlow(ins)) { //TODO: I've changed it to !INS_IsCall(ins) from INS_IsDirectBranch(ins)
            /*checks if the branch is a loop:*/
            ADDRINT speculative_target_address = INS_DirectControlFlowTargetAddress(ins);
            if (speculative_target_address < ins_addr) {
                /*this is a loop:*/
                /*checks if insertion of a new loop is needed:*/
                //auto search=LOOP_MAP.find(ins_addr);
                if ((LOOP_MAP.find(ins_addr) == LOOP_MAP.end())) { //check condition
                    LOOP_MAP[ins_addr].CountSeen = 0; //WOW! COULDN'T BELIEVE I'VE FOUND IT
                    LOOP_MAP[ins_addr].targetAddress = speculative_target_address;
                    LOOP_MAP[ins_addr].RTN_Address = rtn_addr;
                    LOOP_MAP[ins_addr].RTN_Name = rtn_name;
                    LOOP_MAP[ins_addr].CountLoopInvoked = 0;
                    LOOP_MAP[ins_addr].DiffCount = 0;
                    LOOP_MAP[ins_addr].PREVIOUS_iterations_counter = 0;
                    LOOP_MAP[ins_addr].CURRENT_iterations_counter = 0;
                    LOOP_MAP[ins_addr].previously_taken = false;
                    //MAP_FOR_SORTING[speculative_target_address]=0;
                }
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) inc_loop, IARG_BRANCH_TAKEN, IARG_ADDRINT, ins_addr,
                               IARG_END);
            }
	}
    }
}

VOID Instruction (INS ins, void *v)
{
    RTN ins_rtn = INS_Rtn(ins);
    if (ins_rtn == RTN_Invalid())
        return;
    ADDRINT rtn_addr = RTN_Address(ins_rtn);
    if (RTN_MAP.find(rtn_addr) == RTN_MAP.end()) {
        RTN_MAP[rtn_addr] = 0;
    }
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) docount, IARG_PTR, &(RTN_MAP[rtn_addr]), IARG_END);
}


VOID Fini(int n, void *v) {
    /*sorting the map by Countseen*/
    multimap <UINT64, LOOP_INFO> OUT_MAP;
    for (map<ADDRINT, LOOP_INFO>::iterator it = LOOP_MAP.begin(); it != LOOP_MAP.end(); ++it) { 
            OUT_MAP.insert(pair<UINT64, LOOP_INFO>((it->second).CountSeen, (it->second)));
        }
        /*opens a file:*/
        std::ofstream file_pointer("loop-count.csv");
        if (!file_pointer)
            std::cerr << "Can't open data file!" << std::endl;

        double get_mean = 0;
        /*prints as mentioned in the PDF:*/
	multimap<UINT64, LOOP_INFO>::iterator it = OUT_MAP.end();
	it--;
        for ( ; it != OUT_MAP.begin(); --it) {
            //ADDRINT iterator_key =it->first;
            LOOP_INFO iterator_value = it->second; //", 0x"
            if (iterator_value.CountSeen == 0)//ok
                continue;
            if (iterator_value.CountLoopInvoked > 0) {
	      get_mean = (double) (iterator_value.CountSeen) /( iterator_value.CountLoopInvoked);
            }
            /*PLASTER. Don't ask, it works!:*/
            iterator_value.DiffCount = iterator_value.DiffCount - 1;
            /*printing:*/

	    file_pointer << "0x" << hex << iterator_value.targetAddress << ", "
                    << dec << iterator_value.CountSeen<< ", "
                    << iterator_value.CountLoopInvoked<< ", "
                    << get_mean << ", "<< dec <<iterator_value.DiffCount<< ", "<<iterator_value.RTN_Name<< ", "<< "0x" << hex << iterator_value.RTN_Address << ", "
                    << dec <<RTN_MAP[iterator_value.RTN_Address]<< endl;
        }
        file_pointer.close();
    }

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        //return Usage();
    }
    TRACE_AddInstrumentFunction(Trace, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns

    PIN_StartProgram();

    return 0;
}
