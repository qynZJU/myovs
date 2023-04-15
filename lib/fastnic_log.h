#ifndef FASTNIC_LOG_H
#define FASTNIC_LOG_H 1

//qq 记得改
#define FASTNIC_PMD_N_STATS 1
#define FASTNIC_OFFLOAD_N_STATS 5

#include "ovs-atomic.h"
#include "openvswitch/thread.h"

enum fastnic_pmd_stat_type {
    OFFLOAD_CREATE_PMD, /* call queue_netdev_flow_put */
    // OFFLOAD_DEL_INFO,/**/
};

struct fastnic_pmd_counters {
    atomic_uint64_t n[FASTNIC_PMD_N_STATS];     /* Value since _init(). */
    uint64_t zero[FASTNIC_PMD_N_STATS];         /* Value at last _clear().  */
};

struct fastnic_pmd_perf_stats{ 
    /* Prevents interference between PMD polling and stats clearing. */
    struct ovs_mutex stats_mutex;
    /* Set by CLI thread to order clearing of PMD stats. */
    volatile bool clear;
    /* Prevents stats retrieval while clearing is in progress. */
    struct ovs_mutex clear_mutex;

    /* Start of the current performance measurement period. */
    uint64_t start_ms;
    
    struct fastnic_pmd_counters counters;

    // /* Start of the current iteration. */
    // uint64_t start_tsc;
    // /* Latest TSC time stamp taken in PMD. */
    // uint64_t last_tsc;
    // /* Used to space certain checks in time. */
    // uint64_t next_check_tsc;
    // /* If non-NULL, outermost cycle timer currently running in PMD. */
    // struct cycle_timer *cur_timer;

    // /* Iteration history buffer. */
    // struct history iterations;
    // /* Millisecond history buffer. */
    // struct history milliseconds;

    // /* Suspicious iteration log. */
    // uint32_t log_susp_it;
    // /* Start of iteration range to log. */
    // uint32_t log_begin_it;
    // /* End of iteration range to log. */
    // uint32_t log_end_it;
    // /* Reason for logging suspicious iteration. */
    // char *log_reason;
};

// enum fastnic_offload_stat_type {
//     OFFLOAD_CREATE_CALL,/* call dp_netdev_flow_offload_put*/
//     OFFLOAD_CREATE_RTE_OK, /* call rte_flow_create and succeed */
//     OFFLOAD_CREATE_RTE_FAIL,/* call rte_flow_create and fail */
//     OFFLOAD_DEL_RTE_OK,/**/
//     OFFLOAD_DEL_RTE_FAIL/**/
// };

// struct fastnic_offload_counters {
//     atomic_uint64_t n[FASTNIC_OFFLOAD_N_STATS];     /* Value since _init(). */
//     uint64_t zero[FASTNIC_OFFLOAD_N_STATS];         /* Value at last _clear().  */
// };

// struct fastnic_offload_perf_stats{ 
//     volatile bool init;
//     /* Prevents interference between PMD polling and stats clearing. */
//     struct ovs_mutex stats_mutex;
//     /* Set by CLI thread to order clearing of PMD stats. */
//     volatile bool clear;
//     /* Prevents stats retrieval while clearing is in progress. */
//     struct ovs_mutex clear_mutex;

//     /* Start of the current performance measurement period. */
//     uint64_t start_ms;
    
//     struct fastnic_offload_counters counters;
// };

// struct fastnic_offload_perf_stats fastnic_offload_stats = {
//     .init = false,
// };

void fastnic_pmd_perf_stats_init(struct fastnic_pmd_perf_stats *s);
void fastnic_pmd_perf_start_iteration(struct fastnic_pmd_perf_stats *s);
int print_log(pthread_t thread_id);


/* PMD performance counters are updated lock-less. For real PMDs
 * they are only updated from the PMD thread itself. In the case of the
 * NON-PMD they might be updated from multiple threads, but we can live
 * with losing a rare update as 100% accuracy is not required.
 * However, as counters are read for display from outside the PMD thread
 * with e.g. pmd-stats-show, we make sure that the 64-bit read and store
 * operations are atomic also on 32-bit systems so that readers cannot
 * not read garbage. On 64-bit systems this incurs no overhead. */
//changed from lib/dpif-netdev-perf.h: pmd_perf_update_counter
static inline void
fastnic_perf_update_counter(struct fastnic_pmd_perf_stats *s,
                        enum fastnic_pmd_stat_type counter, int delta)
{
    uint64_t tmp;
    atomic_read_relaxed(&s->counters.n[counter], &tmp);
    tmp += delta;
    atomic_store_relaxed(&s->counters.n[counter], tmp);
}

#endif /* FASTNIC_LOG_H */
