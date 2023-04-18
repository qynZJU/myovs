// #include <config.h>
// #include <netinet/in.h>
// #include <netinet/ip6.h>

#include "fastnic_offload.h"

// #include "dp-packet.h"
// #include "packets.h"
// #include "coverage.h"
// #include "openvswitch/vlog.h"
// #include "dpif-netdev-private.h"

// VLOG_DEFINE_THIS_MODULE(fastnic_offload);

// //qq don't know if we should change
// COVERAGE_DEFINE(miniflow_extract_ipv4_pkt_len_error);
// COVERAGE_DEFINE(miniflow_extract_ipv4_pkt_too_short);
// COVERAGE_DEFINE(miniflow_extract_ipv6_pkt_len_error);
// COVERAGE_DEFINE(miniflow_extract_ipv6_pkt_too_short);

// /* Use per thread recirc_depth to prevent recirculation loop. */
// #define MAX_RECIRC_DEPTH 6
// DEFINE_STATIC_PER_THREAD_DATA(uint32_t, recirc_depth, 0)

// /* Asserts that field 'f1' follows immediately after 'f0' in struct flow,
//  * without any intervening padding. */
// #define FASTNIC_ASSERT_SEQUENTIAL(f0, f1)                       \
//     BUILD_ASSERT_DECL(offsetof(struct flow, f0)         \
//                       + MEMBER_SIZEOF(struct flow, f0)  \
//                       == offsetof(struct flow, f1))

// /* miniflow_push_* macros allow filling in a miniflow data values in order.
//  * Assertions are needed only when the layout of the struct flow is modified.
//  * 'ofs' is a compile-time constant, which allows most of the code be optimized
//  * away.  Some GCC versions gave warnings on ALWAYS_INLINE, so these are
//  * defined as macros. */

// #if (FLOW_WC_SEQ != 42)
// #define FASTNIC_MINIFLOW_ASSERT(X) ovs_assert(X)
// BUILD_MESSAGE("FLOW_WC_SEQ changed: miniflow_extract() will have runtime "
//                "assertions enabled. Consider updating FLOW_WC_SEQ after "
//                "testing")
// #else
// #define FASTNIC_MINIFLOW_ASSERT(X)
// #endif

// #if WORDS_BIGENDIAN
// #define FASTNIC_TCP_FLAGS_BE32(tcp_ctl) ((OVS_FORCE ovs_be32)TCP_FLAGS_BE16(tcp_ctl) \
//                                  << 16)
// #else
// #define FASTNIC_TCP_FLAGS_BE32(tcp_ctl) ((OVS_FORCE ovs_be32)TCP_FLAGS_BE16(tcp_ctl))
// #endif

// /* True if 'IDX' and higher bits are not set. */
// #define FASTNIC_ASSERT_FLOWMAP_NOT_SET(FM, IDX)                                 \
// {                                                                       \
//     FASTNIC_MINIFLOW_ASSERT(!((FM)->bits[(IDX) / MAP_T_BITS] &                  \
//                       (MAP_MAX << ((IDX) % MAP_T_BITS))));              \
//     for (size_t i = (IDX) / MAP_T_BITS + 1; i < FLOWMAP_UNITS; i++) {   \
//         FASTNIC_MINIFLOW_ASSERT(!(FM)->bits[i]);                                \
//     }                                                                   \
// }

// #define fastnic_miniflow_set_map(MF, OFS)            \
//     {                                        \
//     FASTNIC_ASSERT_FLOWMAP_NOT_SET(&MF.map, (OFS));  \
//     flowmap_set(&MF.map, (OFS), 1);          \
// }

// #define fastnic_miniflow_assert_in_map(MF, OFS)              \
//     FASTNIC_MINIFLOW_ASSERT(flowmap_is_set(&MF.map, (OFS))); \
//     FASTNIC_ASSERT_FLOWMAP_NOT_SET(&MF.map, (OFS) + 1)

// #define fastnic_miniflow_set_maps(MF, OFS, N_WORDS)                     \
// {                                                               \
//     size_t ofs = (OFS);                                         \
//     size_t n_words = (N_WORDS);                                 \
//                                                                 \
//     FASTNIC_MINIFLOW_ASSERT(n_words && MF.data + n_words <= MF.end);    \
//     FASTNIC_ASSERT_FLOWMAP_NOT_SET(&MF.map, ofs);                       \
//     flowmap_set(&MF.map, ofs, n_words);                         \
// }

// /* Data at 'valuep' may be unaligned. */
// #define fastnic_miniflow_push_words_(MF, OFS, VALUEP, N_WORDS)          \
// {                                                               \
//     FASTNIC_MINIFLOW_ASSERT((OFS) % 8 == 0);                            \
//     fastnic_miniflow_set_maps(MF, (OFS) / 8, (N_WORDS));                \
//     memcpy(MF.data, (VALUEP), (N_WORDS) * sizeof *MF.data);     \
//     MF.data += (N_WORDS);                                       \
// }

// /* Push 32-bit words padded to 64-bits. */
// #define fastnic_miniflow_push_words_32_(MF, OFS, VALUEP, N_WORDS)               \
// {                                                                       \
//     fastnic_miniflow_set_maps(MF, (OFS) / 8, DIV_ROUND_UP(N_WORDS, 2));         \
//     memcpy(MF.data, (VALUEP), (N_WORDS) * sizeof(uint32_t));            \
//     MF.data += DIV_ROUND_UP(N_WORDS, 2);                                \
//     if ((N_WORDS) & 1) {                                                \
//         *((uint32_t *)MF.data - 1) = 0;                                 \
//     }                                                                   \
// }

// #define fastnic_miniflow_push_uint8_(MF, OFS, VALUE)            \
// {                                                       \
//     FASTNIC_MINIFLOW_ASSERT(MF.data < MF.end);                  \
//                                                         \
//     if ((OFS) % 8 == 0) {                               \
//         fastnic_miniflow_set_map(MF, OFS / 8);                  \
//         *(uint8_t *)MF.data = VALUE;                    \
//     } else if ((OFS) % 8 == 7) {                        \
//         fastnic_miniflow_assert_in_map(MF, OFS / 8);            \
//         *((uint8_t *)MF.data + 7) = VALUE;              \
//         MF.data++;                                      \
//     } else {                                            \
//         fastnic_miniflow_assert_in_map(MF, OFS / 8);            \
//         *((uint8_t *)MF.data + ((OFS) % 8)) = VALUE;    \
//     }                                                   \
// }

// #define fastnic_miniflow_push_uint16_(MF, OFS, VALUE)   \
// {                                               \
//     FASTNIC_MINIFLOW_ASSERT(MF.data < MF.end);          \
//                                                 \
//     if ((OFS) % 8 == 0) {                       \
//         fastnic_miniflow_set_map(MF, OFS / 8);          \
//         *(uint16_t *)MF.data = VALUE;           \
//     } else if ((OFS) % 8 == 2) {                \
//         fastnic_miniflow_assert_in_map(MF, OFS / 8);    \
//         *((uint16_t *)MF.data + 1) = VALUE;     \
//     } else if ((OFS) % 8 == 4) {                \
//         fastnic_miniflow_assert_in_map(MF, OFS / 8);    \
//         *((uint16_t *)MF.data + 2) = VALUE;     \
//     } else if ((OFS) % 8 == 6) {                \
//         fastnic_miniflow_assert_in_map(MF, OFS / 8);    \
//         *((uint16_t *)MF.data + 3) = VALUE;     \
//         MF.data++;                              \
//     }                                           \
// }

