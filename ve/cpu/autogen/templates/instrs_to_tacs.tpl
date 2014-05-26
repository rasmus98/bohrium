#compiler-settings
directiveStartToken= %
#end compiler-settings
%slurp
#include "utils.hpp"
//
//  NOTE: This file is autogenerated based on the tac-definition.
//        You should therefore not edit it manually.
//
using namespace std;
namespace bohrium{
namespace core{

void instrs_to_tacs(bh_instruction* instrs, bh_intp ninstrs, vector<tac_t>& tacs, SymbolTable& symbol_table)
{
    // Reset metadata
    int omask = 0;        // And the operation mask
    
    for(bh_intp idx=0; idx<ninstrs; ++idx) {

        bh_instruction& instr = instrs[idx];        

        uint32_t out=0, in1=0, in2=0;

        //
        // Program packing: output argument
        // NOTE: All but BH_NONE has an output which is an array
        if (instr.opcode != BH_NONE) {
            out = symbol_table.map_operand(instr, 0);
        }

        //
        // Program packing; operator, operand and input argument(s).
        switch (instr.opcode) {    // [OPCODE_SWITCH]

            %for $opcode, $operation, $operator, $nin in $operations
            case $opcode:
                %if nin >= 1
                in1 = symbol_table.map_operand(instr, 1);
                %end if
                %if nin >= 2
                in2 = symbol_table.map_operand(instr, 2);
                %end if

                tacs[idx].op    = $operation;  // TAC
                tacs[idx].oper  = $operator;
                tacs[idx].ext   = NULL;
                tacs[idx].out   = out;
                tacs[idx].in1   = in1;
                tacs[idx].in2   = in2;
            
                omask |= $operation;    // Operationmask
                break;
            %end for

            default:
                if (instr.opcode>=BH_MAX_OPCODE_ID) {   // Handle extensions here

                    in1 = symbol_table.map_operand(instr, 1);
                    in2 = symbol_table.map_operand(instr, 2);

                    tacs[idx].op   = EXTENSION;
                    tacs[idx].oper = EXTENSION_OPERATOR;
                    tacs[idx].ext  = &instr;
                    tacs[idx].out  = out;
                    tacs[idx].in1  = in1;
                    tacs[idx].in2  = in2;

                    omask |= EXTENSION;
                    break;

                } else {
                    fprintf(stderr, "Block::compose: Err=[Unsupported instruction] {\n");
                    bh_pprint_instr(&instr);
                    fprintf(stderr, "}\n");
                }
        }

        //
        // Update the ref count
        symbol_table.count_rw(tacs[idx]);
    }
}

}}