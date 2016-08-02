
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include "libxcast.h"
#include "libxcast_priv.h"

int
main(int argc, char *argv[])
{
	struct OverlayRoute pOverlayRoute;
	struct xcast_member m;
	char buf[] = "Khoa hello from XcastTreemap2.0";
	int g;
	int i;
	int ret;

	g = XcastCreateGroup(0, NULL, 0);
	if (g != XCAST_SUCCESS)
		errx(1, "XcastCreateGroup() failed.");
	
	/* ndests = 5: Source A sends data to B C D E F*/
	pOverlayRoute.ndests = 5;
	
	/* treemap = [A B C D E F | 1 2 0 2 0 0] */

	pOverlayRoute.tree_map[0] = 1; //node A has 1 child (node B)

	strcpy(pOverlayRoute.dests[0],"2001:200:161:152::2"); // node B
	pOverlayRoute.tree_map[1] = 2; //node B has 2 children (node C and D)

	strcpy(pOverlayRoute.dests[1],"2001:200:161:153::2"); // node C
	pOverlayRoute.tree_map[2] = 0; //node C has no child
	strcpy(pOverlayRoute.dests[2],"2001:200:161:157::2"); //node D
	pOverlayRoute.tree_map[3] = 2; //node D has 2 children (node E and F)

	strcpy(pOverlayRoute.dests[3],"2001:200:161:155::2"); //node E
	pOverlayRoute.tree_map[4] = 0; //node E has no child
	strcpy(pOverlayRoute.dests[4],"2001:200:161:156::2"); //node F
	pOverlayRoute.tree_map[5] = 0; //node F has no child
	
	AddTree(g, &pOverlayRoute);

	ret = XcastTreemapSend(g, buf, sizeof(buf));
	if (ret < 0)
		err(1, "XcastTreemapSend");

	ret = XcastDeleteGroup(g);
	if (g != XCAST_SUCCESS)
		errx(1, "XcastDeleteGroup() failed.");

	return 0;
}