// #define fastnic_miniflow_push_uint32_(MF, OFS, VALUE)   \
//     {                                           \
//     FASTNIC_MINIFLOW_ASSERT(MF.data < MF.end);          \
//                                                 \
//     if ((OFS) % 8 == 0) {                       \
//         fastnic_miniflow_set_map(MF, OFS / 8);          \
//         *(uint32_t *)MF.data = VALUE;           \
//     } else if ((OFS) % 8 == 4) {                \
//         fastnic_miniflow_assert_in_map(MF, OFS / 8);    \
//         *((uint32_t *)MF.data + 1) = VALUE;     \
//         MF.data++;                              \
//     }                                           \
// }

// #define fastnic_miniflow_pad_to_64_(MF, OFS)                            \
// {                                                               \
//     FASTNIC_MINIFLOW_ASSERT((OFS) % 8 != 0);                            \
//     fastnic_miniflow_assert_in_map(MF, OFS / 8);                        \
//                                                                 \
//     memset((uint8_t *)MF.data + (OFS) % 8, 0, 8 - (OFS) % 8);   \
//     MF.data++;                                                  \
// }

// #define fastnic_miniflow_pad_from_64_(MF, OFS)                          \
// {                                                               \
//     FASTNIC_MINIFLOW_ASSERT(MF.data < MF.end);                          \
//                                                                 \
//     FASTNIC_MINIFLOW_ASSERT((OFS) % 8 != 0);                            \
//     fastnic_miniflow_set_map(MF, OFS / 8);                              \
//                                                                 \
//     memset((uint8_t *)MF.data, 0, (OFS) % 8);                   \
// }

// #define fastnic_miniflow_push_macs_(MF, OFS, VALUEP)                    \
// {                                                               \
//     fastnic_miniflow_set_maps(MF, (OFS) / 8, 2);                        \
//     memcpy(MF.data, (VALUEP), 2 * ETH_ADDR_LEN);                \
//     MF.data += 1;                   /* First word only. */      \
// }

// #define fastnic_miniflow_push_words(MF, FIELD, VALUEP, N_WORDS)                 \
//     fastnic_miniflow_push_words_(MF, offsetof(struct flow, FIELD), VALUEP, N_WORDS)

// #define fastnic_miniflow_push_words_32(MF, FIELD, VALUEP, N_WORDS)              \
//     fastnic_miniflow_push_words_32_(MF, offsetof(struct flow, FIELD), VALUEP, N_WORDS)

// #define fastnic_miniflow_push_be16_(MF, OFS, VALUE)                     \
//     fastnic_miniflow_push_uint16_(MF, OFS, (OVS_FORCE uint16_t)VALUE);

// #define fastnic_miniflow_push_be32_(MF, OFS, VALUE)                     \
//     fastnic_miniflow_push_uint32_(MF, OFS, (OVS_FORCE uint32_t)(VALUE))

// #define fastnic_miniflow_push_uint8(MF, FIELD, VALUE)                      \
//     fastnic_miniflow_push_uint8_(MF, offsetof(struct flow, FIELD), VALUE)

// #define fastnic_miniflow_push_uint16(MF, FIELD, VALUE)                      \
//     fastnic_miniflow_push_uint16_(MF, offsetof(struct flow, FIELD), VALUE)

// #define fastnic_miniflow_push_uint32(MF, FIELD, VALUE)                      \
//     fastnic_miniflow_push_uint32_(MF, offsetof(struct flow, FIELD), VALUE)

// #define fastnic_miniflow_push_be16(MF, FIELD, VALUE)                        \
//     fastnic_miniflow_push_be16_(MF, offsetof(struct flow, FIELD), VALUE)

// #define fastnic_miniflow_push_be32(MF, FIELD, VALUE)                        \
//     fastnic_miniflow_push_be32_(MF, offsetof(struct flow, FIELD), VALUE)

// #define fastnic_miniflow_pad_to_64(MF, FIELD)                       \
//     fastnic_miniflow_pad_to_64_(MF, OFFSETOFEND(struct flow, FIELD))

// #define fastnic_miniflow_pad_from_64(MF, FIELD)                       \
//     fastnic_miniflow_pad_from_64_(MF, offsetof(struct flow, FIELD))

// #define fastnic_miniflow_pointer(MF, FIELD)                                     \
//     (void *)((uint8_t *)MF.data + ((offsetof(struct flow, FIELD)) % 8))

// #define fastnic_miniflow_push_macs(MF, FIELD, VALUEP)                       \
//     fastnic_miniflow_push_macs_(MF, offsetof(struct flow, FIELD), VALUEP)

// /* Context for pushing data to a miniflow. */
// struct fastnic_mf_ctx {
//     struct flowmap map;
//     uint64_t *data;
//     uint64_t * const end;
// };

// struct fastnic_packet_batch_per_flow {
//     unsigned int byte_count;
//     uint16_t tcp_flags;
//     struct dp_netdev_flow *flow;

//     struct dp_packet_batch array;
// };

// /* Data structure to keep packet order till fastpath processing. */
// struct fastnic_dp_packet_flow_map {
//     struct dp_packet *packet;
//     struct dp_netdev_flow *flow;
//     uint16_t tcp_flags;
// };

// /* Removes 'size' bytes from the head end of '*datap', of size '*sizep', which
//  * must contain at least 'size' bytes of data.  Returns the first byte of data
//  * removed. */
// static inline const void *
// fastnic_data_pull(const void **datap, size_t *sizep, size_t size)
// {
//     const char *data = *datap;
//     *datap = data + size;
//     *sizep -= size;
//     return data;
// }

// /* If '*datap' has at least 'size' bytes of data, removes that many bytes from
//  * the head end of '*datap' and returns the first byte removed.  Otherwise,
//  * returns a null pointer without modifying '*datap'. */
// static inline const void *
// fastnic_data_try_pull(const void **datap, size_t *sizep, size_t size)
// {
//     return OVS_LIKELY(*sizep >= size) ? fastnic_data_pull(datap, sizep, size) : NULL;
// }

// static inline ALWAYS_INLINE ovs_be16
// fastnic_parse_ethertype(const void **datap, size_t *sizep)
// {
//     const struct llc_snap_header *llc;
//     ovs_be16 proto;

//     proto = *(ovs_be16 *) fastnic_data_pull(datap, sizep, sizeof proto);
//     if (OVS_LIKELY(ntohs(proto) >= ETH_TYPE_MIN)) {
//         return proto;
//     }

