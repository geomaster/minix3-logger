#include "proto.h"
#include "config-parse.h"
#include "inc.h"

int do_initialize(message* msg, const ls_request_t* req) {
	parse_config_file("/etc/logs.conf");
	return OK;
}
