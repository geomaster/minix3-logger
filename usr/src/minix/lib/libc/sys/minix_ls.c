#include <lib.h>
#include <string.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <sys/errno.h>
#include <minix/syslib.h>
#include <minix/ls.h>
#define OK 0

int wrap_syscall(int, message*);

int wrap_syscall(int nr, message* m) {
	int ret = _syscall(LS_PROC_NR, nr, m);
	if (ret != OK) {
		return m->m_type;
	} else {
		return OK;
	}
}

int minix_ls_initialize() {
	message m;
	memset(&m, 0, sizeof(m));
	return wrap_syscall(LS_INITIALIZE, &m);
}

int minix_ls_start_log(const char* logger) {
	if (strlen(logger) >= LS_IPC_LOGGER_MAX_NAME_LEN - 1) {
		return -EINVAL;
	}

	message m;
	memset(&m, 0, sizeof(m));
	strncpy(m.m_ls_start_log.logger, logger, LS_IPC_LOGGER_MAX_NAME_LEN);
	return wrap_syscall(LS_START_LOG, &m);
}

int minix_ls_close_log(const char* logger) {
	if (strlen(logger) >= LS_IPC_LOGGER_MAX_NAME_LEN - 1) {
		return -EINVAL;
	}

	message m;
	memset(&m, 0, sizeof(m));
	strncpy(m.m_ls_close_log.logger, logger, LS_IPC_LOGGER_MAX_NAME_LEN);
	return wrap_syscall(LS_CLOSE_LOG, &m);
}

int minix_ls_write_log(const char* logger, const char* _message, minix_ls_log_level_t message_level) {
	if (strlen(logger) >= LS_IPC_LOGGER_MAX_NAME_LEN - 1) {
		return -EINVAL;
	}

	message m;
	memset(&m, 0, sizeof(m));
	strncpy(m.m_ls_write_log.logger, logger, LS_IPC_LOGGER_MAX_NAME_LEN);
	m.m_ls_write_log.message = _message;
	m.m_ls_write_log.message_len = strlen(_message);
	m.m_ls_write_log.severity = (int) message_level;
	return wrap_syscall(LS_WRITE_LOG, &m);

}

int minix_ls_set_logger_level(const char* logger, minix_ls_log_level_t new_level) {
	if (strlen(logger) >= LS_IPC_LOGGER_MAX_NAME_LEN - 1) {
		return -EINVAL;
	}

	message m;
	memset(&m, 0, sizeof(m));
	strncpy(m.m_ls_set_severity.logger, logger, LS_IPC_LOGGER_MAX_NAME_LEN);
	m.m_ls_set_severity.severity = (int) new_level;
	return wrap_syscall(LS_SET_SEVERITY, &m);
}

#define MAX_LOGGERS_LEN                     1024

int minix_ls_clear_logs(const char* loggers) {
	if (!loggers) {
		message m;
		memset(&m, 0, sizeof(m));
		return wrap_syscall(LS_CLEAR_ALL, &m);
	}

	if (strlen(loggers) > MAX_LOGGERS_LEN - 1) {
		return -EINVAL;
	}

	static char loggers_buf[MAX_LOGGERS_LEN];
	memset(loggers_buf, 0, sizeof(loggers_buf));

	const char *p;
	char *pb;
	for (p = loggers, pb = loggers_buf; *p; p++, pb++) {
		if (*p == ',') {
			*pb = '\0';
			pb = loggers_buf - 1;
			if (strlen(loggers_buf) > LS_IPC_LOGGER_MAX_NAME_LEN - 1) {
				return -EINVAL;
			}
		} else {
			*pb = *p;
		}
	}
	*pb = '\0';

	if (strlen(loggers_buf) > LS_IPC_LOGGER_MAX_NAME_LEN - 1) {
		return -EINVAL;
	}

	memset(loggers_buf, 0, sizeof(loggers_buf));
	int ret = OK;
	for (p = loggers, pb = loggers_buf; *p; p++, pb++) {
		if (*p == ',') {
			message m;
			memset(&m, 0, sizeof(m));
			*pb = '\0';
			pb = loggers_buf - 1;
			strcpy(m.m_ls_clear_log.logger, loggers_buf);
			int ret_logger = wrap_syscall(LS_CLEAR_LOG, &m);
			if (ret_logger != OK) {
				ret = ret_logger;
			}
		} else {
			*pb = *p;
		}
	}
	*pb = '\0';

	message m;
	memset(&m, 0, sizeof(m));
	strcpy(m.m_ls_clear_log.logger, loggers_buf);
	int ret_logger = wrap_syscall(LS_CLEAR_LOG, &m);
	if (ret_logger != OK) {
		ret = ret_logger;
	}

	return ret;
}
