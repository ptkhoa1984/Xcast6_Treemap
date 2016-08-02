#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
//#include "libxcast.h"
#define IN6ADDR_ALL_XCAST6_ROUTERS_INIT \
	{ 0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 }


static const struct in6_addr in6addr_all_xcast6 =
	IN6ADDR_ALL_XCAST6_ROUTERS_INIT;
int
main(int argc, char *argv[])
{
	int fd;
	int ret;
	struct sockaddr_in6 sin6;
	struct addrinfo hints, *ai;
	struct ipv6_mreq join;
	socklen_t len;
	char buf[128], name[INET6_ADDRSTRLEN];

	fd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		return;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	ret = getaddrinfo(NULL, "11111", &hints, &ai);
	if (ret != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(1);
	}

	ret = bind(fd, ai->ai_addr, ai->ai_addrlen);
	if (ret < 0) {
		perror("bind");
		exit(1);
	}

	memset(&join, 0, sizeof(struct ipv6_mreq));
	memcpy(&join.ipv6mr_multiaddr, &in6addr_all_xcast6,
	       sizeof(struct in6_addr));

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
		       &join, sizeof(struct ipv6_mreq)) < 0) {
		perror("IPV6_ADD_MEMBERSHIP");
		exit(1);
	}

	freeaddrinfo(ai);

	len = sizeof(sin6);
int count = 1;
while(1)
{
	ret = recvfrom(fd, buf, sizeof(buf), 0,
		       (struct sockaddr *)&sin6, &len);
	if (ret < 0) {
		perror("recvfrom");
		exit(1);
	}

	if (inet_ntop(sin6.sin6_family, &sin6.sin6_addr, name, sizeof(name))
	    == NULL) {
		perror("inet_ntop");
		exit(1);
	}

	printf("%d.  xrecv: got from %s: %s\n", count++,name, buf);

	
}
return 0;
}
