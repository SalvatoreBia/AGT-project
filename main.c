#include <stdio.h>
#include <stdlib.h>
#include "include/algorithm.h"
#include "include/data_structures.h"
#include <time.h>
#include <inttypes.h>

// --- CONFIGURAZIONE ALGORITMO ---
// Definisci gli ID degli algoritmi
#define ALGO_BRD 1  // Best Response Dynamics
#define ALGO_RM  2  // Regret Matching
#define ALGO_FP  3  // Fictitious Play (Predisposto)

// SELEZIONA QUI L'ALGORITMO DA USARE
#define CURRENT_ALGORITHM ALGO_RM

// Define filename constant
#define GRAPH_FILENAME "graph_dump.bin"
// Max iterations safety net (se non definito altrove)
#ifndef MAX_IT
#define MAX_IT 1000
#endif

int main(void)
{
    game_system game;
    graph *g = NULL;

    // --- 1. GRAPH LOADING / GENERATION (Invariato) ---
    printf("Checking for existing graph file '%s'...\n", GRAPH_FILENAME);
    // g = load_graph_from_file(GRAPH_FILENAME); // Decommentare se hai la funzione load

    if (g != NULL) 
    {
        printf("Graph loaded successfully!\n");
    }
    else 
    {
        printf("File not found or load disabled. Generating new random regular graph...\n");
        
        // 1 milione di nodi, grado 6 (esempio)
        g = generate_random_regular(100, 6);

        if (g == NULL) 
        {
            fprintf(stderr, "Error: Failed to generate graph.\n");
            return 1;
        }
        printf("Graph generated.\n");
        save_graph_to_file(g, GRAPH_FILENAME);
    }

    // --- 2. GAME INITIALIZATION ---
    clock_t start_time = clock();
    
    // Inizializzazione base (comune a tutti)
    init_game(&game, g);

    // Inizializzazioni specifiche per Algoritmo
#if CURRENT_ALGORITHM == ALGO_BRD
    printf("Algoritmo selezionato: Best Response Dynamics (BRD)\n");
    // BRD non richiede allocazioni extra oltre a init_game

#elif CURRENT_ALGORITHM == ALGO_RM
    printf("Algoritmo selezionato: Regret Matching (RM)\n");
    // RM richiede l'array dei regret cumulativi (inizializzato a 1.0)
    init_cumulative_regrets(&game);

#elif CURRENT_ALGORITHM == ALGO_FP
    printf("Algoritmo selezionato: Fictitious Play (FP)\n");
    // FP richiederà la storia delle frequenze
    // init_fictitious_play(&game); 
#endif

    printf("Stato iniziale (Random) stabilito.\n");

    // --- 3. MAIN LOOP ---
    uint64_t converged = 0;
    
    // Resettiamo il contatore iterazioni se necessario
    game.iteration = 0;

    while (game.iteration < MAX_IT)
    {
        // Stampa progresso ogni 10 iterazioni
        if (game.iteration % 10 == 0) {
            printf("--- Iterazione %lu ---\n", game.iteration + 1);
        }

#if CURRENT_ALGORITHM == ALGO_BRD
        // --- LOGICA BRD ---
        // Ritorna 1 se qualcuno ha cambiato strategia, 0 se Nash Equilibrium
        uint64_t changed = run_best_response_iteration(&game);
        
        if (!changed)
        {
            converged = 1;
            printf("Equilibrio di Nash puro raggiunto alla it %lu\n", game.iteration);
            break; // Stop immediato per BRD
        }

#elif CURRENT_ALGORITHM == ALGO_RM
        // --- LOGICA RM ---
        // RM non ha un "stop" deterministico come BRD, converge statisticamente.
        // Eseguiamo l'iterazione e aggiorniamo i regret.
        run_regret_matching_iteration(&game);
        
        // Nota: In RM solitamente si va avanti fino a MAX_IT, 
        // oppure si controlla la "epsilon-convegenza" (più costoso da calcolare)

#elif CURRENT_ALGORITHM == ALGO_FP
        // run_fictitious_play_iteration(&game);
#endif

        // Incremento gestito internamente alle funzioni o qui se serve
        // Assumiamo che le funzioni run_ incrementino game.iteration o lo facciamo qui se non lo fanno
        if (game.iteration >= MAX_IT) break;
    }

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nSimulazione terminata.\n");
    printf("Elapsed time: %.2f seconds\n", elapsed_time);

    // --- 4. RESULTS ANALYSIS ---
    
    // Verifica minimalità (Valida per tutti, ma per RM è uno snapshot istantaneo)
    int minimal = is_minimal(&game);
    printf("Is solution minimal? %s\n", minimal ? "YES" : "NO");

    // Conteggio nodi attivi (Vertex Cover Size)
    long count = 0;
    for (uint64_t i = 0; i < game.num_players; ++i)
    {
       if (game.strategies[i] == 1) count++;
    }
    printf("Active Nodes (Cover Size): %lu\n", count);


    // --- 5. CLEANUP ---
    
#if CURRENT_ALGORITHM == ALGO_RM
    free_cumulative_regrets(&game);
#elif CURRENT_ALGORITHM == ALGO_FP
    // free_fictitious_play(&game);
#endif

    // Pulizia base
    free_game(&game);
    free_graph(g);

    return 0;
}