#include <sys/errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "proto.h"
#include "mini-printf.h"
#include "config-parse.h"
#include <minix/safecopies.h>
#include <minix/syslib.h>
#include <string.h>

#include "../pm/mproc.h"
#include <minix/sysinfo.h>

struct mproc proctable[NR_PROCS];

#define LOGBUF_LEN				4096
char g_logbuf[LOGBUF_LEN];

#define TRY_ENSURE_INITIALIZED() \
	do { \
		int ret = ensure_initialized(); \
		if (ret != OK) { \
			return LS_ERR_INIT_FAILED; \
		} \
	} while(0)

#define TRY_FIND_LOGGER(logger, l) \
	do { \
		l = find_logger(logger); \
		if (!l) { \
			LS_LOG_PRINTF(warn, "Logger not found: '%s'", logger); \
			return LS_ERR_NO_SUCH_LOGGER; \
		} \
	} while(0)

int get_process_table() {
	int ret;
    if ((ret = getsysinfo(PM_PROC_NR, SI_PROC_TAB, proctable, sizeof(proctable))) != OK) {
		LS_LOG_PRINTF(warn, "Failed to get process table: %d", ret);
        return LS_ERR_EXTERNAL;
    }

    return OK;
}

int procname_from_pid(endpoint_t pid, char* buffer, int buffer_len) {
	for (int i = 0; i < NR_PROCS; i++) {
		if (proctable[i].mp_endpoint == pid) {
			strncpy(buffer, proctable[i].mp_name, buffer_len);
			buffer[buffer_len - 1] = '\0';

			return TRUE;
		}
	}

	return FALSE;
}

int do_initialize() {
	if (g_loggers) {
		ls_logger_list_t* nxt;
		for (ls_logger_list_t* l = g_loggers; l; l = nxt) {
			nxt = l->tail;
			free(l);
		}
	}
	g_loggers = NULL;

	int ret = parse_config_file("/etc/logs.conf", &g_loggers);
	if (ret != OK) {
		return ret;
	}

	return get_process_table();
}

int do_start_log(const char* logger, endpoint_t who) {
	LS_LOG_PRINTF(info, "Starting logger '%s' by pid %d", logger, who);

	ls_logger_list_t* l;
	TRY_ENSURE_INITIALIZED();
	TRY_FIND_LOGGER(logger, l);

	if (l->state.is_open) {
		LS_LOG_PRINTF(warn, "Logger already open: '%s'", logger);
		return LS_ERR_LOGGER_OPEN;
	}

	if (l->logger.dest_type == LS_DESTINATION_FILE) {
		int flags = O_WRONLY | O_CREAT;
		if (l->logger.append) {
			flags |= O_APPEND;
		} else {
			flags |= O_TRUNC;
		}

		int fd = open(l->logger.dest_filename, flags);
		if (fd < 0) {
			LS_LOG_PRINTF(warn, "Failed to open file '%s' for writing to logger '%s'", l->logger.dest_filename, logger);
			return LS_ERR_EXTERNAL;
		}

		l->state.fd = fd;
	}

	l->state.severity = l->logger.severity;
	l->state.is_open = TRUE;
	l->state.opened_by = who;
	LS_LOG_PRINTF(info, "Opened logger '%s' with severity %s", logger, severity_to_str(l->state.severity));

	// Need to update the process table since we've got a new process on the
	// block.
	return get_process_table();
}

int do_close_log(const char* logger, endpoint_t who) {
	LS_LOG_PRINTF(info, "Closing logger '%s' by pid %d", logger, who);

	ls_logger_list_t* l;
	TRY_ENSURE_INITIALIZED();
	TRY_FIND_LOGGER(logger, l);

	if (!l->state.is_open) {
		LS_LOG_PRINTF(warn, "Logger '%s' is not open, but closing was requested", logger);
		return LS_ERR_LOGGER_NOT_OPEN;
	}

	if (l->state.opened_by != who) {
		LS_LOG_PRINTF(warn, "Closing of logger '%s' requested by %d, but it is not the owner", logger, who);
		return LS_ERR_PERMISSION_DENIED;
	}

	if (l->logger.dest_type == LS_DESTINATION_FILE) {
		int ret;
		if ((ret = close(l->state.fd)) != OK) {
			LS_LOG_PRINTF(warn, "Failed to close file for logger '%s'", logger);
			goto set_closed;
			return LS_ERR_EXTERNAL;
		}
	}

set_closed:
	l->state.is_open = FALSE;
	l->state.opened_by = -1;
	l->state.fd = -1;

	return OK;
}

