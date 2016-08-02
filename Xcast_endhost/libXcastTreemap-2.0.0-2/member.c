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
#include <errno.h>
#include <unistd.h>
#include <string.h>
#endif
#include "libxcast.h"
#include "libxcast_priv.h"

#ifdef WIN32
#define DISCARD_PORT		9	/* XXX should use getaddrinfo() */

/* IN6_IS_ADDR_{UNSPECIFIED,LOOPBACK} are broken in TPIPV6-20001205... */
#define IN6_IS_ADDR_UNSPECIFIED_MACRO(a)	\
	((*(u_int32_t *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[8]) == 0) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[12]) == 0))

#define IN6_IS_ADDR_LOOPBACK_MACRO(a)		\
	((*(u_int32_t *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[8]) == 0) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[12]) == ntohl(1)))

#define IN6_IS_ADDR_UNSPECIFIED(a)	IN6_IS_ADDR_UNSPECIFIED_MACRO(a)
#define IN6_IS_ADDR_LOOPBACK(a)		IN6_IS_ADDR_LOOPBACK_MACRO(a)

/*
 * This check is for preventing from picking up a default(?) 6to4 interface 
 * address on Windows 2000/XP. Nonstandard and probably expensive.
 */
#define IN6_IS_ADDR_DEF6TO4(a)			\
	((*(u_int32_t *)(&(a)->s6_addr[0]) == ntohl(0x20020400)) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[4]) == ntohl(0x01000000)) &&	\
	 (*(u_int32_t *)(&(a)->s6_addr[8]) == 0) &&			\
	 (*(u_int32_t *)(&(a)->s6_addr[12]) == ntohl(0x04000100)))

static int
Xcast6GetSrc(struct xcast_group *g, struct sockaddr_in6 *dst)
{
	int fd;
	int ret;
	int i, found;
	struct sockaddr_storage src, dstn;
	struct sockaddr_in6 dstd, *sin6;
	socklen_t len;
	SOCKET_ADDRESS_LIST *psal;
	DWORD alen;

	fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0)
		return fd;

	dstd = *dst;
	dstd.sin6_port = htons(DISCARD_PORT);
	ret = Xcast6NormalizeAddr(&dstd, &dstn, &len);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	ret = connect(fd, (struct sockaddr *)&dstn, len);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	len = sizeof(src);
	ret = getsockname(fd, (struct sockaddr *)&src, &len);
	if (ret < 0) {
		close(fd);
		return ret;
	}

#define SIN6_CAST(S)	((struct sockaddr_in6 *)(S))

	g->hdrinfo.v6.uh->uh_sport = SIN6_CAST(&src)->sin6_port;


	/* We can't get the source IPv6 address with getsockname(). Sigh. */
	ret = WSAIoctl(fd, SIO_ADDRESS_LIST_QUERY, NULL, 0,
		       NULL, 0, &alen, NULL, NULL);
	if (! (ret == SOCKET_ERROR && WSAGetLastError() == WSAEFAULT)) {
		close(fd);
		return ret;
	}

	psal = (SOCKET_ADDRESS_LIST *)malloc(alen);
	if (psal == NULL) {
		close(fd);
		return -1;
	}

	ret = WSAIoctl(fd, SIO_ADDRESS_LIST_QUERY, NULL, 0,
		       (LPVOID)psal, alen, &alen, NULL, NULL);
	if (ret == SOCKET_ERROR) {
		free(psal);
		close(fd);
		return ret;
	}

	for (i = 0, found = -1; i < psal->iAddressCount; i++) {
		sin6 = SIN6_CAST(psal->Address[i].lpSockaddr);
		if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
			continue;
		else if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			continue;
		else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
			continue;
		else if (IN6_IS_ADDR_DEF6TO4(&sin6->sin6_addr))
			continue;
		else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)) {
			/* Not the best. Should look for global. */
			found = i;
			continue;
		} else {
			/* Best match (but maybe sometimes wrong...) */
			found = i;
			break;
		}
	}

	if (found < 0) {
		free(psal);
		close(fd);
		return -1;
	}
	
	sin6 = SIN6_CAST(psal->Address[found].lpSockaddr);
	g->hdrinfo.v6.ip6->ip6_src = sin6->sin6_addr;
	free(psal);

