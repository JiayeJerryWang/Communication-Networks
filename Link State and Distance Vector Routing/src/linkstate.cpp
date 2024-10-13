#include<stdio.h>
#include<string.h>
#include<stdlib.h>

// CPP
#include<iostream>
#include <fstream> 
#include <vector>
#include <string>
#include<map>
#include <tuple>
#include<set>
#include <limits>
#include <algorithm>

std::map<std::tuple<int, int>, std::tuple<int, int>> curr;

std::ofstream output; 

std::ifstream fpMsg; 
std::vector<std::tuple<int,int,std::string>> msgs;

std::ifstream fpChange;
std::vector<std::tuple<int,int,int>> changes;

std::ifstream fpTopo; 
// create 2d dictionary
std::map<std::tuple<int, int>, std::tuple<int, int>> topo;
// all nodes in the network
std::set<int> nodes;
// neigbers
std::map<int, std::set<int>> neighbors;

void parse_files(char** argv){
    std::string line;

    // "messagefile"
    fpMsg.open(argv[2], std::ifstream::in);
    while(std::getline(fpMsg, line)){
        // source
        std::string source = line.substr(0,  line.find(" "));
        // dest
        line = line.substr(line.find(" ")+1, line.length());
        std::string dest = line.substr(0, line.find(" "));
        // message
        std::string m = line.substr(line.find(" ")+1, line.length());

        msgs.push_back({std::stoi(source), std::stoi(dest), m});
    }

    // "changesfile"
    fpChange.open(argv[3], std::ifstream::in);
    while(std::getline(fpChange, line)){
        // source
        std::string source = line.substr(0,  line.find(" "));
        // dest
        line = line.substr(line.find(" ")+1, line.length());
        std::string dest = line.substr(0, line.find(" "));
        // message
        std::string cost = line.substr(line.find(" ")+1, line.length());

        // potentially broken links
        int cost_int = std::stoi(cost);
        if(cost_int == -999) cost_int = std::numeric_limits<int>::max();
        changes.push_back({std::stoi(source), std::stoi(dest), cost_int});
    }

    // output.txt
    output.open("output.txt", std::ifstream::out);

    // "topofile"
    fpTopo.open(argv[1], std::ifstream::in);
    while(std::getline(fpTopo, line)){
        // parse one line
        // source
        std::string source = line.substr(0,  line.find(" "));
        // dest
        line = line.substr(line.find(" ")+1, line.length());
        std::string dest = line.substr(0, line.find(" "));
        // cost
        std::string cost = line.substr(line.find(" ")+1, line.length());

        // initial topo
        // source, dest -> cost, nextHop
        topo[{std::stoi(source), std::stoi(dest)}] = {std::stoi(cost), std::stoi(dest)};
        topo[{std::stoi(dest), std::stoi(source)}] = {std::stoi(cost), std::stoi(source)};

        // self link
        topo[{std::stoi(source), std::stoi(source)}] = {0, std::stoi(source)};
        topo[{std::stoi(dest), std::stoi(dest)}] = {0, std::stoi(dest)};

        // collect all nodes 
        nodes.insert(std::stoi(source));
        nodes.insert(std::stoi(dest));

        // collect neighbors
        neighbors[std::stoi(source)].insert(std::stoi(dest));
        neighbors[std::stoi(dest)].insert(std::stoi(source));
    }
}

void write_message(){
    // write to output.txt
    // topology
    for (auto source = nodes.begin(); source != nodes.end(); source++)
        {   
            // output << *source << std::endl;
            // dest
            for (auto dest = nodes.begin(); dest != nodes.end(); dest++){
                int pathcost = std::get<0>(curr[{*source, *dest}]);
                int nexthop = std::get<1>(curr[{*source, *dest}]);

                // unreachable, skip
                if(pathcost == std::numeric_limits<int>::max()) continue;

                output << *dest << " " << nexthop << " " << pathcost << std::endl;
                
            }
            // next node
            output << std::endl;
        }

    // write messages
    for (auto m = msgs.begin(); m != msgs.end(); m++){
        std::vector<int> hops;
        int source = std::get<0>(*m);
        int dest = std::get<1>(*m);
        std::string message = std::get<2>(*m);

        int cost = std::get<0>(curr[{source, dest}]);
        int next_hop = std::get<1>(curr[{source, dest}]);
        hops.push_back(source);
        if(cost != std::numeric_limits<int>::max()){
            while(next_hop != dest){
                hops.push_back(next_hop);
                next_hop = std::get<1>(curr[{next_hop, dest}]);
            }
        }
        // unreachable
        if(cost == std::numeric_limits<int>::max()){
            output << "from " << source << " " 
               << "to " << dest << " "
               << "cost " << "infinite" << " " 
               << "hops " << "unreachable" << " ";
        }else{
            output << "from " << source << " " 
               << "to " << dest << " "
               << "cost " << cost << " " 
               << "hops ";
            
            for (auto h = hops.begin(); h != hops.end(); h++){
                output << *h << " ";
            }
        }

        output << "message " << message << std::endl;
                
    }
}

