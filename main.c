#include <stdio.h>
#include "include/algorithm.h"
#include "include/data_structures.h"
#include <time.h>
#include <inttypes.h>

// Define filename constant for easy changing
#define GRAPH_FILENAME "graph_dump.bin"

int main(void)
{
    game_system game;
    graph *g = NULL;

    // 1. Try to load the graph from file first
    printf("Checking for existing graph file '%s'...\n", GRAPH_FILENAME);
    g = NULL; // load_graph_from_file(GRAPH_FILENAME);

    if (g != NULL) 
    {
        printf("Graph loaded successfully!\n");
    }
    else 
    {
        // 2. If load failed (file doesn't exist or is corrupt), generate new one
        printf("File not found. Generating new random regular graph...\n");
        
        // Generating 10 Million nodes as per your original code
        g = generate_random_regular(1000000, 6);

        if (g == NULL) 
        {
            fprintf(stderr, "Error: Failed to generate graph.\n");
            return 1;
        }

        printf("Graph generated.\n");

        // 3. Save the newly generated graph for next time
        save_graph_to_file(g, GRAPH_FILENAME);
    }

    // --- GAME LOGIC STARTS HERE (Unchanged) ---

    clock_t start_time = clock();
    init_game(&game, g);

    printf("Stato iniziale (Random):\n");

    uint64_t converged = 0;
    while (!converged)
    {
        // Reduced print verbosity for 1000 iterations
        //if (game.iteration % 10 == 0) 
        printf("--- Iterazione %lu ---\n", game.iteration + 1);

        uint64_t changed = run_best_response_iteration(&game);
        if (!changed)
        {
            converged = 1;
            printf("Equilibrio di Nash raggiunto alla it %lu\n", game.iteration);
        }

        if (game.iteration > MAX_IT) break;
    }

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Elapsed time: %.2f seconds\n", elapsed_time);

    // --- CHECK MINIMALITY ---
    int minimal = is_minimal(&game);
    printf("Is solution minimal? %s\n", minimal ? "YES" : "NO");

    printf("Network Security Set:");
    long count = 0;
    for (uint64_t i = 0; i < game.num_players; ++i)
    {
       if (game.strategies[i] == 1) count++;
    }
    printf("%lu\n", count);

    
    free_game(&game);
    free_graph(g);

    return 0;
}