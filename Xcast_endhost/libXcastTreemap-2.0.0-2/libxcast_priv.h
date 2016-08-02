/*
 * Copyright (C) 2001 FUJITSU LABORATRIES LTD.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LIBXCAST_PRIV_H_
#define _LIBXCAST_PRIV_H_

#ifdef WIN32
#ifndef LIBXCAST_RAW
#error "Must define LIBXCAST_RAW. Only raw socket version on Win32."
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tpipv6.h>
#include <windows.h>
#include <io.h>
#include "winglue.h"
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "netinet6/xcast6.h"
#ifdef LIBXCAST_RAW
#include <netinet/udp.h>
#endif
#endif
#include <assert.h>

#ifdef linux
#include "usagiglue.h"
#endif

#define XCAST_ASSERT(x)		assert(x)

#ifdef LIBXCAST_RAW
#define LIBXCAST_RAW_DEFAULT_HLIM	1
#endif

#ifndef XCAST6_MAX_DESTS
#define XCAST6_MAX_DESTS	64
#endif

#ifndef IP6_ADDR_MAX_LEN
#define IP6_ADDR_MAX_LEN	32
#endif

struct OverlayRoute {
	int ndests;
	char dests[XCAST6_MAX_DESTS][IP6_ADDR_MAX_LEN];
	u_int8_t tree_map[XCAST6_MAX_DESTS];
};

struct xcast_group {
	int grpid;
	int family;
	int ndests;	/* members of this group. */
	int maxdests;
	u_int16_t port;
	int fd;
	int dummyfd;	/* dummy socket for the source address and port. */

	u_int8_t *ctldata;

	union {
		struct {
			struct sockaddr_in src;
			int dummy;
		} v4;
		struct {
			socklen_t ctllen;
			struct cmsghdr *cmsg_pktinfo;
			struct cmsghdr *cmsg_rt;
			struct cmsghdr *cmsg_treemap;
			struct in6_pktinfo *pktinfo;
			struct ip6_rthdrx *rthdr;
			struct in6_addr *addrs;
#ifdef LIBXCAST_RAW
#ifdef WIN32
			struct sockaddr_storage dst;
			socklen_t dstlen;
#endif
			struct ip6_hdr *ip6;
			struct udphdr *uh;
			u_int8_t *sndbuf;
			size_t sndbuf_sz;
#endif
		} v6;
	} hdrinfo;

	u_int64_t _pad;
};

struct xcast_grpentry {
	int nextgrp;
	int issubgrp;
	int last;
	struct xcast_group *grp;
};

#define xcast_bit_set(bm, pos) \
	((bm)[(pos) >> 3] |= (1 << ((pos) & 7)))

#define xcast_bit_clear(bm, pos) \
	((bm)[(pos) >> 3] &= ~(1 << ((pos) & 7)))

#define xcast_bit_test(bm, pos) \
	((bm)[(pos) >> 3] & (1 << ((pos) & 7)))

extern struct in6_addr __xcast6_all_routers;

extern struct xcast_grpentry *XcastGetGroupEntry(int groupid);
extern int Xcast6CreateSubgroup(int groupid, struct xcast_group *lastg);
extern void Xcast6DeleteSubgroup(int groupid, int subgroupid);

#ifdef LIBXCAST_RAW
extern u_int16_t XcastCksum(u_int8_t *buf, size_t len);
#endif

#endif /* _LIBXCAST_PRIV_H_ */
