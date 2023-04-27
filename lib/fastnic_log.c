#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "fastnic_log.h"

#include "cmap.h"
#include "dpif-netdev.h"
#include "dpif-netdev-perf.h"
#include "dpif-netdev-private-thread.h"
#include "openvswitch/vlog.h"

#define PKT_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/pmd_pkt_stats.csv"
#define CYCLE_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/pmd_cycle_stats.csv"
#define RXQ_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/pmd_rxq_stats.csv"
#define FASTNIC_PMD_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/fastnic_pmd_stats.csv"
#define FASTNIC_OFFLOAD_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/fastnic_ol_stats.csv"
#define FASTNIC_REVAL_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/fastnic_reval_stats.csv"

VLOG_DEFINE_THIS_MODULE(fastnic_log);

struct fastnic_offload_perf_stats fastnic_offload_stats = {
    .init = false,
};

static void now_time_log(pthread_t thread_id);
static uint64_t pmd_perf_read_counter(const struct pmd_perf_stats *s, enum pmd_stat_type counter);
static void one_pmd_sta(struct dp_netdev_pmd_thread *pmd);
static void one_pmd_show_rxq(struct dp_netdev_pmd_thread *pmd);
// static void pmd_stats_log(void);
static void fastnic_pmd_perf_stats_clear_lock(struct fastnic_pmd_perf_stats *s);
static void fastnic_pmd_perf_stats_clear(struct fastnic_pmd_perf_stats *s);
static void fastnic_pmd_perf_read_counters(const struct fastnic_pmd_perf_stats *s, 
                                           uint64_t stats[FASTNIC_PMD_N_STATS]);
static void fastnic_offload_perf_stats_clear_lock(struct fastnic_offload_perf_stats *s);
static void fastnic_offload_perf_stats_clear(struct fastnic_offload_perf_stats *s);
static void fastnic_offload_perf_read_counters(const struct fastnic_offload_perf_stats *s, 
                                               uint64_t stats[FASTNIC_OFFLOAD_N_STATS]);
static void fastnic_pmd_sta(struct dp_netdev_pmd_thread *pmd);
static void fastnic_offload_sta(void);
static void fastnic_reval_perf_stats_clear_lock(struct fastnic_revalidate_perf_stats *s);
static void fastnic_reval_perf_stats_clear(struct fastnic_revalidate_perf_stats *s);
static void fastnic_reval_perf_read_counters(const struct fastnic_revalidate_perf_stats *s,
                                             uint64_t stats[FASTNIC_REVALIDATE_N_STATS]);
static void fastnic_reval_sta(unsigned int revalidator_thread_id,
                              struct fastnic_revalidate_perf_stats *perf_stats);

static void
now_time_log(pthread_t thread_id){
    struct timeval tv;
    struct tm tzone;
    char datetime[20];

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tzone);
    strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", &tzone);
    
    VLOG_INFO("thread %ld, start dump at %s, accurate time as %lds-%ldus",thread_id, datetime, tv.tv_sec, tv.tv_usec);
}

static uint64_t pmd_perf_read_counter(const struct pmd_perf_stats *s, enum pmd_stat_type counter) {
    uint64_t val;
    
    atomic_read_relaxed(&s->counters.n[counter], &val);
    if (val > s->counters.zero[counter]) {
        val = val - s->counters.zero[counter];
    } else {
        val = 0;
    }

    return val;
}

