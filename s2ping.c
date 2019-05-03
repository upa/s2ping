/* s2ping.c */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <signal.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>

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

uint64_t get_ts(void)
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return ts.tv_usec * 1000000 + ts.tv_sec;
}


void usage(void) {
	printf("s2ping [options] DSTMAC\n"
	       "    -i interface, outgoing interface name (required)\n"
	       "    -c count, how many packets xmit (0 means infinite)\n"
	       "    -s size, size of probe packet\n"
	       "    -I interval, second\n"
	       "    -T timeout, second\n"
		);
}

#define BUFLEN 9220

int main(int argc, char **argv) {

	int ch, ret, sock;
	char send_buf[BUFLEN], recv_buf[BUFLEN];
	struct s2ping_frame *frame, *reply;
	struct pollfd x;
	struct timespec to = { .tv_sec = 0, .tv_nsec = 100000000 }; /* 100ms */
	sigset_t sigset;

	char *ifname = NULL;
	uint8_t srcmac[ETH_ALEN], dstmac[ETH_ALEN];
	int sent = 0, count = 0;	/* infinite */
	int size = 64;
	double interval = 1000;	/* 1 sec in msec */
	double timeout = 1000;	/* 1 sec in msec */

	int received = 0;


	memset(send_buf, 0, sizeof(send_buf));
	memset(recv_buf, 0, sizeof(recv_buf));
	
	/* scan options */
	while ((ch = getopt(argc, argv, "i:c:I:T:")) != -1) {
		switch (ch) {
		case 'i':
			ifname = optarg;
			break;
		case 'c':
			count = atoi(optarg);
			if (count < 1) {
				fprintf(stderr, "invalid -c %s\n", optarg);
				return 1;
			}
			break;
		case 's':
			size = atoi(optarg);
			if (size < sizeof(struct s2ping_frame)) {
				fprintf(stderr, "too small -s %s\n", optarg);
				return 1;
			}
			if (size > BUFLEN) {
				fprintf(stderr, "too large -s %s\n", optarg);
				return 1;
			}
			break;
		case 'I':
			interval = atof(optarg) * 1000;
			if (interval < 0) {
				fprintf(stderr, "invalid -I %s\n", optarg);
				return 1;
			}
			break;
		case 'T':
			timeout = atof(optarg) * 1000;
			if (timeout < 0) {
				fprintf(stderr, "invalid -T %s\n", optarg);
				return 1;
			}
			break;
		default:
			usage();
			return 1;
		}
	}

	/* scan dst mac */
	ret = sscanf(argv[argc - 1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		     &dstmac[0], &dstmac[1], &dstmac[2], 
		     &dstmac[3], &dstmac[4], &dstmac[5]);
	if (ret < 6) {
		fprintf(stderr, "invalid dst mac address '%s'\n",
			argv[argc - 1]);
		return 1;
	}

	if (!ifname) {
		fprintf(stderr, "-i interface must be specified\n");
		return 1;
	}

	if (get_mac(ifname, srcmac) < 0)
		return 1;

	sock = create_packet_sock(ifname);
	if (sock < 0)
		return 1;

	/* build s2ping frame */
	frame = (struct s2ping_frame *)send_buf;
	memcpy(frame->eth.ether_shost, srcmac, ETH_ALEN);
	memcpy(frame->eth.ether_dhost, dstmac, ETH_ALEN);
	memcpy(frame->src, srcmac, ETH_ALEN);
	frame->eth.ether_type = htons(ETH_P_S2PING);
	frame->ver = S2PING_VERSION;
	frame->type = S2PING_TYPE_ECHO;
	frame->seq = 0;
	frame->rsv = 0;

	x.fd = sock;
	x.events = POLLIN;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);

	printf("S2PING to "
	       "%02x:%02x:%02x:%02x:%02x:%02x from "
	       "%02x:%02x:%02x:%02x:%02x:%02x (%s) %d bytes frame\n",
	       dstmac[0], dstmac[1], dstmac[2],
	       dstmac[3], dstmac[4], dstmac[5],
	       srcmac[0], srcmac[1], srcmac[2],
	       srcmac[3], srcmac[4], srcmac[5],
	       ifname, size);

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		exit(EXIT_FAILURE);
	}
		
	while (1) {

		if (caught_signal)
			break;

		/* send echo frame */
		frame->seq++;
		frame->ts = get_ts();

		ret = write(sock, frame, size);
		if (ret < 0) {
			perror("write");
			return 1;
		}

		/* recv reply frame */
		int elapsed = 0;
		while (1) {

			if (caught_signal)
				break;

			if (elapsed >= timeout) {
				printf("timeout for seq %u\n", frame->seq);
				break;
			}

			if (ppoll(&x, 1, &to, &sigset) < 0) {
				perror("poll");
				return 1;
			}
			elapsed += 100;

			if (!x.revents & POLLIN)
				continue;

			ret = read(sock, recv_buf, sizeof(recv_buf));
			reply = (struct s2ping_frame *)recv_buf;

			/* validate */
			if (reply->seq != frame->seq)
				continue;

			if (memcmp(reply->eth.ether_shost, dstmac, ETH_ALEN)
			    != 0)
				continue;

			/* ok, this is the the packet we want */
			printf("%d bytes from %02x:%02x:%02x:%02x:%02x:%02x "
			       "seq=%u time=%.2f ms\n",
			       ret,
			       reply->eth.ether_shost[0],
			       reply->eth.ether_shost[1],
			       reply->eth.ether_shost[2],
			       reply->eth.ether_shost[3],
			       reply->eth.ether_shost[4],
			       reply->eth.ether_shost[5],
			       reply->seq,
			       ((double)reply->ts - (double)frame->ts) * 1000);
			received++;
			break;
		}

		sent++;
		if (count > 0 && sent >= count)
			break;

		usleep((interval > elapsed ? interval - elapsed : 0) * 1000);
	}

	close(sock);

	double lost = (double)(sent - received) / (double)sent * 100.0;
	printf("\n%d frame(s) sent, %d frame(s) received, %.2f%% lost\n",
	       sent, received, lost);

	return (int)lost;	/* return loss rate as int */
}