//     if (OVS_UNLIKELY(*sizep < sizeof *llc)) {
//         return htons(FLOW_DL_TYPE_NONE);
//     }

//     llc = *datap;
//     if (OVS_UNLIKELY(llc->llc.llc_dsap != LLC_DSAP_SNAP
//                      || llc->llc.llc_ssap != LLC_SSAP_SNAP
//                      || llc->llc.llc_cntl != LLC_CNTL_SNAP
//                      || memcmp(llc->snap.snap_org, SNAP_ORG_ETHERNET,
//                                sizeof llc->snap.snap_org))) {
//         return htons(FLOW_DL_TYPE_NONE);
//     }

//     fastnic_data_pull(datap, sizep, sizeof *llc);

//     if (OVS_LIKELY(ntohs(llc->snap.snap_type) >= ETH_TYPE_MIN)) {
//         return llc->snap.snap_type;
//     }

//     return htons(FLOW_DL_TYPE_NONE);
// }

// /* passed vlan_hdrs arg must be at least size FLOW_MAX_VLAN_HEADERS. */
// static inline ALWAYS_INLINE size_t
// fastnic_parse_vlan(const void **datap, size_t *sizep, union flow_vlan_hdr *vlan_hdrs)
// {
//     const ovs_be16 *eth_type;

//     fastnic_data_pull(datap, sizep, ETH_ADDR_LEN * 2);

//     eth_type = *datap;

//     size_t n;
//     for (n = 0; eth_type_vlan(*eth_type) && n < flow_vlan_limit; n++) {
//         if (OVS_UNLIKELY(*sizep < sizeof(ovs_be32) + sizeof(ovs_be16))) {
//             break;
//         }

//         memset(vlan_hdrs + n, 0, sizeof(union flow_vlan_hdr));
//         const ovs_16aligned_be32 *qp = fastnic_data_pull(datap, sizep, sizeof *qp);
//         vlan_hdrs[n].qtag = get_16aligned_be32(qp);
//         vlan_hdrs[n].tci |= htons(VLAN_CFI);
//         eth_type = *datap;
//     }
//     return n;
// }

// /* Pulls the MPLS headers at '*datap' and returns the count of them. */
// static inline int
// fastnic_parse_mpls(const void **datap, size_t *sizep)
// {
//     const struct mpls_hdr *mh;
//     int count = 0;

//     while ((mh = fastnic_data_try_pull(datap, sizep, sizeof *mh))) {
//         count++;
//         if (mh->mpls_lse.lo & htons(1 << MPLS_BOS_SHIFT)) {
//             break;
//         }
//     }
//     return MIN(count, FLOW_MAX_MPLS_LABELS);
// }

// static inline bool
// fastnic_ipv4_sanity_check(const struct ip_header *nh, size_t size,
//                   int *ip_lenp, uint16_t *tot_lenp)
// {
//     int ip_len;
//     uint16_t tot_len;

//     if (OVS_UNLIKELY(size < IP_HEADER_LEN)) {
//         COVERAGE_INC(miniflow_extract_ipv4_pkt_too_short);
//         return false;
//     }
//     ip_len = IP_IHL(nh->ip_ihl_ver) * 4;

//     if (OVS_UNLIKELY(ip_len < IP_HEADER_LEN || size < ip_len)) {
//         COVERAGE_INC(miniflow_extract_ipv4_pkt_len_error);
//         return false;
//     }

//     tot_len = ntohs(nh->ip_tot_len);
//     if (OVS_UNLIKELY(tot_len > size || ip_len > tot_len ||
//                 size - tot_len > UINT16_MAX)) {
//         COVERAGE_INC(miniflow_extract_ipv4_pkt_len_error);
//         return false;
//     }

//     *ip_lenp = ip_len;
//     *tot_lenp = tot_len;

//     return true;
// }

// static inline uint8_t
// factnic_ipv4_get_nw_frag(const struct ip_header *nh)
// {
//     uint8_t nw_frag = 0;

//     if (OVS_UNLIKELY(IP_IS_FRAGMENT(nh->ip_frag_off))) {
//         nw_frag = FLOW_NW_FRAG_ANY;
//         if (nh->ip_frag_off & htons(IP_FRAG_OFF_MASK)) {
//             nw_frag |= FLOW_NW_FRAG_LATER;
//         }
//     }

//     return nw_frag;
// }

// static inline bool
// fastnic_ipv6_sanity_check(const struct ovs_16aligned_ip6_hdr *nh, size_t size)
// {
//     uint16_t plen;

//     if (OVS_UNLIKELY(size < sizeof *nh)) {
//         COVERAGE_INC(miniflow_extract_ipv6_pkt_too_short);
//         return false;
//     }

//     plen = ntohs(nh->ip6_plen);
//     if (OVS_UNLIKELY(plen + IPV6_HEADER_LEN > size)) {
//         COVERAGE_INC(miniflow_extract_ipv6_pkt_len_error);
//         return false;
//     }

//     if (OVS_UNLIKELY(size - (plen + IPV6_HEADER_LEN) > UINT16_MAX)) {
//         COVERAGE_INC(miniflow_extract_ipv6_pkt_len_error);
//         return false;
//     }

//     return true;
// }

// static inline bool
// fastnic_parse_ipv6_ext_hdrs__(const void **datap, size_t *sizep, uint8_t *nw_proto,
//                       uint8_t *nw_frag,
//                       const struct ovs_16aligned_ip6_frag **frag_hdr)
// {
//     *frag_hdr = NULL;
//     while (1) {
//         if (OVS_LIKELY((*nw_proto != IPPROTO_HOPOPTS)
//                        && (*nw_proto != IPPROTO_ROUTING)
//                        && (*nw_proto != IPPROTO_DSTOPTS)
//                        && (*nw_proto != IPPROTO_AH)
//                        && (*nw_proto != IPPROTO_FRAGMENT))) {
//             /* It's either a terminal header (e.g., TCP, UDP) or one we
//              * don't understand.  In either case, we're done with the
//              * packet, so use it to fill in 'nw_proto'. */
//             return true;
//         }

//         /* We only verify that at least 8 bytes of the next header are
//          * available, but many of these headers are longer.  Ensure that
//          * accesses within the extension header are within those first 8
//          * bytes. All extension headers are required to be at least 8
//          * bytes. */
//         if (OVS_UNLIKELY(*sizep < 8)) {
//             return false;
//         }

//         if ((*nw_proto == IPPROTO_HOPOPTS)
//             || (*nw_proto == IPPROTO_ROUTING)
//             || (*nw_proto == IPPROTO_DSTOPTS)) {
//             /* These headers, while different, have the fields we care
//              * about in the same location and with the same
//              * interpretation. */
//             const struct ip6_ext *ext_hdr = *datap;
//             *nw_proto = ext_hdr->ip6e_nxt;
//             if (OVS_UNLIKELY(!fastnic_data_try_pull(datap, sizep,
//                                             (ext_hdr->ip6e_len + 1) * 8))) {
//                 return false;
//             }
//         } else if (*nw_proto == IPPROTO_AH) {
//             /* A standard AH definition isn't available, but the fields
//              * we care about are in the same location as the generic
//              * option header--only the header length is calculated
//              * differently. */
//             const struct ip6_ext *ext_hdr = *datap;
//             *nw_proto = ext_hdr->ip6e_nxt;
//             if (OVS_UNLIKELY(!fastnic_data_try_pull(datap, sizep,
//                                             (ext_hdr->ip6e_len + 2) * 4))) {
//                 return false;
//             }
//         } else if (*nw_proto == IPPROTO_FRAGMENT) {
//             *frag_hdr = *datap;

