#include "log.h"
#include <string.h>
#include <lib.h>
#include <minix/syslib.h>
#include <time.h>
#include <sys/errno.h>
#include <limits.h>
#include <minix/rs.h>
#include "mini-printf.h"

#define PUTC(c, ret) \
	do { \
		if (pb < pend) { \
			*pb++ = (c); \
		} else { \
			return ret; \
		} \
	} while(0)

#define PUTS(s, ret) \
	do { \
		const char *p = s; \
		while (*p) { \
			PUTC(*p++, ret); \
		} \
	} while(0)

const char* severity_to_str(ls_severity_level_t severity) {
	switch (severity) {
		case LS_SEV_TRACE: return "trace";
		case LS_SEV_DEBUG: return "debug";
		case LS_SEV_INFO: return "info";
		case LS_SEV_WARN: return "warn";
		default: return "unknown";
	}
}

int lookup_proc(const char *name, endpoint_t *value)
{
	message m;
	size_t len_key;

	len_key = strlen(name)+1;

	memset(&m, 0, sizeof(m));
	m.m_rs_req.name = name;
	m.m_rs_req.name_len = len_key;

	if (_syscall(RS_PROC_NR, RS_LOOKUP, &m) != -1) {
		*value = m.m_rs_req.endpoint;
		return OK;
	}

	return -1;
}

char* put_time(char* pb, char* pend) {
	int r;
	message m;
	endpoint_t ep;
	struct tm tm;

	r = lookup_proc("readclock.drv", &ep);
	if (r != OK) {
		LS_LOG_PRINTF(warn, "Couldn't locate readclock.drv: %d", r);
		PUTS("unknown-time", pb);
		return pb;
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_readclock_rtcdev.tm = (vir_bytes)&tm;
	m.m_lc_readclock_rtcdev.flags = RTCDEV_NOFLAGS;

	r = _syscall(ep, RTCDEV_GET_TIME, &m);
	if (r != RTCDEV_REPLY || m.m_readclock_lc_rtcdev.status != 0) {
		LS_LOG_PRINTF(warn, "Call to readclock.drv failed: %d", r);
		PUTS("unknown-time", pb);
		return pb;
	}

	char buffer[1024];
	mini_snprintf(buffer, 1023, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1980, tm.tm_mon + 1, tm.tm_mday + 1, tm.tm_hour, tm.tm_min, tm.tm_sec);
	buffer[1023] = '\0';

	PUTS(buffer, pb);
	return pb;
}

int print_log(const char* format, const char* message, int msg_len, ls_severity_level_t severity, const char* procname, char* buffer, int buffer_len) {
	char *pb = buffer;
	char *pend = buffer + buffer_len;

	while (*format) {
		if (*format == '%') {
			format++;
			if (!*format) {
				return pb - buffer;
			}

			switch (*format) {
				case 'l': PUTS(severity_to_str(severity), pb - pend); break;
				case 't': pb = put_time(pb, pend); break;
				case 'n': PUTS(procname, pb - buffer); break;
				case 'm':
					for (const char* pmsg = message; pmsg < message + msg_len; ++pmsg) {
						PUTC(*pmsg, pb - buffer);
					}

					break;

				case '%': PUTC('%', pb - buffer); break;
				default: PUTC('%', pb - buffer); PUTC(*format, pb - buffer); break;
			}
		} else {
			PUTC(*format, pb - buffer);
		}

		format++;
	}
	PUTC('\n', pb - buffer);

	return pb - buffer;
}
