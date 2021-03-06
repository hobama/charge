#include "experiments/experiment_runner.hpp"
#include "experiments/files.hpp"

#include "common/files.hpp"
#include "common/graph_statistics.hpp"
#include "common/graph_transform.hpp"
#include "common/timed_logger.hpp"

#include "ev/battery.hpp"
#include "ev/fp_dijkstra.hpp"
#include "ev/graph_transform.hpp"

#include <string>

int main(int argc, char **argv) {
    if (argc < 10) {
        std::cerr << argv[0] << " EXPERIMENT_PATH NUM_RUNS NUM_QUERIES THREADS LOG_PATH "
                                "GRAPH_BASE_PATH CAPACITY_WH POTENTIAL X_EPS Y_EPS"
                  << std::endl;
        std::cerr
            << "Example:" << argv[0]
            << " cache/luxev/random 10 10000 2 results/random_dijkstra data/luxev 16000 fastest 0.1 1"
            << std::endl;
        return EXIT_FAILURE;
    }

    const std::string experiment_path = argv[1];
    const std::size_t num_runs = std::stoi(argv[2]);
    const std::size_t threads = std::stoi(argv[3]);
    const std::string experiment_log = argv[4];
    const std::string graph_base = argv[5];
    const double capacity = std::stof(argv[6]);
    const std::string potential = argv[7];
    const double x_eps = std::stof(argv[8]);
    const double y_eps = std::stof(argv[9]);

    using namespace charge;

    common::TimedLogger load_timer("Loading graph");
    const auto graph = ev::TradeoffGraph{
        common::files::read_weighted_graph<ev::TradeoffGraph::weight_t>(graph_base)};
    double min_tradeoff_rate = graph.min_tradeoff_rate();
    const auto reverse_min_duration_graph = common::invert(ev::tradeoff_to_min_duration(graph));
    const auto min_consumption_graph = ev::tradeoff_to_min_consumption(graph);
    const auto reverse_consumption_graph = common::invert(min_consumption_graph);
    const auto reverse_omega_graph =
        common::invert(ev::tradeoff_to_omega_graph(graph, min_tradeoff_rate));
    const auto heights = common::files::read_heights(graph_base);
    const auto coordinates = common::files::read_coordinates(graph_base);
    const auto queries = experiments::files::read_queries(experiment_path);
    load_timer.finished();

    std::cerr << common::get_statistics(graph) << std::endl;

    experiments::ResultLogger result_logger{experiment_log + ".json", coordinates, heights};

    if (potential == "fastest") {
        common::TimedLogger setup_timer("Setting up experiment");
        auto runner = experiments::make_experiment_runner(
            ev::FPAStarContext{x_eps, y_eps, capacity, graph, reverse_min_duration_graph}, std::move(queries),
            experiment_log, result_logger, num_runs);
        setup_timer.finished();

        runner.run(threads);
        runner.summary();
    } else if (potential == "omega") {
        common::TimedLogger setup_timer("Setting up experiment");
        auto runner = experiments::make_experiment_runner(
            ev::FPAStarOmegaContext{x_eps, y_eps, capacity, min_tradeoff_rate, graph, reverse_min_duration_graph,
                                    reverse_consumption_graph, reverse_omega_graph},
            std::move(queries), experiment_log, result_logger, num_runs);
        setup_timer.finished();

        runner.run(threads);
        runner.summary();
    } else if (potential == "none") {
        common::TimedLogger setup_timer("Setting up experiment");
        auto runner = experiments::make_experiment_runner(ev::FPDijkstraContext{x_eps, y_eps, capacity, graph},
                                                          std::move(queries), experiment_log,
                                                          result_logger, num_runs);
        setup_timer.finished();

        runner.run(threads);
        runner.summary();
    } else {
        throw std::runtime_error("Unknown potential name: " + potential);
    }

    return EXIT_SUCCESS;
}
