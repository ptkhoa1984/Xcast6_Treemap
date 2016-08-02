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

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/xcast6.h>
#ifdef LIBXCAST_RAW
#include <netinet/udp.h>
#endif
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <netdb.h>
#include <err.h>
#include "libxcast.h"
#include "libxcast_priv.h"

#define XCAST_GRPENTRY_CHUNKS		9
//#define XCAST_GRPENTRY_CHUNKS		8
#define XCAST_GRPENTRY_TO_ID(GE)	((GE) - grplist_root)
#define XCAST_GRPID_TO_ENTRY(GE)	(&grplist_root[(GE)])

#ifdef LIBXCAST_RAW
#define XCAST6_CTLDATALEN(dests)			\
	(CMSG_SPACE(sizeof(struct in6_pktinfo)) +	\
	 CMSG_SPACE(XCAST6_RTHDR_LEN(dests)) +		\
	CMSG_SPACE(XCAST6_DEST_HDRLEN(dests))+		\
	 CMSG_SPACE(sizeof(struct ip6_hdr)) +		\
	 CMSG_SPACE(sizeof(struct udphdr)))
#else
#define XCAST6_CTLDATALEN(dests)			\
	(CMSG_SPACE(sizeof(struct in6_pktinfo)) +	\
	 CMSG_SPACE(XCAST6_RTHDR_LEN(dests))+\
	 CMSG_SPACE(XCAST6_DEST_HDRLEN(dests)))

#endif

static struct xcast_grpentry *grplist_root = NULL;
static int grplist_free = -1;
static int xcast_ngroups = 0;

static struct xcast_grpentry *
XcastAllocGroupEntry(struct xcast_group *g, int issubgrp)
{
	struct xcast_grpentry *ge;
	int i;

	if (grplist_free >= 0) {
		ge = &grplist_root[grplist_free];
		grplist_free = ge->nextgrp;
	} else {
		ge = (struct xcast_grpentry *)realloc
			(grplist_root,
			 sizeof(*ge) *
			 (xcast_ngroups +
			  XCAST_GRPENTRY_CHUNKS));
		if (ge == NULL)
			return NULL;

		grplist_root = ge;
		ge = &grplist_root[xcast_ngroups];
		memset(ge, 0, sizeof(*ge) * XCAST_GRPENTRY_CHUNKS);
		grplist_free = xcast_ngroups + 1;
		xcast_ngroups += XCAST_GRPENTRY_CHUNKS;
		for (i = grplist_free; i < xcast_ngroups - 1; i++) {
			grplist_root[i].nextgrp = i + 1;
		}
		grplist_root[i].nextgrp = -1;
	}

	ge->nextgrp = -1;
	ge->issubgrp = issubgrp;
	g->grpid = XCAST_GRPENTRY_TO_ID(ge);
	ge->last = g->grpid;
	ge->grp = g;

	return ge;
}

static void
XcastFreeGroupEntry(int grpid)
{
	struct xcast_grpentry *ge;

	ge = &grplist_root[grpid];
	ge->grp = NULL;
	ge->nextgrp = grplist_free;
	grplist_free = grpid;
}

