#include "algo.h"

#include <chrono>
#include <stdlib.h>
#include <version.h>

#include "boost/program_options.hpp"

#include "boost/log/core.hpp"
#include "boost/log/expressions.hpp"
#include "boost/log/trivial.hpp"
#include "boost/log/utility/setup/common_attributes.hpp"
#include "boost/log/utility/setup/console.hpp"
#include "boost/log/utility/setup/file.hpp"

// Initializes the logger
void log_init() {
      std::string log_format = "[%TimeStamp%] [%Severity%] [%Message%]";
      std::string log_path = "planarity_filter.log";
      boost::log::add_file_log(log_path, boost::log::keywords::format = log_format,
                               boost::log::keywords::open_mode = std::ios_base::app);
      boost::log::add_console_log(std::cout, boost::log::keywords::format = log_format);
      boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
      boost::log::add_common_attributes();
  }

int main(int argc, char *argv[]) {
    log_init();
    
    int num_threads = 1;
    bool large_graph = false;
    size_t num_input_nodes = 100000000;

    // Get args
    namespace po = boost::program_options;

    po::options_description desc("Arguments");
    desc.add_options()("help,h", "display help message")
        ("input,i", po::value<std::string>()->required(), "input file path")
        ("output,o", po::value<std::string>()->required(), "output file path")
	("threads,t", po::value<int>(&num_threads), "number of threads to use")
	("large,l", "large graph flag, input must use unsigned ints for node identifiers")
	("nodes,n", po::value<size_t>(&num_input_nodes), "number of nodes, use with large graph flag");

    po::variables_map var_map;

    try {
        po::store(po::parse_command_line(argc, argv, desc), var_map);
        if (var_map.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(var_map);
    } catch (po::error &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        std::cerr << desc << "\n";
        return 1;
    } catch (std::exception &e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 2;
    }
    
    if (var_map.count("large")) {
	large_graph = true;
    } 

    // Log metadata about the run
    BOOST_LOG_TRIVIAL(info) << "#######################################";
    BOOST_LOG_TRIVIAL(info) << "New run, options listed below";
    BOOST_LOG_TRIVIAL(info) << "git branch: " << GIT_BRANCH;
    BOOST_LOG_TRIVIAL(info) << "abbrev. commit hash: " << GIT_COMMIT_HASH;
    BOOST_LOG_TRIVIAL(info) << "Input: " << var_map["input"].as<std::string>();
    BOOST_LOG_TRIVIAL(info) << "Output: " << var_map["output"].as<std::string>();
    BOOST_LOG_TRIVIAL(info) << "Num. threads: " << num_threads;
    BOOST_LOG_TRIVIAL(info) << "Large graph flag: " << large_graph;

    BOOST_LOG_TRIVIAL(info) << "Loading input";
    
    adjacency_list input_graph;
    std::unordered_map<node, std::string> node_labels;
    load_result lr;

    if (large_graph ) {
	input_graph = load_adj_list(var_map["input"].as<std::string>(),
				    num_input_nodes);
    } else {
	load_result lr = load_edge_list(var_map["input"].as<std::string>());
	input_graph = to_adj_list(std::get<0>(lr));
	node_labels = std::get<2>(lr);
    
    
	BOOST_LOG_TRIVIAL(info) << "Checking to see if graph is already planar";

	if (boyer_myrvold_test(input_graph)) {
	    BOOST_LOG_TRIVIAL(info) << "The provided graph is already planar";
	    exit(EXIT_SUCCESS);
	}
    }
    // dedup input graph
    dedup(input_graph);

    BOOST_LOG_TRIVIAL(info) << "Running algo_routine";
    auto start = std::chrono::high_resolution_clock::now();
    adjacency_list result_graph = algo_routine(input_graph, num_threads);
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    
    dedup(result_graph);
    
    if (!large_graph) {
	if (!boyer_myrvold_test(result_graph)) {
	    BOOST_LOG_TRIVIAL(error) << "Error: the result graph is not planar";
	    exit(EXIT_FAILURE);
	}
    }
    size_t input_n_edges = num_edges(input_graph);
    size_t result_n_edges = num_edges(result_graph);

    BOOST_LOG_TRIVIAL(info) << "Execution time: " << elapsed.count() << "s";
    BOOST_LOG_TRIVIAL(info) << "Initial graph - " << "nodes: " << input_graph.size()
        << " edges: " << input_n_edges;
    BOOST_LOG_TRIVIAL(info) << "Result graph - " << "nodes: " << result_graph.size()
        << " edges: " << result_n_edges;
    BOOST_LOG_TRIVIAL(info) << "Percent edges retained: "
        << (float) result_n_edges / (float) input_n_edges * 100;
    
    if (!large_graph) {
	write_graph(result_graph, node_labels, var_map["output"].as<std::string>());
    }
    
    return 0;
}
