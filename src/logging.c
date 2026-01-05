#include "../include/logging.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef ENABLE_LOGGING

static FILE *log_file = NULL;
static int first_update = 1;

void log_init(const char *fmt, ...) {
    char filename[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(filename, sizeof(filename), fmt, args);
    va_end(args);

    log_file = fopen(filename, "w");
    if (!log_file) {
        perror("Failed to open log file");
    }
}

void log_step_begin(long iteration, const char *algo_name) {
    if (!log_file) return;
    fprintf(log_file, "{\"iteration\": %ld, \"algorithm\": \"%s\", \"updates\": [", iteration, algo_name);
    first_update = 1;
}

void log_node_update(long node_id, int old_strat, int new_strat, double utility_val) {
    if (!log_file) return;
    if (!first_update) {
        fprintf(log_file, ", ");
    }
    fprintf(log_file, "{\"id\": %ld, \"old\": %d, \"new\": %d, \"u\": %.4f}", node_id, old_strat, new_strat, utility_val);
    first_update = 0;
}

void log_msg(const char *msg) {
    // No-op for now unless we add a message field
    (void)msg;
}

void log_step_end() {
    if (!log_file) return;
    fprintf(log_file, "]}\n");
    fflush(log_file);
}

void log_close() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

// --- PART 3 ---

void log_part3_start(const char *mode) {
    if (!log_file) return;
    // We treat this as a new "step" or section.
    // JSONL: {"algorithm": "MATCHING", "mode": "...", "events": [
    fprintf(log_file, "{\"algorithm\": \"MATCHING\", \"mode\": \"%s\", \"events\": [", mode);
    first_update = 1;
}

void log_part3_iter(int iteration, int flow_added, double cost_added) {
    if (!log_file) return;
    if (!first_update) fprintf(log_file, ", ");
    fprintf(log_file, "{\"type\": \"iter\", \"it\": %d, \"flow\": %d, \"cost\": %.2f}", iteration, flow_added, cost_added);
    first_update = 0;
}

void log_part3_match(int buyer, int vendor, int budget, int price, double utility) {
    if (!log_file) return;
    if (!first_update) fprintf(log_file, ", ");
    fprintf(log_file, "{\"type\": \"match\", \"buyer\": %d, \"vendor\": %d, \"budget\": %d, \"price\": %d, \"u\": %.2f}", 
            buyer, vendor, budget, price, utility);
    first_update = 0;
}

// --- PART 4 ---

void log_part4_start(int s, int t) {
    if (!log_file) return;
    fprintf(log_file, "{\"algorithm\": \"VCG\", \"request\": {\"s\": %d, \"t\": %d}, \"events\": [", s, t);
    first_update = 1;
}

void log_part4_path(const char *label, int *nodes, int len, double cost) {
    if (!log_file) return;
    if (!first_update) fprintf(log_file, ", ");
    
    fprintf(log_file, "{\"type\": \"path\", \"label\": \"%s\", \"cost\": %.2f, \"nodes\": [", label, cost);
    for (int i = 0; i < len; i++) {
        fprintf(log_file, "%d%s", nodes[i], (i < len - 1) ? "," : "");
    }
    fprintf(log_file, "]}");
    first_update = 0;
}

void log_part4_payment(int node, int bid, double payment) {
    if (!log_file) return;
    if (!first_update) fprintf(log_file, ", ");
    fprintf(log_file, "{\"type\": \"payment\", \"node\": %d, \"bid\": %d, \"pay\": %.2f}", node, bid, payment);
    first_update = 0;
}

#endif
