#pragma once

#include <stdio.h>
#include <minix/log.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <stdlib.h>

#define LS_MAX_LOGGER_NAME_LEN              32
#define LS_MAX_LOGGER_LOGFILE_PATH_LEN      64
#define LS_MAX_LOGGER_FORMAT_LEN			128
#define LS_MAX_PROC_NAME_LEN				2048
#define LS_ERR_BUF_LEN						1024
#define LS_MAX_MESSAGE_LEN					2048

#define LS_LOG_PRINTF(level, fmt, ...) \
	do { \
		char _ls_logbuf[4096]; \
		mini_snprintf(_ls_logbuf, 4096, fmt "\n", __VA_ARGS__); \
		log_##level(&ls_syslog, _ls_logbuf); \
	} while (0)

#define LS_LOG_PUTS(level, str) \
	do { \
		log_##level(&ls_syslog, str "\n"); \
	} while (0)

static struct log ls_syslog = {
	.name = "ls",
	.log_level = LEVEL_TRACE,
	.log_func = default_log
};

/* Data structures. */
typedef struct ls_request_t {
	endpoint_t source;
	int type;
} ls_request_t;

typedef enum ls_log_destination_t {
	LS_DESTINATION_FILE,
	LS_DESTINATION_STDERR,
	LS_DESTINATION_STDOUT
} ls_log_destination_t;

typedef enum ls_severity_level_t {
	LS_SEV_TRACE,
	LS_SEV_DEBUG,
	LS_SEV_INFO,
	LS_SEV_WARN
} ls_severity_level_t;

typedef struct ls_logger_t {
	char name[LS_MAX_LOGGER_NAME_LEN];
	ls_log_destination_t dest_type;
	ls_severity_level_t severity;
	char dest_filename[LS_MAX_LOGGER_LOGFILE_PATH_LEN];
	char format[LS_MAX_LOGGER_FORMAT_LEN];
	int append;
} ls_logger_t;

typedef struct ls_logger_state_t {
	int is_open;
	ls_severity_level_t severity;
	endpoint_t opened_by;
	int fd;
	char opened_by_name[LS_MAX_PROC_NAME_LEN];
	char msg_buf[LS_MAX_MESSAGE_LEN];
	int msg_len;
} ls_logger_state_t;

typedef struct ls_logger_list_t {
	ls_logger_t logger;
	ls_logger_state_t state;
	struct ls_logger_list_t* tail;
} ls_logger_list_t;

/* Function prototypes. */

/* main.c */
extern ls_logger_list_t* g_loggers;
extern int g_is_initialized;

int main(int argc, char **argv);
void reply(endpoint_t destination, message* msg);

/* requests.c */
int do_initialize();
int do_start_log(const char* logger, endpoint_t who);
int do_close_log(const char* logger, endpoint_t who);
int do_write_log(const char* logger, ls_severity_level_t severity, char* msg, int msg_len, endpoint_t who);
int do_clear_log(const char* logger);
int do_set_severity(const char* logger, ls_severity_level_t severity);
int do_clear_logs();
ls_logger_t* find_logger(const char* logger);
void ensure_initialized();
