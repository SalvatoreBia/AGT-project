#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "include/algorithm.h"
#include "include/data_structures.h"

#define NUM_RUNS 20
#define NUM_NODES 10000
#define K_PARAM 4

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

void print_stats(const char *algo_name, benchmark_stats *stats, int num_runs) {
    printf("\n=== %s ===\n", algo_name);
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

int run_single_benchmark(graph *g, int algorithm, int max_it) {
    game_system game;
    init_game(&game, g);
    
    if (algorithm == ALGO_RM) {
        init_regret_system(&game);
    } else if (algorithm == ALGO_FP || algorithm == ALGO_FP_ASYNC) {
        init_fictitious_system(&game);
    }
    
    int result = run_simulation(&game, algorithm, max_it, 0);
    int converged = (result != -1);
    int iterations = converged ? result : max_it;
    int valid = is_valid_cover(&game);
    int minimal = is_minimal(&game);
    
    if (algorithm == ALGO_RM) {
        free_regret_system(&game);
    } else if (algorithm == ALGO_FP || algorithm == ALGO_FP_ASYNC) {
        free_fictitious_system(&game);
    }
    
    free_game(&game);
    
    return (converged << 24) | (valid << 16) | (minimal << 8) | (iterations & 0xFF);
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
    printf("  Runs: %d\n", num_runs);
    printf("  Nodes: %d\n", num_nodes);
    printf("  Degree (k): %d\n", k_param);
    printf("  Max Iterations: %d\n", max_it);
    printf("  FP Restart Interval: %d (0=disabled)\n\n", fp_restart_interval);
    
    srand((unsigned int)time(NULL));
    
    benchmark_stats brd_stats, rm_stats, fp_stats, fp_async_stats;
    init_stats(&brd_stats);
    init_stats(&rm_stats);
    init_stats(&fp_stats);
    init_stats(&fp_async_stats);
    
    for (int run = 0; run < num_runs; run++) {
        printf("Run %d/%d: Generating graph...\n", run + 1, num_runs);
        
        graph *g = generate_random_regular(num_nodes, k_param);
        if (!g) {
            fprintf(stderr, "Error: Failed to generate graph for run %d\n", run + 1);
            continue;
        }
        
        printf("  Testing BRD... ");
        fflush(stdout);
        {
            game_system game;
            init_game(&game, g);
            
            clock_t start = clock();
            int result = run_simulation(&game, ALGO_BRD, max_it, 0);
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            
            int converged = (result != -1);
            int iterations = converged ? result : max_it;
            int valid = is_valid_cover(&game);
            int minimal = is_minimal(&game);
            
            update_stats(&brd_stats, elapsed, iterations, converged, valid, minimal);
            printf("%.3fs, %d iters, %s\n", elapsed, iterations, converged ? "converged" : "not converged");
            
            free_game(&game);
        }
        
        printf("  Testing RM... ");
        fflush(stdout);
        {
            game_system game;
            init_game(&game, g);
            init_regret_system(&game);
            
            clock_t start = clock();
            int result = run_simulation(&game, ALGO_RM, max_it, 0);
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            
            int converged = (result != -1);
            int iterations = converged ? result : max_it;
            int valid = is_valid_cover(&game);
            int minimal = is_minimal(&game);
            
            update_stats(&rm_stats, elapsed, iterations, converged, valid, minimal);
            printf("%.3fs, %d iters, %s\n", elapsed, iterations, converged ? "converged" : "not converged");
            
            free_regret_system(&game);
            free_game(&game);
        }
        
        printf("  Testing FP... ");
        fflush(stdout);
        {
            game_system game;
            init_game(&game, g);
            init_fictitious_system(&game);
            
            clock_t start = clock();
            int result = run_simulation_with_restart(&game, ALGO_FP, max_it, 0, fp_restart_interval);
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            
            int converged = (result != -1);
            int iterations = converged ? result : max_it;
            int valid = is_valid_cover(&game);
            int minimal = is_minimal(&game);
            
            update_stats(&fp_stats, elapsed, iterations, converged, valid, minimal);
            printf("%.3fs, %d iters, %s\n", elapsed, iterations, converged ? "converged" : "not converged");
            
            free_fictitious_system(&game);
            free_game(&game);
        }
        
        printf("  Testing FP_ASYNC... ");
        fflush(stdout);
        {
            game_system game;
            init_game(&game, g);
            init_fictitious_system(&game);
            
            clock_t start = clock();
            int result = run_simulation(&game, ALGO_FP_ASYNC, max_it, 0);
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            
            int converged = (result != -1);
            int iterations = converged ? result : max_it;
            int valid = is_valid_cover(&game);
            int minimal = is_minimal(&game);
            
            update_stats(&fp_async_stats, elapsed, iterations, converged, valid, minimal);
            printf("%.3fs, %d iters, %s\n", elapsed, iterations, converged ? "converged" : "not converged");
            
            free_fictitious_system(&game);
            free_game(&game);
        }
        
        free_graph(g);
        printf("\n");
    }
    
    printf("\n========================================\n");
    printf("         BENCHMARK RESULTS SUMMARY       \n");
    printf("========================================\n");
    
    print_stats("Best Response Dynamics (BRD)", &brd_stats, num_runs);
    print_stats("Regret Matching (RM)", &rm_stats, num_runs);
    print_stats("Fictitious Play (FP)", &fp_stats, num_runs);
    print_stats("Async Fictitious Play (FP_ASYNC)", &fp_async_stats, num_runs);
    
    printf("\n========================================\n");
    printf("         COMPARISON TABLE                \n");
    printf("========================================\n\n");
    
    printf("%-15s | %-12s | %-12s | %-10s | %-10s\n", 
           "Algorithm", "Avg Time", "Avg Iters", "Conv Rate", "Valid Rate");
    printf("----------------|--------------|--------------|------------|------------\n");
    printf("%-15s | %10.4fs | %10.1f | %8.1f%% | %8.1f%%\n",
           "BRD",
           brd_stats.total_time / num_runs,
           (double)brd_stats.total_iterations / num_runs,
           100.0 * brd_stats.converged_count / num_runs,
           100.0 * brd_stats.valid_cover_count / num_runs);
    printf("%-15s | %10.4fs | %10.1f | %8.1f%% | %8.1f%%\n",
           "RM",
           rm_stats.total_time / num_runs,
           (double)rm_stats.total_iterations / num_runs,
           100.0 * rm_stats.converged_count / num_runs,
           100.0 * rm_stats.valid_cover_count / num_runs);
    printf("%-15s | %10.4fs | %10.1f | %8.1f%% | %8.1f%%\n",
           "FP",
           fp_stats.total_time / num_runs,
           (double)fp_stats.total_iterations / num_runs,
           100.0 * fp_stats.converged_count / num_runs,
           100.0 * fp_stats.valid_cover_count / num_runs);
    printf("%-15s | %10.4fs | %10.1f | %8.1f%% | %8.1f%%\n",
           "FP_ASYNC",
           fp_async_stats.total_time / num_runs,
           (double)fp_async_stats.total_iterations / num_runs,
           100.0 * fp_async_stats.converged_count / num_runs,
           100.0 * fp_async_stats.valid_cover_count / num_runs);
    
    return 0;
}
