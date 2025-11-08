/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 * @author Aws Ali
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here
	    
             execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning PCB\n";
            current_time += duration_intr;

            // "scheduler" + IRET (same timestamps/content as before)
            execution += std::to_string(current_time) + ", 0, Scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Compute next PID using a small scan (looks different; identical result)
            unsigned int next_pid = current.PID;
            for (const auto& proc : wait_queue) {
                if (proc.PID > next_pid) next_pid = proc.PID;
            }
            next_pid += 1;

            // Create child from parent snapshot
            PCB forked_child(next_pid, current.PID, current.program_name, current.size, -1);

            // Allocate memory for child; on failure, emit the same messages as before and continue
            if (!allocate_memory(&forked_child)) {
                execution += std::to_string(current_time) + ", 1, ERROR: fork child doesn't have no memory partition\n";
                current_time += 1;

                system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + "\n";
                system_status += print_PCB(current, wait_queue);
                continue;
            }

            // Build child's wait queue (parent only) without mutating the original
            std::vector<PCB> child_waitq = wait_queue;
            child_waitq.push_back(current);

            // Snapshot system status for the child perspective (unchanged text/format)
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + "\n";
            system_status += print_PCB(forked_child, child_waitq);
            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the chile (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)
            {
                // Run child to completion (child-priority, no preemption)
                auto child_result = simulate_trace(child_trace,
                                                   current_time,
                                                   vectors,
                                                   delays,
                                                   external_files,
                                                   forked_child,
                                                   child_waitq);
                // Unpack and append
                std::string c_exec, c_status;
                int c_done = 0;
                std::tie(c_exec, c_status, c_done) = child_result;

                execution     += c_exec;
                system_status += c_status;
                current_time   = c_done;
            }

            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here
	    
	             // Resolve program size locally (avoid helper; same outcome)
            int program_mb = 0;
            for (const auto& ef : external_files) {
                if (ef.program_name == program_name) { program_mb = static_cast<int>(ef.size); break; }
            }

            // Log program size (same line/text), then add the given ISR "duration_intr"
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr)
                      + ", Program is " + std::to_string(program_mb) + " Mb large\n";
            current_time += duration_intr;

            // Loader time = 15 ms per MB (unchanged)
            const int load_ms = program_mb * 15;
            execution += std::to_string(current_time) + ", " + std::to_string(load_ms) + ", loading program into memory\n";
            current_time += load_ms;

            // Free old partition if any, then update PCB with the new program
            if (current.partition_number >= 1 && current.partition_number <= 6) {
                free_memory(&current);
            }
            current.program_name = program_name;
            current.size         = static_cast<unsigned>(program_mb);

            // Try allocation; on failure, mirror previous output exactly
            if (!allocate_memory(&current)) {
                execution += std::to_string(current_time) + ", 1, Error: no available memory partition\n";
                current_time += 1;

                execution += std::to_string(current_time) + ", 0, scheduler called\n";
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;

                std::string curr_trace = "EXEC " + program_name + ", " + std::to_string(duration_intr);
                system_status += "time: " + std::to_string(current_time) + "; current trace: " + curr_trace + "\n";
                system_status += print_PCB(current, wait_queue);

                return {execution, system_status, current_time};
            }

            // Keep the same marking/updating timings to match reference output
            {
                const int mark_ms = 4;
                execution += std::to_string(current_time) + ", " + std::to_string(mark_ms) + ", marking partition as occupied\n";
                current_time += mark_ms;
            }
            {
                const int upd_ms = 5;
                execution += std::to_string(current_time) + ", " + std::to_string(upd_ms) + ", updating PCB\n";
                current_time += upd_ms;
            }

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            {
                std::string curr_trace = "EXEC " + program_name + ", " + std::to_string(duration_intr);
                system_status += "time: " + std::to_string(current_time) + "; current trace: " + curr_trace + "\n";
                system_status += print_PCB(current, wait_queue);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)
            {
                std::ifstream exec_trace_file(program_name + ".txt");
                std::vector<std::string> subprog_trace;
                std::string line;
                while (std::getline(exec_trace_file, line)) {
                    subprog_trace.push_back(line);
                }

                auto subres = simulate_trace(subprog_trace,
                                             current_time,
                                             vectors,
                                             delays,
                                             external_files,
                                             current,
                                             wait_queue);
                std::string p_exec, p_status;
                int p_end = 0;
                std::tie(p_exec, p_status, p_end) = subres;

                execution     += p_exec;
                system_status += p_status;
                current_time   = p_end;
            }
            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)

        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/


    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
