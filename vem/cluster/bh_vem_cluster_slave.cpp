/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the 
GNU Lesser General Public License along with Bohrium. 

If not, see <http://www.gnu.org/licenses/>.
*/

#include <bh.h>
#include <cassert>
#include <map>
#include "bh_vem_cluster.h"
#include "dispatch.h"
#include "pgrid.h"
#include "exec.h"


//Check for error. Will exit on error.
static void check_error(bh_error err, const char *file, int line)
{
    if(err != BH_SUCCESS)
    {
        fprintf(stderr, "[VEM-CLUSTER] Slave (rank %d) encountered the error %s at %s:%d\n",
                pgrid_myrank, bh_error_text(err), file, line);
        MPI_Abort(MPI_COMM_WORLD,-1);
    }
}

int main()
{
    dispatch_msg *msg;
    
    //Initiate the process grid
    pgrid_init();

    while(1)
    {
        //Receive the dispatch message from the master-process
        dispatch_reset();
        dispatch_recv(&msg);

        //Handle the message
        switch(msg->type) 
        {
            case BH_CLUSTER_DISPATCH_INIT:
            {
                char *name = msg->payload;
                printf("Slave (rank %d) received INIT. name: %s\n", pgrid_myrank, name);
                check_error(exec_init(name),__FILE__,__LINE__);
                break;
            }
            case BH_CLUSTER_DISPATCH_SHUTDOWN:
            {
                printf("Slave (rank %d) received SHUTDOWN\n",pgrid_myrank);
                check_error(exec_shutdown(),__FILE__,__LINE__);
                return 0;
            }
            case BH_CLUSTER_DISPATCH_UFUNC:
            {
                bh_intp *id = (bh_intp *)msg->payload;
                char *fun = msg->payload+sizeof(bh_intp);
                printf("Slave (rank %d) received UFUNC. fun: %s, id: %ld\n",pgrid_myrank, fun, *id);
                check_error(exec_reg_func(fun, id),__FILE__,__LINE__);
                break;
            }
            case BH_CLUSTER_DISPATCH_EXEC:
            {
                //The number of instructions
                bh_intp *noi = (bh_intp *)msg->payload;                 
                //The master-instruction list
                bh_instruction *master_list = (bh_instruction *)(noi+1);
                //The number of new arrays
                bh_intp *noa = (bh_intp *)(master_list + *noi);
                //The list of new arrays
                dispatch_array *darys = (dispatch_array*)(noa+1); //number of new arrays
                //The number of user-defined functions
                bh_intp *nou = (bh_intp *)(darys + *noa);
                //The list of user-defined functions
                bh_userfunc *ufunc = (bh_userfunc*)(nou+1); //number of new arrays
               
                //Insert the new array into the array store and the array maps
                std::stack<bh_array*> base_darys;
                for(bh_intp i=0; i < *noa; ++i)
                {
                    bh_array *ary = dispatch_new_slave_array(&darys[i].ary, darys[i].id);
                    if(ary->base == NULL)//This is a base array.
                        base_darys.push(ary);
                } 
                //Update the base-array-pointers
                for(bh_intp i=0; i < *noa; ++i)
                {
                    bh_array *ary = dispatch_master2slave(darys[i].id);
                    if(ary->base != NULL)//This is NOT a base array
                    {
                        assert(dispatch_slave_exist(((bh_intp)ary->base)));
                        ary->base = dispatch_master2slave((bh_intp)ary->base);
                    }
                }

                //Receive the dispatched array-data from the master-process
                dispatch_array_data(base_darys);
                    
                //Allocate the local instruction list that should reference local arrays
                bh_instruction *local_list = (bh_instruction *)malloc(*noi*sizeof(bh_instruction));
                if(local_list == NULL)
                    check_error(BH_OUT_OF_MEMORY,__FILE__,__LINE__);
        
                memcpy(local_list, master_list, (*noi)*sizeof(bh_instruction));
            
                //De-serialize all user-defined function pointers.
                for(bh_intp i=0; i < *noi; ++i)
                {
                    bh_instruction *inst = &local_list[i];
                    if(inst->opcode == BH_USERFUNC)
                    {   
                        inst->userfunc = (bh_userfunc*) malloc(ufunc->struct_size);
                        if(inst->userfunc == NULL)
                            check_error(BH_OUT_OF_MEMORY,__FILE__,__LINE__);
                        //Save a local copy of the user-defined function
                        memcpy(inst->userfunc, ufunc, ufunc->struct_size);
                        //Iterate to the next user-defined function
                        ufunc = (bh_userfunc*)(((char*)ufunc) + ufunc->struct_size);
                    }
                }

                //Update all instruction to reference local arrays 
                for(bh_intp i=0; i < *noi; ++i)
                {
                    bh_instruction *inst = &local_list[i];
                    int nop = bh_operands_in_instruction(inst);
                    bh_array **ops;
                    if(inst->opcode == BH_USERFUNC)
                        ops = inst->userfunc->operand;
                    else
                        ops = inst->operand;

                    //Convert all instructon operands
                    for(bh_intp j=0; j<nop; ++j)
                    { 
                        if(bh_is_constant(ops[j]))
                            continue;
                        assert(dispatch_slave_exist((bh_intp)ops[j]));
                        ops[j] = dispatch_master2slave((bh_intp)ops[j]);
                    }
                }

                check_error(exec_execute(*noi, local_list),__FILE__,__LINE__);

                //Free all user-defined function structs
                for(bh_intp i=0; i < *noi; ++i)
                {
                    bh_instruction *inst = &local_list[i];
                    if(inst->opcode == BH_USERFUNC)
                    {
                        assert(inst->userfunc != NULL);
                        free(inst->userfunc);
                    }
                }
                free(local_list);
                break;
            }
            default:
                fprintf(stderr, "[VEM-CLUSTER] Slave (rank %d) "
                        "received unknown message type\n", pgrid_myrank);
                MPI_Abort(MPI_COMM_WORLD,BH_ERROR);
        }
    }
    return BH_SUCCESS; 
}