//             *nw_proto = (*frag_hdr)->ip6f_nxt;
//             if (!fastnic_data_try_pull(datap, sizep, sizeof **frag_hdr)) {
//                 return false;
//             }

//             /* We only process the first fragment. */
//             if ((*frag_hdr)->ip6f_offlg != htons(0)) {
//                 *nw_frag = FLOW_NW_FRAG_ANY;
//                 if (((*frag_hdr)->ip6f_offlg & IP6F_OFF_MASK) != htons(0)) {
//                     *nw_frag |= FLOW_NW_FRAG_LATER;
//                     *nw_proto = IPPROTO_FRAGMENT;
//                     return true;
//                 }
//             }
//         }
//     }
// }

// /* Returns 'true' if the packet is an ND packet. In that case the '*nd_target'
//  * and 'arp_buf[]' are filled in.  If the packet is not an ND packet, 'false'
//  * is returned and no values are filled in on '*nd_target' or 'arp_buf[]'. */
// static inline bool
// fastnic_parse_icmpv6(const void **datap, size_t *sizep,
//              const struct icmp6_data_header *icmp6,
//              ovs_be32 *rso_flags, const struct in6_addr **nd_target,
//              struct eth_addr arp_buf[2], uint8_t *opt_type)
// {
//     if (icmp6->icmp6_base.icmp6_code != 0 ||
//         (icmp6->icmp6_base.icmp6_type != ND_NEIGHBOR_SOLICIT &&
//          icmp6->icmp6_base.icmp6_type != ND_NEIGHBOR_ADVERT)) {
//         return false;
//     }

//     arp_buf[0] = eth_addr_zero;
//     arp_buf[1] = eth_addr_zero;
//     *opt_type = 0;

//     *rso_flags = get_16aligned_be32(icmp6->icmp6_data.be32);

//     *nd_target = fastnic_data_try_pull(datap, sizep, sizeof **nd_target);
//     if (OVS_UNLIKELY(!*nd_target)) {
//         return true;
//     }

//     while (*sizep >= 8) {
//         /* The minimum size of an option is 8 bytes, which also is
//          * the size of Ethernet link-layer options. */
//         const struct ovs_nd_lla_opt *lla_opt = *datap;
//         int opt_len = lla_opt->len * ND_LLA_OPT_LEN;

//         if (!opt_len || opt_len > *sizep) {
//             return true;
//         }

//         /* Store the link layer address if the appropriate option is
//          * provided.  It is considered an error if the same link
//          * layer option is specified twice. */
//         if (lla_opt->type == ND_OPT_SOURCE_LINKADDR && opt_len == 8) {
//             if (OVS_LIKELY(eth_addr_is_zero(arp_buf[0]))) {
//                 arp_buf[0] = lla_opt->mac;
//                 /* We use only first option type present in ND packet. */
//                 if (*opt_type == 0) {
//                     *opt_type = lla_opt->type;
//                 }
//             } else {
//                 goto invalid;
//             }
//         } else if (lla_opt->type == ND_OPT_TARGET_LINKADDR && opt_len == 8) {
//             if (OVS_LIKELY(eth_addr_is_zero(arp_buf[1]))) {
//                 arp_buf[1] = lla_opt->mac;
//                 /* We use only first option type present in ND packet. */
//                 if (*opt_type == 0) {
//                     *opt_type = lla_opt->type;
//                 }
//             } else {
//                 goto invalid;
//             }
//         }

//         if (OVS_UNLIKELY(!fastnic_data_try_pull(datap, sizep, opt_len))) {
//             return true;
//         }
//     }
//     return true;

// invalid:
//     *nd_target = NULL;
//     arp_buf[0] = eth_addr_zero;
//     arp_buf[1] = eth_addr_zero;
//     return true;
// }

// static void
// fastnic_dump_invalid_packet(struct dp_packet *packet, const char *reason)
// {
//     static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
//     struct ds ds = DS_EMPTY_INITIALIZER;
//     size_t size;

//     if (VLOG_DROP_DBG(&rl)) {
//         return;
//     }
//     size = dp_packet_size(packet);
//     ds_put_hex_dump(&ds, dp_packet_data(packet), size, 0, false);
//     VLOG_DBG("invalid packet for %s: port %"PRIu32", size %"PRIuSIZE"\n%s",
//              reason, packet->md.in_port.odp_port, size, ds_cstr(&ds));
//     ds_destroy(&ds);
// }

// //change from lib/flow.c: miniflow_extract
// static void
// fastnic_miniflow_extract(struct dp_packet *packet, struct miniflow *dst, struct offload_meta *of_meta)
// {
//     /* Add code to this function (or its callees) to extract new fields. */
//     BUILD_ASSERT_DECL(FLOW_WC_SEQ == 42);

//     const struct pkt_metadata *md = &packet->md;
//     const void *data = dp_packet_data(packet);
//     size_t size = dp_packet_size(packet);
//     ovs_be32 packet_type = packet->packet_type;
//     uint64_t *values = miniflow_values(dst);
//     struct fastnic_mf_ctx mf = { FLOWMAP_EMPTY_INITIALIZER, values,
//                          values + FLOW_U64S };
//     const char *frame;
//     ovs_be16 dl_type = OVS_BE16_MAX;
//     uint8_t nw_frag, nw_tos, nw_ttl, nw_proto;
//     uint8_t *ct_nw_proto_p = NULL;
//     ovs_be16 ct_tp_src = 0, ct_tp_dst = 0;

//     /* Metadata. */
//     if (flow_tnl_dst_is_set(&md->tunnel)) {
//         fastnic_miniflow_push_words(mf, tunnel, &md->tunnel,
//                             offsetof(struct flow_tnl, metadata) /
//                             sizeof(uint64_t));

