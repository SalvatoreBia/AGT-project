#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>
#include "include/algorithm.h"
#include "include/data_structures.h"

#define ALGO_BRD 1
#define ALGO_RM 2
#define ALGO_FP 3

#define GRAPH_FILENAME "graph_dump.bin"

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -n <nodes>       Number of nodes (default: 10000)\n");
    printf("  -k <edges>       Number of edges per node (default: 4)\n");
    printf("  -i <iterations>  Maximum number of iterations (default: 1000)\n");
    printf("  -a <algorithm>   Algorithm to use (1=BRD, 2=RM, 3=FP) (default: 3)\n");
    printf("  -h               Show this help message\n");
}

int main(int argc, char *argv[])
{
    // Default values
    uint64_t num_nodes = 10000;
    uint64_t num_edges_per_node = 4;
    uint64_t max_it = 1000;
    int algorithm = ALGO_FP;

    int opt;
    while ((opt = getopt(argc, argv, "n:k:i:a:h")) != -1) {
        switch (opt) {
            case 'n':
                num_nodes = strtoull(optarg, NULL, 10);
                break;
            case 'k':
                num_edges_per_node = strtoull(optarg, NULL, 10);
                break;
            case 'i':
                max_it = strtoull(optarg, NULL, 10);
                break;
            case 'a':
                algorithm = atoi(optarg);
                if (algorithm < ALGO_BRD || algorithm > ALGO_FP) {
                    fprintf(stderr, "Invalid algorithm selection. Use 1, 2, or 3.\n");
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    game_system game;
    graph *g = NULL;
    srand((unsigned int)time(NULL));

    printf("Checking for graph '%s'...\n", GRAPH_FILENAME);

    // decommenta per caricare il dump
    // g = load_graph_from_file(GRAPH_FILENAME);

    if (g)
    {
        printf("Graph loaded successfully!\n");
    }
    else
    {
        printf("Generating new random regular graph with %" PRIu64 " nodes and degree %" PRIu64 "...\n", num_nodes, num_edges_per_node);
        g = generate_random_regular(num_nodes, num_edges_per_node);
        if (!g)
        {
            fprintf(stderr, "Error: Failed to generate graph.\n");
            return 1;
        }
        save_graph_to_file(g, GRAPH_FILENAME);
    }

    clock_t start_time = clock();
    init_game(&game, g);

    if (algorithm == ALGO_BRD) {
        printf("Algorithm: Best Response Dynamics (BRD)\n");
    } else if (algorithm == ALGO_RM) {
        printf("Algorithm: Regret Matching (RM)\n");
        init_regret_system(&game);
    } else if (algorithm == ALGO_FP) {
        printf("Algorithm: Fictitious Play (FP)\n");
        init_fictious_system(&game);
    }

    uint64_t converged = 0;

    while (game.iteration < max_it)
    {
        if (game.iteration % 100 == 0)
        {
            printf("--- It %" PRIu64 " ---\n", game.iteration + 1);
        }

        int change = 0;
        if (algorithm == ALGO_BRD) {
            change = run_best_response_iteration(&game);
        } else if (algorithm == ALGO_RM) {
            change = run_regret_matching_iteration(&game);
        } else if (algorithm == ALGO_FP) {
            change = run_fictious_play_iteration(&game);
        }
        
        if (!change) {
            converged = 1;
            printf("Convergence reached at it %" PRIu64 "\n", game.iteration);
            break;
        }

        game.iteration++;
    }

    double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("\nSimulation finished in %.2fs\n", elapsed);
    
    if (converged) {
        printf("Converged: YES\n");
    } else {
        printf("Converged: NO\n");
    }

    // 4. ANALISI RISULTATI
    int minimal = is_minimal(&game);
    int valid = is_valid_cover(&game);

    long active_count = 0;
    for (uint64_t i = 0; i < game.num_players; ++i)
    {
        if (game.strategies[i] == 1)
            active_count++;
    }

    printf("Cover Size: %ld / %" PRIu64 "\n", active_count, game.num_players);
    printf("Valid Cover: %s\n", valid ? "YES" : "NO");
    printf("Minimal Local: %s\n", minimal ? "YES" : "NO");

    if (algorithm == ALGO_RM) {
        free_regret_system(&game);
    } else if (algorithm == ALGO_FP) {
        free_fictious_system(&game);
    }

    free_game(&game);
    free_graph(g);

    return 0;
}