int do_write_log(const char* logger, ls_severity_level_t severity, char* msg, int msg_len, endpoint_t who) {
	int ret;
	LS_LOG_PRINTF(debug, "Writing to logger '%s' from pid %d", logger, who);

	ls_logger_list_t* l;
	TRY_ENSURE_INITIALIZED();
	TRY_FIND_LOGGER(logger, l);

	if (!l->state.is_open) {
		LS_LOG_PRINTF(warn, "Logger not open: '%s'", logger);
		return LS_ERR_LOGGER_NOT_OPEN;
	}

	if (l->state.opened_by != who) {
		LS_LOG_PRINTF(warn, "Process %d tried to log through logger '%s', but it is not the owner", who, logger);
		return LS_ERR_PERMISSION_DENIED;
	}

	if ((ret = sys_vircopy(who, (vir_bytes) msg, LS_PROC_NR, (vir_bytes) l->state.msg_buf, msg_len, 0)) != OK) {
		LS_LOG_PRINTF(warn, "Copying from user address space failed: %d", ret);
		return ret;
	}

	if (severity < l->state.severity) {
		LS_LOG_PRINTF(debug, "Ignored message for logger '%s' due to its severity (%s)", logger, severity_to_str(severity));
		return OK;
	}

	char procname[256];
	if (!procname_from_pid(who, procname, 256)) {
		strncpy(procname, "unknown-pid", 256);
	}

	int sz = print_log(l->logger.format, l->state.msg_buf, msg_len, severity, procname, g_logbuf, LOGBUF_LEN - 1);
	g_logbuf[LOGBUF_LEN - 1] = '\0';

	if (l->logger.dest_type == LS_DESTINATION_FILE) {
		int ret = write(l->state.fd, g_logbuf, sz);
		LS_LOG_PRINTF(debug, "Message buffer size is %d, %d written, fd %d", sz, ret, l->state.fd);

		if (ret == -1 || ret < sz) {
			LS_LOG_PRINTF(warn, "Failed writing log line to file '%s' for logger '%s'", l->logger.dest_filename, logger);
			return LS_ERR_EXTERNAL;
		}

		fsync(l->state.fd);
	} else {
		g_logbuf[sz] = 0;
		printf("[L] %s", g_logbuf);
	}

	return OK;
}

int do_set_severity(const char* logger, ls_severity_level_t severity) {
	LS_LOG_PRINTF(info, "Setting severity of logger '%s' to %s", logger, severity_to_str(severity));

	ls_logger_list_t* l;
	TRY_ENSURE_INITIALIZED();
	TRY_FIND_LOGGER(logger, l);

	if (l->state.is_open) {
		LS_LOG_PRINTF(warn, "Cannot set the severity for logger '%s' because it is open", logger);
		return LS_ERR_LOGGER_OPEN;
	}

	l->state.severity = severity;
	return OK;
}

int do_clear_logs() {
	LS_LOG_PUTS(info, "Clearing all logs");
	TRY_ENSURE_INITIALIZED();

	int ret = OK;
	for (ls_logger_list_t* l = g_loggers; l; l = l->tail) {
		if (do_clear_log(l->logger.name) != OK) {
			ret = LS_ERR_LOGGER_OPEN;
		}
	}

	return ret;
}

int do_clear_log(const char* logger) {
	LS_LOG_PRINTF(info, "Clearing log for logger '%s'", logger);

	ls_logger_list_t* l;
	TRY_ENSURE_INITIALIZED();
	TRY_FIND_LOGGER(logger, l);

	if (l->state.is_open) {
		LS_LOG_PRINTF(warn, "Cannot clear log for '%s' as it is open", l->logger.name);
		return LS_ERR_LOGGER_OPEN;
	} else {
		if (l->logger.dest_type == LS_DESTINATION_FILE) {
			int fd = open(l->logger.dest_filename, O_WRONLY | O_TRUNC | O_CREAT);
			if (fd < 0) {
				LS_LOG_PRINTF(warn, "Failed to open file '%s' for truncation of logger '%s'", l->logger.dest_filename, logger);
				return LS_ERR_EXTERNAL;
			}

			int ret = close(fd);
			if (ret != OK) {
				LS_LOG_PRINTF(warn, "Failed to close file for truncation '%s' for logger '%s'", l->logger.dest_filename, logger);
				return LS_ERR_EXTERNAL;
			}
		}
	}

	return OK;
}