#undef SIN6_CAST

	if (g->hdrinfo.v6.cmsg_treemap == NULL)
		g->hdrinfo.v6.uh->uh_dport = g->port;
	else
		g->hdrinfo.v6.uh->uh_dport = 0;

	g->hdrinfo.v6.uh->uh_sum = 0;
	g->dummyfd = fd;

	return 0;
}

#undef IN6_IS_ADDR_UNSPECIFIED
#undef IN6_IS_ADDR_LOOPBACK

#else /* WIN32 */

static int
Xcast6GetSrc(struct xcast_group *g, struct sockaddr_in6 *dst)
{
	int fd;
	int ret;
	struct sockaddr_in6 src;
	socklen_t len;

	fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0)
		return fd;

	ret = connect(fd, (struct sockaddr *)dst, sizeof(*dst));
	if (ret < 0) {
		close(fd);
		return ret;
	}

	len = sizeof(src);
	ret = getsockname(fd, (struct sockaddr *)&src, &len);
	if (ret < 0) {
		close(fd);
		return ret;
	}
	g->dummyfd = fd;

	g->hdrinfo.v6.pktinfo->ipi6_addr = src.sin6_addr;
	g->hdrinfo.v6.pktinfo->ipi6_ifindex = 0;

#ifdef LIBXCAST_RAW
	g->hdrinfo.v6.ip6->ip6_src = src.sin6_addr;
	g->hdrinfo.v6.uh->uh_sport = src.sin6_port;

	if (g->hdrinfo.v6.cmsg_treemap == NULL)
		g->hdrinfo.v6.uh->uh_dport = g->port;
	else
		g->hdrinfo.v6.uh->uh_dport = 0;

	g->hdrinfo.v6.uh->uh_sum = 0;
#endif

	return 0;
}
#endif /* WIN32 */

static struct xcast_grpentry *
Xcast6FindMember(int groupid, struct xcast_member *member, int *offset)
{
	struct sockaddr_in6 *sin6;
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	struct in6_addr *addrs;
	int i;

	sin6 = (struct sockaddr_in6 *)member->xm_dest;
	ge = XcastGetGroupEntry(groupid);
	while (ge != NULL) {
		g = ge->grp;
		addrs = g->hdrinfo.v6.addrs;

		for (i = 0; i < g->ndests; i++) {
			if (memcmp(&addrs[i], &sin6->sin6_addr,
				   sizeof(sin6->sin6_addr)) != 0)
				continue;

			/* found */
			*offset = i;
			return ge;
		}
		ge = XcastGetGroupEntry(ge->nextgrp);
	}

	return NULL;
}

#if defined(LIBXCAST_RAW) && defined(WIN32)
/*
 * We can't pass "struct sockaddr_in6" as itself on Windows...
 * Windows requests larger data than sockaddr_in6.  There seems hidden
 * information after sockaddr_in6...
 */
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN	46
#endif

