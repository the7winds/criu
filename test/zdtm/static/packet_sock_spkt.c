#include "zdtmtst.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <unistd.h>
#include <fcntl.h>

#define FILLZERO(a) \
	memset(&(a), 0, sizeof(a))

const char *test_doc = "Support of SOCK_PACKET sockets";
const char *test_author = "Gleb Valin <the7winds@yandex.ru>";

struct ethframe {
	struct ethhdr header;
	char data[ETH_DATA_LEN];
};

static int do_bind(int sk)
{
	struct sockaddr addr;

	FILLZERO(addr);
	addr.sa_family = AF_PACKET;
	strcpy(addr.sa_data, "lo");

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
	strcpy(addr.sa_data, "lo");

	if (sendto(sk, &efr, sizeof(efr), 0, &addr, sizeof(addr)) >= 0)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 1;

	test_init(argc, argv);

	int sk1 = socket(AF_PACKET, SOCK_PACKET, htons(ETH_P_ALL));

	if (sk1  < 0) {
		pr_perror("can't open socket 1");
		goto err;
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

	if (check_socket_binding(sk1, "lo") < 0) {
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
err:
	return ret;
}
