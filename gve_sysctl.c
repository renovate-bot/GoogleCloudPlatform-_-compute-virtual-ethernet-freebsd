/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gve.h"

static void
gve_setup_rxq_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_rx_ring *rxq)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *list;
	struct gve_rxq_stats *stats;
	char namebuf[16];

	snprintf(namebuf, sizeof(namebuf), "rxq%d", rxq->com.id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Receive Queue");
	list = SYSCTL_CHILDREN(node);

	stats = &rxq->stats;

	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_bytes", CTLFLAG_RD,
	    &stats->rbytes, "Bytes received");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_packets", CTLFLAG_RD,
	    &stats->rpackets, "Packets received");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_copybreak_cnt",
	    CTLFLAG_RD, &stats->rx_copybreak_cnt,
	    "Total frags with mbufs allocated for copybreak");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_frag_flip_cnt",
	    CTLFLAG_RD, &stats->rx_frag_flip_cnt,
	    "Total frags that allocated mbuf with page flip");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_frag_copy_cnt",
	    CTLFLAG_RD, &stats->rx_frag_copy_cnt,
	    "Total frags with mbuf that copied payload into mbuf");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_dropped_pkt",
	    CTLFLAG_RD, &stats->rx_dropped_pkt,
	    "Total rx packets dropped");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_dropped_pkt_desc_err", CTLFLAG_RD,
	    &stats->rx_dropped_pkt_desc_err,
	    "Packets dropped due to descriptor error");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_dropped_pkt_mbuf_alloc_fail", CTLFLAG_RD,
	    &stats->rx_dropped_pkt_mbuf_alloc_fail,
	    "Packets dropped due to failed mbuf allocation");
	SYSCTL_ADD_U32(ctx, list, OID_AUTO,
	    "num_desc_posted", CTLFLAG_RD,
	    &rxq->fill_cnt, rxq->fill_cnt,
	    "Toal number of descriptors posted");
}

static void
gve_setup_txq_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_tx_ring *txq)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *tx_list;
	struct gve_txq_stats *stats;
	char namebuf[16];

	snprintf(namebuf, sizeof(namebuf), "txq%d", txq->com.id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Transmit Queue");
	tx_list = SYSCTL_CHILDREN(node);

	stats = &txq->stats;

	SYSCTL_ADD_U32(ctx, tx_list, OID_AUTO,
	    "tx_posted_desc", CTLFLAG_RD,
	    &txq->req, 0, "Number of descriptors posted by NIC");
	SYSCTL_ADD_U32(ctx, tx_list, OID_AUTO,
	    "tx_completed_desc", CTLFLAG_RD,
	    &txq->done, 0, "Number of descriptors completed");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_packets", CTLFLAG_RD,
	    &stats->tpackets, "Packets transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_tso_packets", CTLFLAG_RD,
	    &stats->tso_packet_cnt, "TSO Packets transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_bytes", CTLFLAG_RD,
	    &stats->tbytes, "Bytes transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_dropped_pkt_nospace_device", CTLFLAG_RD,
	    &stats->tx_dropped_pkt_nospace_device,
	    "Packets dropped due to no space in device");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_dropped_pkt_nospace_bufring", CTLFLAG_RD,
	    &stats->tx_dropped_pkt_nospace_bufring,
	    "Packets dropped due to no space in br ring");
}

static void
gve_setup_queue_stat_sysctl(struct sysctl_ctx_list *ctx, struct sysctl_oid_list *child,
    struct gve_priv *priv)
{
	int i;

	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		gve_setup_rxq_sysctl(ctx, child, &priv->rx[i]);
	}
	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		gve_setup_txq_sysctl(ctx, child, &priv->tx[i]);
	}
}

static void
gve_setup_adminq_stat_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_priv *priv)
{
	struct sysctl_oid *admin_node;
	struct sysctl_oid_list *admin_list;

	/* Admin queue stats */
	admin_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "adminq_stats",
				     CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Admin Queue statistics");
	admin_list = SYSCTL_CHILDREN(admin_node);

	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_prod_cnt", CTLFLAG_RD,
	    &priv->adminq_prod_cnt, 0, "Adminq Commands issued");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_cmd_fail", CTLFLAG_RD,
	    &priv->adminq_cmd_fail, 0, "Aqminq Failed commands");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_timeouts", CTLFLAG_RD,
	    &priv->adminq_timeouts, 0, "Adminq Timedout commands");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_describe_device_cnt",
	    CTLFLAG_RD, &priv->adminq_describe_device_cnt, 0,
	    "adminq_describe_device_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_cfg_device_resources_cnt", CTLFLAG_RD,
	    &priv->adminq_cfg_device_resources_cnt, 0,
	    "adminq_cfg_device_resources_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_register_page_list_cnt", CTLFLAG_RD,
	    &priv->adminq_register_page_list_cnt, 0,
	    "adminq_register_page_list_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_unregister_page_list_cnt", CTLFLAG_RD,
	    &priv->adminq_unregister_page_list_cnt, 0,
	    "adminq_unregister_page_list_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_create_tx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_create_tx_queue_cnt, 0,
	    "adminq_create_tx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_create_rx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_create_rx_queue_cnt, 0,
	    "adminq_create_rx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_destroy_tx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_destroy_tx_queue_cnt, 0,
	    "adminq_destroy_tx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_destroy_rx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_destroy_rx_queue_cnt, 0,
	    "adminq_destroy_rx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_dcfg_device_resources_cnt", CTLFLAG_RD,
	    &priv->adminq_dcfg_device_resources_cnt, 0,
	    "adminq_dcfg_device_resources_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_set_driver_parameter_cnt", CTLFLAG_RD,
	    &priv->adminq_set_driver_parameter_cnt, 0,
	    "adminq_set_driver_parameter_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_verify_driver_compatibility_cnt", CTLFLAG_RD,
	    &priv->adminq_verify_driver_compatibility_cnt, 0,
	    "adminq_verify_driver_compatibility_cnt");
}

void gve_setup_sysctl(struct gve_priv *priv)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = priv->dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	gve_setup_queue_stat_sysctl(ctx, child, priv);
	gve_setup_adminq_stat_sysctl(ctx, child, priv);
}

void
gve_accum_stats(struct gve_priv *priv, uint64_t *rpackets,
    uint64_t *rbytes, uint64_t *rx_dropped_pkt, uint64_t *tpackets,
    uint64_t *tbytes, uint64_t *tx_dropped_pkt)
{
	struct gve_rxq_stats *rxqstats;
	struct gve_txq_stats *txqstats;
	int i;

	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		rxqstats = &priv->rx[i].stats;
		*rpackets += counter_u64_fetch(rxqstats->rpackets);
		*rbytes += counter_u64_fetch(rxqstats->rbytes);
		*rx_dropped_pkt += counter_u64_fetch(rxqstats->rx_dropped_pkt);
	}

	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		txqstats = &priv->tx[i].stats;
		*tpackets += counter_u64_fetch(txqstats->tpackets);
		*tbytes += counter_u64_fetch(txqstats->tbytes);
		*tx_dropped_pkt += counter_u64_fetch(txqstats->tx_dropped_pkt);
	}
}
