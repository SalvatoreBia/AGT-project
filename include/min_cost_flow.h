// include/min_cost_flow.h
#ifndef MIN_COST_FLOW_H
#define MIN_COST_FLOW_H

#include <stdint.h>
#include "data_structures.h" // Reuse your existing graph struct definition if needed

// Structure to represent a matching result
typedef struct {
    uint64_t buyer_id;
    uint64_t vendor_id;
    double utility;
} match_t;

// Main function to solve Part 3
void run_part3_matching_market(graph *g, unsigned char *security_set, int limited_capacity);

#endif