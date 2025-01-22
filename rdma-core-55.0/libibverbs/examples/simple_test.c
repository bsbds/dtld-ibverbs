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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const uint64_t SHM_START_ADDR = 0x7f7e8e600000ULL;
const size_t SHM_LENGTH_BYTES = 64 * 1024 * 1024;

int main(int argc, char *argv[])
{
	struct ibv_device **dev_list;
	struct ibv_context *context;
	struct ibv_pd *pd;
	struct ibv_mr *mr;
	struct ibv_qp *qp;
	struct ibv_qp_init_attr qp_init_attr = { 0 };
	int num_devices;
	char *buffer = (char *)SHM_START_ADDR + 128;
	uint32_t length = 128;

	dev_list = ibv_get_device_list(&num_devices);
	context = ibv_open_device(dev_list[0]);
	pd = ibv_alloc_pd(context);

	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.cap.max_send_wr = 1;
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;

	qp = ibv_create_qp(pd, &qp_init_attr);

	strcpy(buffer, "Hello RDMA");
	mr = ibv_reg_mr(pd, buffer, 4096,
			IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	/*uint64_t t = 0xbfff000 + SHM_START_ADDR;*/
	/*printf("%lx", *(uint64_t *)t);*/
	struct ibv_sge sge = { .addr = (uint64_t)buffer,
			       .length = length,
			       .lkey = mr->lkey };

	struct ibv_send_wr wr = {
		.sg_list = &sge,
		.num_sge = 1,
		.opcode = IBV_WR_RDMA_WRITE,
		.wr.rdma.remote_addr = 0, // Set remote address
		.wr.rdma.rkey = 0 // Set remote key
	};

	struct ibv_send_wr *bad_wr;
	ibv_post_send(qp, &wr, &bad_wr);

	sleep(1);
	uint64_t addr = 0x7f7e9a5fc000;
	for (int i = 0; i < 32; i++) {
		printf("%02x ", *((unsigned char *)(addr + i)));
		if ((i + 1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
	addr += 32;
	for (int i = 0; i < 32; i++) {
		printf("%02x ", *((unsigned char *)(addr + i)));
		if ((i + 1) % 16 == 0)
			printf("\n");
	}
	printf("\n");

	sleep(10);

	ibv_destroy_qp(qp);
	ibv_dereg_mr(mr);
	ibv_dealloc_pd(pd);
	ibv_close_device(context);
	ibv_free_device_list(dev_list);

	return 0;
}