static int
Xcast6CreateGroupSubr(int flags, struct sockaddr_in6 *src,
		      unsigned short maxmbrs, int fd)
{
	struct xcast_group *g;
	struct xcast_grpentry *ge;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct in6_pktinfo *pktinfo;
	struct ip6_dest *ip6d;
	struct ip6_rthdrx *rthdr;
	xcast6_dest_t *x6d;
	socklen_t ctllen;
	int issubgrp = (fd >= 0);
	int ret;
#ifdef LIBXCAST_RAW
	struct ip6_hdr *ip6;
#endif

	if (maxmbrs > XCAST6_MAX_DESTS)
		return XCAST_FAILURE;
	else if (maxmbrs == 0)
		maxmbrs = XCAST6_MAX_DESTS;

	g = (struct xcast_group *)calloc(1, sizeof(*g) +
					 XCAST6_CTLDATALEN(maxmbrs));
	if (g == NULL)
		return XCAST_FAILURE;

	g->dummyfd = -1;
	if (issubgrp) {
		g->fd = fd;
	} else {
		/* XXX  dirty code. */
#ifdef LIBXCAST_RAW
		int proto = IPPROTO_IPV6;
# ifdef WIN32
		int hdrincl = 1;
# endif
		g->fd = socket(PF_INET6, SOCK_RAW, proto);
#else
		g->fd = socket(PF_INET6, SOCK_DGRAM, 0);
#endif
		if (g->fd < 0) {
			free(g);
			return XCAST_FAILURE;
		}
#ifdef WIN32
		ret = setsockopt(g->fd, IPPROTO_IPV6, IP_HDRINCL,
				 &hdrincl, sizeof(hdrincl));
		if (ret < 0) {
			close(g->fd);
			free(g);
			return XCAST_FAILURE;
		}
#endif
	}

	ctllen = 0;
#ifdef LIBXCAST_RAW
	ip6 = g->hdrinfo.v6.ip6 = (struct ip6_hdr *)((u_int8_t *)(g + 1));
	ip6->ip6_vfc = IPV6_VERSION;
	ip6->ip6_dst = __xcast6_all_routers;
	ip6->ip6_nxt = IPPROTO_ROUTING;
	ip6->ip6_hlim = LIBXCAST_RAW_DEFAULT_HLIM;
	g->hdrinfo.v6.uh = (struct udphdr *)(g->hdrinfo.v6.ip6 + 1);
	g->ctldata = (u_int8_t *)(g->hdrinfo.v6.uh + 1);
	g->hdrinfo.v6.sndbuf = NULL;
	g->hdrinfo.v6.sndbuf_sz = 0;
#else
	g->ctldata = (u_int8_t *)(g + 1);
#endif
	msg.msg_control = g->ctldata;
	msg.msg_controllen = XCAST6_CTLDATALEN(maxmbrs);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type = IPV6_PKTINFO;
	pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
	g->hdrinfo.v6.pktinfo = pktinfo;
	g->hdrinfo.v6.cmsg_pktinfo = cmsg;
	if (src != NULL) {
		/* XXX not tested */
		pktinfo->ipi6_ifindex = 0;
		pktinfo->ipi6_addr = src->sin6_addr;
		if (!issubgrp) {
			ret = bind(g->fd, (struct sockaddr *)src,
				   sizeof(*src));
			if (ret < 0) {
				close(g->fd);
				free(g);
				return XCAST_FAILURE;
			}
		}
	}

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	ctllen = (uintptr_t)cmsg - (uintptr_t)g->hdrinfo.v6.cmsg_pktinfo;

	cmsg->cmsg_len = CMSG_LEN(XCAST6_RTHDR_LEN(0));
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type = IPV6_RTHDR;
	g->hdrinfo.v6.cmsg_rt = cmsg;
	rthdr = (struct ip6_rthdrx *)CMSG_DATA(cmsg);
	/* ip6rx_len: length in unit of 8 octets 
	 * minus by 1 maybe is the rthr header size
	 */
	rthdr->ip6rx_len = (XCAST6_RTHDR_LEN(0) >> 3) - 1;	
	rthdr->ip6rx_type = IPV6_RTHDR_TYPE_XCAST6;
	rthdr->ip6rx_flags = flags;
	ctllen += cmsg->cmsg_len;

	g->hdrinfo.v6.rthdr = rthdr;
	g->family = AF_INET6;
	g->maxdests = maxmbrs;
	g->hdrinfo.v6.addrs = (struct in6_addr *)(rthdr + 1);

#ifdef XCAST6_DESTOPT
	cmsg = CMSG_NXTHDR(&msg, cmsg);
	g->hdrinfo.v6.cmsg_treemap = cmsg; 
	cmsg->cmsg_len = CMSG_LEN(XCAST6_DEST_HDRLEN(0));
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type = IPV6_DSTOPTS;
	ip6d = (struct ip6_dest *)CMSG_DATA(cmsg);
	ip6d->ip6d_len = (XCAST6_DEST_HDRLEN(0) >> 3) - 1;
	
	x6d = (xcast6_dest_t *)(ip6d + 1);
	x6d->x6d_type = IP6OPT_TYPE_TREEMAP;
	x6d->x6d_optdlen = 0;
	ctllen += cmsg->cmsg_len;
	cmsg = CMSG_NXTHDR(&msg, cmsg);
#endif

	g->hdrinfo.v6.ctllen = ctllen;

	ge = XcastAllocGroupEntry(g, issubgrp);
	if (ge == NULL) {
		if (!issubgrp)
			close(g->fd);
		free(g);
		return XCAST_FAILURE;
	}

	return g->grpid;
}

