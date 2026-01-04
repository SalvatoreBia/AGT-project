// src/min_cost_flow.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include "../include/min_cost_flow.h"

#define INF_COST 1e9
#define INF_CAP  1000000

// --- internal flow network structures ---

typedef struct {
    int to;
    int rev;      // index of the reverse edge in the "to" node's list
    int cap;      // residual capacity
    double cost;  // cost per unit flow
} flow_edge;

typedef struct {
    flow_edge *edges;
    int count;
    int capacity;
} adj_list;

typedef struct {
    int num_nodes;
    adj_list *adj;
} flow_network;

typedef struct {
    int price;
    int quality;
    int capacity;
} vendor_t;

// --- Helper Functions for Flow Network ---

static flow_network* create_flow_network(int n) {
    flow_network *fn = malloc(sizeof(flow_network));
    fn->num_nodes = n;
    fn->adj = malloc(n * sizeof(adj_list));
    for(int i=0; i<n; i++) {
        fn->adj[i].edges = NULL;
        fn->adj[i].count = 0;
        fn->adj[i].capacity = 0;
    }
    return fn;
}

static void add_flow_edge(flow_network *fn, int u, int v, int cap, double cost) {
    // Ensure capacity for forward edge
    if (fn->adj[u].count == fn->adj[u].capacity) {
        fn->adj[u].capacity = (fn->adj[u].capacity == 0) ? 4 : fn->adj[u].capacity * 2;
        fn->adj[u].edges = realloc(fn->adj[u].edges, fn->adj[u].capacity * sizeof(flow_edge));
    }
    // Ensure capacity for backward edge
    if (fn->adj[v].count == fn->adj[v].capacity) {
        fn->adj[v].capacity = (fn->adj[v].capacity == 0) ? 4 : fn->adj[v].capacity * 2;
        fn->adj[v].edges = realloc(fn->adj[v].edges, fn->adj[v].capacity * sizeof(flow_edge));
    }

    // Add forward edge
    flow_edge a = {v, fn->adj[v].count, cap, cost};
    fn->adj[u].edges[fn->adj[u].count] = a;
    
    // Add backward edge (0 capacity, negative cost)
    flow_edge b = {u, fn->adj[u].count, 0, -cost};
    fn->adj[v].edges[fn->adj[v].count] = b;

    fn->adj[u].count++;
    fn->adj[v].count++;
}

static void free_flow_network(flow_network *fn) {
    for(int i=0; i<fn->num_nodes; i++) {
        if(fn->adj[i].edges) free(fn->adj[i].edges);
    }
    free(fn->adj);
    free(fn);
}

// --- SPFA Algorithm (Shortest Path Faster Algorithm) ---
// Returns 1 if a path exists from s to t with residual capacity, 0 otherwise.
// Populates 'dist', 'parent_node', and 'parent_edge' arrays.
static int spfa(flow_network *fn, int s, int t, double *dist, int *p_node, int *p_edge) {
    int n = fn->num_nodes;
    int *in_queue = calloc(n, sizeof(int));
    int *queue = malloc((n + 5) * sizeof(int)); // Circular queue
    int q_head = 0, q_tail = 0;

    for(int i=0; i<n; i++) {
        dist[i] = INF_COST;
        p_node[i] = -1;
        p_edge[i] = -1;
    }

    dist[s] = 0;
    queue[q_tail++] = s;
    in_queue[s] = 1;

    while(q_head != q_tail) {
        int u = queue[q_head];
        q_head = (q_head + 1) % (n + 5);
        in_queue[u] = 0;

        for(int i=0; i<fn->adj[u].count; i++) {
            flow_edge *e = &fn->adj[u].edges[i];
            
            // If capacity available and we found a cheaper path
            if (e->cap > 0 && dist[e->to] > dist[u] + e->cost + 1e-9) {
                dist[e->to] = dist[u] + e->cost;
                p_node[e->to] = u;
                p_edge[e->to] = i;

                if (!in_queue[e->to]) {
                    queue[q_tail] = e->to;
                    q_tail = (q_tail + 1) % (n + 5);
                    in_queue[e->to] = 1;
                }
            }
        }
    }

    free(in_queue);
    free(queue);

    return (dist[t] < INF_COST / 2); // Return true if reachable
}

