#ifndef AUCTION_H
#define AUCTION_H

#include "data_structures.h"

// Runs the VCG Auction mechanism
// g: The graph
// security_set: The binary array (1=Secure, 0=Unsecure)
void run_part4_vcg_auction(graph *g, unsigned char *security_set);

#endif