static void
Xcast6PadOptHdr(u_int8_t *p, int len)
{
	if (len <= 0)
		return;

	memset(p, 0, len);
	if (len == 1) {
		*p = IP6OPT_PAD1;
	} else {
		*p = IP6OPT_PADN;
		*(p + 1) = len - 2;
	}
}

int
Xcast6AddTreemap(int groupid, int ndests, u_int8_t *tree_map)
{
	struct sockaddr_in6 *sin6;
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	struct ip6_rthdrx *rthdr;
	xcast6_dest_t *x6d;
	struct ip6_dest *ip6d;
	struct cmsghdr *cmsg;
	u_int8_t *array;
	socklen_t ctllen;
	int dst_hdr_len;
	int i, j;

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->issubgrp)
		return XCAST_FAILURE;

	if (ge->last != groupid) {
		ge = XcastGetGroupEntry(ge->last);
		XCAST_ASSERT(ge != NULL);
	}

	g = ge->grp;
	ctllen = g->hdrinfo.v6.ctllen;
	cmsg = g->hdrinfo.v6.cmsg_treemap; 

	dst_hdr_len = XCAST6_DEST_HDRLEN(XCAST6_MAX_DESTS + 1);
	cmsg->cmsg_len = CMSG_LEN(dst_hdr_len);
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type = IPV6_DSTOPTS;

	ip6d = (struct ip6_dest *)CMSG_DATA(cmsg);
	ip6d->ip6d_len = (dst_hdr_len >> 3) - 1;
	x6d = (xcast6_dest_t *)(ip6d + 1);
	x6d->x6d_type = IP6OPT_TYPE_TREEMAP;
	x6d->x6d_optdlen = sizeof(u_int8_t) * (ndests + 1) / 2;

	array = (u_int8_t *)(x6d + 1);
	j = (ndests + 1) / 2;
	// each element uses 4 bits
	for (i = 0; i < ndests; i++) {
		if (tree_map[i] > 15) {
			printf("number of branches > 15 --> fail\n");
			return 0;
		}
	}	
	for (i = 0; i < j; i++) 
		array[i] = (tree_map[2*i] << 4) | tree_map[2*i + 1];	
	if (ndests % 2 == 0) array[j] = tree_map[ndests] << 4;
	ctllen += dst_hdr_len;
	//ctllen += dst_hdr_len - XCAST6_DEST_HDRLEN(ndests + 1);
	g->hdrinfo.v6.ctllen = ctllen;
}

int AddTree(int groupid, struct OverlayRoute *pOverlayRoute) {
	struct xcast_member m;
	struct sockaddr_in6 dsts[XCAST6_MAX_DESTS];
	int ndsts;
	int i;
	int ret;

	ndsts = pOverlayRoute->ndests;
	if (ndsts < 1)
		errx(1, "Need at least 1 destination.");

	if (ndsts > XCAST6_MAX_DESTS)
		errx(1, "%d: too many destinations.", ndsts);

	for (i = 0; i < ndsts; i++) {
		struct addrinfo *res, hints;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		ret = getaddrinfo(pOverlayRoute->dests[i], "11111", &hints, &res);
		if (ret != 0)
			errx(1, "%s: %s.", pOverlayRoute->dests[i], gai_strerror(ret));

		memcpy(&dsts[i], res->ai_addr, sizeof(dsts[i]));
		freeaddrinfo(res);
	}


	m.xm_dscp = 0;
	for (i = 0; i < ndsts; i++) {
		m.xm_dest = (struct sockaddr *)&dsts[i];
		ret = XcastAddMember(groupid, &m);
		if (ret != XCAST_SUCCESS)
			errx(1, "XcastAddMember() failed.");
	}
	ret = XcastGetHdrLen(groupid);
	if (ret < 0)
		errx(1, "XcastGetHdrLen() failed.");
	//Xcast6AddTreemap(groupid, pOverlayRoute->ndests, &pOverlayRoute->tree_map);
	Xcast6AddTreemap(groupid, pOverlayRoute->ndests, pOverlayRoute->tree_map);	
	return 1;
}