// --- Min Cost Max Flow Solver ---
static double min_cost_max_flow(flow_network *fn, int s, int t, int *flow_out) {
    double total_cost = 0;
    int total_flow = 0;
    double *dist = malloc(fn->num_nodes * sizeof(double));
    int *p_node = malloc(fn->num_nodes * sizeof(int));
    int *p_edge = malloc(fn->num_nodes * sizeof(int));

    // Successive shortest path using SPFA
    while(spfa(fn, s, t, dist, p_node, p_edge)) {
        // Find bottleneck capacity along the path
        int push = INF_CAP;
        int curr = t;
        while(curr != s) {
            int prev = p_node[curr];
            int idx = p_edge[curr];
            if (fn->adj[prev].edges[idx].cap < push) {
                push = fn->adj[prev].edges[idx].cap;
            }
            curr = prev;
        }

        // Apply flow
        curr = t;
        while(curr != s) {
            int prev = p_node[curr];
            int idx = p_edge[curr];
            int rev_idx = fn->adj[prev].edges[idx].rev;

            fn->adj[prev].edges[idx].cap -= push;
            fn->adj[curr].edges[rev_idx].cap += push;
            
            total_cost += push * fn->adj[prev].edges[idx].cost;
            curr = prev;
        }
        total_flow += push;
    }

    free(dist);
    free(p_node);
    free(p_edge);
    
    if (flow_out) *flow_out = total_flow;
    return total_cost;
}


// Helper function to verify matching constraints
static void verify_matching_constraints(flow_network *fn, int *budgets, 
                                        vendor_t *vendors, 
                                        int num_buyers, int num_vendors) 
{
    printf("\n--- VERIFYING CONSTRAINTS ---\n");
    int all_passed = 1;
    int *vendor_sales = calloc(num_vendors, sizeof(int));

    // 1. Check Budget Constraints for every match
    for (int i = 0; i < num_buyers; i++) {
        int u = i + 1; // Buyer Node ID in flow network
        
        // Check outgoing edges from Buyer
        for (int k = 0; k < fn->adj[u].count; k++) {
            flow_edge e = fn->adj[u].edges[k];
            
            // Identify if this edge goes to a Vendor
            // Vendor Node IDs are: [num_buyers + 1] to [num_buyers + num_vendors]
            if (e.to > num_buyers && e.to <= num_buyers + num_vendors) {
                
                // In Min-Cost Flow with capacity 1:
                // residual_cap == 0 means flow was pushed (Matched)
                // residual_cap == 1 means no flow (Unmatched)
                if (e.cap == 0) { 
                    int v_idx = e.to - num_buyers - 1;
                    
                    // CHECK A: Budget >= Price
                    if (budgets[i] < vendors[v_idx].price) {
                        printf("[FAIL] Budget Violation! Buyer %d (Budget: %d) matched with Vendor %d (Price: %d)\n",
                               u, budgets[i], v_idx, vendors[v_idx].price);
                        all_passed = 0;
                    }
                    
                    vendor_sales[v_idx]++;
                }
            }
        }
    }

    // 2. Check Capacity Constraints for every vendor
    for (int j = 0; j < num_vendors; j++) {
        // CHECK B: Sales <= Capacity
        if (vendor_sales[j] > vendors[j].capacity) {
            printf("[FAIL] Capacity Violation! Vendor %d sold %d items (Capacity: %d)\n",
                   j, vendor_sales[j], vendors[j].capacity);
            all_passed = 0;
        }
    }

    if (all_passed) {
        printf("[SUCCESS] All constraints (Budget >= Price, Capacity Limits) are satisfied.\n");
    } else {
        printf("[WARNING] Some constraints were violated. Check graph construction.\n");
    }

    free(vendor_sales);
    printf("-----------------------------\n");
}

// --- Part 3 Wrapper Implementation ---

