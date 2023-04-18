#ifndef FASTNIC_LOG_H
#define FASTNIC_LOG_H 1

#include "ovs-atomic.h"
#include "openvswitch/thread.h"

#ifdef DPDK_NETDEV
#include <rte_cycles.h>
#endif

#define FASTNIC_LOG

enum fastnic_pmd_stat_type {
    OFFLOAD_CREATE_PMD, /* call queue_netdev_flow_put */
    OFFLOAD_DEL_PMD,/* call queue_netdev_flow_del */
    FASTNIC_PMD_N_STATS
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
    /* counter of performance measurement periods since the program run*/
    uint64_t measure_cnt;    

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

enum fastnic_offload_stat_type {
    OFFLOAD_CREATE_PUT_OK, /* call dp_netdev_flow_offload_put to add flow and excute offload*/
    OFFLOAD_CREATE_PUT_FAIL, /* call dp_netdev_flow_offload_put to add flow but stop offload, flow dead or already offloaded*/
    OFFLOAD_CREATE_MOD_OK, /* call dp_netdev_flow_offload_put to modify flow and excute offload*/
    OFFLOAD_CREATE_MOD_FAIL, /* call dp_netdev_flow_offload_put to modify flow but stop offload, flow dead*/
    OFFLOAD_CREATE_RTE_OK, /* call rte_flow_create and succeed */
    OFFLOAD_CREATE_RTE_FAIL, /* call rte_flow_create and fail */
    OFFLOAD_CREATE_PUT_OK_CYCLE, /*  cpu cycle when OFFLOAD_CREATE_PUT_OK. below is the same*/
    OFFLOAD_CREATE_PUT_FAIL_CYCLE,
    OFFLOAD_CREATE_MOD_OK_CYCLE,
    OFFLOAD_CREATE_MOD_FAIL_CYCLE,
    
    OFFLOAD_DEL_API_OK, /* call dp_netdev_flow_offload_del and succeed */
    OFFLOAD_DEL_API_FAIL, /* call dp_netdev_flow_offload_del and fail */
    OFFLOAD_DEL_RTE_OK, /**/
    OFFLOAD_DEL_RTE_FAIL, /**/
    OFFLOAD_DEL_API_OK_CYCLE, /*  cpu cycle when OFFLOAD_DEL_API_OK. below is the same*/
    OFFLOAD_DEL_API_FAIL_CYCLE,
    FASTNIC_OFFLOAD_N_STATS
};

struct fastnic_offload_counters {
    atomic_uint64_t n[FASTNIC_OFFLOAD_N_STATS];     /* Value since _init(). */
    uint64_t zero[FASTNIC_OFFLOAD_N_STATS];         /* Value at last _clear().  */
};

struct fastnic_offload_perf_stats{ 
    volatile bool init;
    /* Prevents interference between PMD polling and stats clearing. */
    struct ovs_mutex stats_mutex;
    /* Set by CLI thread to order clearing of PMD stats. */
    volatile bool clear;
    /* Prevents stats retrieval while clearing is in progress. */
    struct ovs_mutex clear_mutex;

    /* Start of the current performance measurement period. */
    uint64_t start_ms;
    /* counter of performance measurement periods since the program run*/
    uint64_t measure_cnt;    
    
    struct fastnic_offload_counters counters;
};

extern struct fastnic_offload_perf_stats fastnic_offload_stats;

void fastnic_pmd_perf_stats_init(struct fastnic_pmd_perf_stats *s);
void fastnic_pmd_perf_start_pmditeration(struct fastnic_pmd_perf_stats *s);
void fastnic_offload_perf_stats_init(struct fastnic_offload_perf_stats *s);
void fastnic_offload_perf_start_offloaditeration(struct fastnic_offload_perf_stats *s);
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

static inline void
fastnic_offload_update_counter(struct fastnic_offload_perf_stats *s,
                        enum fastnic_offload_stat_type counter, int delta)
{
    uint64_t tmp;
    atomic_read_relaxed(&s->counters.n[counter], &tmp);
    tmp += delta;
    atomic_store_relaxed(&s->counters.n[counter], tmp);
}

/* Support for accurate timing of PMD execution on TSC clock cycle level.
 * These functions are intended to be invoked in the context of pmd threads. */

/* Read the TSC cycle register and cache it. Any function not requiring clock
 * cycle accuracy should read the cached value using cycles_counter_get() to
 * avoid the overhead of reading the TSC register. */
//change from lib/dpif-netdev-perf.h : cycles_counter_update
static inline uint64_t
cycles_cnt_update(void)
{
#ifdef DPDK_NETDEV
    return rte_get_tsc_cycles();
#elif !defined(_MSC_VER) && defined(__x86_64__)
    uint32_t h, l;
    asm volatile("rdtsc" : "=a" (l), "=d" (h));

    return ((uint64_t) h << 32) | l;
#elif !defined(_MSC_VER) && defined(__aarch64__)
    uint64_t tsc;
    asm volatile("mrs %0, cntvct_el0" : "=r" (tsc));

    return tsc;
#elif defined(__linux__)
    struct timespec val;
    uint64_t v;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &val) != 0) {
       return 0;
    }

    v  = val.tv_sec * UINT64_C(1000000000) + val.tv_nsec;
    return v;
#else
    return 0;
#endif
}

#endif /* FASTNIC_LOG_H */