//         if (!(md->tunnel.flags & FLOW_TNL_F_UDPIF)) {
//             if (md->tunnel.metadata.present.map) {
//                 fastnic_miniflow_push_words(mf, tunnel.metadata, &md->tunnel.metadata,
//                                     sizeof md->tunnel.metadata /
//                                     sizeof(uint64_t));
//             }
//         } else {
//             if (md->tunnel.metadata.present.len) {
//                 fastnic_miniflow_push_words(mf, tunnel.metadata.present,
//                                     &md->tunnel.metadata.present, 1);
//                 fastnic_miniflow_push_words(mf, tunnel.metadata.opts.gnv,
//                                     md->tunnel.metadata.opts.gnv,
//                                     DIV_ROUND_UP(md->tunnel.metadata.present.len,
//                                                  sizeof(uint64_t)));
//             }
//         }
//     }
//     if (md->skb_priority || md->pkt_mark) {
//         fastnic_miniflow_push_uint32(mf, skb_priority, md->skb_priority);
//         fastnic_miniflow_push_uint32(mf, pkt_mark, md->pkt_mark);
//     }
//     fastnic_miniflow_push_uint32(mf, dp_hash, md->dp_hash);
//     fastnic_miniflow_push_uint32(mf, in_port, odp_to_u32(md->in_port.odp_port));
//     if (md->ct_state) {
//         fastnic_miniflow_push_uint32(mf, recirc_id, md->recirc_id);
//         fastnic_miniflow_push_uint8(mf, ct_state, md->ct_state);
//         ct_nw_proto_p = fastnic_miniflow_pointer(mf, ct_nw_proto);
//         fastnic_miniflow_push_uint8(mf, ct_nw_proto, 0);
//         fastnic_miniflow_push_uint16(mf, ct_zone, md->ct_zone);
//         fastnic_miniflow_push_uint32(mf, ct_mark, md->ct_mark);
//         fastnic_miniflow_push_be32(mf, packet_type, packet_type);
//         if (!ovs_u128_is_zero(md->ct_label)) {
//             fastnic_miniflow_push_words(mf, ct_label, &md->ct_label,
//                                 sizeof md->ct_label / sizeof(uint64_t));
//         }
//     } else {
//         if (md->recirc_id) {
//             fastnic_miniflow_push_uint32(mf, recirc_id, md->recirc_id);
//             fastnic_miniflow_pad_to_64(mf, recirc_id);
//         }
//         fastnic_miniflow_pad_from_64(mf, packet_type);
//         fastnic_miniflow_push_be32(mf, packet_type, packet_type);
//     }

//     /* Initialize packet's layer pointer and offsets. */
//     frame = data;
//     dp_packet_reset_offsets(packet);

//     if (packet_type == htonl(PT_ETH)) {
//         /* Must have full Ethernet header to proceed. */
//         if (OVS_UNLIKELY(size < sizeof(struct eth_header))) {
//             goto out;
//         } else {
//             /* Link layer. */
//             FASTNIC_ASSERT_SEQUENTIAL(dl_dst, dl_src);
//             fastnic_miniflow_push_macs(mf, dl_dst, data);

//             /* VLAN */
//             union flow_vlan_hdr vlans[FLOW_MAX_VLAN_HEADERS];
//             size_t num_vlans = fastnic_parse_vlan(&data, &size, vlans);

//             dl_type = fastnic_parse_ethertype(&data, &size);
//             fastnic_miniflow_push_be16(mf, dl_type, dl_type);
//             fastnic_miniflow_pad_to_64(mf, dl_type);
//             if (num_vlans > 0) {
//                 fastnic_miniflow_push_words_32(mf, vlans, vlans, num_vlans);
//             }

//         }
//     } else {
//         /* Take dl_type from packet_type. */
//         dl_type = pt_ns_type_be(packet_type);
//         fastnic_miniflow_pad_from_64(mf, dl_type);
//         fastnic_miniflow_push_be16(mf, dl_type, dl_type);
//         /* Do not push vlan_tci, pad instead */
//         fastnic_miniflow_pad_to_64(mf, dl_type);
//     }

//     /* Parse mpls. */
//     if (OVS_UNLIKELY(eth_type_mpls(dl_type))) {
//         int count;
//         const void *mpls = data;

//         packet->l2_5_ofs = (char *)data - frame;
//         count = fastnic_parse_mpls(&data, &size);
//         fastnic_miniflow_push_words_32(mf, mpls_lse, mpls, count);
//     }

//     /* Network layer. */
//     packet->l3_ofs = (char *)data - frame;

//     nw_frag = 0;
//     if (OVS_LIKELY(dl_type == htons(ETH_TYPE_IP))) {
//         const struct ip_header *nh = data;
//         int ip_len;
//         uint16_t tot_len;

//         if (OVS_UNLIKELY(!fastnic_ipv4_sanity_check(nh, size, &ip_len, &tot_len))) {
//             if (OVS_UNLIKELY(VLOG_IS_DBG_ENABLED())) {
//                 fastnic_dump_invalid_packet(packet, "fastnic_ipv4_sanity_check");
//             }
//             goto out;
//         }
//         dp_packet_set_l2_pad_size(packet, size - tot_len);
//         size = tot_len;   /* Never pull padding. */

//         /* Push both source and destination address at once. */
//         fastnic_miniflow_push_words(mf, nw_src, &nh->ip_src, 1);
//         if (ct_nw_proto_p && !md->ct_orig_tuple_ipv6) {
//             *ct_nw_proto_p = md->ct_orig_tuple.ipv4.ipv4_proto;
//             if (*ct_nw_proto_p) {
//                 fastnic_miniflow_push_words(mf, ct_nw_src,
//                                     &md->ct_orig_tuple.ipv4.ipv4_src, 1);
//                 ct_tp_src = md->ct_orig_tuple.ipv4.src_port;
//                 ct_tp_dst = md->ct_orig_tuple.ipv4.dst_port;
//             }
//         }

//         fastnic_miniflow_push_be32(mf, ipv6_label, 0); /* Padding for IPv4. */

//         nw_tos = nh->ip_tos;
//         nw_ttl = nh->ip_ttl;
//         nw_proto = nh->ip_proto;
//         nw_frag = factnic_ipv4_get_nw_frag(nh);
//         fastnic_data_pull(&data, &size, ip_len);
//     } else if (dl_type == htons(ETH_TYPE_IPV6)) {
//         const struct ovs_16aligned_ip6_hdr *nh = data;
//         ovs_be32 tc_flow;
//         uint16_t plen;

//         if (OVS_UNLIKELY(!fastnic_ipv6_sanity_check(nh, size))) {
//             if (OVS_UNLIKELY(VLOG_IS_DBG_ENABLED())) {
//                 fastnic_dump_invalid_packet(packet, "fastnic_ipv6_sanity_check");
//             }
//             goto out;
//         }
//         fastnic_data_pull(&data, &size, sizeof *nh);

//         plen = ntohs(nh->ip6_plen);
//         dp_packet_set_l2_pad_size(packet, size - plen);
//         size = plen;   /* Never pull padding. */

//         fastnic_miniflow_push_words(mf, ipv6_src, &nh->ip6_src,
//                             sizeof nh->ip6_src / 8);
//         fastnic_miniflow_push_words(mf, ipv6_dst, &nh->ip6_dst,
//                             sizeof nh->ip6_dst / 8);
//         if (ct_nw_proto_p && md->ct_orig_tuple_ipv6) {
//             *ct_nw_proto_p = md->ct_orig_tuple.ipv6.ipv6_proto;
//             if (*ct_nw_proto_p) {
//                 fastnic_miniflow_push_words(mf, ct_ipv6_src,
//                                     &md->ct_orig_tuple.ipv6.ipv6_src,
//                                     2 *
//                                     sizeof md->ct_orig_tuple.ipv6.ipv6_src / 8);
//                 ct_tp_src = md->ct_orig_tuple.ipv6.src_port;
//                 ct_tp_dst = md->ct_orig_tuple.ipv6.dst_port;
//             }
//         }

