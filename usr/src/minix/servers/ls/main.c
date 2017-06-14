/* Data Store Server. 
 * This service implements a little publish/subscribe data store that is 
 * crucial for the system's fault tolerance. Components that require state
 * can store it here, for later retrieval, e.g., after a crash and subsequent
 * restart by the reincarnation server. 
 * 
 * Created:
 *   Oct 19, 2005	by Jorrit N. Herder
 */

#include "inc.h"	/* include master header file */
#include <minix/endpoint.h>

/* SEF functions and variables. */
void sef_local_startup(void);

int wait_request(message* msg, ls_request_t* req);

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
int main(int argc, char **argv)
{
	/* This is the main routine of this service. The main loop consists of 
	 * three major activities: getting new work, processing the work, and
	 * sending the reply. The loop never terminates, unless a panic occurs.
	 */

	int result;                 

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
				result = do_initialize(&m, &req);
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
