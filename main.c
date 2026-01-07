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

#define GRAPH_FILENAME "graph.txt"

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
    printf("  -i <iterations>  Maximum number of iterations (default: 10000)\n");
    printf("  -a <algorithm>   Algorithm to use (1=BRD, 2=RM, 3=FP, 4=Shapley, 5=FP_Async) (default: 3)\n");
    printf("  -v <version>     Characteristic function version for Shapley (1, 2, or 3) (default: 3)\n");
    printf("  -c <capacity>    Capacity Mode (0=Infinite, 1=Limited, 2=Both) (default: 0)\n");
    printf("  -f <file>        Load graph from file instead of generating one\n");
    printf("  -h               Show this help message\n");
}

int main(int argc, char *argv[])
{

    int num_nodes = 10000;
    int k_param = 4;
    int max_it = 10000;
    int algorithm = ALGO_FP;
    int graph_type = TYPE_REGULAR;
    int shapley_version = 3;
    int capacity_mode = 0;
    char *input_file = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "n:k:i:a:t:v:c:f:h")) != -1)
    {
        switch (opt)
        {
        case 'n':
            num_nodes = atoi(optarg);
            break;
        case 'k':
            k_param = atoi(optarg);
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
            max_it = atoi(optarg);
            break;
        case 'a':
            algorithm = atoi(optarg);
            if ((algorithm < ALGO_BRD || algorithm > ALGO_SHAPLEY) && algorithm != ALGO_FP_ASYNC)
            {
                fprintf(stderr, "Invalid algorithm selection. Use 1, 2, 3, 4, or 5.\n");
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
        case 'f':
            input_file = optarg;
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


    if (input_file != NULL)
    {
        printf("[INFO] Loading graph from file: %s\n", input_file);
        g = load_graph_from_text(input_file);
        if (!g)
        {
            fprintf(stderr, "Error: Failed to load graph from file '%s'.\n", input_file);
            return 1;
        }
        printf("[INFO] Loaded graph with %d nodes\n", g->num_nodes);
    }
    else
    {
        printf("[INFO] Generating graph type %d with %d nodes and param %d...\n", graph_type, num_nodes, k_param);

        if (graph_type == TYPE_REGULAR)
        {
            g = generate_random_regular(num_nodes, k_param);
        }
        else if (graph_type == TYPE_ERDOS)
        {
            double p = (double)k_param / (double)(num_nodes - 1);
            printf("[INFO] Erdos-Renyi: calculated p = %lf\n", p);
            g = generate_erdos_renyi(num_nodes, p);
        }
        else if (graph_type == TYPE_BARABASI)
        {
            printf("[INFO] Barabasi-Albert: m = %d\n", k_param);
            g = generate_barabasi_albert(num_nodes, k_param);
        }

        if (!g)
        {
            fprintf(stderr, "Error: Failed to generate graph.\n");
            return 1;
        }

        save_graph_to_text(g, GRAPH_FILENAME);
    }


    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "log_n%d_k%d_t%d_a%d_c%d.log", 
             num_nodes, k_param, graph_type, algorithm, capacity_mode);
    LOG_INIT(log_filename);

    clock_t start_time = clock();

    if (algorithm == ALGO_SHAPLEY)
    {

        printf("\n=== COALITIONAL GAME APPROACH ===\n");
        printf("Algorithm: Shapley Values (Monte Carlo)\n");
        printf("Characteristic function version: %d\n", shapley_version);
        printf("Monte Carlo iterations: %d\n\n", max_it);


        double *shapley_values = calculate_shapley_values(g, (int)max_it, shapley_version);


        unsigned char *shapley_set = build_security_set_from_shapley(g, shapley_values);

        double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        printf("\n[OK] Shapley computation finished in %.2fs\n", elapsed);


        int active_count = 0;
        for (int i = 0; i < g->num_nodes; ++i)
        {
            if (shapley_set[i])
                active_count++;
        }


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


        printf("\n--- Top 10 Nodes by Shapley Value ---\n");
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

        init_game(&game, g);

        memcpy(game.strategies, shapley_set, g->num_nodes * sizeof(unsigned char));
        free(shapley_set);
        


    }
    else
    {

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
        else if (algorithm == ALGO_FP_ASYNC)
        {
            printf("Algorithm: Async Fictitious Play (FP_Async)\n");
            init_fictitious_system(&game);
        }

        int result = run_simulation(&game, algorithm, max_it, 1);
        int converged = (result != -1);

        double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        printf("\n[OK] Simulation finished in %.2fs\n", elapsed);

        if (converged)
        {
            printf("[OK] Converged: YES\n");
        }
        else
        {
            printf("[WARN] Converged: NO\n");
        }


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
        else if (algorithm == ALGO_FP || algorithm == ALGO_FP_ASYNC)
        {
            free_fictitious_system(&game);
        }

    }


    if (capacity_mode == 0 || capacity_mode == 2) {
        run_part3_matching_market(g, game.strategies, 0);
    }
    if (capacity_mode == 1 || capacity_mode == 2) {
        run_part3_matching_market(g, game.strategies, 1);
    }


    run_part4_vcg_auction(g, game.strategies);


    LOG_CLOSE();
    free_game(&game);
    free_graph(g);

    return 0;
}