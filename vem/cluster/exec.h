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

//Public function pointer to the Node VEM
extern bh_execute exec_vem_execute;


/* Initialize the VEM
 *
 * @return Error codes (BH_SUCCESS)
 */
bh_error exec_init(const char *component_name);


/* Shutdown the VEM, which include a instruction flush
 *
 * @return Error codes (BH_SUCCESS)
 */
bh_error exec_shutdown(void);


/* Register a new user-defined function.
 *
 * @lib Name of the shared library e.g. libmyfunc.so
 *      When NULL the default library is used.
 * @fun Name of the function e.g. myfunc
 * @id Identifier for the new function. The bridge should set the
 *     initial value to Zero. (in/out-put)
 * @return Error codes (BH_SUCCESS)
 */
bh_error exec_reg_func(char *fun, bh_intp *id);


/* Execute a BhIR where all operands are global arrays
 *
 * @bhir   The BhIR in question
 * @return Error codes
 */
bh_error exec_execute(bh_ir *bhir);