static int
Xcast6NormalizeAddr(struct sockaddr_in6 *src,
		    struct sockaddr_storage *dst,
		    socklen_t *lenp)
{
	struct addrinfo hints, *res;
	char host[INET6_ADDRSTRLEN], port[8];

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	/* Windows does not seem to have inet_ntop... */
	sprintf(host,
		"%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
		"%02x%02x:%02x%02x:%02x%02x:%02x%02x",
		src->sin6_addr.s6_addr[0x0], src->sin6_addr.s6_addr[0x1],
		src->sin6_addr.s6_addr[0x2], src->sin6_addr.s6_addr[0x3],
		src->sin6_addr.s6_addr[0x4], src->sin6_addr.s6_addr[0x5],
		src->sin6_addr.s6_addr[0x6], src->sin6_addr.s6_addr[0x7],
		src->sin6_addr.s6_addr[0x8], src->sin6_addr.s6_addr[0x9],
		src->sin6_addr.s6_addr[0xa], src->sin6_addr.s6_addr[0xb],
		src->sin6_addr.s6_addr[0xc], src->sin6_addr.s6_addr[0xd],
		src->sin6_addr.s6_addr[0xe], src->sin6_addr.s6_addr[0xf]);
	sprintf(port, "%d", ntohs(src->sin6_port));
	if (getaddrinfo(host, port, &hints, &res) != 0)
		return -1;

	memcpy(dst, res->ai_addr, res->ai_addrlen);
	*lenp = res->ai_addrlen;
	freeaddrinfo(res);

	return 0;
}
#endif /* defined(LIBXCAST_RAW) && defined(WIN32) */

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

static int
Xcast6AddMember(int groupid, struct xcast_member *member)
{
	struct sockaddr_in6 *sin6;
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	struct ip6_rthdrx *rthdr;
	struct in6_addr *addrs;
	xcast6_dest_t *x6d;
	struct ip6_dest *ip6d;
	struct cmsghdr *cmsg;
	u_int16_t *ports;
	socklen_t ctllen;
	int rthlen, dsthdrlen;
	int sgid;
#ifdef WIN32
	struct sockaddr_in6 dst;
#endif

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->issubgrp)
		return XCAST_FAILURE;

	if (ge->last != groupid) {
		ge = XcastGetGroupEntry(ge->last);
		XCAST_ASSERT(ge != NULL);
	}
	g = ge->grp;

	sin6 = (struct sockaddr_in6 *)member->xm_dest;
	if (sin6->sin6_port == 0)
		return XCAST_FAILURE;

	if (g->port == 0)
		g->port = sin6->sin6_port;
	else if (sin6->sin6_port != g->port)
		return XCAST_FAILURE;

	if (g->dummyfd < 0 &&
	    IN6_IS_ADDR_UNSPECIFIED(&g->hdrinfo.v6.pktinfo->ipi6_addr)) {
		if (Xcast6GetSrc(g, sin6) < 0)
			return XCAST_FAILURE;
	}

	if (g->ndests + 1 > g->maxdests) {
		sgid = Xcast6CreateSubgroup(groupid, g);
		if (sgid < 0)
			return XCAST_FAILURE;
		ge = XcastGetGroupEntry(sgid);
		g = ge->grp;
	}

	ctllen = g->hdrinfo.v6.ctllen;
	addrs = g->hdrinfo.v6.addrs;

#ifdef XCAST6_DESTOPT
	cmsg = g->hdrinfo.v6.cmsg_treemap;
	if (cmsg != NULL) { 
		cmsg = g->hdrinfo.v6.cmsg_treemap = (struct cmsghdr *)((u_int8_t *)cmsg + (sizeof(*addrs)));
/*		cmsg->cmsg_len = CMSG_LEN(dsthdrlen);
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_DSTOPTS;

		ip6d = (struct ip6_dest *)CMSG_DATA(cmsg);
		ip6d->ip6d_len = (dsthdrlen >> 3) - 1;
		x6d = (xcast6_dest_t *)(ip6d + 1);
		x6d->x6d_type = IP6OPT_TYPE_XCAST6;
		x6d->x6d_optdlen = sizeof(u_int8_t) * (g->ndests + 1);

		dsthdrlen = XCAST6_DEST_HDRLEN(g->ndests + 1);
		ctllen += dsthdrlen - XCAST6_DEST_HDRLEN(g->ndests);/**/
	}
#endif
/**/
	rthdr = g->hdrinfo.v6.rthdr;
	rthlen = (rthdr->ip6rx_len + 1) << 3;

	xcast_bit_set(g->hdrinfo.v6.rthdr->ip6rx_dmap, g->ndests);
	addrs[g->ndests] = sin6->sin6_addr;
	ctllen += sizeof(*addrs);
	rthlen += sizeof(*addrs);

	rthdr->ip6rx_ndest = g->ndests + 1;
	rthdr->ip6rx_len = (rthlen >> 3) - 1;
	g->hdrinfo.v6.cmsg_rt->cmsg_len = CMSG_LEN(rthlen);
	g->hdrinfo.v6.ctllen = ctllen;
	g->ndests++;
	return XCAST_SUCCESS;
}

