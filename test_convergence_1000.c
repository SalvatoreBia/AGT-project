#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "include/algorithm.h"
#include "include/data_structures.h"

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int total_runs = 1000;
    if (argc > 1) {
        total_runs = atoi(argv[1]);
    }
    int success_count = 0;
    int total_iterations = 0;
    int min_iterations = UINT64_MAX;
    int max_iterations = 0;
    
    printf("Starting batch test 1000 runs...\n");
    
    for (int i = 0; i < total_runs; i++) {
        // Generate graph (N=10000, K=4)
        graph *g = generate_random_regular(10000, 4);
        if (!g) {
            fprintf(stderr, "Failed to generate graph at run %d\n", i);
            continue;
        }
        
        game_system game;
        init_game(&game, g);
        init_fictitious_system(&game);
        
        // Run verification with 2M iteration cap
        // The algorithm now has Random Restarts every 10k iterations if stuck
        int result = run_simulation(&game, ALGO_FP, 2000000, 0);
        
        int converged = (result != -1);
        int minimal = 0;
        int valid = 0;
        
        if (converged) {
            minimal = is_minimal(&game);
            valid = is_valid_cover(&game);
        }
        
        if (converged && minimal && valid) {
            success_count++;
            int iters = game.iteration;
            total_iterations += iters;
            if (iters < min_iterations) min_iterations = iters;
            if (iters > max_iterations) max_iterations = iters;
        } else {
            printf("Run %d FAILED. Converged: %d, Valid: %d, Minimal: %d, Iters: %d\n", 
                   i, converged, valid, minimal, game.iteration);
        }

        // Cleanup
        free_fictitious_system(&game);
        free(game.strategies);
        free_graph(g);
        
        if ((i + 1) % 10 == 0) {
            printf("Progress: %d/%d... Success so far: %d\n", i + 1, total_runs, success_count);
            fflush(stdout);
        }
    }
    
    printf("\n=== BATCH TEST RESULTS ===\n");
    printf("Total Runs: %d\n", total_runs);
    printf("Successes: %d (%.2f%%)\n", success_count, (double)success_count/total_runs * 100.0);
    if (success_count > 0) {
        printf("Iterations Stats (Successes only):\n");
        printf("  Min: %d\n", min_iterations);
        printf("  Max: %d\n", max_iterations);
        printf("  Avg: %.2f\n", (double)total_iterations / success_count);
    }
    
    return 0;
}
