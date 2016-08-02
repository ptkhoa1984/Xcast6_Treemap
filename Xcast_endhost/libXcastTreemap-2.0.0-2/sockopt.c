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
#include <unistd.h>
#endif
#include "libxcast.h"
#include "libxcast_priv.h"

int
XcastSetSockOpt(int groupid, int optlev, int optname, void *optval,
		size_t optlen)
{
	struct xcast_grpentry *ge;
	struct xcast_group *g;
#ifdef LIBXCAST_RAW
	int hlim;
#endif

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL)
		return XCAST_FAILURE;
	g = ge->grp;

#ifdef LIBXCAST_RAW
	switch (optlev) {
	case IPPROTO_IPV6:
		if (optname == IPV6_MULTICAST_HOPS) {
			if (optlen == sizeof(u_int8_t))
				hlim = *(u_int8_t *)optval;
			else if (optlen == sizeof(int))
				hlim = *(int *)optval;
			else
				return XCAST_FAILURE;
			g->hdrinfo.v6.ip6->ip6_hlim = hlim;

			/*
			 * Override optname with IPV6_UNICAST_HOPS.
			 * Semi-permeable tunneling packets have unicast
			 * destination addresses.
			 */
			optname = IPV6_UNICAST_HOPS;
		}
		break;
	}
#endif
	return setsockopt(g->fd, optlev, optname, optval, optlen);
}

int
XcastGetSockOpt(int groupid, int optlev, int optname, void *optval,
		size_t *optlen)
{
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	socklen_t len;
	int ret;

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL)
		return XCAST_FAILURE;
	g = ge->grp;

#ifdef LIBXCAST_RAW
	switch (optlev) {
	case IPPROTO_IPV6:
		if (optname == IPV6_MULTICAST_HOPS)
			optname = IPV6_UNICAST_HOPS;
		break;
	}
#endif
	ret = getsockopt(g->fd, optlev, optname, optval, &len);
	if (ret == 0)
		*optlen = len;

	return ret;
}
