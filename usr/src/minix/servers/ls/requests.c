#include <sys/errno.h>
#include "proto.h"
#include "mini-printf.h"
#include "config-parse.h"
#include <minix/safecopies.h>
#include <minix/syslib.h>

char buf[4096];

int do_initialize() {
	return parse_config_file("/etc/logs.conf", &g_loggers);
}

int do_start_log(const char* logger, endpoint_t who) {
	LS_LOG_PRINTF(info, "Starting logger '%s' by pid %d", logger, who);
	return OK;
}

int do_close_log(const char* logger, endpoint_t who) {
	LS_LOG_PRINTF(info, "Closing logger '%s' by pid %d", logger, who);
	return OK;
}

int do_write_log(const char* logger, ls_severity_level_t severity, char* msg, int msg_len, endpoint_t who) {
	int ret;
	LS_LOG_PRINTF(debug, "Writing to logger '%s' from pid %d", logger, who);
	if ((ret = sys_vircopy(who, (vir_bytes) msg, LS_PROC_NR, (vir_bytes) buf, msg_len, 0)) != OK) {
		LS_LOG_PRINTF(warn, "Copying from user address space failed: %d", ret);
		return ret;
	}

	return OK;
}

int do_set_severity(const char* logger, ls_severity_level_t severity) {
	LS_LOG_PRINTF(debug, "Closing logger '%s'", logger);
	return OK;
}

int do_clear_logs() {
	LS_LOG_PUTS(info, "Clearing all logs");
	return OK;
}

int do_clear_log(const char* logger) {
	LS_LOG_PRINTF(info, "Clearing log for logger '%s'", logger);
	return OK;
}
