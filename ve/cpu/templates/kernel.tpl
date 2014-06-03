#ifndef BH_CPU_LIBS
#define BH_CPU_LIBS
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <complex.h>
#include <math.h>
#include <Random123/threefry.h>
#include <tac.h>
#if defined(_OPENMP)
#include <omp.h>
#else
inline int omp_get_max_threads() { return 1; }
inline int omp_get_thread_num()  { return 0; }
inline int omp_get_num_threads() { return 1; }
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_CIR 360.0
#define DEG_RAD (M_PI / (DEG_CIR / 2.0))
#define RAD_DEG ((DEG_CIR / 2.0) / M_PI)

#ifndef CPU_MISC
#define CPU_MAXDIM 16
#endif

// KERNEL-DESCRIPTION [
//  MODE={{MODE}}, NINSTR={{NINSTR}}, NARGS={{NARGS}}
//  SYMBOL_TEXT {{SYMBOL_TEXT}}
// ]
void KRN_{{SYMBOL}}(operand_t** args)
{
    //
    // Argument unpacking
    //
    {{#ARGUMENT}}
    ///////////////////////////////////////////////////
    // Argument {{NR}} - [{{#CONSTANT}}CONSTANT{{/CONSTANT}}{{#SCALAR}}SCALAR{{/SCALAR}}{{#ARRAY}}ARRAY{{/ARRAY}}]
    //
    {{TYPE}} *a{{NR}}_first = *(args[{{NR}}]->data);
    assert(a{{NR}}_first != NULL);

    {{#CONSTANT}}
    const {{TYPE}} a{{NR}}_current = *a{{NR}}_first;
    {{/CONSTANT}}

    {{#SCALAR}}
    {{TYPE}} a{{NR}}_current = *a{{NR}}_first;
    {{/SCALAR}}

    {{#ARRAY}}
    int64_t  a{{NR}}_start  = args[{{NR}}]->start;
    int64_t  a{{NR}}_nelem  = args[{{NR}}]->nelem;
    int64_t  a{{NR}}_ndim   = args[{{NR}}]->ndim;    
    int64_t *a{{NR}}_shape  = args[{{NR}}]->shape;
    int64_t *a{{NR}}_stride = args[{{NR}}]->stride;
    a{{NR}}_first += a{{NR}}_start;
    {{/ARRAY}}

    {{/ARGUMENT}}
    
    //
    // Operation(s)
    //
    {{>OPERATIONS}}

    // Write scalar to main-memory.
    // TODO: This is incorrect when using threading! AHHH THAT IS WHY
    //       it fails! Multiple threads are writing to the same register!
    //       that is why! When fusing this should be handled as scalar-expansion!
    //       The unpacking should be handled within the operation!
    {{#SCALAR}}
    *a{{NR}}_first = a{{NR}}_current;
    {{/SCALAR}}
}
