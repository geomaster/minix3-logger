#include "inc.h"
#include <minix/endpoint.h>
#include "mini-printf.h"

/* SEF functions and variables. */
void sef_local_startup(void);

int wait_request(message* msg, ls_request_t* req);

ls_logger_list_t* g_loggers;
int g_is_initialized;

int valid_severity(int sev) {
	switch (sev) {
		case LS_SEV_TRACE:
		case LS_SEV_DEBUG:
		case LS_SEV_WARN:
		case LS_SEV_INFO:
			return TRUE;

		default:
			return FALSE;
	}
}

int main(int argc, char **argv)
{
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		uint16_t severity;
		ls_request_t req;
		message m;
		int result;

		if (wait_request(&m, &req) != OK) {
			result = EINVAL;
		} else switch (req.type) {
			case LS_INITIALIZE:
				result = do_initialize();
				break;

			case LS_START_LOG:
				result = do_start_log(m.m_ls_start_log.logger, m.m_source);
				break;

			case LS_CLOSE_LOG:
				result = do_close_log(m.m_ls_close_log.logger, m.m_source);
				break;

			case LS_WRITE_LOG:
				if (m.m_ls_write_log.message_len > LS_MAX_MESSAGE_LEN || !valid_severity(m.m_ls_write_log.severity)) {
					result = EINVAL;
				} else {
					result = do_write_log(m.m_ls_write_log.logger, m.m_ls_write_log.severity, m.m_ls_write_log.message, m.m_ls_write_log.message_len, m.m_source);

				}
				break;

			case LS_CLEAR_LOG:
				result = do_clear_log(m.m_ls_clear_log.logger);
				break;

			case LS_SET_SEVERITY:
				severity = m.m_ls_set_severity.severity;
				if (valid_severity(severity)) {
					result = do_set_severity(m.m_ls_set_severity.logger, (ls_severity_level_t)m.m_ls_set_severity.severity);
				} else {
					result = EINVAL;
				}

				break;

			case LS_CLEAR_ALL:
				result = do_clear_logs();
				break;

			default:
				result = EINVAL;
				break;
		}

		if (result != EDONTREPLY) {
			m.m_type = result;
			reply(req.source, &m);
		}
	}

	return OK;
}

void sef_local_startup()
{
	sef_startup();
}

int wait_request(message *msg, ls_request_t *req)
{
	int status = sef_receive(ANY, msg);
	if (OK != status) {
		LS_LOG_PRINTF(warn, "Failed to receive message from pid %d: %d", msg->m_source, status);
		return status;
	}

	req->source = msg->m_source;
	if (msg->m_type < LS_BASE || msg->m_type >= LS_END) {
		LS_LOG_PRINTF(warn, "Invalid message type %d from pid %d", msg->m_type, msg->m_source);
		return -1;
	}

	req->type = msg->m_type;
	return OK;
}

int ensure_initialized() {
	if (g_is_initialized) {
		return OK;
	}

	int ret = do_initialize();
	if (ret != OK) {
		LS_LOG_PRINTF(warn, "Initialization failed: %d", ret);
		return LS_ERR_INIT_FAILED;
	}

	g_is_initialized = TRUE;

	return OK;
}

void reply(endpoint_t destination, message *msg) {
	int s = ipc_send(destination, msg);
	if (OK != s) {
		LS_LOG_PRINTF(warn, "Unable to send reply to %d: %d", destination, s);
	}
}

ls_logger_list_t* find_logger(const char* logger) {
	if (!g_loggers) {
		return NULL;
	}

	for (ls_logger_list_t* l = g_loggers; l; l = l->tail) {
		if (strcmp(l->logger.name, logger) == 0) {
			return l;
		}
	}

	return NULL;
}
