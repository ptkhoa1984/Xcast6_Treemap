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
#include <unistd.h>
#endif
#include "libxcast.h"
#include "libxcast_priv.h"

static int
Xcast6GetHdrLen(int groupid)
{
	struct xcast_grpentry *ge;
	struct xcast_group *g;
	int hdrlen;

	ge = XcastGetGroupEntry(groupid);
	g = ge->grp;
	
	hdrlen = (g->hdrinfo.v6.rthdr->ip6rx_len + 1) << 3;
	hdrlen += sizeof(struct ip6_hdr);

	return hdrlen;
}

int
XcastGetHdrLen(int groupid)
{
	struct xcast_grpentry *ge;
	int (*f)(int);

	ge = XcastGetGroupEntry(groupid);
	if (ge == NULL || ge->grp == NULL)
		return XCAST_FAILURE;

	switch (ge->grp->family) {
	case AF_INET6:
		f = Xcast6GetHdrLen;
		break;
	case AF_INET:
	default:
		return XCAST_FAILURE;
	}

	return (*f)(groupid);
}