// change from lib/dpif-netdev.c:pmd_info_show_stats
static void
one_pmd_sta(struct dp_netdev_pmd_thread *pmd) 
{
    uint64_t stats[PMD_N_STATS];
    uint64_t total_cycles, total_packets;
    double passes_per_pkt = 0;
    double lookups_per_hit = 0;
    double packets_per_batch = 0;
    const char pkt_stats_file[100] = PKT_STATS_FILE;
    const char cycle_stats_file[100] = CYCLE_STATS_FILE;
    struct timeval tv;
    uint64_t time_interval;
    FILE *fp = NULL;
    
    gettimeofday(&tv, NULL);
    time_interval = (tv.tv_sec - pmd->fastnic_stats.start_t.tv_sec)*1000 + (tv.tv_usec - pmd->fastnic_stats.start_t.tv_usec)/1000; //accurate to millisecond

    pmd_perf_read_counters(&pmd->perf_stats, stats);
    
    total_cycles = stats[PMD_CYCLES_ITER_IDLE]
                         + stats[PMD_CYCLES_ITER_BUSY];
    total_packets = stats[PMD_STAT_RECV];

    if (total_packets > 0){
        passes_per_pkt = (total_packets + stats[PMD_STAT_RECIRC])
                         / (double) total_packets;
    }
    if (stats[PMD_STAT_MASKED_HIT] > 0) {
        lookups_per_hit = stats[PMD_STAT_MASKED_LOOKUP]
                            / (double) stats[PMD_STAT_MASKED_HIT];
    }
    if (stats[PMD_STAT_SENT_BATCHES] > 0) {
        packets_per_batch = stats[PMD_STAT_SENT_PKTS]
                            / (double) stats[PMD_STAT_SENT_BATCHES];
    }

    /* stats of packets*/
    if (OVS_UNLIKELY(access(pkt_stats_file, 0) != 0)) {
        fp = fopen(pkt_stats_file, "a+");
        fprintf(fp, "measure_cnt,timestamp,interval/ms,dimension,numa_id,core_id,\
                    rcv_pkts,rcircu_pkts,PHWOL_pkts,MFEX_pkts,EMC_pkts,SMC_pkts,\
                    MEGA_pkts,UPCALLS_pkts,UPCALLF_pkts,\
                    avgpkt_batch,passes_per_pkt,megalookup_num\r\n");
    } else {
        fp = fopen(pkt_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", pkt_stats_file);
    }

    fprintf(fp, "%"PRIu64",", pmd->fastnic_stats.measure_cnt);
    fprintf(fp, "%ld,", tv.tv_sec);
    fprintf(fp, "%"PRIu64",", time_interval);
    fprintf(fp, ((pmd->core_id == NON_PMD_CORE_ID) ? "all," : "pmd,"));//dimension 
    fprintf(fp, "%d,", pmd->numa_id);
    fprintf(fp, "%u,", pmd->core_id);
    fprintf(fp, "%"PRIu64",", total_packets);
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_RECIRC]);//Packets reentering the datapath pipeline due to recirculation
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_PHWOL_HIT]); //partial hardware offload hit
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_MFEX_OPT_HIT]);//miniflow optimized match
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_EXACT_HIT]);//exact match (emc)
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_SMC_HIT]);//sig match hit (smc)
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_MASKED_HIT]);//megaflow hit
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_MISS]);//did not match and upcall was ok
    fprintf(fp, "%"PRIu64",", stats[PMD_STAT_LOST]);//did not match and upcall failed


    fprintf(fp, "%.02f,", packets_per_batch);
    fprintf(fp, "%.02f,", passes_per_pkt); //avg pass times to pipeline of every packets
    fprintf(fp, "%.02f\r\n", lookups_per_hit);//megaflow lookup times per hit
    fclose(fp);

    /* stats of cycles*/
    fp = NULL;
    if (OVS_UNLIKELY(access(cycle_stats_file, 0) != 0)) {
        fp = fopen(cycle_stats_file, "a+");
        fprintf(fp, "measure_cnt,timestamp,dimension,numa_id,core_id,rcv_pkts\r\n");
    } else {
        fp = fopen(cycle_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", cycle_stats_file);
    }

    fprintf(fp, "%"PRIu64",", pmd->fastnic_stats.measure_cnt);
    fprintf(fp, "%ld,", tv.tv_sec);
    fprintf(fp, ((pmd->core_id == NON_PMD_CORE_ID) ? "all," : "pmd,"));//dimension 
    fprintf(fp, "%d,", pmd->numa_id);
    fprintf(fp, "%u,", pmd->core_id);
    fprintf(fp, "%"PRIu64",", total_packets);

    //total_cycles != 0
    fprintf(fp, "%"PRIu64",", stats[PMD_CYCLES_ITER_IDLE]); //idle cycle
    fprintf(fp, "%"PRIu64",", stats[PMD_CYCLES_ITER_BUSY]); //busy cycle
    if (total_cycles != 0){
        fprintf(fp, "%.02f%%,", stats[PMD_CYCLES_ITER_IDLE] / (double) total_cycles * 100); //percentage of idle cycle
        fprintf(fp, "%.02f%%,", stats[PMD_CYCLES_ITER_BUSY] / (double) total_cycles * 100); //percentage of busy cycle
    } else {
        fprintf(fp, "NaN,"); //percentage of idle cycle
        fprintf(fp, "NaN,"); //percentage of busy cycle
    }
    if (total_packets != 0){
        fprintf(fp, "%.02f%%,", total_cycles / (double) total_packets);
        fprintf(fp, "%.02f\r\n", stats[PMD_CYCLES_ITER_BUSY] / (double) total_packets);
    }else {
        fprintf(fp, "NaN,"); //avg cycles per packet
        fprintf(fp, "NaN\r\n"); //avg processing cycles per packet
    }
    fclose(fp);
}

