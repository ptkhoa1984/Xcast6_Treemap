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
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#endif
#include "libxcast.h"
#include "libxcast_priv.h"

struct in6_addr __xcast6_all_routers = IN6ADDR_ALLXCASTNODES_INIT;

#ifdef WIN32
static int
Xcast6SendVec(int fd,
	      struct iovec *iov,
	      int iovcnt,
	      struct sockaddr *sin,
	      socklen_t sinlen,
	      u_int8_t **buf,
	      size_t *buflen)
{
	int i;
	size_t l;
	u_int8_t *p;

	for (i = 0, l = 0; i < iovcnt; i++)
		l += iov[i].iov_len;

	if (l <= *buflen) {
		p = *buf;
	} else {
		p = (u_int8_t *)realloc(*buf, l);
		if (p == NULL)
			return -1;
		*buf = p;
		*buflen = l;
	}

	for (i = 0; i < iovcnt; i++) {
		memcpy(p, iov[i].iov_base, iov[i].iov_len);
		p += iov[i].iov_len;
	}
	
	return sendto(fd, *buf, l, 0, sin, sinlen);

}

#endif

#ifdef LIBXCAST_RAW
static int
Xcast6Send(int groupid, char *datap, int datalen)
{
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	struct msghdr msg;
	struct sockaddr_in6 sin6;
	struct ip6_ext *ip6e;
	struct iovec iov[5];	/* (IP6|IP6|RTHDR|UDP|payload) */
	struct ip6_hdr ip6;

	int success;
	int fd;
	int ret;
	int i, rthidx, dstidx;
	u_int32_t sum;
	u_int16_t plen;
	struct {
		u_int32_t	ph_len;
		u_int8_t	ph_zero[3];
		u_int8_t	ph_nxt;
		struct in6_addr	ph_src;
		struct in6_addr	ph_dst;
	} ph;

	ge = XcastGetGroupEntry(groupid);
	g = ge->grp;
	if (g->ndests == 0)
		return XCAST_SUCCESS;

	memset(&msg, 0, sizeof(msg));
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
	sin6.sin6_len = sizeof(sin6);
#endif

	i = 0;
	plen = 0;

	/* External IP6 header */
	iov[i].iov_base = (char *)&ip6;
	iov[i].iov_len = sizeof(ip6);
	memset(&ip6, 0, sizeof(ip6));
	ip6.ip6_vfc = IPV6_VERSION;
	ip6.ip6_flow |= htonl(XCAST6_FLOWINFO);
	ip6.ip6_nxt = IPPROTO_IPV6;
	ip6.ip6_hlim = g->hdrinfo.v6.ip6->ip6_hlim;
	ip6.ip6_src = g->hdrinfo.v6.ip6->ip6_src;
	i++;

	/* IP6 Header */
	iov[i].iov_base = (char *)g->hdrinfo.v6.ip6;
	iov[i].iov_len = sizeof(*g->hdrinfo.v6.ip6);
	i++;

	/* Routing Header */
	/*
	 * The length of the routing header may vary on subgroups.
	 * The payload length should be calculated later.
	 */
	rthidx = i;
	i++;

	/* UDP Header */
	iov[i].iov_base = (char *)g->hdrinfo.v6.uh;
	iov[i].iov_len = sizeof(*g->hdrinfo.v6.uh);
	plen += iov[i].iov_len;
	g->hdrinfo.v6.uh->uh_ulen
		= htons((u_short)(datalen + sizeof(struct udphdr)));
	g->hdrinfo.v6.uh->uh_sum = 0;
	sum = ~XcastCksum((u_int8_t *)g->hdrinfo.v6.uh,
			  sizeof(*g->hdrinfo.v6.uh)) & 0xffff;
	i++;

	/* payload */
	iov[i].iov_base = datap;
	iov[i].iov_len = datalen;
	plen += iov[i].iov_len;
	sum += ~XcastCksum((u_int8_t *)datap, datalen) & 0xffff;
	i++;

	msg.msg_name = &sin6;
	msg.msg_namelen = sizeof(sin6);
	msg.msg_control = g->hdrinfo.v6.cmsg_pktinfo;
	msg.msg_controllen = g->hdrinfo.v6.cmsg_pktinfo->cmsg_len;
	msg.msg_iov = iov;
	msg.msg_iovlen = i;
	msg.msg_flags = 0;

	/* pseudo header (for UDP checksum) */
	memset(&ph, 0, sizeof(ph));
	ph.ph_src = g->hdrinfo.v6.ip6->ip6_src;
	ph.ph_dst = __xcast6_all_routers;
	ph.ph_nxt = IPPROTO_UDP;
	ph.ph_len = htonl(sizeof(struct udphdr) + datalen);
	sum += ~XcastCksum((u_int8_t *)&ph, sizeof(ph)) & 0xffff;
	sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
	if (sum > 65535)
		sum -= 65535;
	sum = (~sum) & 0xffff;
	if (sum == 0)
		sum = 0xffff;
	
	success = 0;
	fd = g->fd;
	while (ge != NULL) {
		u_int16_t len;

		g = ge->grp;

		len = plen;
		iov[rthidx].iov_base = (char *)g->hdrinfo.v6.rthdr;
		iov[rthidx].iov_len = (g->hdrinfo.v6.rthdr->x6r_len + 1) << 3;
		len += iov[rthidx].iov_len;

		if (dstidx < 0) {
			g->hdrinfo.v6.rthdr->x6r_nxt = IPPROTO_UDP;
		} else {
			g->hdrinfo.v6.rthdr->x6r_nxt = IPPROTO_DSTOPTS;
			ip6e = (struct ip6_ext *)
				CMSG_DATA(g->hdrinfo.v6.cmsg_treemap);
			ip6e->ip6e_nxt = IPPROTO_UDP;
			iov[dstidx].iov_base = (char *)ip6e;
			iov[dstidx].iov_len = (ip6e->ip6e_len + 1) << 3;
			len += iov[dstidx].iov_len;
		}

		g->hdrinfo.v6.ip6->ip6_plen = htons(len);
		sin6.sin6_addr = g->hdrinfo.v6.addrs[0];

		/* UDP Checksum */
		g->hdrinfo.v6.uh->uh_sum = (u_int16_t)sum;
		ip6.ip6_dst = g->hdrinfo.v6.addrs[0];
		ip6.ip6_plen = htons(len + sizeof(struct ip6_hdr) +
				     ((ip6e->ip6e_len + 1) << 3));
#ifdef WIN32
		ret = Xcast6SendVec(fd,
				    msg.msg_iov,
				    msg.msg_iovlen,
				    (struct sockaddr *)&g->hdrinfo.v6.dst,
				    g->hdrinfo.v6.dstlen,
				    &g->hdrinfo.v6.sndbuf,
				    &g->hdrinfo.v6.sndbuf_sz);
#else
		ret = sendmsg(fd, &msg, 0);
#endif
		if (ret >= 0)
			success++;

		ge = XcastGetGroupEntry(ge->nextgrp);
	}

	if (success == 0)
		return XCAST_FAILURE;

	return XCAST_SUCCESS;
}

