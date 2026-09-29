/* Windows compatibility shim: include this instead of <sys/socket.h>.
 * Provides the POSIX socket API surface via Winsock2. Part of the mingw
 * Windows port; only used when building on/for Windows (CPPFLAGS=-Isrc/windows).
 */
#ifndef __IPERF_WIN_SYS_SOCKET_H
#define __IPERF_WIN_SYS_SOCKET_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/* POSIX shutdown() how-values */
#ifndef SHUT_RD
#define SHUT_RD    SD_RECEIVE
#define SHUT_WR    SD_SEND
#define SHUT_RDWR  SD_BOTH
#endif

/* POSIX socket type flags (not supported by Winsock; keep code compiling) */
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

/* Message flags used by the code that Winsock does not define.
 * MSG_TRUNC is only tested as a bit in `sock_opt & MSG_TRUNC`, and sock_opt
 * is only ever set (under HAVE_MSG_TRUNC, i.e. with skip_rx_copy) on
 * platforms where the real MSG_TRUNC exists.  The value is irrelevant on
 * Windows; it just needs to exist for the expression to compile. */
#ifndef MSG_TRUNC
#define MSG_TRUNC 0
#endif

#endif /* __IPERF_WIN_SYS_SOCKET_H */
