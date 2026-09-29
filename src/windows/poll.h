/* Windows shim: poll(2) via WSAPoll (some mingw sysroots have no <poll.h>).
 * mingw headers already declare struct pollfd (layout matches WSAPOLLFD). */
#ifndef __IPERF_WIN_POLL_H
#define __IPERF_WIN_POLL_H

#include "iperf_win_compat.h"   /* winsock2.h / ws2tcpip.h first */

#ifndef POLLIN
#define POLLIN    0x001
#define POLLPRI   0x002
#define POLLOUT   0x004
#define POLLERR   0x008
#define POLLHUP   0x010
#define POLLNVAL  0x020
#endif

static __inline int
iperf_poll_w32(struct pollfd *fds, ULONG nfds, int timeout)
{
    int r = WSAPoll((WSAPOLLFD *) fds, nfds, timeout);
    return (r == SOCKET_ERROR) ? -1 : r;
}
#define poll(fds, nfds, timeout) iperf_poll_w32((fds), (ULONG) (nfds), (timeout))

#endif /* __IPERF_WIN_POLL_H */
