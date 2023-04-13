#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "fastnic_log.h"
#include "../lib/cmap.h"
#include "../lib/dpif-netdev.h"
#include "../lib/dpif-netdev-perf.h"
#include "../lib/dpif-netdev-private-thread.h"
#include "openvswitch/thread.h"
#include "openvswitch/vlog.h"
#include "openvswitch/shash.h"

#define PKT_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/pmd_pkt_stats.csv"
#define CYCLE_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/pmd_cycle_stats.csv"
#define RXQ_STATS_FILE "/home/ubuntu/software/FastNIC/lab_results/ovs_log/rxq_stats.csv"

VLOG_DEFINE_THIS_MODULE(fastnic_log);

static void now_time_log(pthread_t thread_id);
static void one_pmd_stats(struct dp_netdev_pmd_thread *pmd);
static void one_pmd_show_rxq(struct dp_netdev_pmd_thread *pmd);
static void pmd_stats_log(void);

int
print_log(pthread_t revalidator_thread_id){
    now_time_log(revalidator_thread_id);
    pmd_stats_log(); 

    return 0;
}

static void
now_time_log(pthread_t thread_id){
    struct timeval tv;
    struct tm *tm;
    char datetime[20];

    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm);
    
    VLOG_INFO("thread %ld, start dump at %s",thread_id, datetime);
}

// change from lib/dpif-netdev.c:pmd_info_show_stats
static void
one_pmd_stats(struct dp_netdev_pmd_thread *pmd) {
    uint64_t stats[PMD_N_STATS];
    uint64_t total_cycles, total_packets;
    double passes_per_pkt = 0;
    double lookups_per_hit = 0;
    double packets_per_batch = 0;
    const char pkt_stats_file[100] = PKT_STATS_FILE;
    const char cycle_stats_file[100] = CYCLE_STATS_FILE;
    struct timeval tv;
    FILE *fp = NULL;
    
    gettimeofday(&tv, NULL);

    pmd_perf_read_counters(&pmd->perf_stats, stats);
    total_cycles = stats[PMD_CYCLES_ITER_IDLE]
                         + stats[PMD_CYCLES_ITER_BUSY];
    total_packets = stats[PMD_STAT_RECV];

    if (total_packets > 0) {
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
    if (unlikely(access(pkt_stats_file, 0) != 0)) {
        fp = fopen(pkt_stats_file, "a+");
        fprintf(fp, "timestamp,dimension,numa_id,core_id,rcv_pkts,rcircu_pkts,PHWOL_pkts,MFEX_pkts,EMC_pkts,SMC_pkts,MEGA_pkts,UPCALLS_pkts,UPCALLF_pkts,avgpkt_batch,passes_per_pkt,megalookup_num\r\n");
    } else {
        fp = fopen(pkt_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", pkt_stats_file);
    }

    fprintf(fp, "%ld,", tv.tv_sec);
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
    if (unlikely(access(cycle_stats_file, 0) != 0)) {
        fp = fopen(cycle_stats_file, "a+");
        fprintf(fp, "timestamp,dimension,numa_id,core_id,rcv_pkts\r\n");
    } else {
        fp = fopen(cycle_stats_file, "a+");
    }
    if (fp == NULL) {
        VLOG_INFO("can not open file %s", cycle_stats_file);
    }

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
        FILE *fp = NULL;

        gettimeofday(&tv, NULL);
        /* stats of rxq*/
        if (unlikely(access(rxq_stats_file, 0) != 0)) {
            fp = fopen(rxq_stats_file, "a+");
            fprintf(fp, "timestamp,dimension,numa_id,core_id,isolated,port,queue_id,queue_state,pmd_usage\r\n");
        } else {
            fp = fopen(rxq_stats_file, "a+");
        }
        if (fp == NULL) {
            VLOG_INFO("can not open file %s", rxq_stats_file);
        }
        
        n_rxq = read_rxq_list (pmd, &rxq_list);

        for (int i = 0; i < n_rxq; i++) {
            fprintf(fp, "%ld,", tv.tv_sec);
            fprintf(fp, ((pmd->core_id == NON_PMD_CORE_ID) ? "all," : "pmd,"));//dimension 
            fprintf(fp, "%d,", pmd->numa_id);
            fprintf(fp, "%u,", pmd->core_id);
            fprintf(fp, "%s,", ((pmd->isolated) ? "true" : "false"));
            fprintf(fp, "%s,", rxq_list[i].port); //port
            fprintf(fp, "%d,", rxq_list[i].queue_id); //queue id
            fprintf(fp, "%s,", rxq_list[i].queue_state ? "enabled" : "disabled"); //queue state
            fprintf(fp, "%f,",rxq_list[i].pmd_usage);
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

//change from lib/dpif-netdev.c:dpif_netdev_pmd_info
static void
pmd_stats_log(void){
    struct dp_netdev_pmd_thread **pmd_list;
    size_t n;

    pmd_list = read_pmd_thread(&n);

    for (size_t i = 0; i < n; i++) {
        struct dp_netdev_pmd_thread *pmd = pmd_list[i];

        if (!pmd) {
            break;
        }

        /* show info*/
        one_pmd_stats(pmd);
        one_pmd_show_rxq(pmd);
        // pmd_info_show_perf(&reply, pmd, (struct pmd_perf_params *)aux);

        /* clear stats*/ //qq: use OVS api temporarily
        pmd_perf_stats_clear(&pmd->perf_stats);

    }
    free(pmd_list);

}

