// src/auction.c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include "../include/auction.h"
#include "../include/logging.h"

#define INF_DIST 1e14
#define PENALTY_COST 200.0 // Buyer disutility for traversing an unsecure node

// --- Data Structures for Pathfinding ---

typedef struct {
    int *nodes;      // Array of node IDs in the path
    int length;      // Number of nodes
    double cost;     // Total Social Cost (Bids + Penalties)
} path_t;

typedef struct {
    int id;
    double dist;
} pq_node;

typedef struct {
    pq_node *data;
    int size;
    int capacity;
} min_heap;

// --- Min Heap Implementation (for Dijkstra) ---

static min_heap* create_heap(int capacity) {
    min_heap *h = malloc(sizeof(min_heap));
    h->data = malloc(capacity * sizeof(pq_node));
    h->size = 0;
    h->capacity = capacity;
    return h;
}

static void free_heap(min_heap *h) {
    free(h->data);
    free(h);
}

static void heap_push(min_heap *h, int id, double dist) {
    if (h->size == h->capacity) {
        fprintf(stderr, "Warning: Heap capacity reached, node %d not added\n", id);
        return;
    }
    int i = h->size++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p].dist <= dist) break;
        h->data[i] = h->data[p];
        i = p;
    }
    h->data[i].id = id;
    h->data[i].dist = dist;
}

static pq_node heap_pop(min_heap *h) {
    pq_node top = h->data[0];
    pq_node last = h->data[--h->size];
    int i = 0;
    while (i * 2 + 1 < h->size) {
        int child = i * 2 + 1;
        if (child + 1 < h->size && h->data[child + 1].dist < h->data[child].dist) {
            child++;
        }
        if (last.dist <= h->data[child].dist) break;
        h->data[i] = h->data[child];
        i = child;
    }
    h->data[i] = last;
    return top;
}

// --- Pathfinding ---

// Calculates the weight of a node: Bid + (Penalty if unsecure)
// Fixed: Removed unused 'node_id' parameter
static double get_node_weight(int bid, unsigned char is_secure) {
    return (double)bid + (is_secure ? 0.0 : PENALTY_COST);
}

// Dijkstra's Algorithm
// exclude_node: ID of a node to treat as "removed" (for VCG calculations). Pass -1 to include all.
static path_t get_shortest_path(graph *g, int s, int t, int *bids,
                                unsigned char *sec_set, int exclude_node)
{
    int n = g->num_nodes;
    double *dist = malloc(n * sizeof(double));
    int *parent = malloc(n * sizeof(int));
    int *visited = calloc(n, sizeof(int));
    
    if (!dist || !parent || !visited) {
        fprintf(stderr, "Error: Memory allocation failed in get_shortest_path\n");
        free(dist); free(parent); free(visited);
        path_t empty = {NULL, 0, INF_DIST};
        return empty;
    }

    for(int i=0; i<n; i++) { dist[i] = INF_DIST; parent[i] = -1; }

    min_heap *pq = create_heap(n * 2); // safety buffer

    // Initialization: Cost is incurred upon ENTERING/USING a node.
    // We assume the source node's cost is part of the path cost.
    if (s != exclude_node) {
        dist[s] = get_node_weight(bids[s], sec_set[s]);
        heap_push(pq, s, dist[s]);
    }

    while(pq->size > 0) {
        pq_node curr = heap_pop(pq);
        int u = curr.id;

        if (visited[u]) continue;
        visited[u] = 1;
        if (u == t) break; // Found target

        // Relax Neighbors
        int start = g->row_ptr[u];
        int end = g->row_ptr[u + 1];

        for(int k=start; k<end; k++) {
            int v = g->col_ind[k];

            if (v == exclude_node) continue; // Effectively remove node from graph

            double weight_v = get_node_weight(bids[v], sec_set[v]);
            if (dist[u] + weight_v < dist[v]) {
                dist[v] = dist[u] + weight_v;
                parent[v] = u;
                heap_push(pq, v, dist[v]);
            }
        }
    }

    // Reconstruct Path
    path_t res;
    res.cost = dist[t];
    res.length = 0;
    res.nodes = NULL;

    if (dist[t] < INF_DIST) {
        // First pass: count nodes
        int count = 0;
        int curr = t;
        while (curr != -1) { count++; curr = parent[curr]; }

        // Second pass: fill array
        res.length = count;
        res.nodes = malloc(count * sizeof(int));
        curr = t;
        for (int i = count - 1; i >= 0; i--) {
            res.nodes[i] = curr;
            curr = parent[curr];
        }
    }

    free(dist);
    free(parent);
    free(visited);
    free_heap(pq);
    return res;
}

// --- Verification Logic ---

