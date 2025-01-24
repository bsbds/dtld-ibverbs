/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>
#include <endian.h>
#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const uint64_t SHM_START_ADDR = 0x7f7e8e600000ULL;
const size_t SHM_LENGTH_BYTES = 64 * 1024 * 1024;
const uint64_t SRC_BUFFER_OFFSET = 0;
const uint64_t DST_BUFFER_OFFSET = 65536;

int main(int argc, char *argv[])
{
	struct ibv_device **dev_list;
	struct ibv_context *context;
	struct ibv_pd *pd;
	struct ibv_mr *src_mr;
	struct ibv_mr *dst_mr;
	struct ibv_qp *qp0;
	struct ibv_qp *qp1;
	struct ibv_qp_init_attr qp_init_attr = { 0 };
	struct ibv_cq *send_cq;
	struct ibv_cq *recv_cq;

	int num_devices;
	char *src_buffer = (char *)(SHM_START_ADDR + SRC_BUFFER_OFFSET);
	char *dst_buffer = (char *)(SHM_START_ADDR + DST_BUFFER_OFFSET);

	dev_list = ibv_get_device_list(&num_devices);
	context = ibv_open_device(dev_list[0]);
	pd = ibv_alloc_pd(context);

	send_cq = ibv_create_cq(context, 512, NULL, NULL, 0);
	recv_cq = ibv_create_cq(context, 512, NULL, NULL, 0);
	if (!send_cq || !recv_cq) {
		fprintf(stderr, "Error creating CQ\n");
		return 1;
	}

	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.cap.max_send_wr = 1;
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;
	qp_init_attr.send_cq = send_cq;
	qp_init_attr.recv_cq = recv_cq;

	qp0 = ibv_create_qp(pd, &qp_init_attr);
	qp1 = ibv_create_qp(pd, &qp_init_attr);
	struct ibv_qp_attr qp_attr = { .qp_state = IBV_QPS_INIT,
				       .pkey_index = 0,
				       .port_num = 1,
				       .qp_access_flags =
					       IBV_ACCESS_LOCAL_WRITE |
					       IBV_ACCESS_REMOTE_READ |
					       IBV_ACCESS_REMOTE_WRITE };

	if (ibv_modify_qp(qp0, &qp_attr,
			  IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
				  IBV_QP_ACCESS_FLAGS)) {
		fprintf(stderr, "Failed to modify QP0 to INIT\n");
		return 1;
	}

	if (ibv_modify_qp(qp1, &qp_attr,
			  IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
				  IBV_QP_ACCESS_FLAGS)) {
		fprintf(stderr, "Failed to modify QP1 to INIT\n");
		return 1;
	}

	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.path_mtu = IBV_MTU_1024;
	qp_attr.dest_qp_num = qp1->qp_num;
	qp_attr.rq_psn = 0;
	qp_attr.ah_attr.port_num = 1;

	if (ibv_modify_qp(qp0, &qp_attr,
			  IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
				  IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
				  IBV_QP_MAX_DEST_RD_ATOMIC |
				  IBV_QP_MIN_RNR_TIMER)) {
		fprintf(stderr, "Failed to modify QP0 to RTR\n");
		return 1;
	}

	qp_attr.dest_qp_num = qp0->qp_num;
	if (ibv_modify_qp(qp1, &qp_attr,
			  IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
				  IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
				  IBV_QP_MAX_DEST_RD_ATOMIC |
				  IBV_QP_MIN_RNR_TIMER)) {
		fprintf(stderr, "Failed to modify QP1 to RTR\n");
		return 1;
	}

	/*qp_attr.qp_state = IBV_QPS_RTS;*/
	/*qp_attr.timeout = 14;*/
	/*qp_attr.retry_cnt = 7;*/
	/*qp_attr.rnr_retry = 7;*/
	/*qp_attr.sq_psn = 0;*/
	/*qp_attr.max_rd_atomic = 1;*/
	/**/
	/*if (ibv_modify_qp(qp0, &qp_attr,*/
	/*		  IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |*/
	/*			  IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |*/
	/*			  IBV_QP_MAX_QP_RD_ATOMIC)) {*/
	/*	fprintf(stderr, "Failed to modify QP0 to RTS\n");*/
	/*	return 1;*/
	/*}*/
	/**/
	/*if (ibv_modify_qp(qp1, &qp_attr,*/
	/*		  IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |*/
	/*			  IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |*/
	/*			  IBV_QP_MAX_QP_RD_ATOMIC)) {*/
	/*	fprintf(stderr, "Failed to modify QP1 to RTS\n");*/
	/*	return 1;*/
	/*}*/

	uint64_t length = 4097;
	memset(src_buffer, 'a', length);
	/*strcpy(src_buffer,*/
	/*       "Hello World!aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");*/
	/*uint64_t length = strlen(src_buffer);*/

	src_mr = ibv_reg_mr(pd, src_buffer, length,
			    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	dst_mr = ibv_reg_mr(pd, dst_buffer, length,
			    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

	struct ibv_sge sge = { .addr = (uint64_t)src_buffer,
			       .length = length,
			       .lkey = src_mr->lkey };

	struct ibv_send_wr wr = { .sg_list = &sge,
				  .num_sge = 1,
				  .opcode = IBV_WR_RDMA_WRITE,
				  .send_flags = IBV_SEND_SIGNALED,
				  .wr_id = 17,
				  .wr.rdma.remote_addr = (uint64_t)dst_buffer,
				  .wr.rdma.rkey = dst_mr->lkey };

	struct ibv_send_wr *bad_wr;
	printf("QP0 number = %u\n", qp0->qp_num);
	printf("QP1 number = %u\n", qp1->qp_num);
	ibv_post_send(qp0, &wr, &bad_wr);

	sleep(30);
	/*for (int j = 0; j < 30; j++) {*/
	/*	for (int i = 128; i < 256; i++) {*/
	/*		printf("%02x ", (unsigned char)src_buffer[i]);*/
	/*	}*/
	/*	struct ibv_wc wc;*/
	/*	printf("\n");*/
	/*	ibv_poll_cq(send_cq, 1, &wc);*/
	/*	printf("wc: %u\n", wc.qp_num);*/
	/*}*/
	struct ibv_wc wc;
	memset(&wc, 0, sizeof(wc));
	ibv_poll_cq(send_cq, 1, &wc);
	printf("wc qp: %u\n", wc.qp_num);
	printf("wc wr_id: %lu\n", wc.wr_id);
	/*printf("%s\n", dst_buffer);*/
	FILE *fp = fopen("output.txt", "w");
	if (fp != NULL) {
		fwrite(dst_buffer, 1, length, fp);
		fclose(fp);
	}

	ibv_destroy_qp(qp0);
	ibv_dereg_mr(src_mr);
	ibv_dealloc_pd(pd);
	ibv_close_device(context);
	ibv_free_device_list(dev_list);

	return 0;
}
