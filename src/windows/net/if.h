/* Windows shim: <net/if.h> */
#ifndef __IPERF_WIN_NET_IF_H
#define __IPERF_WIN_NET_IF_H
#include <winsock2.h>
#include <ws2tcpip.h>   /* if_nametoindex */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#endif