//change from lib/dpif-netdev.c:pmd_info_show_rxq
static void
one_pmd_show_rxq(struct dp_netdev_pmd_thread *pmd)
{
    if (pmd->core_id != NON_PMD_CORE_ID) {
        struct rxq_info *rxq_list;

        size_t n_rxq;

        const char rxq_stats_file[100] = RXQ_STATS_FILE;
        struct timeval tv;
        uint64_t time_interval;
        FILE *fp = NULL;

        gettimeofday(&tv, NULL);
        time_interval = (tv.tv_sec - pmd->fastnic_stats.start_t.tv_sec)*1000 + (tv.tv_usec - pmd->fastnic_stats.start_t.tv_usec)/1000; //accurate to millisecond
        
        n_rxq = read_rxq_list (pmd, &rxq_list);

        /* stats of rxq*/
        if (OVS_UNLIKELY(access(rxq_stats_file, 0) != 0)) {
            fp = fopen(rxq_stats_file, "a+");
            fprintf(fp, "measure_cnt,timestamp,interval/ms,dimension,numa_id,core_id,\
                        isolated,port,queue_id,queue_state,pmd_usage\r\n");
        } else {
            fp = fopen(rxq_stats_file, "a+");
        }
        if (fp == NULL) {
            VLOG_INFO("can not open file %s", rxq_stats_file);
        }
        
        for (int i = 0; i < n_rxq; i++) {
            fprintf(fp, "%"PRIu64",", pmd->fastnic_stats.measure_cnt);
            fprintf(fp, "%ld,", tv.tv_sec);
            fprintf(fp, "%"PRIu64",", time_interval);
            fprintf(fp, ((pmd->core_id == NON_PMD_CORE_ID) ? "all," : "pmd,"));//dimension 
            fprintf(fp, "%d,", pmd->numa_id);
            fprintf(fp, "%u,", pmd->core_id);
            fprintf(fp, "%s,", ((pmd->isolated) ? "true" : "false"));
            fprintf(fp, "%s,", rxq_list[i].port); //port
            fprintf(fp, "%d,", rxq_list[i].queue_id); //queue id
            fprintf(fp, "%s,", rxq_list[i].queue_state ? "enabled" : "disabled"); //queue state
            fprintf(fp, "%f\r\n",rxq_list[i].pmd_usage);
        }

        // if (n_rxq > 0) {
        //     ds_put_cstr(reply, "  overhead: ");
        //     if (total_cycles) {
        //         uint64_t overhead_cycles = 0;

        //         if (total_rxq_proc_cycles < busy_cycles) {
        //             overhead_cycles = busy_cycles - total_rxq_proc_cycles;
        //         }
        //         ds_put_format(reply, "%2"PRIu64" %%",
        //                       overhead_cycles * 100 / total_cycles);
        //     } else {
        //         ds_put_cstr(reply, "NOT AVAIL");
        //     }
        //     ds_put_cstr(reply, "\n");
        // }

        free(rxq_list);
        fclose(fp);
    }
}

// //qq: waiting to edit, or may not use (pmd_info_show_perf)
// //change from lib/dpif-netdev.c:pmd_info_show_perf
// static void
// pmd_info_show_perf(struct ds *reply,
//                    struct dp_netdev_pmd_thread *pmd,
//                    struct pmd_perf_params *par)
// {
//     if (pmd->core_id != NON_PMD_CORE_ID) {
//         char *time_str =
//                 xastrftime_msec("%H:%M:%S.###", time_wall_msec(), true);
//         long long now = time_msec();
//         double duration = (now - pmd->perf_stats.start_ms) / 1000.0;
//
//         ds_put_cstr(reply, "\n");
//         ds_put_format(reply, "Time: %s\n", time_str);
//         ds_put_format(reply, "Measurement duration: %.3f s\n", duration);
//         ds_put_cstr(reply, "\n");
//         format_pmd_thread(reply, pmd);
//         ds_put_cstr(reply, "\n");
//         pmd_perf_format_overall_stats(reply, &pmd->perf_stats, duration);
//         if (pmd_perf_metrics_enabled(pmd)) {
//             /* Prevent parallel clearing of perf metrics. */
//             ovs_mutex_lock(&pmd->perf_stats.clear_mutex);
//             if (par->histograms) {
//                 ds_put_cstr(reply, "\n");
//                 pmd_perf_format_histograms(reply, &pmd->perf_stats);
//             }
//             if (par->iter_hist_len > 0) {
//                 ds_put_cstr(reply, "\n");
//                 pmd_perf_format_iteration_history(reply, &pmd->perf_stats,
//                         par->iter_hist_len);
//             }
//             if (par->ms_hist_len > 0) {
//                 ds_put_cstr(reply, "\n");
//                 pmd_perf_format_ms_history(reply, &pmd->perf_stats,
//                         par->ms_hist_len);
//             }
//             ovs_mutex_unlock(&pmd->perf_stats.clear_mutex);
//         }
//         free(time_str);
//     }
// }

