/* s2pingd.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <s2ping.h>

#define MAX_INTF_NUM	128

struct s2pingd_thread {
	pthread_t	tid;
	char	*ifname;	/* interface name of this thread */
	uint8_t	mac[ETH_ALEN];	/* mac of this interface */
	int	sock;		/* af_packet socket bound to this interface */
};

static int caught_signal = 0;

void sig_handler(int sig) {

	if (sig == SIGINT)
		caught_signal = 1;

	printf("\nSIGINT received\n");
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


#define BUFLEN 9220

int s2ping_handle_packet(int sock, struct s2ping_frame *frame, size_t len,
			 char *ifname, uint8_t *mac)
{
	int ret;
	char replybuf[BUFLEN];
	struct s2ping_frame *reply = (struct s2ping_frame *)replybuf;


	/* validation */
	if (frame->ver != S2PING_VERSION)
		return -1;

	if (frame->type != S2PING_TYPE_ECHO)
		return -1;

	/* build reply frame */
	memcpy(reply, frame, len);
	memcpy(reply->eth.ether_dhost, reply->src, ETH_ALEN);
	memcpy(reply->eth.ether_shost, mac, ETH_ALEN);
	reply->type = S2PING_TYPE_REPLY;

	/* xmit the reply frame */
	ret = write(sock, reply, len);
	if (ret < 0) {
		fprintf(stderr, "failed to write reply frame on %s\n", ifname);
		return -1;
	}

	return 0;
}

void *s2pingd_thread_body(void *param)
{
	int ret;
	char buf[BUFLEN];
	struct s2pingd_thread *th = param;
	struct pollfd x = { .fd = th->sock, .events = POLLIN };

	while (1) {
		if (caught_signal)
			break;

		if (poll(&x, 1, 100) < 0) {
			fprintf(stderr, "poll failed on %s: %s",
				th->ifname, strerror(errno));
			return NULL;
		}

		if (!x.revents & POLLIN)
			continue;

		ret = read(th->sock, buf, sizeof(buf));
		if (ret < 0) {
			fprintf(stderr, "read failed of %s: %s",
				th->ifname, strerror(errno));
			continue;
		}

		s2ping_handle_packet(th->sock, (struct s2ping_frame *)buf, ret,
				     th->ifname, th->mac);
	}
		

	return NULL;
}

void usage(void) {

	printf("s2pingd [list of interfaces watched by s2pingd]\n");
}

int main(int argc, char **argv) {
	
	int n, ret;
	int nths = 0;
	struct s2pingd_thread ths[MAX_INTF_NUM];

	memset(ths, 0, sizeof(ths));

	for (n = 1; n < argc; n++) {
		struct s2pingd_thread *th = &ths[n - 1];

		if (strncmp(argv[n], "-h", 2) == 0 ||
		    strncmp(argv[n], "--help", 6) == 0) {
			usage();
			return 1;
		}


		printf("spawn a thread for %s\n", argv[n]);

		th->ifname = argv[n];

		ret = get_mac(th->ifname, th->mac);
		if (ret < 0) {
			fprintf(stderr, "failed to get mac addr for '%s\n'",
				th->ifname);
			exit(EXIT_FAILURE);
		}

		th->sock = create_packet_sock(th->ifname);
		if (th->sock < 0) {
			fprintf(stderr, "failed to create socket on '%s'\n",
				th->ifname);
			exit(EXIT_FAILURE);
		}

		ret = pthread_create(&th->tid, NULL, s2pingd_thread_body, th);
		if (ret < 0) {
			fprintf(stderr, "failed to spawn thread for '%s'\n",
				th->ifname);
			exit(EXIT_FAILURE);
		}
				     
		nths++;
	}

	if (nths == 0) {
		fprintf(stderr, "no interface specified\n");
		return 1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		exit(EXIT_FAILURE);
	}

	for (n = 0; n < nths; n++) {
		pthread_join(ths[n].tid, NULL);
		printf("thread for %s joined\n", ths[n].ifname);
	}

	return 0;
}