void run_part3_matching_market(graph *g, unsigned char *security_set, int limited_capacity) {
    printf("\n=== PART 3: RESOURCE ALLOCATION (Min-Cost Flow) ===\n");
    printf("Mode: %s Capacity\n", limited_capacity ? "Limited" : "Infinite");

    // 1. Identify Buyers (Players in Security Set)
    int *buyers = malloc(g->num_nodes * sizeof(int));
    int num_buyers = 0;
    for(uint64_t i=0; i<g->num_nodes; i++) {
        if(security_set[i]) {
            buyers[num_buyers++] = i;
        }
    }
    
    if (num_buyers == 0) {
        printf("No buyers in security set. Skipping.\n");
        free(buyers);
        return;
    }

    // 2. Generate Random Budgets for Buyers
    int *budgets = malloc(num_buyers * sizeof(int));
    for(int i=0; i<num_buyers; i++) {
        budgets[i] = (rand() % 100) + 1; // 1 to 100
    }

    // 3. Generate Random Vendors
    // Let's assume number of vendors is roughly 1/2 of number of buyers for interesting competition
    int num_vendors = (num_buyers / 2) + 1; 
    vendor_t *vendors = malloc(num_vendors * sizeof(vendor_t));

    for(int i=0; i<num_vendors; i++) {
        vendors[i].price = (rand() % 100) + 1;    // Price 1-100
        vendors[i].quality = (rand() % 10) + 1;   // Quality 1-10
        if (limited_capacity) {
            vendors[i].capacity = (rand() % 5) + 1; // 1 to 5 items
        } else {
            vendors[i].capacity = num_buyers;     // effectively infinite
        }
    }

    // 4. Build Flow Network
    // Nodes: 0 (Source), 1..B (Buyers), B+1..B+V (Vendors), B+V+1 (Sink)
    int s = 0;
    int t = num_buyers + num_vendors + 1;
    int num_nodes_flow = t + 1;
    
    flow_network *fn = create_flow_network(num_nodes_flow);

    // Edges Source -> Buyers
    for(int i=0; i<num_buyers; i++) {
        add_flow_edge(fn, s, i + 1, 1, 0.0);
    }

    // Edges Buyers -> Vendors
    // Only if Budget >= Price. Cost = -Utility.
    // Utility = (Budget - Price) + (Quality * 10)
    for(int i=0; i<num_buyers; i++) {
        for(int j=0; j<num_vendors; j++) {
            if (budgets[i] >= vendors[j].price) {
                double utility = (double)(budgets[i] - vendors[j].price) + (vendors[j].quality * 10.0);
                // We minimize cost, so cost = -utility
                add_flow_edge(fn, i + 1, num_buyers + j + 1, 1, -utility);
            }
        }
    }

    // Edges Vendors -> Sink
    for(int j=0; j<num_vendors; j++) {
        add_flow_edge(fn, num_buyers + j + 1, t, vendors[j].capacity, 0.0);
    }

    // 5. Solve
    int total_flow = 0;
    double min_cost = min_cost_max_flow(fn, s, t, &total_flow);
    double max_welfare = -min_cost;

    // 6. Print Results
    printf("Matching Calculation Complete.\n");
    printf("Total Matched: %d / %d buyers\n", total_flow, num_buyers);
    printf("Total Social Welfare: %.2f\n", max_welfare);

    // Optional: Reconstruct matching to see who bought what
    // Iterate over Buyer nodes
    /*
    for(int i=0; i<num_buyers; i++) {
        int u = i + 1;
        for(int k=0; k<fn->adj[u].count; k++) {
            flow_edge e = fn->adj[u].edges[k];
            // If edge goes to a vendor (range check) and has 0 residual cap (meaning flow passed)
            if (e.to > num_buyers && e.to <= num_buyers + num_vendors && e.cap == 0) {
                int v_idx = e.to - num_buyers - 1;
                printf("  Buyer %d (Bud: %d) -> Vendor %d (Price: %d, Q: %d)\n", 
                       buyers[i], budgets[i], v_idx, vendors[v_idx].price, vendors[v_idx].quality);
            }
        }
    }
    */
    verify_matching_constraints(fn, budgets, vendors, num_buyers, num_vendors);
    // Cleanup
    free_flow_network(fn);
    free(buyers);
    free(budgets);
    free(vendors);
}