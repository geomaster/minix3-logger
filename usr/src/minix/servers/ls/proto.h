#pragma once

#include <minix/ipc.h>
#include <minix/com.h>

#define LS_MAX_LOGGER_NAME_LEN              32
#define LS_MAX_LOGGER_LOGFILE_PATH_LEN      64
#define LS_MAX_LOGGER_FORMAT_LEN			128

/* Data structures. */
typedef struct ls_request_t {
	endpoint_t source;
	int type;
} ls_request_t;

typedef enum ls_log_destination_t {
	LS_LOG_DESTINATION_FILE,
	LS_LOG_DESTINATION_STDERR,
	LS_LOG_DESTINATION_STDOUT
} ls_log_destination_t;

typedef enum ls_severity_level_t {
	LS_SEV_LEVEL_TRACE,
	LS_SEV_LEVEL_DEBUG,
	LS_SEV_LEVEL_ERROR
} ls_severity_level_t;

typedef struct ls_logger_t {
	char name[LS_MAX_LOGGER_NAME_LEN];
	ls_log_destination_t dest_type;
	ls_severity_level_t default_severity;
	char dest_filename[LS_MAX_LOGGER_LOGFILE_PATH_LEN];
	char format[LS_MAX_LOGGER_FORMAT_LEN];
	int append;
} ls_logger_t;

typedef struct ls_logger_list_t {
	ls_logger_t logger;
	struct ls_logger_list_t* tail;
} ls_logger_list_t;

/* Function prototypes. */

/* main.c */
int main(int argc, char **argv);
void reply(endpoint_t destination, message* msg);

/* requests.c */
int do_initialize(message *msg, const ls_request_t* req);