int
XcastAddMember(int groupid, struct xcast_member *member)
{
	struct xcast_grpentry *ge;
	int (*f)(int, struct xcast_member *);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->grp == NULL)
		return XCAST_FAILURE;

	if (ge->grp->family != member->xm_dest->sa_family)
		return XCAST_FAILURE;

	switch (member->xm_dest->sa_family) {
	case AF_INET6:
		f = Xcast6AddMember;
		break;
	case AF_INET:
	default:
		return XCAST_FAILURE;
	}

	return (*f)(groupid, member);
}

static int
Xcast6DeleteMember(int groupid, struct xcast_member *member)
{
	struct xcast_group *g, *ng;
	struct xcast_grpentry *ge, *nge;
	struct sockaddr_in6 *sin6;
	struct in6_addr *addrs;
	struct ip6_rthdrx *rthdr;
	socklen_t ctllen;
	u_int8_t b;
	int victim, bitoff, byteoff;
	int ngid;
	int i;
	
	sin6 = (struct sockaddr_in6 *)member->xm_dest;

	ge = Xcast6FindMember(groupid, member, &victim);
	if (ge == NULL)
		return XCAST_FAILURE;

	g = ge->grp;
	if (g->ndests == 1 && ge->issubgrp) {
		/* no members in this sub-group. */
		Xcast6DeleteSubgroup(groupid, g->grpid);
		return XCAST_SUCCESS;
	}

	rthdr = g->hdrinfo.v6.rthdr;
	/* the group that has the victim. */
	bitoff = victim & 7;
	byteoff = victim / 8;
	if (bitoff != 0) {
		b = rthdr->ip6rx_dmap[byteoff];
		b = ((b & ~((1 << (bitoff + 1)) - 1)) >> 1) |
			(b & ((1 << bitoff) - 1));
		rthdr->ip6rx_dmap[byteoff] = b;
	}
		
	for (i = byteoff + 1; i < (g->ndests + 7) / 8; i++) {
		if ((rthdr->ip6rx_dmap[i] & 1) == 0)
			rthdr->ip6rx_dmap[i - 1] &= ~0x80;
		else
			rthdr->ip6rx_dmap[i - 1] |= 0x80;
		rthdr->ip6rx_dmap[i] >>= 1;
	}

	if (g->ndests - victim - 1 > 0) {
		memmove(&g->hdrinfo.v6.addrs[victim],
			&g->hdrinfo.v6.addrs[victim + 1],
			sizeof(struct in6_addr) * (g->ndests - victim - 1));
	}

	ngid = ge->nextgrp;
	while (ngid >= 0) {
		nge = XcastGetGroupEntry(ngid);
		ng = nge->grp;

		/* bitmap */
		if ((ng->hdrinfo.v6.rthdr->ip6rx_dmap[0] & 1) == 0)
			xcast_bit_clear(g->hdrinfo.v6.rthdr->ip6rx_dmap,
					g->maxdests - 1);
		else
			xcast_bit_set(g->hdrinfo.v6.rthdr->ip6rx_dmap,
				      g->maxdests - 1);

		ng->hdrinfo.v6.rthdr->ip6rx_dmap[0] >>= 1;
		for (i = 1; i < (ng->ndests + 7) / 8; i++) {
			if ((ng->hdrinfo.v6.rthdr->ip6rx_dmap[i] & 1) == 0)
				ng->hdrinfo.v6.rthdr->ip6rx_dmap[i - 1] &= ~0x80;
			else
				ng->hdrinfo.v6.rthdr->ip6rx_dmap[i - 1] |= 0x80;
			ng->hdrinfo.v6.rthdr->ip6rx_dmap[i] >>= 1;
		}

		/* address list */
		g->hdrinfo.v6.addrs[g->ndests - 1] =
			ng->hdrinfo.v6.addrs[0];

		memmove(&ng->hdrinfo.v6.addrs[0],
			&ng->hdrinfo.v6.addrs[1],
			sizeof(struct in6_addr) * (ng->ndests - 1));

		g = ng;
		ge = nge;
		ngid = nge->nextgrp;
	}

	if (g->ndests == 1 && ge->issubgrp) {
		Xcast6DeleteSubgroup(groupid, g->grpid);
		return XCAST_SUCCESS;
	}

	ctllen = g->hdrinfo.v6.ctllen;
	addrs = g->hdrinfo.v6.addrs;
	rthdr = g->hdrinfo.v6.rthdr;
	xcast_bit_clear(g->hdrinfo.v6.rthdr->ip6rx_dmap, g->ndests - 1);

	g->ndests--;
	ctllen -= sizeof(struct in6_addr);
	rthdr->ip6rx_len -= sizeof(struct in6_addr) >> 3;
	rthdr->ip6rx_ndest = g->ndests;
	g->hdrinfo.v6.cmsg_rt->cmsg_len -= sizeof(struct in6_addr);
	g->hdrinfo.v6.ctllen = ctllen;

	return XCAST_SUCCESS;
}

