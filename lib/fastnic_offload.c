#include <config.h>

#include "fastnic_offload.h"

#include "openvswitch/match.h"
#include "dpif-netdev-private-flow.h"
#include "dpif-netdev-private-thread.h"
#include "odp-util.h"
#include "packets.h" 
#include "flow.h"
/* Generate wildcard for flows, but not fairly sure about whether it is right.
 * Since the wildcard in ovs will perform the function to insert into dpcls, 
 * which have a complex wildcard generate rules. just use it temporarily */

static void
netflow_mask_wc(const struct flow *flow, struct flow_wildcards *wc)
{
    if (flow->dl_type != htons(ETH_TYPE_IP)) {
        return;
    }
    memset(&wc->masks.nw_proto, 0xff, sizeof wc->masks.nw_proto);
    memset(&wc->masks.nw_src, 0xff, sizeof wc->masks.nw_src);
    memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
    flow_unwildcard_tp_ports(flow, wc);
    wc->masks.nw_tos |= IP_DSCP_MASK;
}

/* Returns true if 'flow' should be submitted to tnl_port_receive(). */
static inline bool
tnl_port_should_receive(const struct flow *flow)
{
    return flow_tnl_dst_is_set(&flow->tunnel);
}

static void
tnl_wc_init(struct flow *flow, struct flow_wildcards *wc)
{
    if (tnl_port_should_receive(flow)) {
        wc->masks.tunnel.tun_id = OVS_BE64_MAX;
        if (flow->tunnel.ip_dst) {
            wc->masks.tunnel.ip_src = OVS_BE32_MAX;
            wc->masks.tunnel.ip_dst = OVS_BE32_MAX;
        } else {
            wc->masks.tunnel.ipv6_src = in6addr_exact;
            wc->masks.tunnel.ipv6_dst = in6addr_exact;
        }
        wc->masks.tunnel.flags = (FLOW_TNL_F_DONT_FRAGMENT |
                                  FLOW_TNL_F_CSUM |
                                  FLOW_TNL_F_KEY);
        wc->masks.tunnel.ip_tos = UINT8_MAX;
        wc->masks.tunnel.ip_ttl = 0;
        /* The tp_src and tp_dst members in flow_tnl are set to be always
         * wildcarded, not to unwildcard them here. */
        wc->masks.tunnel.tp_src = 0;
        wc->masks.tunnel.tp_dst = 0;

        if (is_ip_any(flow)
            && IP_ECN_is_ce(flow->tunnel.ip_tos)) {
            wc->masks.nw_tos |= IP_ECN_MASK;
        }
    }
}

/*change from xlate_wc_init(struct xlate_ctx *ctx)*/
static inline int
fastnic_wc_init(struct flow_wildcards *wc, struct flow *flow){
    flow_wildcards_init_catchall(wc);

    /* Some fields we consider to always be examined. */
    WC_MASK_FIELD(wc, packet_type);
    WC_MASK_FIELD(wc, in_port);
    WC_MASK_FIELD(wc, dl_type);
    if (is_ip_any(flow)) {
        WC_MASK_FIELD_MASK(wc, nw_frag, FLOW_NW_FRAG_MASK);
    }

    /* Always exactly match recirc_id when datapath supports
        * recirculation.  */
    WC_MASK_FIELD(wc, recirc_id);

    // if (ctx->xbridge->netflow) {
    netflow_mask_wc(flow, wc);

    tnl_wc_init(flow, wc);

    return 0;
}
/*change from xlate_wc_finish(struct xlate_ctx *ctx)*/