//         tc_flow = get_16aligned_be32(&nh->ip6_flow);
//         nw_tos = ntohl(tc_flow) >> 20;
//         nw_ttl = nh->ip6_hlim;
//         nw_proto = nh->ip6_nxt;

//         const struct ovs_16aligned_ip6_frag *frag_hdr;
//         if (!fastnic_parse_ipv6_ext_hdrs__(&data, &size, &nw_proto, &nw_frag,
//                                    &frag_hdr)) {
//             goto out;
//         }

//         /* This needs to be after the fastnic_parse_ipv6_ext_hdrs__() call because it
//          * leaves the nw_frag word uninitialized. */
//         FASTNIC_ASSERT_SEQUENTIAL(ipv6_label, nw_frag);
//         ovs_be32 label = tc_flow & htonl(IPV6_LABEL_MASK);
//         fastnic_miniflow_push_be32(mf, ipv6_label, label);
//     } else {
//         if (dl_type == htons(ETH_TYPE_ARP) ||
//             dl_type == htons(ETH_TYPE_RARP)) {
//             struct eth_addr arp_buf[2];
//             const struct arp_eth_header *arp = (const struct arp_eth_header *)
//                 fastnic_data_try_pull(&data, &size, ARP_ETH_HEADER_LEN);

//             if (OVS_LIKELY(arp) && OVS_LIKELY(arp->ar_hrd == htons(1))
//                 && OVS_LIKELY(arp->ar_pro == htons(ETH_TYPE_IP))
//                 && OVS_LIKELY(arp->ar_hln == ETH_ADDR_LEN)
//                 && OVS_LIKELY(arp->ar_pln == 4)) {
//                 fastnic_miniflow_push_be32(mf, nw_src,
//                                    get_16aligned_be32(&arp->ar_spa));
//                 fastnic_miniflow_push_be32(mf, nw_dst,
//                                    get_16aligned_be32(&arp->ar_tpa));

//                 /* We only match on the lower 8 bits of the opcode. */
//                 if (OVS_LIKELY(ntohs(arp->ar_op) <= 0xff)) {
//                     fastnic_miniflow_push_be32(mf, ipv6_label, 0); /* Pad with ARP. */
//                     fastnic_miniflow_push_be32(mf, nw_frag, htonl(ntohs(arp->ar_op)));
//                 }

//                 /* Must be adjacent. */
//                 FASTNIC_ASSERT_SEQUENTIAL(arp_sha, arp_tha);

//                 arp_buf[0] = arp->ar_sha;
//                 arp_buf[1] = arp->ar_tha;
//                 fastnic_miniflow_push_macs(mf, arp_sha, arp_buf);
//                 fastnic_miniflow_pad_to_64(mf, arp_tha);
//             }
//         } else if (dl_type == htons(ETH_TYPE_NSH)) {
//             struct ovs_key_nsh nsh;

//             if (OVS_LIKELY(parse_nsh(&data, &size, &nsh))) {
//                 fastnic_miniflow_push_words(mf, nsh, &nsh,
//                                     sizeof(struct ovs_key_nsh) /
//                                     sizeof(uint64_t));
//             }
//         }
//         goto out;
//     }

//     packet->l4_ofs = (char *)data - frame;
//     fastnic_miniflow_push_be32(mf, nw_frag,
//                        bytes_to_be32(nw_frag, nw_tos, nw_ttl, nw_proto));

//     if (OVS_LIKELY(!(nw_frag & FLOW_NW_FRAG_LATER))) {
//         if (OVS_LIKELY(nw_proto == IPPROTO_TCP)) {
//             if (OVS_LIKELY(size >= TCP_HEADER_LEN)) {
//                 const struct tcp_header *tcp = data;
//                 size_t tcp_hdr_len = TCP_OFFSET(tcp->tcp_ctl) * 4;

//                 if (OVS_LIKELY(tcp_hdr_len >= TCP_HEADER_LEN)
//                     && OVS_LIKELY(size >= tcp_hdr_len)) {
//                     fastnic_miniflow_push_be32(mf, arp_tha.ea[2], 0);
//                     fastnic_miniflow_push_be32(mf, tcp_flags,
//                                        FASTNIC_TCP_FLAGS_BE32(tcp->tcp_ctl));
//                     fastnic_miniflow_push_be16(mf, tp_src, tcp->tcp_src);
//                     fastnic_miniflow_push_be16(mf, tp_dst, tcp->tcp_dst);
//                     fastnic_miniflow_push_be16(mf, ct_tp_src, ct_tp_src);
//                     fastnic_miniflow_push_be16(mf, ct_tp_dst, ct_tp_dst);
//                 }
//                 #ifdef FASTNIC_OFFLOAD
//                 fastnic_data_pull(&data, &size, sizeof *tcp);
//                 #endif
//             }
//         } else if (OVS_LIKELY(nw_proto == IPPROTO_UDP)) {
//             if (OVS_LIKELY(size >= UDP_HEADER_LEN)) {
//                 const struct udp_header *udp = data;

//                 fastnic_miniflow_push_be16(mf, tp_src, udp->udp_src);
//                 fastnic_miniflow_push_be16(mf, tp_dst, udp->udp_dst);
//                 fastnic_miniflow_push_be16(mf, ct_tp_src, ct_tp_src);
//                 fastnic_miniflow_push_be16(mf, ct_tp_dst, ct_tp_dst);
//                 #ifdef FASTNIC_OFFLOAD
//                 fastnic_data_pull(&data, &size, sizeof *udp);
//                 #endif
//             }
//         } else if (OVS_LIKELY(nw_proto == IPPROTO_SCTP)) {
//             if (OVS_LIKELY(size >= SCTP_HEADER_LEN)) {
//                 const struct sctp_header *sctp = data;

//                 fastnic_miniflow_push_be16(mf, tp_src, sctp->sctp_src);
//                 fastnic_miniflow_push_be16(mf, tp_dst, sctp->sctp_dst);
//                 fastnic_miniflow_push_be16(mf, ct_tp_src, ct_tp_src);
//                 fastnic_miniflow_push_be16(mf, ct_tp_dst, ct_tp_dst);
//             }
//         } else if (OVS_LIKELY(nw_proto == IPPROTO_ICMP)) {
//             if (OVS_LIKELY(size >= ICMP_HEADER_LEN)) {
//                 const struct icmp_header *icmp = data;

//                 fastnic_miniflow_push_be16(mf, tp_src, htons(icmp->icmp_type));
//                 fastnic_miniflow_push_be16(mf, tp_dst, htons(icmp->icmp_code));
//                 fastnic_miniflow_push_be16(mf, ct_tp_src, ct_tp_src);
//                 fastnic_miniflow_push_be16(mf, ct_tp_dst, ct_tp_dst);
//             }
//         } else if (OVS_LIKELY(nw_proto == IPPROTO_IGMP)) {
//             if (OVS_LIKELY(size >= IGMP_HEADER_LEN)) {
//                 const struct igmp_header *igmp = data;

