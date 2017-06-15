#pragma once

/* Possible levels for log messages. */
#define MINIX_LS_LEVEL_TRACE              0
#define MINIX_LS_LEVEL_DEBUG              1
#define MINIX_LS_LEVEL_INFO               2
#define MINIX_LS_LEVEL_WARN               3

typedef int minix_ls_log_level_t;

/*
 * Explicitly initializes the logging server. This includes parsing of the
 * configuration file. If not called explicitly, this initialization will be done
 * on the next call to other ls functions. The configuration file is read from
 * /etc/logs.conf.
 *
 * Return values:
 *     LS_ERR_EXTERNAL:    An error external to ls has occured.
 *     LS_ERR_INIT_FAILED: An internal initialization error has occured. This is
 *                         most likely due to a bad config file. The kernel logs
 *                         should have more info about what went wrong.
 */
int minix_ls_initialize(void);

/*
 * Starts a given logger. This opens the necessary files and ensures that the
 * logger is not currently open by another process. Calling minix_ls_start_log is
 * required before writing to the log. The severity of the log is set to the
 * default severity as defined in the configuration file.
 *
 * Params:
 *     logger:                A null-terminated string containing the logger name.
 *
 * Return values:
 *     LS_ERR_INIT_FAILED:    An internal initialization error has occured. This
 *                            is most likely due to a bad config file. The kernel
 *                            logs should have more info about what went wrong.
 *     LS_ERR_NO_SUCH_LOGGER: There doesn't exist a logger by this name. Check
 *                            your configuration file for errors and ensure the
 *                            logger is defined there.
 *     LS_ERR_LOGGER_OPEN:    The log is already open. Close the log and try
 *                            again. Keep in mind that only the process that
 *                            opened the log can close it.
 *     LS_ERR_EXTERNAL:       An error external to ls has occured.
 *     EINVAL:                The logger name is too big to fit into an IPC
 *                            message.
 */
int minix_ls_start_log(const char* logger);

/*
 * Closes a given logger. Can only be called by the process that last
 * successfully called minix_ls_start_log on tis logger.
 *
 * Params:
 *     logger:                   A null-terminated string containing the logger name.
 *
 * Return values:
 *     LS_ERR_INIT_FAILED:       An internal initialization error has occured. This
 *                               is most likely due to a bad config file. The
 *                               kernel logs should have more info about what
 *                               went wrong.
 *     LS_ERR_NO_SUCH_LOGGER:    There doesn't exist a logger by this name. Check
 *                               your configuration file for errors and ensure
 *                               the logger is defined there.
 *     LS_ERR_LOGGER_NOT_OPEN:   This logger is not open.
 *     LS_ERR_PERMISSION_DENIED: The calling process doesn't have the permission
 *                               to close this logger. Keep in mind that the
 *                               logger can be closed only by the process that
 *                               opened it.
 *     LS_ERR_EXTERNAL:          An error external to ls has occured.
 *     EINVAL:                   The logger name is too big to fit into an IPC
 *                               message.
 */
int minix_ls_close_log(const char* logger);

/*
 * Sets the minimum severity level for logging. Messages below this level will
 * not be logged. For example, writing a log message of level
 * MINIX_LS_LEVEL_DEBUG when the severity level is set to MINIX_LS_LEVEL_INFO
 * will not write anything to the log, while a message of MINIX_LS_LEVEL_DEBUG or
 * MINIX_LS_LEVEL_TRACE will. The logger must be closed in order to set the
 * severity level.
 *
 * Params:
 *     logger:                 A null-terminated string containing the logger
 *                             name.
 *     new_level:              Severity level to set the logger to. Must be one
 *                             of the MINIX_LS_LEVEL_* constants.
 *
 * Return values:
 *     LS_ERR_INIT_FAILED:     An internal initialization error has occured. This
 *                             is most likely due to a bad config file. The
 *                             kernel logs should have more info about what went
 *                             wrong.
 *     LS_ERR_NO_SUCH_LOGGER:  There doesn't exist a logger by this name. Check
 *                             your configuration file for errors and ensure the
 *                             logger is defined there.
 *     LS_ERR_LOGGER_OPEN:     The logger is open, so its severity cannot be
 *                             changed.
 *     EINVAL:                 The log level given is invalid, or the logger name
 *                             is too big to fit in an IPC message.
 */
int minix_ls_set_logger_level(const char* logger, minix_ls_log_level_t
		new_level);

/*
 * Writes a message to the log. The logger must be open by te calling process in
 * order for this call to succeed.
 *
 * Params:
 *     logger:                   A null-terminated string containing the logger
 *                               name.
 *     message:                  A null-terminated string containing the message for
 *                               this log line. The special formatting string
 *                               '%m' inside the logger's format, as defined in
 *                               the configuration file, will be substituted for
 *                               this message in the final log output.
 *     message_level:            Severity level of the message. Must be one of the
 *                               MINIX_LS_LEVEL_* constants. If the level is
 *                               lower in severity than the current severity
 *                               level of the logger, the message will not be
 *                               output to the log.
 *
 * Return values:
 *     LS_ERR_INIT_FAILED:       An internal initialization error has occured. This
 *                               is most likely due to a bad config file. The
 *                               kernel logs should have more info about what
 *                               went wrong.
 *     LS_ERR_NO_SUCH_LOGGER:    There doesn't exist a logger by this name. Check
 *                               your configuration file for errors and ensure the
 *                               logger is defined there.
 *     LS_ERR_LOGGER_NOT_OPEN:   This logger is not open.
 *     LS_ERR_PERMISSION_DENIED: The calling process doesn't have the permission
 *                               to close this logger. Keep in mind that the
 *                               logger can be written to only by the process
 *                               that opened it.
 *     LS_ERR_EXTERNAL:          An error external to ls has occured.
 *     EINVAL:                   The log level given is invalid, the logger name
 *                               is too big to fit in an IPC message, or the
 *                               message is too big.
 */
int minix_ls_write_log(const char* logger, const char* message,
		minix_ls_log_level_t message_level);

/*
 * Clears specific or all logs. This truncates the files backing the logs back to
 * zero size. None of the logs must be open. If any of the logs is open, an error
 * will be returned, but closing will not stop, i.e. it is guaranteed that all
 * logs that can be closed will be closed after this function returns.
 *
 * Params:
 *     loggers:               A null-terminated, comma-separated, nullable string
 *                            containing the list of loggers whose logs to clear.
 *                            If null, all logs are cleared.
 *
 * Return values:
 *     LS_ERR_INIT_FAILED:    An internal initialization error has occured. This
 *                            is most likely due to a bad config file. The kernel
 *                            logs should have more info about what went wrong.
 *     LS_ERR_NO_SUCH_LOGGER: There doesn't exist a logger by a name given in the
 *                            list. Check your configuration file for errors and
 *                            ensure the logger is defined there.
 *     LS_ERR_LOGGER_OPEN:    One of the loggers was open when attempting to
 *                            clear its logs.
 *     LS_ERR_EXTERNAL:       An error external to ls has occured.
 *     EINVAL:                The logger name is too big to fit in an IPC message.
 */
int minix_ls_clear_logs(const char* loggers);
