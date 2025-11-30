/**
 * @file interrupts.cpp
 * @author Abdullah Hasan
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include<interrupts_AbdullahHasan_MarwaDiab.hpp>
#include <map>
#include <algorithm>

void FCFS(std::vector<PCB> &ready_queue) {
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    return (first.arrival_time > second.arrival_time); 
                } 
            );
}

std::tuple<std::string, std::string> run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<unsigned int> wait_until;
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;
    std::string memory_status;

    auto pad_int = [](int value, int width) {
        std::string s = std::to_string(value);
        if (s.size() < static_cast<std::size_t>(width)) {
            s = std::string(width - s.size(), ' ') + s;
        }
        return s;
    };

    auto pad_str = [](const std::string &s, int width) {
        std::string out = s;
        if (out.size() < static_cast<std::size_t>(width)) {
            out += std::string(width - out.size(), ' ');
        }
        return out;
    };

    
    memory_status  = "+--------------------------------------------------------------------------------+\n";
    memory_status += "| Time | Total Used | Total Free | Total Usable| Used Partitions |Free Partitions|\n";
    memory_status += "+--------------------------------------------------------------------------------+\n";


    auto log_memory = [&](unsigned int time) {
        unsigned int total_used = 0;
        unsigned int total_free = 0;
        std::string used_parts;
        std::string free_parts;

        for (int i = 0; i < 6; ++i) {
            if (memory_paritions[i].occupied != -1) {
                total_used += memory_paritions[i].size;
                used_parts += std::to_string(memory_paritions[i].partition_number) + " ";
            } else {
                total_free += memory_paritions[i].size;
                free_parts += std::to_string(memory_paritions[i].partition_number) + " ";
            }
        }

        unsigned int total_usable = total_free;

        memory_status += "| " + pad_int(time, 4)
                       + " | " + pad_int(total_used, 10)
                       + " | " + pad_int(total_free, 10)
                       + " | " + pad_int(total_usable, 11)
                       + " | " + pad_str(used_parts, 15)
                       + " | " + pad_str(free_parts, 13)
                       + " |\n";
    };

    //make the output table (the header row)
    execution_status = print_exec_header();

    std::map<int, unsigned int> cpu_since_io;
    unsigned int quantum_used = 0;
    const unsigned int QUANTUM = 100;

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        //Population of ready queue is given to you as an example.
        //Go through the list of proceeses
        for(auto &process : list_processes) {
            if(process.arrival_time <= current_time && process.state == NOT_ASSIGNED) {//check if the AT <= current time
                if (assign_memory(process)) {
                    process.state = READY;  //Set the process state to READY
                    ready_queue.push_back(process); //Add the process to the ready queue
                    job_list.push_back(process); //Add it to the list of processes

                    execution_status += print_exec_status(current_time, process.PID, NEW, READY);
                    log_memory(current_time);
                }
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue
        for (std::size_t i = 0; i < wait_queue.size(); ) {
            if (wait_until[i] == current_time) {
                PCB p = wait_queue[i];
                p.state = READY;
                execution_status += print_exec_status(current_time, p.PID, WAITING, READY);
                sync_queue(job_list, p);
                ready_queue.insert(ready_queue.begin(), p);
                wait_queue.erase(wait_queue.begin() + i);
                wait_until.erase(wait_until.begin() + i);
            } else {
                ++i;
            }
        }
        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        FCFS(ready_queue); //example of FCFS is shown here
        if (running.state != RUNNING && !ready_queue.empty()) {
            run_process(running, job_list, ready_queue, current_time);
            execution_status += print_exec_status(current_time, running.PID, READY, RUNNING);
            cpu_since_io[running.PID] = 0;
            quantum_used = 0;
        }
        /////////////////////////////////////////////////////////////////

        if (running.state == RUNNING) {
            unsigned int next_time = current_time + 1;
            cpu_since_io[running.PID]++;
            quantum_used++;

            if (running.remaining_time > 0) {
                running.remaining_time--;
            }

            bool finished = (running.remaining_time == 0);
            bool do_io = (!finished &&
                          running.io_freq > 0 &&
                          cpu_since_io[running.PID] >= running.io_freq);
            bool quantum_expired = (!finished && !do_io && quantum_used >= QUANTUM);

            if (finished) {
                execution_status += print_exec_status(next_time, running.PID, RUNNING, TERMINATED);
                terminate_process(running, job_list);
                cpu_since_io[running.PID] = 0;
                quantum_used = 0;
                idle_CPU(running);
            } else if (do_io) {
                cpu_since_io[running.PID] = 0;
                quantum_used = 0;
                running.state = WAITING;
                execution_status += print_exec_status(next_time, running.PID, RUNNING, WAITING);
                sync_queue(job_list, running);
                wait_queue.push_back(running);
                wait_until.push_back(next_time + running.io_duration);
                idle_CPU(running);
            } else if (quantum_expired) {
                running.state = READY;
                execution_status += print_exec_status(next_time, running.PID, RUNNING, READY);
                sync_queue(job_list, running);
                ready_queue.insert(ready_queue.begin(), running);
                cpu_since_io[running.PID] = 0;
                quantum_used = 0;
                idle_CPU(running);
            }

            current_time = next_time;
        } else {
            current_time++;
        }
    }
    
    //Close the output table
    execution_status += print_exec_footer();
    memory_status += "+--------------------------------------------------------------------------------+\n";

    return std::make_tuple(execution_status, memory_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec, mem] = run_simulation(list_process);

    write_output(exec, "execution.txt");   
    write_output(mem,  "memory_status.txt");


    return 0;
}
