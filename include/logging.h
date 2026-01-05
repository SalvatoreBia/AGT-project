#ifndef LOGGING_H
#define LOGGING_H

#include <stddef.h>

// If compiled with -DENABLE_LOGGING, these macros transform into function calls.
// Otherwise, they expand to empty statements.

#ifdef ENABLE_LOGGING

void log_init(const char *fmt, ...);
void log_step_begin(long iteration, const char *algo_name);
void log_node_update(long node_id, int old_strat, int new_strat, double utility_val);
void log_msg(const char *msg);
void log_step_end();
void log_close();

// New functions for Parts 3 & 4
void log_part3_start(const char *mode);
void log_part3_iter(int iteration, int flow_added, double cost_added);
void log_part3_match(int buyer, int vendor, int budget, int price, double utility);
void log_part4_start(int s, int t);
void log_part4_path(const char *label, int *nodes, int len, double cost);
void log_part4_payment(int node, int bid, double payment);

#define LOG_INIT(...) log_init(__VA_ARGS__)
#define LOG_STEP_BEGIN(iter, name) log_step_begin(iter, name)
#define LOG_NODE_UPDATE(id, old, new, util) log_node_update(id, old, new, util) 
#define LOG_MSG(msg) log_msg(msg)
#define LOG_STEP_END() log_step_end()
#define LOG_CLOSE() log_close()

// Macros for Parts 3 & 4
#define LOG_P3_START(mode) log_part3_start(mode)
#define LOG_P3_ITER(it, f, c) log_part3_iter(it, f, c)
#define LOG_P3_MATCH(b, v, bud, p, u) log_part3_match(b, v, bud, p, u)
#define LOG_P4_START(s, t) log_part4_start(s, t)
#define LOG_P4_PATH(l, n, len, c) log_part4_path(l, n, len, c)
#define LOG_P4_PAY(node, bid, pay) log_part4_payment(node, bid, pay)

#else

#define LOG_INIT(...)
#define LOG_STEP_BEGIN(iter, name)
#define LOG_NODE_UPDATE(id, old, new, util)
#define LOG_MSG(msg)
#define LOG_STEP_END()
#define LOG_CLOSE()

#define LOG_P3_START(mode)
#define LOG_P3_ITER(it, f, c)
#define LOG_P3_MATCH(b, v, bud, p, u)
#define LOG_P4_START(s, t)
#define LOG_P4_PATH(l, n, len, c)
#define LOG_P4_PAY(node, bid, pay)

#endif

#endif // LOGGING_H
