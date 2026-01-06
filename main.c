#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include "include/algorithm.h"
#include "include/data_structures.h"
#include "include/min_cost_flow.h"
#include "include/auction.h"
#include "include/logging.h"

#define GRAPH_FILENAME "graph_dump.bin"

#define TYPE_REGULAR 0
#define TYPE_ERDOS 1
#define TYPE_BARABASI 2

#define ALGO_SHAPLEY 4

void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -n <nodes>       Number of nodes (default: 10000)\n");
    printf("  -k <val>         Degree/Param (Reg: degree, ER: avg degree, BA: m) (default: 4)\n");
    printf("  -t <type>        Graph Type (0=Regular, 1=Erdos, 2=Barabasi) (default: 0)\n");
    printf("  -i <iterations>  Maximum number of iterations (default: 1000)\n");
    printf("  -a <algorithm>   Algorithm to use (1=BRD, 2=RM, 3=FP, 4=Shapley) (default: 3)\n");
    printf("  -v <version>     Characteristic function version for Shapley (1, 2, or 3) (default: 3)\n");
    printf("  -c <capacity>    Capacity Mode (0=Infinite, 1=Limited, 2=Both) (default: 0)\n");
    printf("  -h               Show this help message\n");
}

int main(int argc, char *argv[])
{
    // Default values
    int num_nodes = 10000;
    int k_param = 4;
    int max_it = 100000;
    int algorithm = ALGO_FP;
    int graph_type = TYPE_REGULAR;
    int shapley_version = 3;
    int capacity_mode = 0; // 0=Inf, 1=Lim, 2=Both

    int opt;
    while ((opt = getopt(argc, argv, "n:k:i:a:t:v:c:h")) != -1)
    {
        switch (opt)
        {
        case 'n':
            num_nodes = strtoull(optarg, NULL, 10);
            break;
        case 'k':
            k_param = strtoull(optarg, NULL, 10);
            break;
        case 't':
            graph_type = atoi(optarg);
            if (graph_type < 0 || graph_type > 2)
            {
                fprintf(stderr, "Invalid graph type. Use 0, 1, or 2.\n");
                return 1;
            }
            break;
        case 'i':
            max_it = strtoull(optarg, NULL, 10);
            break;
        case 'a':
            algorithm = atoi(optarg);
            if (algorithm < ALGO_BRD || algorithm > ALGO_SHAPLEY)
            {
                fprintf(stderr, "Invalid algorithm selection. Use 1, 2, 3, or 4.\n");
                return 1;
            }
            break;
        case 'v':
            shapley_version = atoi(optarg);
            if (shapley_version < 1 || shapley_version > 3)
            {
                fprintf(stderr, "Invalid version. Use 1, 2, or 3.\n");
                return 1;
            }
            break;
        case 'c':
            capacity_mode = atoi(optarg);
            if (capacity_mode < 0 || capacity_mode > 2)
            {
                fprintf(stderr, "Invalid capacity mode. Use 0, 1, or 2.\n");
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

    // NOTE: If you want to force regeneration of graph based on type,
    // you might want to disable the file loading or save different files for different types.
    // For now, I will overwrite logic to prefer generation if arguments are specific.

    printf("Generating graph type %d with %d nodes and param %d...\n", graph_type, num_nodes, k_param);

    if (graph_type == TYPE_REGULAR)
    {
        g = generate_random_regular(num_nodes, k_param);
    }
    else if (graph_type == TYPE_ERDOS)
    {
        // Calculate probability p from average degree k: k = p * (N-1) => p = k / (N-1)
        double p = (double)k_param / (double)(num_nodes - 1);
        printf("Erdos-Renyi: calculated p = %lf\n", p);
        g = generate_erdos_renyi(num_nodes, p);
    }
    else if (graph_type == TYPE_BARABASI)
    {
        printf("Barabasi-Albert: m = %d\n", k_param);
        g = generate_barabasi_albert(num_nodes, k_param);
    }

    if (!g)
    {
        fprintf(stderr, "Error: Failed to generate graph.\n");
        return 1;
    }

    // Save generated graph
    save_graph_to_file(g, GRAPH_FILENAME);

    // Initialize Logging
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "log_n%d_k%d_t%d_a%d_c%d.log", 
             num_nodes, k_param, graph_type, algorithm, capacity_mode);
    LOG_INIT(log_filename);

    clock_t start_time = clock();

    if (algorithm == ALGO_SHAPLEY)
    {
        // COALITIONAL GAME APPROACH
        printf("\n=== COALITIONAL GAME APPROACH ===\n");
        printf("Algorithm: Shapley Values (Monte Carlo)\n");
        printf("Characteristic function version: %d\n", shapley_version);
        printf("Monte Carlo iterations: %d\n\n", max_it);

        // Calcola Shapley values
        double *shapley_values = calculate_shapley_values(g, (int)max_it, shapley_version);

        // Costruisci security set
        // NOTE: assigning to local variable first, then we will use a common pointer later
        unsigned char *shapley_set = build_security_set_from_shapley(g, shapley_values);

        double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        printf("\nShapley computation finished in %.2fs\n", elapsed);

        // Analisi risultati
        int active_count = 0;
        for (int i = 0; i < g->num_nodes; ++i)
        {
            if (shapley_set[i])
                active_count++;
        }

        // Crea coalizione per verifiche
        int *coalition = malloc(active_count * sizeof(int));
        size_t idx = 0;
        for (int i = 0; i < g->num_nodes; ++i)
        {
            if (shapley_set[i])
            {
                coalition[idx++] = i;
            }
        }

        int valid = is_coalition_valid_cover(g, coalition, active_count);
        int minimal = is_coalition_minimal(g, coalition, active_count);

        printf("\n=== RESULTS ===\n");
        printf("Cover Size: %d / %d (%.2f%%)\n", 
               active_count, g->num_nodes, (double)active_count / g->num_nodes * 100.0);
        printf("Valid Cover: %s\n", valid ? "YES" : "NO");
        printf("Minimal: %s\n", minimal ? "YES" : "NO");

        // Stampa top 10 Shapley values
        printf("\nTop 10 nodes by Shapley value:\n");
        typedef struct
        {
            int id;
            double value;
        } node_shapley;

        node_shapley *sorted = malloc(g->num_nodes * sizeof(node_shapley));
        for (int i = 0; i < g->num_nodes; ++i)
        {
            sorted[i].id = i;
            sorted[i].value = shapley_values[i];
        }

        // Simple bubble sort per top 10
        for (int i = 0; i < 10 && i < g->num_nodes; ++i)
        {
            for (int j = i + 1; j < g->num_nodes; ++j)
            {
                if (sorted[j].value > sorted[i].value)
                {
                    node_shapley temp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = temp;
                }
            }
        }

        for (int i = 0; i < 10 && i < g->num_nodes; ++i)
        {
            printf("  %2d. Node %d: %.6f %s\n", 
                   i + 1, sorted[i].id, sorted[i].value,
                   shapley_set[sorted[i].id] ? "(in set)" : "");
        }

        free(coalition);
        free(sorted);
        free(shapley_values);

        // IMPORTANT: We do NOT free shapley_set here, we will use it for Part 3/4
        // But since we need a common interface, let's just run parts here or assign to a common pointer.
        // To keep clean decoupling, let's assign to a pointer declared outside or trigger parts here? 
        // Better: We will fall through to common code.
        // Assign to a variable visible outside. But 'security_set' name conflicts if we just did that.
        // Let's rely on a common 'final_set' pointer.
        
        // Hack: Since 'game' variable is local to main but initialized only in else, 
        // we need a common pointer for the final security set array.
        
        // We will execute the parts AT THE END using this array.
        // But wait, shapley_set is malloc'ed, game.strategies is malloc'ed.
        // We need to manage memory.
        
        // Let's use a goto or simply continue execution.
        // We need to store the result in a common place.
        
        // Let's allocate a new array for the unified result? Or just point to it.
        // Simplest: use 'game.strategies' is not available here.
        // Wait, game_system game; IS declared at top of main.
        // So we can populate game.strategies manually for Shapley case!
        
        init_game(&game, g); // Reset game struct (allocates strategies)
        // Copy shapley_set to strategies
        memcpy(game.strategies, shapley_set, g->num_nodes * sizeof(unsigned char));
        free(shapley_set);
        
        // Mark as converged for logic consistency
        // game.strategies is now populated.

    }
    else
    {
        // STRATEGIC GAME APPROACH
        printf("\n=== STRATEGIC GAME APPROACH ===\n");
        init_game(&game, g);

        if (algorithm == ALGO_BRD)
        {
            printf("Algorithm: Best Response Dynamics (BRD)\n");
        }
        else if (algorithm == ALGO_RM)
        {
            printf("Algorithm: Regret Matching (RM)\n");
            init_regret_system(&game);
        }
        else if (algorithm == ALGO_FP)
        {
            printf("Algorithm: Fictitious Play (FP)\n");
            init_fictitious_system(&game);
        }

        int result = run_simulation(&game, algorithm, max_it, 1);
        int converged = (result != -1);

        double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        printf("\nSimulation finished in %.2fs\n", elapsed);

        if (converged)
        {
            printf("Converged: YES\n");
        }
        else
        {
            printf("Converged: NO\n");
        }

        // ANALISI RISULTATI
        int minimal = is_minimal(&game);
        int valid = is_valid_cover(&game);

        long active_count = 0;
        for (int i = 0; i < game.num_players; ++i)
        {
            if (game.strategies[i] == 1)
                active_count++;
        }

        printf("\n=== RESULTS ===\n");
        printf("Cover Size: %ld / %d (%.2f%%)\n", active_count, game.num_players, (double)active_count / game.num_players * 100.0);
        printf("Valid Cover: %s\n", valid ? "YES" : "NO");
        printf("Minimal Local: %s\n", minimal ? "YES" : "NO");

        if (algorithm == ALGO_RM)
        {
            free_regret_system(&game);
        }
        else if (algorithm == ALGO_FP)
        {
            free_fictitious_system(&game);
        }

    } // End of if/else (Shapley vs Game)

    // === COMMON POST-PROCESSING (Parts 3 & 4) ===
    // At this point, game.strategies (and game.g) should be populated with the result.
    
    // Run Matching Market (Part 3)
    if (capacity_mode == 0 || capacity_mode == 2) {
        run_part3_matching_market(g, game.strategies, 0); // Infinite
    }
    if (capacity_mode == 1 || capacity_mode == 2) {
        run_part3_matching_market(g, game.strategies, 1); // Limited
    }

    // Run VCG Auction (Part 4)
    run_part4_vcg_auction(g, game.strategies);

    // Final Cleanup
    // Final Cleanup
    LOG_CLOSE();
    free_game(&game);
    free_graph(g);

    return 0;
}