//                 fastnic_miniflow_push_be16(mf, tp_src, htons(igmp->igmp_type));
//                 fastnic_miniflow_push_be16(mf, tp_dst, htons(igmp->igmp_code));
//                 fastnic_miniflow_push_be16(mf, ct_tp_src, ct_tp_src);
//                 fastnic_miniflow_push_be16(mf, ct_tp_dst, ct_tp_dst);
//                 fastnic_miniflow_push_be32(mf, igmp_group_ip4,
//                                    get_16aligned_be32(&igmp->group));
//                 fastnic_miniflow_pad_to_64(mf, igmp_group_ip4);
//             }
//         } else if (OVS_LIKELY(nw_proto == IPPROTO_ICMPV6)) {
//             if (OVS_LIKELY(size >= sizeof(struct icmp6_data_header))) {
//                 const struct in6_addr *nd_target;
//                 struct eth_addr arp_buf[2];
//                 /* This will populate whether we received Option 1
//                  * or Option 2. */
//                 uint8_t opt_type;
//                 /* This holds the ND Reserved field. */
//                 ovs_be32 rso_flags;
//                 const struct icmp6_data_header *icmp6;

//                 icmp6 = fastnic_data_pull(&data, &size, sizeof *icmp6);
//                 if (fastnic_parse_icmpv6(&data, &size, icmp6,
//                                  &rso_flags, &nd_target, arp_buf, &opt_type)) {
//                     if (nd_target) {
//                         fastnic_miniflow_push_words(mf, nd_target, nd_target,
//                                             sizeof *nd_target / sizeof(uint64_t));
//                     }
//                     fastnic_miniflow_push_macs(mf, arp_sha, arp_buf);
//                     /* Populate options field and set the padding
//                      * accordingly. */
//                     if (opt_type != 0) {
//                         fastnic_miniflow_push_be16(mf, tcp_flags, htons(opt_type));
//                         /* Pad to align with 64 bits.
//                          * This will zero out the pad3 field. */
//                         fastnic_miniflow_pad_to_64(mf, tcp_flags);
//                     } else {
//                         /* Pad to align with 64 bits.
//                          * This will zero out the tcp_flags & pad3 field. */
//                         fastnic_miniflow_pad_to_64(mf, arp_tha);
//                     }
//                     fastnic_miniflow_push_be16(mf, tp_src,
//                                        htons(icmp6->icmp6_base.icmp6_type));
//                     fastnic_miniflow_push_be16(mf, tp_dst,
//                                        htons(icmp6->icmp6_base.icmp6_code));
//                     fastnic_miniflow_pad_to_64(mf, tp_dst);
//                     /* Fill ND reserved field. */
//                     fastnic_miniflow_push_be32(mf, igmp_group_ip4, rso_flags);
//                     fastnic_miniflow_pad_to_64(mf, igmp_group_ip4);
//                 } else {
//                     /* ICMPv6 but not ND. */
//                     fastnic_miniflow_push_be16(mf, tp_src,
//                                        htons(icmp6->icmp6_base.icmp6_type));
//                     fastnic_miniflow_push_be16(mf, tp_dst,
//                                        htons(icmp6->icmp6_base.icmp6_code));
//                     fastnic_miniflow_push_be16(mf, ct_tp_src, ct_tp_src);
//                     fastnic_miniflow_push_be16(mf, ct_tp_dst, ct_tp_dst);
//                 }
//             }
//         }
//     }

//     #ifdef FASTNIC_OFFLOAD
//     if (nw_proto == IPPROTO_TCP || nw_proto == IPPROTO_UDP){
//         const struct offload_meta *ofm = data;
//         of_meta->test = ofm->test;   
//     }
//     #endif
//  out:
//     dst->map = mf.map;
// }

// /* SMC lookup function for a batch of packets.
//  * By doing batching SMC lookup, we can use prefetch
//  * to hide memory access latency.
//  */
// static inline void
// fastnic_smc_lookup_batch(struct dp_netdev_pmd_thread *pmd,
//             struct netdev_flow_key *keys,
//             struct netdev_flow_key **missed_keys,
//             struct dp_packet_batch *packets_,
//             const int cnt,
//             struct dp_packet_flow_map *flow_map,
//             uint8_t *index_map)
// {
//     int i;
//     struct dp_packet *packet;
//     size_t n_smc_hit = 0, n_missed = 0;
//     struct dfc_cache *cache = &pmd->flow_cache;
//     struct smc_cache *smc_cache = &cache->smc_cache;
//     const struct cmap_node *flow_node;
//     int recv_idx;
//     uint16_t tcp_flags;

//     /* Prefetch buckets for all packets */
//     for (i = 0; i < cnt; i++) {
//         OVS_PREFETCH(&smc_cache->buckets[keys[i].hash & SMC_MASK]);
//     }

//     DP_PACKET_BATCH_REFILL_FOR_EACH (i, cnt, packet, packets_) {
//         struct dp_netdev_flow *flow = NULL;
//         flow_node = smc_entry_get(pmd, keys[i].hash);
//         bool hit = false;
//         /* Get the original order of this packet in received batch. */
//         recv_idx = index_map[i];

//         if (OVS_LIKELY(flow_node != NULL)) {
//             CMAP_NODE_FOR_EACH (flow, node, flow_node) {
//                 /* Since we dont have per-port megaflow to check the port
//                  * number, we need to  verify that the input ports match. */
//                 if (OVS_LIKELY(dpcls_rule_matches_key(&flow->cr, &keys[i]) &&
//                 flow->flow.in_port.odp_port == packet->md.in_port.odp_port)) {
//                     tcp_flags = miniflow_get_tcp_flags(&keys[i].mf);

//                     /* SMC hit and emc miss, we insert into EMC */
//                     keys[i].len =
//                         netdev_flow_key_size(miniflow_n_values(&keys[i].mf));
//                     emc_probabilistic_insert(pmd, &keys[i], flow);
//                     /* Add these packets into the flow map in the same order
//                      * as received.
//                      */
//                     packet_enqueue_to_flow_map(packet, flow, tcp_flags,
//                                                flow_map, recv_idx);
//                     n_smc_hit++;
//                     hit = true;
//                     break;
//                 }
//             }
//             if (hit) {
//                 continue;
//             }
//         }

//         /* SMC missed. Group missed packets together at
//          * the beginning of the 'packets' array. */
//         dp_packet_batch_refill(packets_, packet, i);

//         /* Preserve the order of packet for flow batching. */
//         index_map[n_missed] = recv_idx;

//         /* Put missed keys to the pointer arrays return to the caller */
//         missed_keys[n_missed++] = &keys[i];
//     }

//     pmd_perf_update_counter(&pmd->perf_stats, PMD_STAT_SMC_HIT, n_smc_hit);
// }

