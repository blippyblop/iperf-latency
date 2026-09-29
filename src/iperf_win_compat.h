/* Windows compatibility shim: Winsock error <-> errno translation, socket
 * close mapping, and option-pointer wrappers shared by the whole tree.
 * Included from iperf.h when building for Windows (before any other system
 * header, so winsock2.h is always included first). */
#ifndef __IPERF_WIN_COMPAT_H
#define __IPERF_WIN_COMPAT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <errno.h>
#include <io.h>
#include <unistd.h>

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

/* Sockets are not file descriptors on Windows.  All socket close() calls in
 * iperf go through this mapping; the only real file descriptor closes
 * (diskfile, daemonize) use _close() explicitly. */
#define close(s) closesocket((SOCKET)(s))

/* Translate Winsock errors to the closest errno values so existing
 * EAGAIN/EINTR/ECONNRESET/... checks behave as on POSIX. */
static __inline int
iperf_sock_errno_(void)
{
    switch (WSAGetLastError()) {
        case WSAEWOULDBLOCK:   return EWOULDBLOCK;
        case WSAEINPROGRESS:   return EINPROGRESS;
        case WSAEALREADY:      return EALREADY;
        case WSAEINTR:         return EINTR;
        case WSAECONNRESET:    return ECONNRESET;
        case WSAECONNABORTED:  return ECONNABORTED;
        case WSAECONNREFUSED:  return ECONNREFUSED;
        case WSAEHOSTUNREACH:  return EHOSTUNREACH;
        case WSAENETUNREACH:   return ENETUNREACH;
        case WSAENETDOWN:      return ENETDOWN;
        case WSAETIMEDOUT:     return ETIMEDOUT;
        case WSAEADDRINUSE:    return EADDRINUSE;
        case WSAEACCES:        return EACCES;
        case WSAEINVAL:        return EINVAL;
        case WSAEMSGSIZE:      return EMSGSIZE;
        case WSAENOPROTOOPT:   return ENOPROTOOPT;
        case WSAEOPNOTSUPP:    return EOPNOTSUPP;
        case WSAENOTCONN:      return ENOTCONN;
        case WSAESHUTDOWN:     return EPIPE;
        default:               return WSAGetLastError();
    }
}
#define SOCK_ERRNO iperf_sock_errno_()

/* POSIX shutdown(2) how-values */
#ifndef SHUT_RD
#define SHUT_RD    SD_RECEIVE
#define SHUT_WR    SD_SEND
#define SHUT_RDWR  SD_BOTH
#endif

/* BYTE_ORDER for portable_endian.h (mingw sys/param.h may not define it) */
#ifndef BYTE_ORDER
#define BIG_ENDIAN    4321
#define LITTLE_ENDIAN 1234
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

/* Winsock setsockopt/getsockopt take char* optval; POSIX takes void*.
 * Uniform wrappers are used at all call sites (see the sed rename + the
 * POSIX pass-through variants in iperf.h). */
static __inline int
iperf_setsockopt_w32(SOCKET s, int level, int optname, const void *optval, int optlen)
{
    return setsockopt(s, level, optname, (const char *) optval, optlen);
}
static __inline int
iperf_getsockopt_w32(SOCKET s, int level, int optname, void *optval, int *optlen)
{
    return getsockopt(s, level, optname, (char *) optval, optlen);
}

/* strsignal(3) equivalent for the few signals Windows actually delivers. */
#include <signal.h>
#define strsignal(sig) \
    ((sig) == SIGINT ? "Interrupt" : (sig) == SIGTERM ? "Terminated" : "signal")

#endif /* __IPERF_WIN_COMPAT_H */
