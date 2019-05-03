/* s2ping.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unisted.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <s2ping.h>

static int caught_signal = 0;

void sig_handler(int sig) {
	if (sig == SIGINT)
		caught_signal = 1;
}


int create_packet_sock(char *ifname) {

	int sock, ret;
	unsigned int ifindex;
	struct sockaddr_ll sll;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0) {
		fprintf(stderr, "if_nametoindex for %s: %s", ifname,
			strerror(errno));
		return -1;
	}

	sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_S2PING));
	if (sock < 0) {
		perror("socket");
		return -1;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_S2PING);
	sll.sll_ifindex = ifindex;

	ret = bind(sock, (struct sockaddr *)&sll, sizeof(sll));
	if (ret < 0) {
		fprintf(stderr, "bind for %s: %s", ifname,
			strerror(errno));
		return -1;
	}
	
	return sock;
}

int get_mac(char *ifname, uint8_t *mac)
{
	int fd, ret;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
	if (ret < 0) {
		fprintf(stderr, "ioctl failed to get mac addr of %s: %s",
			ifname, strerror(errno));
		return -1;
	}

	memcpy(mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	return 0;
}



void usage(void) {
	printf("s2ping [options] DSTMAC\n"
	       "    -i interface, outgoing interface name (required)\n"
	       "    -c count, how many packets xmit (0 means infinite)\n"
	       "    -I interval, second\n"
		);
}

#define BUFLEN 9220

int main(int argc, char **argv) {

	int ch, ret, sock;
	char buf[BUFLEN];
	char *ifname;
	uint8_t srcmac[ETH_ALEN], dstmac[ETH_ALEN];
	int count = 0;	/* infinite */
	double interval = 1000000;	/* 1 sec */
	
	/* scan dst mac */
	ret = sscanf(argv[argc - 1], "%x:%x:%x:%x:%x:%x",
		     &dstmac[0], &dstmac[1], &dstmac[2], 
		     &dstmac[3], &dstmac[4], &dstmac[5]);
	if (ret < 6) {
		fprintf(stderr, "invalid dst mac address '%s'\n", argv[argc - 1]);
		return 1;
	}

	/* scan options */
	while ((ch = getopt(argc - 1, argv, "i:c:I:")) != -1) {
		switch (ch) {
		case 'i':
			ifname = optarg;
			break;
		case 'c':
			count = atoi(otparg);
			if (count < 1) {
				fprintf(stderr, "invalid -c %s\n", optarg);
				return 1;
			}
			break;
		case 'I':
			interval = atof(optarg) * 1000000;
			if (optarg < 0) {
				fprintf(stderr, "invalid -I %s\n", optarg);
				return 1;
			}
			break;
		default:
			usage();
			return 1;
		}
	}


	return 0;
}