static void
fastnic_wc_finish(struct flow_wildcards *wc, const struct flow *flow)
{
    int i;

    /* Clear the metadata and register wildcard masks, because we won't
     * use non-header fields as part of the cache. */
    flow_wildcards_clear_non_packet_fields(wc);

    /* Wildcard Ethernet address fields if the original packet type was not
     * Ethernet.
     *
     * (The Ethertype field is used even when the original packet type is not
     * Ethernet.) */
    if (flow->packet_type != htonl(PT_ETH)) {
        wc->masks.dl_dst = eth_addr_zero;
        wc->masks.dl_src = eth_addr_zero;
    }

    /* ICMPv4 and ICMPv6 have 8-bit "type" and "code" fields.  struct flow
     * uses the low 8 bits of the 16-bit tp_src and tp_dst members to
     * represent these fields.  The datapath interface, on the other hand,
     * represents them with just 8 bits each.  This means that if the high
     * 8 bits of the masks for these fields somehow become set, then they
     * will get chopped off by a round trip through the datapath, and
     * revalidation will spot that as an inconsistency and delete the flow.
     * Avoid the problem here by making sure that only the low 8 bits of
     * either field can be unwildcarded for ICMP.
     */
    if (is_icmpv4(flow, NULL) || is_icmpv6(flow, NULL)) {
        wc->masks.tp_src &= htons(UINT8_MAX);
        wc->masks.tp_dst &= htons(UINT8_MAX);
    }
    /* VLAN_TCI CFI bit must be matched if any of the TCI is matched. */
    for (i = 0; i < FLOW_MAX_VLAN_HEADERS; i++) {
        if (wc->masks.vlans[i].tci) {
            wc->masks.vlans[i].tci |= htons(VLAN_CFI);
        }
    }

    /* The classifier might return masks that match on tp_src and tp_dst even
     * for later fragments.  This happens because there might be flows that
     * match on tp_src or tp_dst without matching on the frag bits, because
     * it is not a prerequisite for OpenFlow.  Since it is a prerequisite for
     * datapath flows and since tp_src and tp_dst are always going to be 0,
     * wildcard the fields here. */
    if (flow->nw_frag & FLOW_NW_FRAG_LATER) {
        wc->masks.tp_src = 0;
        wc->masks.tp_dst = 0;
    }

    /* Clear flow wildcard bits for fields which are not present
     * in the original packet header. These wildcards may get set
     * due to push/set_field actions. This results into frequent
     * invalidation of datapath flows by revalidator thread. */

    /* Clear mpls label wc bits if original packet is non-mpls. */
    if (!eth_type_mpls(flow->dl_type)) {
        for (i = 0; i < FLOW_MAX_MPLS_LABELS; i++) {
            wc->masks.mpls_lse[i] = 0;
        }
    }
    /* Clear vlan header wc bits if original packet does not have
     * vlan header. */
    for (i = 0; i < FLOW_MAX_VLAN_HEADERS; i++) {
        if (!eth_type_vlan(flow->vlans[i].tpid)) {
            wc->masks.vlans[i].tpid = 0;
            wc->masks.vlans[i].tci = 0;
        }
    }
}

/*part cite xlate_actions*/
int
fastnic_wc_parse(struct flow_wildcards *wc, struct flow *flow){
    // flow_wildcards_init_for_packet(wc, flow);

    wc = (wc ? wc
         : &(struct flow_wildcards) { .masks = { .dl_type = 0 } });
    fastnic_wc_init(wc, flow);
    wc->masks.tunnel.metadata.tab = flow->tunnel.metadata.tab;

    if (flow->tunnel.flags & FLOW_TNL_F_UDPIF) {
        if (wc->masks.tunnel.metadata.present.map) {
            const struct flow_tnl *upcall_tnl = &flow->tunnel;
            struct geneve_opt opts[TLV_TOT_OPT_SIZE /
                                   sizeof(struct geneve_opt)];

            tun_metadata_to_geneve_udpif_mask(&flow->tunnel,
                                              &wc->masks.tunnel,
                                              upcall_tnl->metadata.opts.gnv,
                                              upcall_tnl->metadata.present.len,
                                              opts);
             memset(&wc->masks.tunnel.metadata, 0,
                    sizeof wc->masks.tunnel.metadata);
             memcpy(&wc->masks.tunnel.metadata.opts.gnv, opts,
                    upcall_tnl->metadata.present.len);
        }
        wc->masks.tunnel.metadata.present.len = 0xff;
        wc->masks.tunnel.metadata.tab = NULL;
        wc->masks.tunnel.flags |= FLOW_TNL_F_UDPIF;
    } else if (!flow->tunnel.metadata.tab) {
        /* If we didn't have options in UDPIF format and didn't have an existing
         * metadata table, then it means that there were no options at all when
         * we started processing and any wildcards we picked up were from
         * action generation. Without options on the incoming packet, wildcards
         * aren't meaningful. To avoid them possibly getting misinterpreted,
         * just clear everything. */
        if (wc->masks.tunnel.metadata.present.map) {
            memset(&wc->masks.tunnel.metadata, 0,
                   sizeof wc->masks.tunnel.metadata);
        } else {
            wc->masks.tunnel.metadata.tab = NULL;
        }
    }
    
    fastnic_wc_finish(wc, flow);

    if (!wc->masks.vlans[0].tci) {
        wc->masks.vlans[0].tci = htons(0xffff);
    }

    return 0;
}
