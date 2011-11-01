/*
 * Copyright 2011 Mads R. B. Kristensen <madsbk@gmail.com>
 *
 * This file is part of cphVB.
 *
 * cphVB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cphVB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cphVB. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>
#include <cstring>
#include <iostream>
#include <cphvb.h>
#include <cphvb_interface.h>
#include <cphvb_conf.h>
#include <cphvb_error.h>

#include "cphvb_vem_node.h"
#include "ArrayManager.hpp"

//Function pointers to the VE.
static cphvb_init ve_init;
static cphvb_execute ve_execute;
static cphvb_shutdown ve_shutdown;


#define PLAININST (1)
#define REDUCEINST (2)

ArrayManager* arrayManager;

//The VE info.
cphvb_support ve_support;

/* Initialize the VEM
 *
 * @return Error codes (CPHVB_SUCCESS)
 */
cphvb_error cphvb_vem_node_init(void)
{
    cphvb_intp opcode_count, type_count;
    cphvb_opcode opcode[CPHVB_NO_OPCODES*2];
    cphvb_type type[CPHVB_NO_TYPES];
    cphvb_error err;

    cphvb_interface f;
    err = cphvb_conf_children("node",&f);
    if(err != CPHVB_SUCCESS)
    {
        printf("Error in cphvb_conf_children()\n");
        return err;
    }

    ve_init = f.init;
    ve_execute = f.execute;
    ve_shutdown = f.shutdown;

    //Let us initiate the simple VE and register what it supports.
    err = ve_init(&opcode_count, opcode, &type_count, type);
    if(err)
        return err;

    //Init to False.
    std::memset(ve_support.opcode, 0, CPHVB_NO_OPCODES*sizeof(cphvb_bool));
    std::memset(ve_support.type, 0, CPHVB_NO_TYPES*sizeof(cphvb_bool));

#ifdef DEBUG
    std::cout << "[VEM node] Supported opcodes:" << opcode_count << std::endl;
#endif
    while(--opcode_count >= 0)
    {
        if (opcode[opcode_count] & CPHVB_REDUCE)
        {
            ve_support.opcode[~CPHVB_REDUCE & opcode[opcode_count]] |=
                REDUCEINST;
        }
        else
        {
            ve_support.opcode[opcode[opcode_count]] |= PLAININST;
        }
#ifdef DEBUG
        std::cout << "\t" << cphvb_opcode_text(opcode[opcode_count]) <<
            std::endl;
#endif
    }

#ifdef DEBUG
    std::cout << "[VEM node] Supported types:" << std::endl;
#endif
    while(--type_count >= 0)
    {
        ve_support.type[type[type_count]] = 1;//Set True
#ifdef DEBUG
        std::cout << "\t" << cphvb_type_text(type[type_count]) << std::endl;
#endif
    }

    try
    {
        arrayManager = new ArrayManager();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return CPHVB_ERROR;
    }
    return CPHVB_SUCCESS;
}




/* Shutdown the VEM, which include a instruction flush
 *
 * @return Error codes (CPHVB_SUCCESS)
 */
cphvb_error cphvb_vem_node_shutdown(void)
{
    return ve_shutdown();
}


/* Create an array, which are handled by the VEM.
 *
 * @base Pointer to the base array. If NULL this is a base array
 * @type The type of data in the array
 * @ndim Number of dimentions
 * @start Index of the start element (always 0 for base-array)
 * @shape[CPHVB_MAXDIM] Number of elements in each dimention
 * @stride[CPHVB_MAXDIM] The stride for each dimention
 * @has_init_value Does the array have an initial value
 * @init_value The initial value
 * @new_array The handler for the newly created array
 * @return Error code (CPHVB_SUCCESS, CPHVB_OUT_OF_MEMORY)
 */
cphvb_error cphvb_vem_node_create_array(cphvb_array*   base,
                                        cphvb_type     type,
                                        cphvb_intp     ndim,
                                        cphvb_index    start,
                                        cphvb_index    shape[CPHVB_MAXDIM],
                                        cphvb_index    stride[CPHVB_MAXDIM],
                                        cphvb_intp     has_init_value,
                                        cphvb_constant init_value,
                                        cphvb_array**  new_array)
{

    try
    {
        *new_array = arrayManager->create(base, type, ndim, start, shape,
                                          stride, has_init_value, init_value);
    }
    catch(std::exception& e)
    {
        return CPHVB_OUT_OF_MEMORY;
    }
    return CPHVB_SUCCESS;
}


