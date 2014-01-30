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
#include <bh_flow.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <fstream>
using namespace std;

//Registrate access by the 'node_idx'
void bh_flow::add_access(bh_intp node_idx)
{
    const bh_flow_node &n = node_list[node_idx];
    bases[n.view->base].push_back(node_idx);

    //All nodes is in sub-DAG zero initially
    set_sub_dag(0, node_idx);
}

//Get the latest access that conflicts with 'view'
bh_intp bh_flow::get_latest_conflicting_access(const bh_view *view, bool readonly)
{
    //Search through all nodes with the same base as 'node_idx'
    const vector<bh_intp> &same_base = bases[view->base];
    for(vector<bh_intp>::const_reverse_iterator it = same_base.rbegin();
        it != same_base.rend(); ++it)
    {
        const bh_flow_node &n = node_list[*it];
        if(readonly && n.readonly)
            continue;//No possible conflict when both is read only

        if(bh_view_overlap(n.view, view))
            return *it;
    }
    return -1;//No conflict
}

//Create a new flow object based on an instruction list
bh_flow::bh_flow(bh_intp ninstr, const bh_instruction *instr_list)
{
    this->ninstr = ninstr;
    this->instr_list = instr_list;

    for(bh_intp i=0; i<ninstr; i++)
    {
        const bh_instruction *instr = &instr_list[i];
        //The write node
        bh_flow_node wnode(0, false, i, &instr->operand[0]);

        //All nodes generated by the same instruction gets the same timestep
        bh_intp timestep = 0;

        //Generate read nodes
        bh_intp nnodes = 1; //Number of new nodes (incl. the write node)
        for(bh_intp o=1; o < bh_operands_in_instruction(instr); o++)
        {
            const bh_view *op = &instr->operand[o];
            if(bh_is_constant(op))
                continue;

            bh_flow_node rnode(0, true, i, op);

            bh_intp node_idx = get_latest_conflicting_access(op, true);
            if(node_idx >= 0)
            {
                const bh_flow_node &node = node_list[node_idx];

                //The new nodes must be placed after this conflicting node
                if(timestep <= node.timestep)
                    timestep = node.timestep+1;

                //If we have a perfect match, we add a flow edge since we
                //known that 'node' is a writing node
                if(bh_view_identical(node.view, op))
                    rnode.parents.insert(node_idx);
            }
            //Add the read node at make sure that the write node points to it
            //NB: we update the timestep later
            node_list.push_back(rnode);
            bh_intp rnode_idx = node_list.size()-1;
            wnode.parents.insert(rnode_idx);
            add_access(rnode_idx);
            nnodes++;
        }
        //Lets find the final timestep value
        bh_intp node_idx = get_latest_conflicting_access(wnode.view, false);
        if(node_idx >= 0)
        {
            const bh_flow_node &node = node_list[node_idx];
            if(timestep <= node.timestep)
                timestep = node.timestep+1;
        }
        node_list.push_back(wnode);
        add_access(node_list.size()-1);
        //Save the final timestep value
        for(vector<bh_flow_node>::iterator it = node_list.end()-nnodes;
            it != node_list.end(); it++)
        {
            it->timestep = timestep;
        }
    }
}

//Pretty print the flow object to 'buf'
void bh_flow::sprint(char *buf)
{
    stringstream str;
    str << "id:\ttime:\tR/W:\tparent:\tinstr:" << endl;
    bh_intp t = 0;//The current time step
    bool not_finished = true;
    while(not_finished)
    {
        not_finished = false;
        for(unsigned int i=0; i<node_list.size(); i++)
        {
            const bh_flow_node &n = node_list[i];
            if(n.timestep == t)
            {
                str << i << "\t";
                str << n.timestep << "\t";
                if(n.readonly)
                    str << " R ";
                else
                    str << " W ";
                str << "\t" << "[";
                for(set<bh_intp>::const_iterator p=n.parents.begin(); p!=n.parents.end(); p++)
                {
                    if(p != n.parents.begin())//Not the first iteration
                        str << ",";
                    str << *p;
                }
                str << "]\t";

                str << n.instr_idx << "." << (bh_opcode_text(instr_list[n.instr_idx].opcode)+3);

                str << endl;

                not_finished = true;//We found one thus there might be more
            }
        }
        t++;
    }
    //Write the stream to a C string
    strcpy(buf, str.str().c_str());
}

//Pretty print the flow object to stdout
void bh_flow::pprint(void)
{
    char buf[10000];
    sprint(buf);
    puts(buf);
}

//Pretty print the flow object to file 'filename'
void bh_flow::fprint(const char* filename)
{
    char buf[10000];
    FILE *file;
    file = fopen(filename, "w");
    sprint(buf);
    fputs(buf, file);
    fclose(file);
}

// Write the flow object in the DOT format.
void bh_flow::dot(const char* filename)
{
    ofstream fs(filename);

    fs << "digraph {" << std::endl;
    fs << "compound=true;" << std::endl;

    //Write all nodes and conflict edges
    map<const bh_base *, vector<bh_intp> >::const_iterator b;
    for(b=bases.begin(); b != bases.end(); b++)
    {
        fs << "subgraph clusterBASE" << b->first << " {" << endl;
        fs << "label=\"" << b->first << "\";" << endl;

        //Define all nodes
        vector<bh_intp>::const_iterator v,w;
        for(v=b->second.begin(); v != b->second.end(); v++)
        {
            const bh_flow_node &n = node_list[*v];
            fs << "n" << *v << "[label=\"" << n.timestep;
            if(n.readonly)
                fs << "R";
            else
                fs << "W";
            fs << n.sub_dag;
            fs << "_" << bh_opcode_text(instr_list[n.instr_idx].opcode)+3;
            fs << "(" << n.instr_idx << ")\"";
            fs << " shape=box style=filled,rounded";
            fs << " colorscheme=paired12 fillcolor=" << n.sub_dag%12+1;
            fs << "]" << endl;

        }
        //Write invisible edges in order to get correct layout
        for(v=b->second.begin(); v != b->second.end()-1; v++)
        {
            fs << "n" << *v << " -> n" << *(v+1) << "[style=\"invis\"];" << endl;
        }

        //Write conflict edges
        for(v=b->second.begin(); v != b->second.end()-1; v++)
        {
            for(w=v+1; w != b->second.end(); w++)
            {
                if(!bh_view_identical(node_list[*v].view, node_list[*w].view))
                    fs << "n" << *v << " -> n" << *w << " [color=red];" << endl;
            }
        }
        fs << "}" << endl;
    }
    //Write all flow edges
    for(vector<bh_flow_node>::size_type i=0; i<node_list.size(); i++)
    {
        const bh_flow_node &n = node_list[i];
        set<bh_intp>::const_iterator it;
        for(it=n.parents.begin(); it != n.parents.end(); it++)
        {

            assert(n.timestep >= node_list[*it].timestep);
            fs << "{";
     //       if(n.timestep == node_list[*it].timestep)
     //           fs << "rank=same; ";
            fs << "n" << *it << " -> n" << i << ";";
            fs << "}" << endl;
        }
    }
    fs << "}" << std::endl;
    fs.close();
}

//Assign a node to a sub-DAG
void bh_flow::set_sub_dag(bh_intp sub_dag, bh_intp node_idx)
{
    sub_dags[sub_dag].insert(node_idx);
}