void converge(){
    curr.clear();

    // create big table
    // source
    for (auto source = nodes.begin(); source != nodes.end(); source++)
    {
        // dest
        for (auto dest = nodes.begin(); dest != nodes.end(); dest++){
            if(topo.find({*source, *dest}) == topo.end()){
                // not exist, equal to infinity
                curr[{*source, *dest}] = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
            }else{
                // exist
                curr[{*source, *dest}] = topo[{*source, *dest}];
            }
        }
    }

    // converging process
    for (auto source = nodes.begin(); source != nodes.end(); source++){
        std::set<int> finished;
        finished.insert(*source);

        while(finished.size() != nodes.size()){
            int min_cost = std::numeric_limits<int>::max();
            int min_hop = std::numeric_limits<int>::max();
            int w = std::numeric_limits<int>::max();
            // dest
            for (auto dest = nodes.begin(); dest != nodes.end(); dest++){
                if(finished.find(*dest) == finished.end()){
                    // not exist
                    // compare cost and break tie Rule 2
                    if((min_cost > std::get<0>(curr[{*source, *dest}])) 
                        || ((min_cost == std::get<0>(curr[{*source, *dest}])) && (w > *dest))){
                        min_cost = std::get<0>(curr[{*source, *dest}]);
                        min_hop = std::get<1>(curr[{*source, *dest}]);
                        w = *dest;
                    }
                }
            }
            finished.insert(w);

            std::set<int> w_neighbor = neighbors[w];
            for (auto v = w_neighbor.begin(); v != w_neighbor.end(); v++){
                if(finished.find(*v) == finished.end()){
                    // unreachable, skip
                    if((std::get<0>(curr[{*source, w}]) == std::numeric_limits<int>::max())
                        || (std::get<0>(topo[{w, *v}]) == std::numeric_limits<int>::max())) continue;

                    if(std::get<0>(curr[{*source, *v}]) > std::get<0>(curr[{*source, w}]) + std::get<0>(topo[{w, *v}])){
                        curr[{*source, *v}] = {std::get<0>(curr[{*source, w}]) + std::get<0>(topo[{w, *v}]), std::get<1>(curr[{*source, w}])};
                    }
                    else if(std::get<0>(curr[{*source, *v}]) == std::get<0>(curr[{*source, w}]) + std::get<0>(topo[{w, *v}])){
                        int last_node_min = *source;
                        while(std::get<1>(curr[{last_node_min, *v}]) != *v){
                            last_node_min = std::get<1>(curr[{last_node_min, *v}]);
                        }
                        int last_node_curr = w;
                        while(std::get<1>(curr[{last_node_curr, *v}]) != *v){
                            last_node_curr = std::get<1>(curr[{last_node_curr, *v}]);
                        }
                        // break tie Rule 3
                        if (last_node_min > last_node_curr){
                            curr[{*source, *v}] = {std::get<0>(curr[{*source, w}]) + std::get<0>(topo[{w, *v}]), std::get<1>(curr[{*source, w}])};
                        }
                    }
                }
            }
        }
    }
}
int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    parse_files(argv);

    converge();
    write_message();

    // handle changes
    for(auto c = changes.begin(); c != changes.end(); c++){
        curr.clear();

        int s = std::get<0>(*c);
        int d = std::get<1>(*c);
        int new_cost = std::get<2>(*c);

        topo[{s, d}] = {new_cost, d};
        topo[{d, s}] = {new_cost, s};

        if(new_cost == std::numeric_limits<int>::max()){
            // broke
            neighbors[s].erase(d);
            neighbors[d].erase(s);
        }else{
            // new link
            neighbors[s].insert(d);
            neighbors[d].insert(s);
        }
        converge();
        write_message();
    }

    // save output file
    output.close();

    return 0;
}

