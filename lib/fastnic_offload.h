#ifndef FASTNIC_OFFLOAD_H
#define FASTNIC_OFFLOAD_H 1

#include "flow.h"

#define FASTNIC_OFFLOAD 1 //only support tcp & udp now
#define OFFLOAD_META 1 //define meta_data for offloading and read it from packets

#define OFFLOAD_THRE 10 

//qq 待施工
struct offload_meta {
    uint32_t flow_size;
    uint32_t pkt_seq;
    uint64_t timestamp;
};

#endif /* FASTNIC_LOG_H */
