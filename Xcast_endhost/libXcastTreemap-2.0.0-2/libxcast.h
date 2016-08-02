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

#define XCAST6_DESTOPT

#ifndef _LIBXCAST_H_
#define _LIBXCAST_H_

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* XCAST API
 * 
 * This is based on the Internet Draft: draft-ooms-xcast-basic-spec-01.txt
 */

/* Pending IANA defined values */
#define IPPROTO_XCAST           0xTBA   /* XCAST protocol number */
#define XCAST_ALLROUTERS        0xTBA   /* XCAST Class D All Routers address */

/*
 * XCAST group flags
 */
#define XC_ANONYMITY     0x08        /* Destination anonymity */
#define XC_NOX2U         0x04        /* Maintain XCAST header */
#define XC_DSCP          0x02        /* DS-byte included per destination */
#define XC_PORT          0x01        /* Port included per destination */


/*
 * The following struct is used to define the members of an XCAST group.
 * If port and/or dscp are to be specified, the XCAST group MUST be created 
 * with the corresponding flags
 */
struct xcast_member {
        struct sockaddr *xm_dest;   /* Dest address and (optional) UDP port */
        unsigned char xm_dscp;      /* Diffserv code point (optional) */
};


/*
 * Function return codes
 */
#define XCAST_SUCCESS         0
#define XCAST_FAILURE         (-1)


/*
 *  XCAST API Functions
 */

/* XcastCreateGroup()
 * Create a new XCAST group.
 * Returns a unique group identifier on success. This MUST be passed as 
 * a parameter to all subsequent XCAST function calls.
 * Optional input parameters MUST be 0/NULL if unused
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastCreateGroup(
        int flags,              /* XCAST group flags */
        struct sockaddr *src,   /* (Optional) Source IP address and/or
                                 * source UDP port, in network byte order;
                                 * Default src address is first IP address
                                 * of outoing interface
                                 * Default src UDP port is 0 */
        unsigned short maxmbrs  /* (Optional) Max members per XCAST header;
                         * If group contains more members than max, multiple
                         * XCAST packets will be transmitted per send request */
);

/* XcastDeleteGroup()
 * Delete an existing XCAST group.
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastDeleteGroup(
        int groupid             /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
);
 
/* XcastAddMember()
 * Add an IP destination to the given group.
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastAddMember(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
        struct xcast_member *member        /* Member information */
);
                                        
/* XcastDeleteMember()
 * Delete an IP destination from the given group.
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastDeleteMember(
        int groupid,             /* Unique group identifier, returned 
                                  * by XcastCreateGroup() */
        struct xcast_member *member        /* Member information */
);
                                        
/* XcastSend()
 * Transmit data over the XCAST socket.
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastSend(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
        char *datap,            /* Data to transmit */
        int datalen             /* Length of data */
);

int XcastTreemapSend(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup(), after adding treemap */
        char *datap,            /* Data to transmit */
        int datalen             /* Length of data */
);

/* XcastSendMsg()
 * Transmit data over the XCAST socket.
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastSendMsg(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
        struct msghdr *msg,     /* Msg to transmit */
        int flags               /* Msg flags */
);


/* XcastGetHdrLen
 * Returns the current size of the XCAST header for the given group.
 * This is useful if an app wants to send large packets and wants to avoid 
 * IP fragmentation.
 * Returns 0 on failure
 */
int XcastGetHdrLen(
        int groupid             /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
);

/* XcastSetSockOpt
 * Set the socket option on the XCAST socket, as defined by setsockopt(),
 * for the given group.
 * Returns 0 on success
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastSetSockOpt(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
        int optlev,             /* Socket level */
        int optname,            /* Option Name */
        void *optval,           /* Option Value */
        size_t optlen           /* Option Length */

);

/* XcastGetSockOpt
 * Get the socket option on the XCAST socket, as defined by getsockopt(),
 * for the given group.
 * Returns 0 on success
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastGetSockOpt(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
        int optlev,             /* Socket level */
        int optname,            /* Option Name */
        void *optval,           /* Option Value */
        size_t *optlen          /* Option Length */
);

/* 
 * XcastDisableMember()         (optional function)
 * Remove the member from the distribution by clearing its corresponding
 * bit in the bitmap; the member remains in the group list.
 * This is useful if an app has members who frequently enter and leave 
 * the destination list (e.g. the group is fixed and the speaker frequently
 * changes).
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastDisableMember(
        int groupid,            /* Unique group identifier, returned 
                                 * by XcastCreateGroup() */
        struct xcast_member *member        /* Member information */
);

/* 
 * XcastEnableMember()                 (optional function)
 * Add a disabled member back into the distribution by resetting its
 * corresponding bit in the bitmap.
 * Returns 0 on success.
 * Returns -1 on failure; Global error variable contains error code.
 */
int XcastEnableMember(
        int groupid,             /* Unique group identifier, returned 
                                  * by XcastCreateGroup() */
        struct xcast_member *member        /* Member information */
);

#ifdef __cplusplus
}
#endif

#endif /* _LIBXCAST_H_ */
