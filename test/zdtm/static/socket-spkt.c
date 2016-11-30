#include "zdtmtst.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#define TUN_DEVICE	"/dev/net/tun"
#define FILLZERO(a) \
	memset(&(a), 0, sizeof(a))
#define TAPNAME	"zdtmtap0"

const char *test_doc	= "Support of SOCK_PACKET sockets";
const char *test_author	= "Gleb Valin <the7winds@yandex.ru>";

struct ethframe {
	struct ethhdr header;
	char data[ETH_DATA_LEN];
};

static inline void init_ifreq(struct ifreq *ifr, int flags)
{
	FILLZERO(*ifr);
	strcpy(ifr->ifr_name, TAPNAME);
	ifr->ifr_flags = flags;
}

static int tap_alloc_persistent(void)
{
	int tapfd = open(TUN_DEVICE, O_RDWR);

	if (tapfd < 0)
		return -1;

	struct ifreq set;

	init_ifreq(&set, IFF_TAP | IFF_NO_PI);

	if (ioctl(tapfd, TUNSETIFF, &set) < 0) {
		close(tapfd);
		return -1;
	}

	if (strcmp(set.ifr_name, TAPNAME)) {
		close(tapfd);
		return -1;
	}

	struct ifreq persist;

	init_ifreq(&persist, 0);

	if (ioctl(tapfd, TUNSETPERSIST, &persist) < 0) {
		close(tapfd);
		return -1;
	}

	close(tapfd);

	return 0;
}

static int tap_free_persistent(void)
{
	int tapfd = open(TUN_DEVICE, O_RDWR);

	if (tapfd < 0)
		return -1;

	struct ifreq set;

	init_ifreq(&set, IFF_TAP | IFF_NO_PI);

	if (ioctl(tapfd, TUNSETIFF, &set) < 0) {
		close(tapfd);
		return -1;
	}

	if (ioctl(tapfd, TUNSETPERSIST, NULL) < 0) {
		close(tapfd);
		return -1;
	}

	close(tapfd);

	return 0;
}

static int network_up(int sk)
{
	struct ifreq up;

	init_ifreq(&up, 0);

	if (ioctl(sk, SIOCGIFFLAGS, &up) < 0)
		return -1;

	up.ifr_flags |= IFF_UP;

	if (ioctl(sk, SIOCSIFFLAGS, &up) < 0)
		return -1;

	return 0;
}

static inline void build_sockaddr(struct sockaddr *addr)
{
	FILLZERO(*addr);
	addr->sa_family = AF_PACKET;
	strcpy(addr->sa_data, TAPNAME);
}

static int do_bind(int sk)
{
	struct sockaddr addr;

	build_sockaddr(&addr);

	return bind(sk, &addr, sizeof(addr));
}

static int check_socket_binding(int sk, char *dev)
{
	struct sockaddr addr;

	FILLZERO(addr);
	socklen_t l = sizeof(addr);

	if (getsockname(sk, &addr, &l) < 0)
		return -1;

	if (addr.sa_family != AF_PACKET)
		return -1;

	if (strcmp(addr.sa_data, dev) != 0)
		return -1;

	return 0;
}

static int check_socket_close(int sk)
{
	struct ethframe efr;

	FILLZERO(efr);

	struct sockaddr addr;

	FILLZERO(addr);
	addr.sa_family = AF_PACKET;
	strcpy(addr.sa_data, TAPNAME);

	if (sendto(sk, &efr, sizeof(efr), 0, &addr, sizeof(addr)) >= 0)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 1;

	test_init(argc, argv);

	if (tap_alloc_persistent() < 0) {
		pr_perror("can't open tap");
		goto err;
	}

	int sk1 = socket(AF_PACKET, SOCK_PACKET, htons(ETH_P_ALL));

	if (sk1  < 0) {
		pr_perror("can't open socket 1");
		goto tap_free;
	}

	if (network_up(sk1) < 0) {
		pr_perror("can't up network");
		goto sk_cl1;
	}

	if (do_bind(sk1) < 0) {
		pr_perror("can't bind sosket 1");
		goto sk_cl1;
	}

	int sk2 = socket(AF_PACKET, SOCK_PACKET, htons(ETH_P_ALL));

	if (sk2 < 0) {
		pr_perror("can't open socket 2");
		goto sk_cl1;
	}

	int sk3 = socket(AF_PACKET, SOCK_PACKET, htons(ETH_P_ALL));

	if (sk3 < 0) {
		pr_perror("can't open socket 3");
		goto sk_cl2;
	}

	if (close(sk3) < 0) {
		pr_perror("can't close socket 3");
		goto sk_cl2;
	}

	test_daemon();
	test_waitsig();

	int any_fail = 0;

	if (check_socket_binding(sk1, TAPNAME) < 0) {
		any_fail = 1;
		fail("socket 1: wrong binding");
	}

	if (check_socket_binding(sk2, "") < 0) {
		any_fail = 1;
		fail("socket 2: wrong binding");
	}

	if (check_socket_close(sk3) < 0) {
		any_fail = 1;
		fail("socket 3: must be closed");
	}

	if (!any_fail) {
		pass();
		ret = 0;
	}

sk_cl2:
	close(sk2);
sk_cl1:
	close(sk1);
tap_free:
	tap_free_persistent();
err:
	return ret;
}
