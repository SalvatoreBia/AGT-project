#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "include/algorithm.h"
#include "include/data_structures.h"

#define NUM_RUNS 20
#define NUM_NODES 10000
#define K_PARAM 4

#define TYPE_REGULAR 0
#define TYPE_ERDOS 1
#define TYPE_BARABASI 2

#define NUM_GRAPH_TYPES 3
#define NUM_ALGORITHMS 4

typedef struct {
    double total_time;
    double min_time;
    double max_time;
    int total_iterations;
    int min_iterations;
    int max_iterations;
    int converged_count;
    int valid_cover_count;
    int minimal_count;
} benchmark_stats;

void init_stats(benchmark_stats *stats) {
    stats->total_time = 0.0;
    stats->min_time = 1e9;
    stats->max_time = 0.0;
    stats->total_iterations = 0;
    stats->min_iterations = 1000000;
    stats->max_iterations = 0;
    stats->converged_count = 0;
    stats->valid_cover_count = 0;
    stats->minimal_count = 0;
}

void update_stats(benchmark_stats *stats, double time, int iterations, int converged, int valid, int minimal) {
    stats->total_time += time;
    if (time < stats->min_time) stats->min_time = time;
    if (time > stats->max_time) stats->max_time = time;
    
    stats->total_iterations += iterations;
    if (iterations < stats->min_iterations) stats->min_iterations = iterations;
    if (iterations > stats->max_iterations) stats->max_iterations = iterations;
    
    if (converged) stats->converged_count++;
    if (valid) stats->valid_cover_count++;
    if (minimal) stats->minimal_count++;
}

void print_stats(const char *algo_name, const char *graph_name, benchmark_stats *stats, int num_runs) {
    printf("\n=== %s on %s ===\n", algo_name, graph_name);
    printf("Runs: %d\n", num_runs);
    printf("Time (avg/min/max): %.4fs / %.4fs / %.4fs\n", 
           stats->total_time / num_runs, 
           stats->min_time, 
           stats->max_time);
    printf("Iterations (avg/min/max): %.1f / %d / %d\n", 
           (double)stats->total_iterations / num_runs, 
           stats->min_iterations, 
           stats->max_iterations);
    printf("Converged: %d/%d (%.1f%%)\n", 
           stats->converged_count, num_runs, 
           100.0 * stats->converged_count / num_runs);
    printf("Valid Cover: %d/%d (%.1f%%)\n", 
           stats->valid_cover_count, num_runs, 
           100.0 * stats->valid_cover_count / num_runs);
    printf("Minimal: %d/%d (%.1f%%)\n", 
           stats->minimal_count, num_runs, 
           100.0 * stats->minimal_count / num_runs);
}

const char* get_graph_type_name(int type) {
    switch (type) {
        case TYPE_REGULAR: return "Regular";
        case TYPE_ERDOS: return "Erdos-Renyi";
        case TYPE_BARABASI: return "Barabasi-Albert";
        default: return "Unknown";
    }
}

const char* get_algo_name(int algo) {
    switch (algo) {
        case ALGO_BRD: return "BRD";
        case ALGO_RM: return "RM";
        case ALGO_FP: return "FP";
        case ALGO_FP_ASYNC: return "FP_ASYNC";
        default: return "Unknown";
    }
}

graph* generate_graph(int type, int num_nodes, int k_param) {
    switch (type) {
        case TYPE_REGULAR:
            return generate_random_regular(num_nodes, k_param);
        case TYPE_ERDOS: {
            double p = (double)k_param / (double)(num_nodes - 1);
            return generate_erdos_renyi(num_nodes, p);
        }
        case TYPE_BARABASI:
            return generate_barabasi_albert(num_nodes, k_param);
        default:
            return NULL;
    }
}

void run_algorithm_benchmark(graph *g, int algorithm, int max_it, int fp_restart_interval,
                             benchmark_stats *stats) {
    game_system game;
    init_game(&game, g);
    
    if (algorithm == ALGO_RM) {
        init_regret_system(&game);
    } else if (algorithm == ALGO_FP || algorithm == ALGO_FP_ASYNC) {
        init_fictitious_system(&game);
    }
    
    clock_t start = clock();
    int result;
    if (algorithm == ALGO_FP) {
        result = run_simulation_with_restart(&game, algorithm, max_it, 0, fp_restart_interval);
    } else {
        result = run_simulation(&game, algorithm, max_it, 0);
    }
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    int converged = (result != -1);
    int iterations = converged ? result : max_it;
    int valid = is_valid_cover(&game);
    int minimal = is_minimal(&game);
    
    update_stats(stats, elapsed, iterations, converged, valid, minimal);
    
    if (algorithm == ALGO_RM) {
        free_regret_system(&game);
    } else if (algorithm == ALGO_FP || algorithm == ALGO_FP_ASYNC) {
        free_fictitious_system(&game);
    }
    
    free_game(&game);
}