// //change from lib/dpif-netdev.c:dpif_netdev_pmd_info
// static void
// pmd_stats_log(void)
// {
//     struct dp_netdev_pmd_thread **pmd_list;
//     size_t n;

//     pmd_list = read_pmd_thread(&n);

//     for (size_t i = 0; i < n; i++) {
//         struct dp_netdev_pmd_thread *pmd = pmd_list[i];

//         if (!pmd) {
//             break;
//         }

//         /* show info*/
//         one_pmd_stats(pmd);
//         one_pmd_show_rxq(pmd);
//         // pmd_info_show_perf(&reply, pmd, (struct pmd_perf_params *)aux);

//         /* clear stats*/ //qq: use OVS api temporarily
//         pmd_perf_stats_clear(&pmd->perf_stats);

//     }
//     free(pmd_list);

// }

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_init
void
fastnic_pmd_perf_stats_init(struct fastnic_pmd_perf_stats *s)
{
    memset(s, 0, sizeof(*s));
    ovs_mutex_init(&s->stats_mutex);
    ovs_mutex_init(&s->clear_mutex);
    
    // s->start_ms = time_msec();
    gettimeofday(&s->start_t, NULL);
    s->measure_cnt = 0;
    // s->log_susp_it = UINT32_MAX;
    // s->log_begin_it = UINT32_MAX;
    // s->log_end_it = UINT32_MAX;
    // s->log_reason = NULL;
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_clear_lock
static void
fastnic_pmd_perf_stats_clear_lock(struct fastnic_pmd_perf_stats *s)
    OVS_REQUIRES(s->stats_mutex)
{
    ovs_mutex_lock(&s->clear_mutex);
    for (int i = 0; i < FASTNIC_PMD_N_STATS; i++) {
        atomic_read_relaxed(&s->counters.n[i], &s->counters.zero[i]);
    }

    // s->start_ms = time_msec();
    gettimeofday(&s->start_t, NULL);
    /* Clearing finished. */
    s->clear = false;
    ovs_mutex_unlock(&s->clear_mutex);
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_clear
static void
fastnic_pmd_perf_stats_clear(struct fastnic_pmd_perf_stats *s)
{
    if (ovs_mutex_trylock(&s->stats_mutex) == 0) {
        /* Locking successful. PMD not polling. */
        fastnic_pmd_perf_stats_clear_lock(s);
        ovs_mutex_unlock(&s->stats_mutex);
    } else {
        /* Request the polling PMD to clear the stats. There is no need to
         * block here as stats retrieval is prevented during clearing. */
        s->clear = true;
    }
}

//change from lib/dpif-netdev-perf.c:pmd_perf_start_iteration
void
fastnic_pmd_perf_start_pmditeration(struct fastnic_pmd_perf_stats *s)
OVS_REQUIRES(s->stats_mutex)
{
    if (s->clear) {
        /* Clear the PMD stats before starting next iteration. */
        fastnic_pmd_perf_stats_clear_lock(s);
    }
}

//change from lib/dpif-netdev-perf.c: pmd_perf_read_counters
static void
fastnic_pmd_perf_read_counters(const struct fastnic_pmd_perf_stats *s,
                       uint64_t stats[FASTNIC_PMD_N_STATS])
{
    uint64_t val;

    /* These loops subtracts reference values (.zero[*]) from the counters.
     * Since loads and stores are relaxed, it might be possible for a .zero[*]
     * value to be more recent than the current value we're reading from the
     * counter.  This is not a big problem, since these numbers are not
     * supposed to be 100% accurate, but we should at least make sure that
     * the result is not negative. */
    for (int i = 0; i < FASTNIC_PMD_N_STATS; i++) {
        atomic_read_relaxed(&s->counters.n[i], &val);
        if (val > s->counters.zero[i]) {
            stats[i] = val - s->counters.zero[i];
        } else {
            stats[i] = 0;
        }
    }
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_init
void
fastnic_offload_perf_stats_init(struct fastnic_offload_perf_stats *s)
{
    memset(s, 0, sizeof(*s));
    s->init = true;
    ovs_mutex_init(&s->stats_mutex);
    ovs_mutex_init(&s->clear_mutex);
    
    // s->start_ms = time_msec();
    gettimeofday(&s->start_t, NULL);
    s->measure_cnt = 0;
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_clear_lock
static void
fastnic_offload_perf_stats_clear_lock(struct fastnic_offload_perf_stats *s)
    OVS_REQUIRES(s->stats_mutex)
{
    ovs_mutex_lock(&s->clear_mutex);
    for (int i = 0; i < FASTNIC_OFFLOAD_N_STATS; i++) {
        atomic_read_relaxed(&s->counters.n[i], &s->counters.zero[i]);
    }

    // s->start_ms = time_msec();
    gettimeofday(&s->start_t, NULL);
    /* Clearing finished. */
    s->clear = false;
    ovs_mutex_unlock(&s->clear_mutex);
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_clear
static void
fastnic_offload_perf_stats_clear(struct fastnic_offload_perf_stats *s)
{
    if (ovs_mutex_trylock(&s->stats_mutex) == 0) {
        /* Locking successful. PMD not polling. */
        fastnic_offload_perf_stats_clear_lock(s);
        ovs_mutex_unlock(&s->stats_mutex);
    } else {
        /* Request the polling PMD to clear the stats. There is no need to
         * block here as stats retrieval is prevented during clearing. */
        s->clear = true;
    }
}

//change from lib/dpif-netdev-perf.c:pmd_perf_start_iteration
void
fastnic_offload_perf_start_offloaditeration(struct fastnic_offload_perf_stats *s)
OVS_REQUIRES(s->stats_mutex)
{
    if (s->clear) {
        /* Clear the PMD stats before starting next iteration. */
        fastnic_offload_perf_stats_clear_lock(s);
    }
}

//change from lib/dpif-netdev-perf.c: pmd_perf_read_counters
static void
fastnic_offload_perf_read_counters(const struct fastnic_offload_perf_stats *s,
                       uint64_t stats[FASTNIC_OFFLOAD_N_STATS])
{
    uint64_t val;

    /* These loops subtracts reference values (.zero[*]) from the counters.
     * Since loads and stores are relaxed, it might be possible for a .zero[*]
     * value to be more recent than the current value we're reading from the
     * counter.  This is not a big problem, since these numbers are not
     * supposed to be 100% accurate, but we should at least make sure that
     * the result is not negative. */
    for (int i = 0; i < FASTNIC_OFFLOAD_N_STATS; i++) {
        atomic_read_relaxed(&s->counters.n[i], &val);
        if (val > s->counters.zero[i]) {
            stats[i] = val - s->counters.zero[i];
        } else {
            stats[i] = 0;
        }
    }
}

// change from lib/dpif-netdev.c:pmd_info_show_stats
static void
fastnic_pmd_sta(struct dp_netdev_pmd_thread *pmd) 
{
    uint64_t stats_pmd[FASTNIC_PMD_N_STATS];
    const char fastnic_pmd_stats_file[100] = FASTNIC_PMD_STATS_FILE;
    struct timeval tv;
    uint64_t time_interval;
    FILE *fp = NULL;
    
    gettimeofday(&tv, NULL);
    time_interval = (tv.tv_sec - pmd->fastnic_stats.start_t.tv_sec)*1000 + (tv.tv_usec - pmd->fastnic_stats.start_t.tv_usec)/1000; //accurate to millisecond


    fastnic_pmd_perf_read_counters(&pmd->fastnic_stats, stats_pmd);
    
    if (OVS_UNLIKELY(access(fastnic_pmd_stats_file, 0) != 0)) {
        fp = fopen(fastnic_pmd_stats_file, "a+");
        fprintf(fp, "measure_cnt,timestamp,interval/ms,dimension,numa_id,core_id,\
                    offload_create_pmd,offload_del_pmd\r\n");
    } else {
        fp = fopen(fastnic_pmd_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", fastnic_pmd_stats_file);
    }

    fprintf(fp, "%"PRIu64",", pmd->fastnic_stats.measure_cnt);
    fprintf(fp, "%ld,", tv.tv_sec);
    fprintf(fp, "%"PRIu64",", time_interval);
    fprintf(fp, ((pmd->core_id == NON_PMD_CORE_ID) ? "all," : "pmd,"));//dimension 
    fprintf(fp, "%d,", pmd->numa_id);
    fprintf(fp, "%u,", pmd->core_id);
    fprintf(fp, "%"PRIu64",", stats_pmd[OFFLOAD_CREATE_PMD]); /* call queue_netdev_flow_put */
    fprintf(fp, "%"PRIu64"\r\n", stats_pmd[OFFLOAD_DEL_PMD]); /* call queue_netdev_flow_put */
    fclose(fp);
}

// change from lib/dpif-netdev.c:pmd_info_show_stats
static void
fastnic_offload_sta(void) 
{
    uint64_t stats_offload[FASTNIC_OFFLOAD_N_STATS];
    const char fastnic_offload_stats_file[100] = FASTNIC_OFFLOAD_STATS_FILE;
    struct timeval tv;
    uint64_t time_interval;
    FILE *fp = NULL;
    
    gettimeofday(&tv, NULL);
    time_interval = (tv.tv_sec - fastnic_offload_stats.start_t.tv_sec)*1000 + (tv.tv_usec - fastnic_offload_stats.start_t.tv_usec)/1000; //accurate to millisecond

    fastnic_offload_perf_read_counters(&fastnic_offload_stats, stats_offload);
    
    if (OVS_UNLIKELY(access(fastnic_offload_stats_file, 0) != 0)) {
        fp = fopen(fastnic_offload_stats_file, "a+");
        fprintf(fp, "offload_measure_cnt,timestamp,interval,\
                    put_ok,put_fail,mod_ok,mod_fail,rte_create_ok,rte_create_fail,\
                    del_ok,del_fail,rte_del_ok,rte_del_fail\
                    put_ok_cycle,put_fail_cycle,mod_ok_cycle,mod_fail_cycle\
                    del_ok_cycle,del_fail_cycle\r\n");
    } else {
        fp = fopen(fastnic_offload_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", fastnic_offload_stats_file);
    }

    uint64_t put_ok_cycle = 0, put_fail_cycle = 0;
    uint64_t mod_ok_cycle = 0, mod_fail_cycle = 0;
    uint64_t del_ok_cycle = 0, del_fail_cycle = 0;

    if (stats_offload[OFFLOAD_CREATE_PUT_OK] != 0) {
        put_ok_cycle = stats_offload[OFFLOAD_CREATE_PUT_OK_CYCLE] / stats_offload[OFFLOAD_CREATE_PUT_OK];
    }
    if (stats_offload[OFFLOAD_CREATE_PUT_FAIL] != 0) {
        put_fail_cycle = stats_offload[OFFLOAD_CREATE_PUT_FAIL_CYCLE] / stats_offload[OFFLOAD_CREATE_PUT_FAIL];
    }
    if (stats_offload[OFFLOAD_CREATE_MOD_OK] != 0) {
        mod_ok_cycle = stats_offload[OFFLOAD_CREATE_MOD_OK_CYCLE] / stats_offload[OFFLOAD_CREATE_MOD_OK];
    }      
    if (stats_offload[OFFLOAD_CREATE_MOD_FAIL] != 0) {
        mod_fail_cycle = stats_offload[OFFLOAD_CREATE_MOD_FAIL_CYCLE] / stats_offload[OFFLOAD_CREATE_MOD_FAIL];
    }        
    if (stats_offload[OFFLOAD_DEL_API_OK] != 0) {
        del_ok_cycle = stats_offload[OFFLOAD_DEL_API_OK_CYCLE] / stats_offload[OFFLOAD_DEL_API_OK];
    }        
    if (stats_offload[OFFLOAD_DEL_API_FAIL] != 0) {
        del_fail_cycle = stats_offload[OFFLOAD_DEL_API_FAIL_CYCLE] / stats_offload[OFFLOAD_DEL_API_FAIL];
    }

    fprintf(fp, "%"PRIu64",", fastnic_offload_stats.measure_cnt);
    fprintf(fp, "%ld,", tv.tv_sec);
    fprintf(fp, "%"PRIu64",", time_interval);
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_CREATE_PUT_OK]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_CREATE_PUT_FAIL]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_CREATE_MOD_OK]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_CREATE_MOD_FAIL]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_CREATE_RTE_OK]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_CREATE_RTE_FAIL]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_DEL_API_OK]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_DEL_API_FAIL]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_DEL_RTE_OK]); 
    fprintf(fp, "%"PRIu64",", stats_offload[OFFLOAD_DEL_RTE_FAIL]); 
    
    fprintf(fp, "%"PRIu64",", put_ok_cycle); 
    fprintf(fp, "%"PRIu64",", put_fail_cycle); 
    fprintf(fp, "%"PRIu64",", mod_ok_cycle); 
    fprintf(fp, "%"PRIu64",", mod_fail_cycle); 
    fprintf(fp, "%"PRIu64",", del_ok_cycle); 
    fprintf(fp, "%"PRIu64"\r\n", del_fail_cycle); 
    
    fclose(fp);
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_init
void
fastnic_reval_perf_stats_init(struct fastnic_revalidate_perf_stats *s)
{
    memset(s, 0, sizeof(*s));
    ovs_mutex_init(&s->stats_mutex);
    ovs_mutex_init(&s->clear_mutex);
    
    // s->start_ms = time_msec();
    gettimeofday(&s->start_t, NULL);
    s->measure_cnt = 0;
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_clear_lock
static void
fastnic_reval_perf_stats_clear_lock(struct fastnic_revalidate_perf_stats *s)
    OVS_REQUIRES(s->stats_mutex)
{
    ovs_mutex_lock(&s->clear_mutex);
    for (int i = 0; i < FASTNIC_REVALIDATE_N_STATS; i++) {
        atomic_read_relaxed(&s->counters.n[i], &s->counters.zero[i]);
    }

    // s->start_ms = time_msec();
    gettimeofday(&s->start_t, NULL);
    /* Clearing finished. */
    s->clear = false;
    ovs_mutex_unlock(&s->clear_mutex);
}

//change from lib/dpif-netdev-perf.c:pmd_perf_stats_clear
static void
fastnic_reval_perf_stats_clear(struct fastnic_revalidate_perf_stats *s)
{
    if (ovs_mutex_trylock(&s->stats_mutex) == 0) {
        /* Locking successful. PMD not polling. */
        fastnic_reval_perf_stats_clear_lock(s);
        ovs_mutex_unlock(&s->stats_mutex);
    } else {
        /* Request the polling PMD to clear the stats. There is no need to
         * block here as stats retrieval is prevented during clearing. */
        s->clear = true;
    }
}

//change from lib/dpif-netdev-perf.c:pmd_perf_start_iteration
void
fastnic_revel_perf_start_revaliteration(struct fastnic_revalidate_perf_stats *s)
OVS_REQUIRES(s->stats_mutex)
{
    if (s->clear) {
        /* Clear the PMD stats before starting next iteration. */
        fastnic_reval_perf_stats_clear_lock(s);
    }
}

void
fastnic_reval_perf_update_counters(struct fastnic_revalidate_perf_stats *s,
                                   struct fastnic_perflow_perf_stats *flow_query)
{
    if(flow_query->flow_type == OFFLOAD_FLOW){
        if(flow_query->flow_offload_query.hits_set == 1 && flow_query->flow_offload_query.bytes_set == 1){
            fastnic_reval_update_counter(s, OFFLOAD_FLOW_NUM, 1);
            fastnic_reval_update_counter(s, OFFLOAD_FLOW_PKTS, flow_query->flow_offload_query.hits);
            fastnic_reval_update_counter(s, OFFLOAD_FLOW_BYTES, flow_query->flow_offload_query.bytes);
        }else{
            fastnic_reval_update_counter(s, OFFLOAD_NONSAVE_FLOW_NUM, 1);
        }
    }else if (flow_query->flow_type == SOFTWARE_FLOW){
        fastnic_reval_update_counter(s, SOFTWARE_FLOW_NUM, 1);
    }else if (flow_query->flow_type == DUMPFAIL_FLOW){
        fastnic_reval_update_counter(s, DUMPFAIL_FLOW_NUM, 1);
    }else if (flow_query->flow_type == EMPTY_FLOW){
        fastnic_reval_update_counter(s, EMPTY_FLOW_NUM, 1);
    }
}

//change from lib/dpif-netdev-perf.c: pmd_perf_read_counters
static void
fastnic_reval_perf_read_counters(const struct fastnic_revalidate_perf_stats *s,
                       uint64_t stats[FASTNIC_REVALIDATE_N_STATS])
{
    uint64_t val;

    /* These loops subtracts reference values (.zero[*]) from the counters.
     * Since loads and stores are relaxed, it might be possible for a .zero[*]
     * value to be more recent than the current value we're reading from the
     * counter.  This is not a big problem, since these numbers are not
     * supposed to be 100% accurate, but we should at least make sure that
     * the result is not negative. */
    for (int i = 0; i < FASTNIC_REVALIDATE_N_STATS; i++) {
        atomic_read_relaxed(&s->counters.n[i], &val);
        if (val > s->counters.zero[i]) {
            stats[i] = val - s->counters.zero[i];
        } else {
            stats[i] = 0;
        }
    }
}

static void
fastnic_reval_sta(unsigned int revalidator_thread_id,
                struct fastnic_revalidate_perf_stats *perf_stats) 
{
    uint64_t stats_reval[FASTNIC_REVALIDATE_N_STATS];
    const char fastnic_reval_stats_file[100] = FASTNIC_REVAL_STATS_FILE;
    struct timeval tv;
    uint64_t time_interval;
    FILE *fp = NULL;
    
    gettimeofday(&tv, NULL);
    time_interval = (tv.tv_sec - perf_stats->start_t.tv_sec)*1000 + (tv.tv_usec -  perf_stats->start_t.tv_usec)/1000; //accurate to millisecond

    fastnic_reval_perf_read_counters(perf_stats, stats_reval);
    
    if (OVS_UNLIKELY(access(fastnic_reval_stats_file, 0) != 0)) {
        fp = fopen(fastnic_reval_stats_file, "a+");
        fprintf(fp, "measure_cnt,timestamp,interval/ms,reval_id\
                    offload_flow_num,offload_pkt_num,offload_bytes,\
                    offload_nonsta_flow_num,software_flow_num,dumpfail_flow_num,empty_flow\r\n");
    } else {
        fp = fopen(fastnic_reval_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", fastnic_reval_stats_file);
    }

    fprintf(fp, "%"PRIu64",", perf_stats->measure_cnt);
    fprintf(fp, "%ld,", tv.tv_sec);
    fprintf(fp, "%"PRIu64",", time_interval);
    fprintf(fp, "%u", revalidator_thread_id);
    fprintf(fp, "%"PRIu64",", stats_reval[OFFLOAD_FLOW_NUM]); 
    fprintf(fp, "%"PRIu64",", stats_reval[OFFLOAD_FLOW_PKTS]); 
    fprintf(fp, "%"PRIu64",", stats_reval[OFFLOAD_FLOW_BYTES]); 
    fprintf(fp, "%"PRIu64",", stats_reval[OFFLOAD_NONSAVE_FLOW_NUM]); 
    fprintf(fp, "%"PRIu64",", stats_reval[SOFTWARE_FLOW_NUM]); 
    fprintf(fp, "%"PRIu64",", stats_reval[DUMPFAIL_FLOW_NUM]); 
    fprintf(fp, "%"PRIu64"\r\n", stats_reval[EMPTY_FLOW_NUM]); 
    fclose(fp);
}

//change from lib/dpif-netdev.c:dpif_netdev_pmd_info
int
print_log(pthread_t revalidator_thread_id)
{
    /* print detection time to ovs default log,
     * the default print interval is 0.5s */
    now_time_log(revalidator_thread_id); 

    struct dp_netdev_pmd_thread **pmd_list;
    size_t n;
    bool pkt_active_flag = false;

    pmd_list = read_pmd_thread(&n);

    for (size_t i = 0; i < n; i++) {
        struct dp_netdev_pmd_thread *pmd = pmd_list[i];

        if (!pmd) {
            break;
        }
        
        if (pmd_perf_read_counter(&pmd->perf_stats,PMD_STAT_RECV) > 0){
            pkt_active_flag = true;
            /* print pmd_info */
            one_pmd_sta(pmd);
            one_pmd_show_rxq(pmd);
            // pmd_info_show_perf(&reply, pmd, (struct pmd_perf_params *)aux);
            /* print fastnic_info */
            fastnic_pmd_sta(pmd);
        }

        /* clear stats*/ //qq: use OVS api temporarily
        pmd_perf_stats_clear(&pmd->perf_stats);
        fastnic_pmd_perf_stats_clear(&pmd->fastnic_stats);
        pmd->fastnic_stats.measure_cnt++;
    }
    free(pmd_list);

    if(pkt_active_flag == true){
        fastnic_offload_sta();
    }
    fastnic_offload_perf_stats_clear(&fastnic_offload_stats);
    fastnic_offload_stats.measure_cnt++;

    return 0;
}

//every revalidate thread print its own stats
int
print_reval_log(unsigned int revalidator_id,
                struct fastnic_revalidate_perf_stats *perf_stats)
{
    fastnic_reval_sta(revalidator_id, perf_stats);
    fastnic_reval_perf_stats_clear(perf_stats);
    perf_stats->measure_cnt++;

    return 0;
}