int
XcastDeleteMember(int groupid, struct xcast_member *member)
{
	struct xcast_grpentry *ge;
	int (*f)(int, struct xcast_member *);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->grp == NULL)
		return XCAST_FAILURE;

	if (ge->grp->family != member->xm_dest->sa_family)
		return XCAST_FAILURE;

	switch (member->xm_dest->sa_family) {
	case AF_INET6:
		f = Xcast6DeleteMember;
		break;
	case AF_INET:
	default:
		return XCAST_FAILURE;
	}

	return (*f)(groupid, member);
}

static int
Xcast6DisableMember(int groupid, struct xcast_member *member)
{
	struct xcast_group *g;
	struct xcast_grpentry *ge;
	int target;
	
	ge = Xcast6FindMember(groupid, member, &target);
	if (ge == NULL)
		return XCAST_FAILURE;

	g = ge->grp;
	xcast_bit_clear(g->hdrinfo.v6.rthdr->ip6rx_dmap, target);

	return XCAST_SUCCESS;
}

int
XcastDisableMember(int groupid, struct xcast_member *member)
{
	struct xcast_grpentry *ge;
	int (*f)(int, struct xcast_member *);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->grp == NULL)
		return XCAST_FAILURE;

	if (ge->grp->family != member->xm_dest->sa_family)
		return XCAST_FAILURE;

	switch (member->xm_dest->sa_family) {
	case AF_INET6:
		f = Xcast6DisableMember;
		break;
	case AF_INET:
	default:
		return XCAST_FAILURE;
	}

	return (*f)(groupid, member);
}

static int
Xcast6EnableMember(int groupid, struct xcast_member *member)
{
	struct xcast_group *g;
	struct xcast_grpentry *ge;
	int target;
	
	ge = Xcast6FindMember(groupid, member, &target);
	if (ge == NULL)
		return XCAST_FAILURE;

	g = ge->grp;
	xcast_bit_set(g->hdrinfo.v6.rthdr->ip6rx_dmap, target);

	return XCAST_SUCCESS;
}

int
XcastEnableMember(int groupid, struct xcast_member *member)
{
	struct xcast_grpentry *ge;
	int (*f)(int, struct xcast_member *);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->grp == NULL)
		return XCAST_FAILURE;

	if (ge->grp->family != member->xm_dest->sa_family)
		return XCAST_FAILURE;

	switch (member->xm_dest->sa_family) {
	case AF_INET6:
		f = Xcast6EnableMember;
		break;
	case AF_INET:
	default:
		return XCAST_FAILURE;
	}

	return (*f)(groupid, member);
}

