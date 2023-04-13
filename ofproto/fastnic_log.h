#ifndef FASTNIC_LOG_H
#define FASTNIC_LOG_H 1

#define FASTNIC_N_STATS 6

// enum fastnic_stat_type {
//     OFFLOAD_CREATE_INFO, /* call queue_netdev_flow_put */
//     OFFLOAD_CREATE_RTE_OK, /* call rte_flow_create and succeed */
//     OFFLOAD_CREATE_RTE_FAIL,/* call rte_flow_create and fail */
//     OFFLOAD_DEL_INFO,/**/
//     OFFLOAD_DEL_RTE_OK,/**/
//     OFFLOAD_DEL_RTE_FAIL/**/
// };

// struct fastnic_counters {
//     atomic_uint64_t n[FASTNIC_N_STATS];     /* Value since _init(). */
//     uint64_t zero[FASTNIC_N_STATS];         /* Value at last _clear().  */
// };

// struct fastnic_perf_stats{ 
//     /* Prevents interference between PMD polling and stats clearing. */
//     struct ovs_mutex stats_mutex;
//     /* Set by CLI thread to order clearing of PMD stats. */
//     volatile bool clear;
//     /* Prevents stats retrieval while clearing is in progress. */
//     struct ovs_mutex clear_mutex;
    
//     struct fastnic_counters counters;
// };

int print_log(pthread_t thread_id);

#endif /* FASTNIC_LOG_H */