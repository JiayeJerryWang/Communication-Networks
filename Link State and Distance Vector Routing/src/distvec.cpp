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

std::map<std::tuple<int, int>, std::tuple<int, int>> prev;
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
    // create big table
    prev.clear();
    curr.clear();

    // source
    for (auto source = nodes.begin(); source != nodes.end(); source++)
    {
        // dest
        for (auto dest = nodes.begin(); dest != nodes.end(); dest++){
            if(topo.find({*source, *dest}) == topo.end()){
                // not exist, equal to infinity
                prev[{*source, *dest}] = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
            }else{
                // exist
                prev[{*source, *dest}] = topo[{*source, *dest}];
            }
        }
    }

    // converging process
    while (prev != curr){
        // avoid first time issue
        if (curr.size() != 0){
            prev = curr;
        }

        // computation / update
        for (auto source = nodes.begin(); source != nodes.end(); source++)
        {
            // dest
            for (auto dest = nodes.begin(); dest != nodes.end(); dest++){
                // pre-assign
                std::tuple<int,int> old_cell = prev[{*source, *dest}];
                int min_cost = std::get<0>(old_cell);
                int min_hop = std::get<1>(old_cell);
                curr[{*source, *dest}] = {min_cost, min_hop};

// std::cout << *source << ":" << *dest << "min_cost: " << min_cost << ", hop: " << min_hop << std::endl;

                // check neighbors to update
                std::set<int> curr_neigb = neighbors[*source];
                for (auto n = curr_neigb.begin(); n != curr_neigb.end(); n++){
                    // unreachable, skip
                    if((std::get<0>(topo[{*source, *n}]) == std::numeric_limits<int>::max())
                        || (std::get<0>(prev[{*n, *dest}]) == std::numeric_limits<int>::max())) continue;

                    int curr_cost = std::get<0>(topo[{*source, *n}]) + std::get<0>(prev[{*n, *dest}]);
                    int curr_hop = *n;

                    // compare cost and break tie Rule 1
                    if((min_cost > curr_cost) || ((min_cost == curr_cost) && (min_hop > curr_hop))){
                        curr[{*source, *dest}] = {curr_cost, curr_hop};
                        min_cost = curr_cost;
                        min_hop = curr_hop;
 // std::cout << "neighb: " << *n << "new_cost: " << curr_cost << ", new_hop: " << curr_hop << std::endl;
                    } else if(min_cost == curr_cost){
                        int last_node_min = min_hop;
                        while(std::get<1>(prev[{last_node_min, *dest}]) != *dest){
                            last_node_min = std::get<1>(prev[{last_node_min, *dest}]);
                        }
                        int last_node_curr = curr_hop;
                        while(std::get<1>(prev[{last_node_curr, *dest}]) != *dest){
                            last_node_curr = std::get<1>(prev[{last_node_curr, *dest}]);
                        }
                        // break tie Rule 3
                        if (last_node_min > last_node_curr){
                            curr[{*source, *dest}] = {curr_cost, curr_hop};
                            min_cost = curr_cost;
                            min_hop = curr_hop;
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

    // test the dictionary
    // for(auto pair: topo)
        //     std::cout << std::get<0>(n.first) << "-" << std::get<1>(n.first) << "+" << n.second << "\n";

    parse_files(argv);

    converge();
    
    // first converged, write messages
    write_message();

    // handle changes
    for(auto c = changes.begin(); c != changes.end(); c++){
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
        
        // converged, write topo and messages
        write_message();
    }

    // save output file
    output.close();

    return 0;
}

