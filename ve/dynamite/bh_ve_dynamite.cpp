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
#include "bh_ve_dynamite.h"
#include <bh_vcache.h>
#include <ctemplate/template.h>  
#include "targets.cpp"
#include <string>

static bh_component *myself = NULL;
static bh_userfunc_impl random_impl = NULL;
static bh_intp random_impl_id = 0;
static bh_userfunc_impl matmul_impl = NULL;
static bh_intp matmul_impl_id = 0;
static bh_userfunc_impl nselect_impl = NULL;
static bh_intp nselect_impl_id = 0;

static bh_intp vcache_size   = 10;

// Arguments
char* target_cmd;
char* kernel_path;
char* object_path;
char* snippet_path;

bh_error bh_ve_dynamite_init(bh_component *self)
{
    myself = self;

    char *env = getenv("BH_CORE_VCACHE_SIZE");      // Override block_size from environment-variable.
    if (NULL != env) {
        vcache_size = atoi(env);
    }
    if (0 >= vcache_size) {                          // Verify it
        fprintf(stderr, "BH_CORE_VCACHE_SIZE (%ld) should be greater than zero!\n", (long int)vcache_size);
        return BH_ERROR;
    }

    bh_vcache_init( vcache_size );

    target_cmd = getenv("BH_VE_DYNAMITE_TARGET");      // DYNAMITE Arguments
    if (NULL==target_cmd) {
        assign_string(target_cmd, "gcc -O2 -march=native -fPIC -x c -shared - -o ");
    }
    printf("%s", target_cmd);

    object_path = getenv("BH_VE_DYNAMITE_OBJECT_PATH");
    if (NULL==object_path) {
        assign_string(kernel_path, "objects/object_XXXXXX");
    }

    kernel_path = getenv("BH_VE_DYNAMITE_KERNEL_PATH");
    if (NULL==kernel_path) {
        assign_string(kernel_path, "kernels/kernel_XXXXXX");
    }

    snippet_path = getenv("BH_VE_DYNAMITE_SNIPPET_PATH");
    if (NULL==snippet_path) {
        assign_string(kernel_path, "snippets/");
    }

    return BH_SUCCESS;
}

bh_error bh_ve_dynamite_execute( bh_intp instruction_count, bh_instruction* instruction_list )
{
    bh_intp count;
    bh_instruction* inst;
    bh_error res = BH_SUCCESS;

    process target(target_cmd);
    ctemplate::TemplateDictionary dict("example");
    std::string sourcecode;

    for (count=0; count < instruction_count; count++) {

        inst = &instruction_list[count];

        res = bh_vcache_malloc( inst );          // Allocate memory for operands
        if ( res != BH_SUCCESS ) {
            printf("Unhandled error returned by bh_vcache_malloc() called from bh_ve_dynamite_execute()\n");
            return res;
        }
                                                    
        switch (inst->opcode) {                     // Dispatch instruction

            case BH_NONE:                        // NOOP.
            case BH_DISCARD:
            case BH_SYNC:
                res = BH_SUCCESS;
                break;
            case BH_FREE:                        // Store data-pointer in malloc-cache
                res = bh_vcache_free( inst );
                break;

            case BH_ADD:

                printf("Doing an add!\n");

                dict.SetValue("TYPE", "float");
                dict.SetValue("OPERATOR", "+");
            
                ctemplate::ExpandTemplate("snippets/traverse_aaa.tpl", ctemplate::DO_NOT_STRIP, &dict, &sourcecode);

                if (target.compile("traverse_aaa", sourcecode.c_str(), sourcecode.size())) {
                    target.f(0,
                        inst->operand[0]->start, inst->operand[0]->stride,
                        (float*)inst->operand[0]->data,
                        inst->operand[1]->start, inst->operand[1]->stride,
                        (float*)inst->operand[1]->data,
                        inst->operand[2]->start, inst->operand[2]->stride,
                        (float*)inst->operand[2]->data,
                        inst->operand[0]->shape, inst->operand[0]->ndim,
                        bh_nelements(inst->operand[0]->ndim, inst->operand[0]->shape)
                    );

                    res = BH_SUCCESS;
                } else {
                    res = BH_ERROR;
                }

                break;

            case BH_USERFUNC:                    // External libraries

                if(inst->userfunc->id == random_impl_id) {
                    res = random_impl(inst->userfunc, NULL);
                } else if(inst->userfunc->id == matmul_impl_id) {
                    res = matmul_impl(inst->userfunc, NULL);
                } else if(inst->userfunc->id == nselect_impl_id) {
                    res = nselect_impl(inst->userfunc, NULL);
                } else {                            // Unsupported userfunc
                    res = BH_USERFUNC_NOT_SUPPORTED;
                }

                break;

            default:                            // Built-in operations
                res = bh_compute_apply_naive( inst );

        }

        if (res != BH_SUCCESS) {    // Instruction failed
            break;
        }

    }

	return res;
}

bh_error bh_ve_dynamite_shutdown( void )
{
    // De-allocate the malloc-cache
    bh_vcache_clear();
    bh_vcache_delete();

    return BH_SUCCESS;
}

bh_error bh_ve_dynamite_reg_func(char *fun, bh_intp *id) 
{
    if(strcmp("bh_random", fun) == 0)
    {
    	if (random_impl == NULL)
    	{
			bh_component_get_func(myself, fun, &random_impl);
			if (random_impl == NULL)
				return BH_USERFUNC_NOT_SUPPORTED;

			random_impl_id = *id;
			return BH_SUCCESS;			
        }
        else
        {
        	*id = random_impl_id;
        	return BH_SUCCESS;
        }
    }
    else if(strcmp("bh_matmul", fun) == 0)
    {
    	if (matmul_impl == NULL)
    	{
            bh_component_get_func(myself, fun, &matmul_impl);
            if (matmul_impl == NULL)
                return BH_USERFUNC_NOT_SUPPORTED;
            
            matmul_impl_id = *id;
            return BH_SUCCESS;			
        }
        else
        {
        	*id = matmul_impl_id;
        	return BH_SUCCESS;
        }
    }
    else if(strcmp("bh_nselect", fun) == 0)
    {
        if (nselect_impl == NULL)
        {
            bh_component_get_func(myself, fun, &nselect_impl);
            if (nselect_impl == NULL)
                return BH_USERFUNC_NOT_SUPPORTED;
            
            nselect_impl_id = *id;
            return BH_SUCCESS;
        }
        else
        {
            *id = nselect_impl_id;
            return BH_SUCCESS;
        }
    }
        
    return BH_USERFUNC_NOT_SUPPORTED;
}

bh_error bh_random( bh_userfunc *arg, void* ve_arg)
{
    return bh_compute_random( arg, ve_arg );
}

bh_error bh_matmul( bh_userfunc *arg, void* ve_arg)
{
    return bh_compute_matmul( arg, ve_arg );
}

bh_error bh_nselect( bh_userfunc *arg, void* ve_arg)
{
    return bh_compute_nselect( arg, ve_arg );
}