#else /* LIBXCAST_RAW */

static int
Xcast6Send(int groupid, char *datap, int datalen)
{
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	struct msghdr msg;
	struct sockaddr_in6 sin6;
	struct iovec iov;
	int success;
	int fd;
	int ret;

	ge = XcastGetGroupEntry(groupid);
	g = ge->grp;
	if (g->ndests == 0)
		return XCAST_SUCCESS;

	memset(&msg, 0, sizeof(msg));
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = __xcast6_all_routers;
	sin6.sin6_port = g->port;
#ifdef SIN6_LEN
	sin6.sin6_len = sizeof(sin6);
#endif
	iov.iov_base = datap;
	iov.iov_len = datalen;
	msg.msg_name = &sin6;
	msg.msg_namelen = sizeof(sin6);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;

	success = 0;
	fd = g->fd;
	while (ge != NULL) {
		g = ge->grp;
		msg.msg_control = g->ctldata;
		msg.msg_controllen = g->hdrinfo.v6.ctllen;
		ret = sendmsg(fd, &msg, 0);
		if (ret >= 0)
			success++;

		ge = XcastGetGroupEntry(ge->nextgrp);
	}
	if (success == 0)
		return XCAST_FAILURE;

	return XCAST_SUCCESS;
}
#endif /* LIBXCAST_RAW */

int
XcastSend(int groupid, char *datap, int datalen)
{
	struct xcast_grpentry *ge;
	int (*f)(int, char *, int);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL)
		return XCAST_FAILURE;

	switch (ge->grp->family) {
	case PF_INET6:
		f = Xcast6Send;
		break;
	case PF_INET:
	default:
		return XCAST_FAILURE;
	}

	return (*f)(groupid, datap, datalen);
}

int
XcastTreemapSend(int groupid, char *datap, int datalen)
{
	struct xcast_grpentry *ge;
	int (*f)(int, char *, int);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL)
		return XCAST_FAILURE;

	switch (ge->grp->family) {
	case PF_INET6:
		f = Xcast6Send;
		break;
	case PF_INET:
	default:
		return XCAST_FAILURE;
	}
	return (*f)(groupid, datap, datalen);
}

#ifdef WIN32
int
XcastSendMsg(int groupid, struct msghdr *msg, int flags)
{
	/* XcastSendMsg unsupported. */
	return XCAST_FAILURE;
}
#else
int
XcastSendMsg(int groupid, struct msghdr *msg, int flags)
{
	struct xcast_grpentry *ge;
	int ret;

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL)
		return XCAST_FAILURE;

	ret = sendmsg(ge->grp->fd, msg, flags);
	if (ret < 0)
		return XCAST_FAILURE;

	return XCAST_SUCCESS;
}
#endif