// /* Try to process all ('cnt') the 'packets' using only the datapath flow cache
//  * 'pmd->flow_cache'. If a flow is not found for a packet 'packets[i]', the
//  * miniflow is copied into 'keys' and the packet pointer is moved at the
//  * beginning of the 'packets' array. The pointers of missed keys are put in the
//  * missed_keys pointer array for future processing.
//  *
//  * The function returns the number of packets that needs to be processed in the
//  * 'packets' array (they have been moved to the beginning of the vector).
//  *
//  * For performance reasons a caller may choose not to initialize the metadata
//  * in 'packets_'.  If 'md_is_valid' is false, the metadata in 'packets'
//  * is not valid and must be initialized by this function using 'port_no'.
//  * If 'md_is_valid' is true, the metadata is already valid and 'port_no'
//  * will be ignored.
//  */
// //change from lib/dpif-netdev.c:dfc_processing
// static inline size_t
// dfc_processing(struct dp_netdev_pmd_thread *pmd,
//                struct dp_packet_batch *packets_,
//                struct netdev_flow_key *keys,
//                struct netdev_flow_key **missed_keys,
//                struct fastnic_packet_batch_per_flow batches[], size_t *n_batches,
//                struct fastnic_dp_packet_flow_map *flow_map,
//                size_t *n_flows, uint8_t *index_map,
//                bool md_is_valid, odp_port_t port_no)
// {
//     struct netdev_flow_key *key = &keys[0];
//     size_t n_missed = 0, n_emc_hit = 0, n_phwol_hit = 0,  n_mfex_opt_hit = 0;
//     struct dfc_cache *cache = &pmd->flow_cache;
//     struct dp_packet *packet;
//     const size_t cnt = dp_packet_batch_size(packets_);
//     uint32_t cur_min = pmd->ctx.emc_insert_min;
//     const uint32_t recirc_depth = *recirc_depth_get();
//     const bool netdev_flow_api = netdev_is_flow_api_enabled();
//     int i;
//     uint16_t tcp_flags;
//     size_t map_cnt = 0;
//     bool batch_enable = true;

//     pmd_perf_update_counter(&pmd->perf_stats,
//                             md_is_valid ? PMD_STAT_RECIRC : PMD_STAT_RECV,
//                             cnt);

//     DP_PACKET_BATCH_REFILL_FOR_EACH (i, cnt, packet, packets_) {
//         struct dp_netdev_flow *flow;

//         if (OVS_UNLIKELY(dp_packet_size(packet) < ETH_HEADER_LEN)) {
//             dp_packet_delete(packet);
//             COVERAGE_INC(datapath_drop_rx_invalid_packet);
//             continue;
//         }

//         if (i != cnt - 1) {
//             struct dp_packet **packets = packets_->packets;
//             /* Prefetch next packet data and metadata. */
//             OVS_PREFETCH(dp_packet_data(packets[i+1]));
//             pkt_metadata_prefetch_init(&packets[i+1]->md);
//         }

//         if (!md_is_valid) {
//             pkt_metadata_init(&packet->md, port_no);
//         }

//         if (netdev_flow_api && recirc_depth == 0) {
//             if (OVS_UNLIKELY(dp_netdev_hw_flow(pmd, port_no, packet, &flow))) {
//                 /* Packet restoration failed and it was dropped, do not
//                  * continue processing.
//                  */
//                 continue;
//             }
//             if (OVS_LIKELY(flow)) {
//                 tcp_flags = parse_tcp_flags(packet);
//                 n_phwol_hit++;
//                 if (OVS_LIKELY(batch_enable)) {
//                     dp_netdev_queue_batches(packet, flow, tcp_flags, batches,
//                                             n_batches);
//                 } else {
//                     /* Flow batching should be performed only after fast-path
//                      * processing is also completed for packets with emc miss
//                      * or else it will result in reordering of packets with
//                      * same datapath flows. */
//                     packet_enqueue_to_flow_map(packet, flow, tcp_flags,
//                                                flow_map, map_cnt++);
//                 }
//                 continue;
//             }
//         }
        
//         #ifdef FASTNIC_OFFLOAD
//         struct offload_meta of_meta;
//         fastnic_miniflow_extract(packet, &key->mf, &of_meta);
//         #endif
//         key->len = 0; /* Not computed yet. */
//         key->hash =
//                 (md_is_valid == false)
//                 ? dpif_netdev_packet_get_rss_hash_orig_pkt(packet, &key->mf)
//                 : dpif_netdev_packet_get_rss_hash(packet, &key->mf);

//         /* If EMC is disabled skip emc_lookup */
//         flow = (cur_min != 0) ? emc_lookup(&cache->emc_cache, key) : NULL;
//         if (OVS_LIKELY(flow)) {
//             tcp_flags = miniflow_get_tcp_flags(&key->mf);
//             n_emc_hit++;
//             if (OVS_LIKELY(batch_enable)) {
//                 dp_netdev_queue_batches(packet, flow, tcp_flags, batches,
//                                         n_batches);
//             } else {
//                 /* Flow batching should be performed only after fast-path
//                  * processing is also completed for packets with emc miss
//                  * or else it will result in reordering of packets with
//                  * same datapath flows. */
//                 packet_enqueue_to_flow_map(packet, flow, tcp_flags,
//                                            flow_map, map_cnt++);
//             }
//         } else {
//             /* Exact match cache missed. Group missed packets together at
//              * the beginning of the 'packets' array. */
//             dp_packet_batch_refill(packets_, packet, i);

//             /* Preserve the order of packet for flow batching. */
//             index_map[n_missed] = map_cnt;
//             flow_map[map_cnt++].flow = NULL;

//             /* 'key[n_missed]' contains the key of the current packet and it
//              * will be passed to SMC lookup. The next key should be extracted
//              * to 'keys[n_missed + 1]'.
//              * We also maintain a pointer array to keys missed both SMC and EMC
//              * which will be returned to the caller for future processing. */
//             missed_keys[n_missed] = key;
//             key = &keys[++n_missed];

//             /* Skip batching for subsequent packets to avoid reordering. */
//             batch_enable = false;
//         }
//     }
//     /* Count of packets which are not flow batched. */
//     *n_flows = map_cnt;

//     pmd_perf_update_counter(&pmd->perf_stats, PMD_STAT_PHWOL_HIT, n_phwol_hit);
//     pmd_perf_update_counter(&pmd->perf_stats, PMD_STAT_MFEX_OPT_HIT,
//                             n_mfex_opt_hit);
//     pmd_perf_update_counter(&pmd->perf_stats, PMD_STAT_EXACT_HIT, n_emc_hit);

//     if (!pmd->ctx.smc_enable_db) {
//         return dp_packet_batch_size(packets_);
//     }

//     /* Packets miss EMC will do a batch lookup in SMC if enabled */
//     fastnic_smc_lookup_batch(pmd, keys, missed_keys, packets_,
//                      n_missed, flow_map, index_map);

//     return dp_packet_batch_size(packets_);
// }