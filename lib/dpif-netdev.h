/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2015 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DPIF_NETDEV_H
#define DPIF_NETDEV_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "dpif.h"
#include "openvswitch/types.h"
#include "dp-packet.h"
#include "packets.h"

#include "fastnic_log.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct rxq_info
{
    const char *port;
    int queue_id;
    bool queue_state;
    double pmd_usage;
};

/* Enough headroom to add a vlan tag, plus an extra 2 bytes to allow IP
 * headers to be aligned on a 4-byte boundary.  */
enum { DP_NETDEV_HEADROOM = 2 + VLAN_HEADER_LEN };

bool dpif_is_netdev(const struct dpif *);

#ifdef FASTNIC_LOG
struct dp_netdev_pmd_thread ** read_pmd_thread(size_t *n);
size_t read_rxq_list (struct dp_netdev_pmd_thread *pmd, struct rxq_info **rxq_list);
#endif

#define NR_QUEUE   1
#define NR_PMD_THREADS 1

#ifdef  __cplusplus
}
#endif

#endif /* netdev.h */