static void verify_vcg_truthfulness(graph *g, int s, int t, int *bids,
                                    unsigned char *sec_set, int winner_id,
                                    double winner_payment)
{
    printf("\n    [Verification] Testing Dominant Strategy for Node %d...\n", winner_id);

    int true_cost = bids[winner_id]; // Assume current bid is the "True Cost"
    double current_utility = winner_payment - true_cost;

    // Test Scenarios: Undercutting and Overcharging
    int fake_bids[] = { true_cost - 20, true_cost - 1, true_cost + 1, true_cost + 50 };
    int num_tests = 4;

    for(int i=0; i<num_tests; i++) {
        int fake_bid = fake_bids[i];
        if (fake_bid <= 0) continue;

        // 1. Temporarily Apply Lie
        bids[winner_id] = fake_bid;

        // 2. Re-calculate Allocation (Winner Determination)
        path_t new_path = get_shortest_path(g, s, t, bids, sec_set, -1);

        // 3. Check if still winning
        int still_winning = 0;
        if (new_path.length > 0) {
            for(int k=0; k<new_path.length; k++) {
                if (new_path.nodes[k] == winner_id) {
                    still_winning = 1;
                    break;
                }
            }
        }

        double new_utility = 0.0;

        if (still_winning) {
            // Recalculate Payment: (Best Path without Winner) - (Chosen Path excluding Winner's Bid)
            // Note: The "Best Path without Winner" doesn't change because Winner isn't in it.
            // The "Chosen Path" weight changes because the bid changed.

            path_t alt_path = get_shortest_path(g, s, t, bids, sec_set, winner_id);

            if (alt_path.cost >= INF_DIST) {
                // Monopoly case (Bridge): utility unchanged, lying doesn't help
                new_utility = current_utility;
            } else {
                double w_winner = get_node_weight(fake_bid, sec_set[winner_id]);
                double cost_others = new_path.cost - w_winner;
                double new_payment = alt_path.cost - cost_others;

                // Utility = Payment - TRUE COST (not the fake bid)
                new_utility = new_payment - true_cost;
            }
            if(alt_path.nodes) free(alt_path.nodes);
        } else {
            // Lost the auction: Utility = 0
            new_utility = 0.0;
        }

        // 4. Output Result
        int profitable = (new_utility > current_utility + 1e-5);
        printf("      -> Lie: %3d | Win: %s | Utility: %6.2f | %s\n",
               fake_bid, still_winning?"Y":"N", new_utility,
               profitable ? "FAIL (Profitable Lie)" : "PASS (Not Better)");

        if(new_path.nodes) free(new_path.nodes);

        // Restore Bid
        bids[winner_id] = true_cost;
    }
}

// --- Main VCG Runner ---

void run_part4_vcg_auction(graph *g, unsigned char *sec_set) {
    printf("\n=== PART 4: VCG AUCTION MECHANISM ===\n");
    printf("Objective: Minimize Social Cost (Bids + Disutility of Unsecure Nodes)\n");
    printf("Disutility Penalty: %.0f\n", PENALTY_COST);

    if (g->num_nodes < 2) {
        printf("Graph too small for routing.\n");
        return;
    }

    // 1. Setup Random Bids (Private Valuations)
    int *bids = malloc(g->num_nodes * sizeof(int));
    if (!bids) {
        fprintf(stderr, "Error: Memory allocation failed for bids\n");
        return;
    }
    for(int i=0; i<(int)g->num_nodes; i++) {
        bids[i] = (rand() % 90) + 10; // Costs between 10 and 100
    }

    // 2. Select Buyers (Source and Target)
    int s = 0;
    int t = 0;
    while (s == t) {
        s = rand() % g->num_nodes;
        t = rand() % g->num_nodes;
    }
    printf("Auction Request: Path from Node %d to %d\n", s, t);

    // 3. Winner Determination (Optimal Allocation)
    path_t optimal = get_shortest_path(g, s, t, bids, sec_set, -1);

    if (optimal.length == 0 || optimal.cost >= INF_DIST) {
        printf("No path exists between %d and %d. Auction cancelled.\n", s, t);
        free(bids);
        return;
    }

    printf("Winning Path: [ ");
    for(int i=0; i<optimal.length; i++) printf("%d ", optimal.nodes[i]);
    printf("]\nTotal Social Cost: %.2f\n", optimal.cost);

    LOG_P4_START(s, t);

    // 4. Payment Calculation (Vickrey-Clarke-Groves)

    printf("\n--- VCG PAYMENTS ---\n");
    printf("| Node | Type  | Bid | External Cost | Payment | Utility |\n");
    printf("|------|-------|-----|---------------|---------|---------|\n");

    // Variables to store data for verification AFTER the table is done
    int verify_node = -1;
    double verify_payment = 0.0;

    for(int i=0; i<optimal.length; i++) {
        int u = optimal.nodes[i];

        // A. Cost of Others in current solution
        double w_u = get_node_weight(bids[u], sec_set[u]);
        double cost_others = optimal.cost - w_u;

        // B. Social Cost if u did not exist
        path_t alt = get_shortest_path(g, s, t, bids, sec_set, u);

        if (alt.cost >= INF_DIST) {
            // Bridge Node Case
            printf("| %4d | %s   | %3d |      INF      |   INF   |   INF   | (Monopoly/Bridge)\n",
                   u, sec_set[u]?"SEC":"UNS", bids[u]);
        } else {
            // Standard VCG Payment
            double payment = alt.cost - cost_others;
            double utility = payment - bids[u];

            printf("| %4d | %s   | %3d | %13.2f | %7.2f | %7.2f |\n",
                   u, sec_set[u]?"SEC":"UNS", bids[u], alt.cost, payment, utility);

            LOG_P4_PAY(u, bids[u], payment);

            // Store the first valid node for verification, but DO NOT run it yet
            if (verify_node == -1) {
                verify_node = u;
                verify_payment = payment;
            }
        }

        if(alt.nodes) free(alt.nodes);
    }
    printf("----------------------------------------------------------\n");

    // 5. Verification (Run outside the loop so table isn't broken)
    if (verify_node != -1) {
        verify_vcg_truthfulness(g, s, t, bids, sec_set, verify_node, verify_payment);
    }

    // Cleanup
    if(optimal.nodes) free(optimal.nodes);
    free(bids);
    LOG_STEP_END();
    printf("\n--- Auction Complete ---\n");
}
