#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include "../include/auction.h"

#define INF_DIST 1e18
#define PENALTY_COST 200.0  // Cost added if traversing an unsecure node

// Struct to hold path results
typedef struct {
    int *nodes;
    int length;
    double total_cost;     // Sum of (Bids + Penalties)
    double bids_only_cost; // Sum of (Bids) only
} path_res_t;

// --- Priority Queue for Dijkstra ---
typedef struct {
    int u;
    double dist;
} pq_node;

// Min-Heapify Helper
static void swap_pq(pq_node *a, pq_node *b) { pq_node t = *a; *a = *b; *b = t; }

static void min_heapify(pq_node *pq, int size, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    if (left < size && pq[left].dist < pq[smallest].dist) smallest = left;
    if (right < size && pq[right].dist < pq[smallest].dist) smallest = right;
    if (smallest != i) {
        swap_pq(&pq[i], &pq[smallest]);
        min_heapify(pq, size, smallest);
    }
}

// --- Dijkstra's Algorithm ---
// Finds shortest path from s to t.
// exclude_node: If >= 0, this node is treated as removed (for VCG calc).
static path_res_t find_shortest_path(graph *g, int s, int t, int *bids, 
                                     unsigned char *sec_set, int exclude_node) 
{
    int n = g->num_nodes;
    double *dist = malloc(n * sizeof(double));
    int *parent = malloc(n * sizeof(int));
    int *visited = calloc(n, sizeof(int));
    
    // Priority Queue
    pq_node *pq = malloc(n * 4 * sizeof(pq_node));
    int pq_size = 0;

    for(int i=0; i<n; i++) { dist[i] = INF_DIST; parent[i] = -1; }

    // Init Source
    // Note: In node-weighted graphs, cost is incurred when entering/using the node.
    // We count the source node's cost.
    if (s != exclude_node) {
        double w_s = (double)bids[s] + (sec_set[s] ? 0.0 : PENALTY_COST);
        dist[s] = w_s;
        pq[pq_size++] = (pq_node){s, w_s};
    }

    while(pq_size > 0) {
        // Extract Min
        int u = pq[0].u;
        // double d = pq[0].dist;
        pq[0] = pq[--pq_size];
        min_heapify(pq, pq_size, 0);

        if (visited[u]) continue;
        visited[u] = 1;
        if (u == t) break; // Found target

        // Relax neighbors
        uint64_t start = g->row_ptr[u];
        uint64_t end = g->row_ptr[u + 1];
        for(uint64_t k=start; k<end; k++) {
            int v = g->col_ind[k];
            if (v == exclude_node) continue;

            double w_v = (double)bids[v] + (sec_set[v] ? 0.0 : PENALTY_COST);
            if (!visited[v] && dist[u] + w_v < dist[v]) {
                dist[v] = dist[u] + w_v;
                parent[v] = u;
                // Add to PQ
                int idx = pq_size++;
                pq[idx] = (pq_node){v, dist[v]};
                // Bubble up
                while(idx > 0 && pq[(idx-1)/2].dist > pq[idx].dist) {
                    swap_pq(&pq[(idx-1)/2], &pq[idx]);
                    idx = (idx-1)/2;
                }
            }
        }
    }

    // Reconstruct Path
    path_res_t res = {NULL, 0, dist[t], 0.0};
    
    if (dist[t] < INF_DIST) {
        // First pass: count length
        int curr = t;
        int len = 0;
        while(curr != -1) { len++; curr = parent[curr]; }
        
        res.length = len;
        res.nodes = malloc(len * sizeof(int));
        
        // Second pass: fill nodes
        curr = t;
        for(int i=len-1; i>=0; i--) {
            res.nodes[i] = curr;
            res.bids_only_cost += bids[curr];
            curr = parent[curr];
        }
    }

    free(dist); free(parent); free(visited); free(pq);
    return res;
}

// --- VCG Logic ---
void run_part4_vcg_auction(graph *g, unsigned char *security_set) {
    printf("\n=== PART 4: VCG AUCTION (Secure Path) ===\n");

    // 1. Setup Random Bids
    int *bids = malloc(g->num_nodes * sizeof(int));
    for(uint64_t i=0; i<g->num_nodes; i++) {
        bids[i] = (rand() % 100) + 1; // Price 1-100
    }

    // 2. Pick Source/Target
    int s = rand() % g->num_nodes;
    int t = rand() % g->num_nodes;
    while(s == t) t = rand() % g->num_nodes;

    printf("Request: Path %d -> %d\n", s, t);
    printf("Utility: Minimize (Sum of Bids) + %.0f per Unsecure Node\n", PENALTY_COST);

    // 3. Find Optimal Path (The Allocation)
    path_res_t opt = find_shortest_path(g, s, t, bids, security_set, -1);

    if (opt.length == 0) {
        printf("No path exists.\n");
        free(bids); return;
    }

    printf("Winning Path: [ ");
    for(int i=0; i<opt.length; i++) printf("%d ", opt.nodes[i]);
    printf("]\nTotal Weight: %.2f (Bids: %.0f)\n", opt.total_cost, opt.bids_only_cost);

    // 4. Calculate Payments
    printf("\n--- PAYMENTS ---\n");
    printf("Node\tBid\tStatus\tPayment\n");
    
    for(int i=0; i<opt.length; i++) {
        int u = opt.nodes[i];
        
        // Cost of optimal path excluding u's contribution
        double w_u = (double)bids[u] + (security_set[u] ? 0.0 : PENALTY_COST);
        double cost_others = opt.total_cost - w_u;

        // Cost of best path if u didn't exist
        path_res_t alt = find_shortest_path(g, s, t, bids, security_set, u);
        
        if (alt.length == 0) {
            printf("%d\t%d\t%s\tINF (Critical Node)\n", 
                   u, bids[u], security_set[u] ? "Sec" : "Unsec");
        } else {
            // VCG Payment = (Social Cost without u) - (Social Cost of others with u)
            double payment = alt.total_cost - cost_others;
            printf("%d\t%d\t%s\t%.2f\n", 
                   u, bids[u], security_set[u] ? "Sec" : "Unsec", payment);
            free(alt.nodes);
        }
    }

    free(opt.nodes);
    free(bids);
}