/* Check whether the instruction is supported by the VEM or not
 *
 * @return non-zero when true and zero when false
 */
cphvb_intp cphvb_vem_node_instruction_check(cphvb_instruction *inst)
{
    switch(inst->opcode)
    {
    case CPHVB_DESTROY:
        return 1;
    case CPHVB_RELEASE:
        return 1;
    default:
        if( //it's a reduce instruction and we support it
            (inst->opcode & CPHVB_REDUCE &&
             ve_support.opcode[~CPHVB_REDUCE & inst->opcode] & REDUCEINST) ||
            //it's a "normal" instuction and we support it
            ve_support.opcode[inst->opcode] & PLAININST)
        {
            cphvb_intp i;
            cphvb_intp nop = cphvb_operands(inst->opcode);
            for(i=0; i<nop; ++i)
            {
                cphvb_type t;
                if(inst->operand[i] == CPHVB_CONSTANT)
                    t = inst->const_type[i];
                else
                    t = inst->operand[i]->type;
                if(!ve_support.type[t] && t != CPHVB_INDEX)
                    return 0;
            }
            return 1;
        }
        else
            return 0;
    }
}


/* Execute a list of instructions (blocking, for the time being).
 * It is required that the VEM supports all instructions in the list.
 *
 * @instruction A list of instructions to execute
 * @return Error codes (CPHVB_SUCCESS)
 */
cphvb_error cphvb_vem_node_execute(cphvb_intp count,
                                   cphvb_instruction inst_list[])
{
    cphvb_intp i;
    cphvb_intp valid_instruction_count = count;
    for(i=0; i<count; ++i)
    {
        cphvb_instruction* inst = &inst_list[i];
        cphvb_array* base = cphvb_base_array(inst->operand[0]);
        switch(inst->opcode)
        {
        case CPHVB_DESTROY:
            if (inst->operand[0]->base != NULL)
            {   // It's a view and we can mark it for deletion
                arrayManager->erasePending(inst->operand[0]);
            }
            --base->ref_count; //decrease refcount
            if(base->ref_count <= 0)
            {
                // Mark the Base for deletion
                arrayManager->erasePending(base);
                if (base->owner != CPHVB_PARENT)
                {
                    //Tell the VE to discard the base array.
                    inst->operand[0] = base;
                    inst->opcode = CPHVB_DISCARD;
                }
                else
                {
                    inst->opcode = CPHVB_NONE;
                    --valid_instruction_count;
                }
            }
            else
            {   //Tell the VE to do nothing
                inst->opcode = CPHVB_NONE;
                --valid_instruction_count;
            }
            break;
        case CPHVB_RELEASE:
            switch (base->owner)
            {
            case CPHVB_PARENT:
                //The owner is upstream so we do nothing
                inst->opcode = CPHVB_NONE;
                --valid_instruction_count;
                break;
            case CPHVB_SELF:
                //We own the date: Send discards down stream
                //and change owner to upstream
                inst->operand[0] = base;
                inst->opcode = CPHVB_DISCARD;
                arrayManager->changeOwnerPending(base,CPHVB_PARENT);
                break;
            default:
                //The owner is downsteam so send the release down
                inst->operand[0] = base;
                arrayManager->changeOwnerPending(base,CPHVB_PARENT);
            }
            break;
        case CPHVB_SYNC:
            switch (base->owner)
            {
            case CPHVB_PARENT:
            case CPHVB_SELF:
                //The owner is not down stream so we do nothing
                inst->opcode = CPHVB_NONE;
                --valid_instruction_count;
                break;
            default:
                //The owner is downsteam so send the sync down
                //and take ownership
                inst->operand[0] = base;
                arrayManager->changeOwnerPending(base,CPHVB_SELF);
            }
            break;
        default:
            // "Regular" operation: set ownership and send down stream
            base->owner = CPHVB_CHILD;
            for (int i = 1; i < cphvb_operands(inst->opcode); ++i)
            {
                if (inst->operand[i] != CPHVB_CONSTANT &&
                    cphvb_base_array(inst->operand[i])->owner == CPHVB_PARENT)
                {
                    cphvb_base_array(inst->operand[i])->owner = CPHVB_SELF;
                }
            }
        }
    }
    if (valid_instruction_count > 0)
    {
        cphvb_error e = ve_execute(count, inst_list);
        arrayManager->flush();
        return e;
    }
    else
    {
        // No validf instructions in batch
        return CPHVB_SUCCESS;
    }
}
