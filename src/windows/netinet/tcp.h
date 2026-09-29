/* Windows shim: <netinet/tcp.h> via Winsock2 (TCP_NODELAY etc.) */
#ifndef __IPERF_WIN_NETINET_TCP_H
#define __IPERF_WIN_NETINET_TCP_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
