
#ifndef MIN_COST_FLOW_H
#define MIN_COST_FLOW_H

#include <stdint.h>
#include "data_structures.h"


typedef struct {
    int buyer_id;
    int vendor_id;
    double utility;
} match_t;


void run_part3_matching_market(graph *g, unsigned char *security_set, int limited_capacity);

#endif