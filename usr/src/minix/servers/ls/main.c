#include "inc.h"
#include <minix/endpoint.h>

/* SEF functions and variables. */
void sef_local_startup(void);

int wait_request(message* msg, ls_request_t* req);

ls_logger_list_t* g_loggers;

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
int main(int argc, char **argv)
{
	/* This is the main routine of this service. The main loop consists of 
	 * three major activities: getting new work, processing the work, and
	 * sending the reply. The loop never terminates, unless a panic occurs.
	 */

	uint16_t severity;
	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Main loop - get work and do it, forever. */
	while (TRUE) {
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
				if (m.m_ls_write_log.message_len > LS_MAX_MESSAGE_LEN) {
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
				switch (severity) {
					case LS_SEV_TRACE:
					case LS_SEV_DEBUG:
					case LS_SEV_WARN:
					case LS_SEV_INFO:
						result = do_set_severity(m.m_ls_set_severity.logger, (ls_severity_level_t)m.m_ls_set_severity.severity);
						break;

					default:
						result = EINVAL;
						break;
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

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
void sef_local_startup()
{
	sef_startup();
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
int wait_request(message *msg, ls_request_t *req)
{
	int status = sef_receive(ANY, msg);   /* blocks until message arrives */
	if (OK != status) {
		panic("LS: failed to receive message!: %d", status);
	}

	req->source = msg->m_source;
	if (msg->m_type < LS_BASE || msg->m_type >= LS_END) {
		printf("LS: invalid message type %d", msg->m_type);
		return -1;
	}

	req->type = msg->m_type;
	return OK;
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
void reply(endpoint_t destination, message *msg) {
	int s = ipc_send(destination, msg);
	if (OK != s)
		printf("LS: unable to send reply to %d: %d\n", destination, s);
}
