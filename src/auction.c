
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include "../include/auction.h"
#include "../include/data_structures.h"
#include "../include/logging.h"

#define INF_DIST 1e14
#define PENALTY_COST 200.0


static double get_node_weight(int bid, unsigned char is_secure) {
    return (double)bid + (is_secure ? 0.0 : PENALTY_COST);
}

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

    min_heap *pq = create_heap(n * 2);

    if (s != exclude_node) {
        dist[s] = get_node_weight(bids[s], sec_set[s]);
        heap_push(pq, s, dist[s]);
    }

    while(pq->size > 0) {
        pq_node curr = heap_pop(pq);
        int u = curr.id;

        if (visited[u]) continue;
        visited[u] = 1;
        if (u == t) break;

        int start = g->row_ptr[u];
        int end = g->row_ptr[u + 1];

        for(int k=start; k<end; k++) {
            int v = g->col_ind[k];

            if (v == exclude_node) continue;

            double weight_v = get_node_weight(bids[v], sec_set[v]);
            if (dist[u] + weight_v < dist[v]) {
                dist[v] = dist[u] + weight_v;
                parent[v] = u;
                heap_push(pq, v, dist[v]);
            }
        }
    }

    path_t res;
    res.cost = dist[t];
    res.length = 0;
    res.nodes = NULL;

    if (dist[t] < INF_DIST) {
        int count = 0;
        int curr = t;
        while (curr != -1) { count++; curr = parent[curr]; }
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

static void verify_vcg_truthfulness(graph *g, int s, int t, int *bids,
                                    unsigned char *sec_set, int winner_id,
                                    double winner_payment)
{
    printf("\n    [INFO] Testing Dominant Strategy for Node %d...\n", winner_id);

    int true_cost = bids[winner_id];
    double current_utility = winner_payment - true_cost;

    int fake_bids[] = { true_cost - 20, true_cost - 1, true_cost + 1, true_cost + 50 };
    int num_tests = 4;

    for(int i=0; i<num_tests; i++) {
        int fake_bid = fake_bids[i];
        if (fake_bid <= 0) continue;

        bids[winner_id] = fake_bid;

        path_t new_path = get_shortest_path(g, s, t, bids, sec_set, -1);

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
            path_t alt_path = get_shortest_path(g, s, t, bids, sec_set, winner_id);

            if (alt_path.cost >= INF_DIST) {
                new_utility = current_utility;
            } else {
                double w_winner = get_node_weight(fake_bid, sec_set[winner_id]);
                double cost_others = new_path.cost - w_winner;
                double new_payment = alt_path.cost - cost_others;

                new_utility = new_payment - true_cost;
            }
            if(alt_path.nodes) free(alt_path.nodes);
        } else {
            new_utility = 0.0;
        }

        int profitable = (new_utility > current_utility + 1e-5);
        printf("      -> Lie: %3d | Win: %s | Utility: %6.2f | %s\n",
               fake_bid, still_winning?"Y":"N", new_utility,
               profitable ? "[FAIL] Profitable Lie" : "[OK] Not Better");

        if(new_path.nodes) free(new_path.nodes);

        bids[winner_id] = true_cost;
    }
}

void run_part4_vcg_auction(graph *g, unsigned char *sec_set) {
    printf("\n=== PART 4: VCG AUCTION MECHANISM ===\n");
    printf("Objective: Minimize Social Cost (Bids + Disutility of Unsecure Nodes)\n");
    printf("Disutility Penalty: %.0f\n", PENALTY_COST);

    if (g->num_nodes < 2) {
        printf("[WARN] Graph too small for routing.\n");
        return;
    }

    int *bids = malloc(g->num_nodes * sizeof(int));
    if (!bids) {
        fprintf(stderr, "Error: Memory allocation failed for bids\n");
        return;
    }
    for(int i=0; i<(int)g->num_nodes; i++) {
        bids[i] = (rand() % 90) + 10;
    }

    int s = 0;
    int t = 0;
    while (s == t) {
        s = rand() % g->num_nodes;
        t = rand() % g->num_nodes;
    }
    printf("Auction Request: Path from Node %d to %d\n", s, t);

    path_t optimal = get_shortest_path(g, s, t, bids, sec_set, -1);

    if (optimal.length == 0 || optimal.cost >= INF_DIST) {
        printf("[WARN] No path exists between %d and %d. Auction cancelled.\n", s, t);
        free(bids);
        return;
    }

    printf("[INFO] Winning Path: [ ");
    for(int i=0; i<optimal.length; i++) printf("%d ", optimal.nodes[i]);
    printf("]\n[INFO] Total Social Cost: %.2f\n", optimal.cost);

    LOG_P4_START(s, t);

    printf("\n--- VCG PAYMENTS ---\n");
    printf("| Node | Type  | Bid | External Cost | Payment | Utility |\n");
    printf("|------|-------|-----|---------------|---------|---------|\n");


    for(int i=0; i<optimal.length; i++) {
        int u = optimal.nodes[i];

        double w_u = get_node_weight(bids[u], sec_set[u]);
        double cost_others = optimal.cost - w_u;

        path_t alt = get_shortest_path(g, s, t, bids, sec_set, u);

        if (alt.cost >= INF_DIST) {
            printf("| %4d | %s   | %3d |      INF      |   INF   |   INF   | (Monopoly/Bridge)\n",
                   u, sec_set[u]?"SEC":"UNS", bids[u]);
        } else {
            double payment = alt.cost - cost_others;
            double utility = payment - bids[u];

            printf("| %4d | %s   | %3d | %13.2f | %7.2f | %7.2f |\n",
                   u, sec_set[u]?"SEC":"UNS", bids[u], alt.cost, payment, utility);

            LOG_P4_PAY(u, bids[u], payment);
        }

        if(alt.nodes) free(alt.nodes);
    }
    printf("----------------------------------------------------------\n");

    printf("\n--- TRUTHFULNESS VERIFICATION (All Path Nodes) ---\n");
    for(int i=0; i<optimal.length; i++) {
        int u = optimal.nodes[i];
        
        double w_u = get_node_weight(bids[u], sec_set[u]);
        double cost_others = optimal.cost - w_u;
        path_t alt = get_shortest_path(g, s, t, bids, sec_set, u);
        
        if (alt.cost < INF_DIST) {
            double payment = alt.cost - cost_others;
            verify_vcg_truthfulness(g, s, t, bids, sec_set, u, payment);
        } else {
            printf("    [INFO] Node %d skipped (Monopoly/Bridge - no alternative path)\n", u);
        }
        if(alt.nodes) free(alt.nodes);
    }

    if(optimal.nodes) free(optimal.nodes);
    free(bids);
    LOG_STEP_END();
    printf("\n[OK] Auction Complete\n");
}
