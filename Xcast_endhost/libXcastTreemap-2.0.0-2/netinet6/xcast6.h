#ifndef _NETINET6_XCAST6_H_
#define _NETINET6_XCAST6_H_

/* flowinfo field in semi-permeable tunnel IP6 header */
#if BYTE_ORDER == BIG_ENDIAN
#define XCAST6_FLOWINFO		0x05c58000
#else
#if BYTE_ORDER == LITTLE_ENDIAN
#define XCAST6_FLOWINFO		0x0080c505
#endif
#endif

/* Destination address specified as ALL_XCAST_NODES in the Internet Draft. */
#define IN6ADDR_ALLXCASTNODES_INIT \
	{{{ 0xff, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x14 }}}

#define IPV6_RTHDR_TYPE_XCAST6	253	/* should be in netinet6/in6.h */
#define IP6OPT_TYPE_TREEMAP     0x27    /* XXX  needs to register */
#define XCAST6_MAX_DESTS	64	/* Khoa defines the max number of destinations */
#define XCAST6_DMAP_LEN		16  /* Khoa maybe change to 8 bytes (or 64 bits) */

/* XCAST6 Routing header */
struct ip6_rthdrx {
	u_int8_t  ip6rx_nxt;		/* next header */
	u_int8_t  ip6rx_len;		/* length in units of 8 octets */
	u_int8_t  ip6rx_type;		/* 253 */
	u_int8_t  ip6rx_zero;		/* always 0 */
	u_int8_t  ip6rx_ndest;		/* number of destinations */
	u_int8_t  ip6rx_flags;		/* flag bits */
	u_int8_t  ip6rx_icmpoff;	/* ICMPv6 offset */
	u_int8_t  ip6rx_reserved;	/* always 0 */
	u_int8_t  ip6rx_dmap[XCAST6_DMAP_LEN];	/* destination bitmap */	
	/* followed by up to XCAST6_MAX_DESTS struct in6_addr */
} __packed;

/* Routing header flags field */
#define XCAST6_F_ANON		0x80
#define XCAST6_F_DONTX2U	0x40

/* Routing header type for OS portability */
#define XCAST6_RTHDR_TYPE	IPV6_RTHDR_TYPE_XCAST6

#define XCAST6_RTHDR_LEN(ndest) \
	(sizeof(struct ip6_rthdrx) + sizeof(struct in6_addr) * (ndest))

/* Xcast6 destination options header */
typedef struct xcast6_dest {
	u_int8_t  x6d_type;		/* IP6OPT_TYPE_XCAST6 */
	u_int8_t  x6d_optdlen;		/* >= 0 */
	/* Xcast Treemap follow... */
} xcast6_dest_t;

#define XCAST6_DEST_HDRLEN(ndest)		\
	((sizeof(struct ip6_dest) +		\
	  sizeof(xcast6_dest_t) +		\
	  sizeof(u_int8_t) * (ndest) / 2 + 7) & ~7)

/* sysctl variables */
#define ROUTE6CTL_XCAST6_ENABLE		1
#define ROUTE6CTL_XCAST6_X2U		2
#define ROUTE6CTL_MAXID                 3

#define ROUTE6CTL_NAMES { \
        { 0, 0 }, \
        { "xcast6_enable", CTLTYPE_INT }, \
        { "xcast6_x2u", CTLTYPE_INT }, \
}

#ifdef _KERNEL
struct xcast6_ctl {
	u_int8_t xc6_hlim;
	struct in6_addr xc6_dst;
	int xc6_branched;
	int xc6_reflectable;
};

extern struct in6_addr in6addr_all_xcast6;
extern int xcast6_enable;
extern int xcast6_x2u;

struct xcast6_ctl *xcast6_addctl(struct mbuf *m);
struct xcast6_ctl *xcast6_getctl(struct mbuf *m);
void xcast6_delctl(struct mbuf *m);
int xcast6_is_addr_xcast6(struct in6_addr *addr);
int xcast6_branch(struct mbuf *m, struct ip6_rthdrx *rthx,
		  int *offp, int hlim);
int xcast6_isreflectable(struct mbuf *m, int require_xcast);
#endif /* _KERNEL */

#endif /* not _NETINET6_XCAST6_H_ */