int main(int argc, char *argv[]) {
    int num_runs = NUM_RUNS;
    int num_nodes = NUM_NODES;
    int k_param = K_PARAM;
    int max_it = 10000;
    int fp_restart_interval = 1000;
    
    if (argc > 1) num_runs = atoi(argv[1]);
    if (argc > 2) num_nodes = atoi(argv[2]);
    if (argc > 3) k_param = atoi(argv[3]);
    if (argc > 4) max_it = atoi(argv[4]);
    if (argc > 5) fp_restart_interval = atoi(argv[5]);
    
    printf("=== STRATEGIC GAME ALGORITHM BENCHMARK ===\n");
    printf("Configuration:\n");
    printf("  Runs per combination: %d\n", num_runs);
    printf("  Nodes: %d\n", num_nodes);
    printf("  Degree/Param (k): %d\n", k_param);
    printf("  Max Iterations: %d\n", max_it);
    printf("  FP Restart Interval: %d (0=disabled)\n", fp_restart_interval);
    printf("\n  Graph Types: Regular, Erdos-Renyi, Barabasi-Albert\n");
    printf("  Algorithms: BRD, RM, FP, FP_ASYNC\n\n");
    
    srand((unsigned int)time(NULL));
    
    // Stats: [graph_type][algorithm]
    benchmark_stats stats[NUM_GRAPH_TYPES][NUM_ALGORITHMS];
    int algo_ids[NUM_ALGORITHMS] = {ALGO_BRD, ALGO_RM, ALGO_FP, ALGO_FP_ASYNC};
    
    for (int gt = 0; gt < NUM_GRAPH_TYPES; gt++) {
        for (int a = 0; a < NUM_ALGORITHMS; a++) {
            init_stats(&stats[gt][a]);
        }
    }
    
    // Run benchmarks for each graph type
    for (int gt = 0; gt < NUM_GRAPH_TYPES; gt++) {
        printf("========================================\n");
        printf("Graph Type: %s\n", get_graph_type_name(gt));
        printf("========================================\n\n");
        
        for (int run = 0; run < num_runs; run++) {
            printf("Run %d/%d: Generating %s graph... ", run + 1, num_runs, get_graph_type_name(gt));
            fflush(stdout);
            
            graph *g = generate_graph(gt, num_nodes, k_param);
            if (!g) {
                fprintf(stderr, "\nError: Failed to generate graph for run %d\n", run + 1);
                continue;
            }
            printf("(%d nodes, %d edges)\n", g->num_nodes, g->num_edges);
            
            for (int a = 0; a < NUM_ALGORITHMS; a++) {
                printf("  Testing %s... ", get_algo_name(algo_ids[a]));
                fflush(stdout);
                
                clock_t start = clock();
                
                game_system game;
                init_game(&game, g);
                
                if (algo_ids[a] == ALGO_RM) {
                    init_regret_system(&game);
                } else if (algo_ids[a] == ALGO_FP || algo_ids[a] == ALGO_FP_ASYNC) {
                    init_fictitious_system(&game);
                }
                
                int result;
                if (algo_ids[a] == ALGO_FP) {
                    result = run_simulation_with_restart(&game, algo_ids[a], max_it, 0, fp_restart_interval);
                } else {
                    result = run_simulation(&game, algo_ids[a], max_it, 0);
                }
                
                double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
                int converged = (result != -1);
                int iterations = converged ? result : max_it;
                int valid = is_valid_cover(&game);
                int minimal = is_minimal(&game);
                
                update_stats(&stats[gt][a], elapsed, iterations, converged, valid, minimal);
                printf("%.3fs, %d iters, %s\n", elapsed, iterations, converged ? "converged" : "not converged");
                
                if (algo_ids[a] == ALGO_RM) {
                    free_regret_system(&game);
                } else if (algo_ids[a] == ALGO_FP || algo_ids[a] == ALGO_FP_ASYNC) {
                    free_fictitious_system(&game);
                }
                free_game(&game);
            }
            
            free_graph(g);
            printf("\n");
        }
    }
    
    // Print detailed results
    printf("\n");
    printf("########################################\n");
    printf("         DETAILED RESULTS               \n");
    printf("########################################\n");
    
    for (int gt = 0; gt < NUM_GRAPH_TYPES; gt++) {
        for (int a = 0; a < NUM_ALGORITHMS; a++) {
            print_stats(get_algo_name(algo_ids[a]), get_graph_type_name(gt), &stats[gt][a], num_runs);
        }
    }
    
    // Print comparison tables per graph type
    printf("\n");
    printf("########################################\n");
    printf("         COMPARISON TABLES              \n");
    printf("########################################\n");
    
    for (int gt = 0; gt < NUM_GRAPH_TYPES; gt++) {
        printf("\n--- %s Graph ---\n\n", get_graph_type_name(gt));
        printf("%-12s | %-10s | %-10s | %-9s | %-9s | %-9s\n", 
               "Algorithm", "Avg Time", "Avg Iters", "Conv %", "Valid %", "Minimal %");
        printf("-------------|------------|------------|-----------|-----------|----------\n");
        
        for (int a = 0; a < NUM_ALGORITHMS; a++) {
            printf("%-12s | %8.4fs | %8.1f | %7.1f%% | %7.1f%% | %7.1f%%\n",
                   get_algo_name(algo_ids[a]),
                   stats[gt][a].total_time / num_runs,
                   (double)stats[gt][a].total_iterations / num_runs,
                   100.0 * stats[gt][a].converged_count / num_runs,
                   100.0 * stats[gt][a].valid_cover_count / num_runs,
                   100.0 * stats[gt][a].minimal_count / num_runs);
        }
    }
    
    // Print grand summary comparing graph types
    printf("\n");
    printf("########################################\n");
    printf("         SUMMARY BY ALGORITHM           \n");
    printf("########################################\n");
    
    for (int a = 0; a < NUM_ALGORITHMS; a++) {
        printf("\n--- %s ---\n\n", get_algo_name(algo_ids[a]));
        printf("%-15s | %-10s | %-10s | %-9s\n", 
               "Graph Type", "Avg Time", "Avg Iters", "Conv %");
        printf("----------------|------------|------------|----------\n");
        
        for (int gt = 0; gt < NUM_GRAPH_TYPES; gt++) {
            printf("%-15s | %8.4fs | %8.1f | %7.1f%%\n",
                   get_graph_type_name(gt),
                   stats[gt][a].total_time / num_runs,
                   (double)stats[gt][a].total_iterations / num_runs,
                   100.0 * stats[gt][a].converged_count / num_runs);
        }
    }
    
    return 0;
}
