/*
 * Copyright 2011 Troels Blum <troels@blum.dk>
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
 * along with cphVB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PTXKERNELPARAMETER_HPP
#define __PTXKERNELPARAMETER_HPP

#include "PTXtype.hpp"
#include "PTXoperand.hpp"

class PTXkernelParameter : public PTXoperand
{
private:
    PTXtype type;
    char name[128];
public:
    int declare(char* buf, 
                int size);
    int declare(const char* prefix, 
                char* buf, 
                int size);
    int snprint(const char* prefix, 
                char* buf, 
                int size, 
                const char* postfix);
    
};

#endif
