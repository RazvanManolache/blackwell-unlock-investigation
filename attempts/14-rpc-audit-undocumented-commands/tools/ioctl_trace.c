/*
 * ioctl_trace.c -- LD_PRELOAD shim that intercepts every ioctl() a process
 * makes and decodes NV_ESC_RM_CONTROL (ioctl low byte 0x2A) calls: prints the
 * real RM_CONTROL command ID and result status actually exchanged with the
 * driver. Used to passively observe what a completely ordinary tool
 * (nvidia-smi) actually sends, rather than reasoning from the public driver
 * source about what should be reachable.
 *
 * Reconstructed from the exact snippet quoted in this project's own
 * gb203-session-findings write-up -- the original was never saved as a
 * file (built and used directly from a disposable live-boot shell).
 *
 * Build:  gcc -O2 -shared -fPIC -o ioctl_trace.so ioctl_trace.c -ldl
 * Usage:  LD_PRELOAD=./ioctl_trace.so nvidia-smi -q > /dev/null 2> rmcontrol.log
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/ioctl.h>

static int (*real_ioctl)(int, unsigned long, ...) = NULL;

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    void *arg;

    if (!real_ioctl)
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");

    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    if ((request & 0xff) == 0x2A && arg != NULL) {
        uint32_t *w = (uint32_t *)arg;
        /* NVOS54_PARAMETERS layout: hClient, hObject, cmd, flags, params,
         * paramsSize, status -- w[2]=cmd, w[6]=paramsSize, w[7]=status
         * (status is only valid to print AFTER the real call returns; this
         * fires the print post-call, see real_ioctl(...) below). */
        int ret = real_ioctl(fd, request, arg);
        fprintf(stderr, "[RM_CONTROL] cmd=0x%08x paramsSize=%u status=0x%x\n",
                w[2], w[6], w[7]);
        return ret;
    }

    return real_ioctl(fd, request, arg);
}