static int
Xcast6CreateGroup(int flags, struct sockaddr *src, unsigned short maxmbrs)
{
	return Xcast6CreateGroupSubr(flags, (struct sockaddr_in6 *)src,
				     maxmbrs, -1);
}

int
Xcast6CreateSubgroup(int groupid, struct xcast_group *lastg)
{
	struct xcast_grpentry *ge;
	struct in6_pktinfo *pisrc, *pidst;
	int subgrpid;
	int flags;

	flags = lastg->hdrinfo.v6.rthdr->ip6rx_flags;
	subgrpid = Xcast6CreateGroupSubr(flags, NULL,
					 (unsigned short)lastg->maxdests, 1);
	if (subgrpid < 0)
		return subgrpid;

	ge = XCAST_GRPID_TO_ENTRY(lastg->grpid);
	ge->nextgrp = subgrpid;
	ge = XCAST_GRPID_TO_ENTRY(groupid);
	ge->last = subgrpid;

	/*
	 * Copy source address infomation.  The source address is already
	 * defined before creating sub-group.
	 */
	pisrc = lastg->hdrinfo.v6.pktinfo;
	pidst = ge->grp->hdrinfo.v6.pktinfo;
	*pidst = *pisrc;

	return subgrpid;
}

int
XcastCreateGroup(int flags, struct sockaddr *src, unsigned short maxmbrs)
{
	int (*f)(int, struct sockaddr *, unsigned short);

	if ((flags & ~(XCAST6_F_ANON | XCAST6_F_DONTX2U)) != 0) {
		return XCAST_FAILURE;
	}

	if (src == NULL) {
		f = Xcast6CreateGroup;
	} else {
		switch (src->sa_family) {
		case AF_INET6:
			f = Xcast6CreateGroup;
			break;
		case AF_INET:
		default:
			return XCAST_FAILURE;
		}
	}

	return (*f)(flags, src, maxmbrs);
}

int
XcastDeleteGroup(int groupid)
{
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	int gid, ngid;

	if (groupid < 0 || groupid >= xcast_ngroups)
		return XCAST_FAILURE;

	ge = XCAST_GRPID_TO_ENTRY(groupid);
	g = ge->grp;
	if (g == NULL)
		return XCAST_FAILURE;
	if (ge->issubgrp)
		return XCAST_FAILURE;

	gid = ge->nextgrp;
	XcastFreeGroupEntry(groupid);
	close(g->fd);
	if (g->dummyfd >= 0)
		close(g->dummyfd);
#ifdef LIBXCAST_RAW
	switch (g->family) {
	case AF_INET6:
		if (g->hdrinfo.v6.sndbuf != NULL)
			free(g->hdrinfo.v6.sndbuf);
		break;
	}
#endif
	free(g);
	while (gid >= 0) {
		ge = XCAST_GRPID_TO_ENTRY(gid);
		g = ge->grp;
		ngid = ge->nextgrp;
		XcastFreeGroupEntry(gid);
		gid = ngid;
#ifdef LIBXCAST_RAW
		switch (g->family) {
		case AF_INET6:
			if (g->hdrinfo.v6.sndbuf != NULL)
				free(g->hdrinfo.v6.sndbuf);
			break;
		}
#endif
		free(g);
	}

	return XCAST_SUCCESS;
}

void
Xcast6DeleteSubgroup(int groupid, int subgroupid)
{
	struct xcast_grpentry *ge, *gebase;
	struct xcast_group *g;

	gebase = ge = XCAST_GRPID_TO_ENTRY(groupid);
	while (ge->nextgrp != subgroupid)
		ge = XCAST_GRPID_TO_ENTRY(ge->nextgrp);

	gebase->last = XCAST_GRPENTRY_TO_ID(ge);
	ge->nextgrp = -1;

	ge = XCAST_GRPID_TO_ENTRY(subgroupid);
	XCAST_ASSERT(ge->issubgrp);
	g = ge->grp;
	XcastFreeGroupEntry(subgroupid);
#ifdef LIBXCAST_RAW
	if (g->hdrinfo.v6.sndbuf != NULL)
		free(g->hdrinfo.v6.sndbuf);
#endif
	free(g);
}

struct xcast_grpentry *
XcastGetGroupEntry(int groupid)
{
	if (groupid < 0 || groupid >= xcast_ngroups)
		return NULL;

	return XCAST_GRPID_TO_ENTRY(groupid);
}
