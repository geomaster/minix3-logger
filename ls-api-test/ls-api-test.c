#include <minix/ls.h>
#include <minix/com.h>
#include <sys/errno.h>
#include <stdio.h>
#include <string.h>

#define OK 0

#undef assert
#define assert(x) \
	if (!(x)) { \
		printf("Assertion %s failed at line %d file %s\n", #x, __LINE__, __FILE__); \
		printf("\tRet is %d\n", ret); \
		return -1; \
	}

/* Copy the logs.conf from this directory to /etc to run
 * this example.
 */

int main() {
	int ret;

	// Test initialization.
	ret = minix_ls_initialize();
	assert( ret == OK );

	// Test starting a log.
	ret = minix_ls_start_log("FileLogger1");
	assert( ret == OK );

	// Test closing a log.
	ret = minix_ls_close_log("FileLogger1");
	assert( ret == OK );

	// Test opening a non-existent log
	ret = minix_ls_start_log("my_log");
	assert( ret == LS_ERR_NO_SUCH_LOGGER );

	// Test closing a non-existent log
	ret = minix_ls_close_log("my_log");
	assert( ret == LS_ERR_NO_SUCH_LOGGER );

	// Test closing a non-open log.
	ret = minix_ls_close_log("StdoutLogger1");
	assert( ret == LS_ERR_LOGGER_NOT_OPEN );

	// Test opening a log twice.
	ret = minix_ls_start_log("StdoutLogger1");
	assert( ret == OK );

	ret = minix_ls_start_log("StdoutLogger1");
	assert( ret == LS_ERR_LOGGER_OPEN );

	// Test opening and writing to a log.
	ret = minix_ls_start_log("FileLogger1");
	assert( ret == OK );
	
	ret = minix_ls_write_log("FileLogger1", "My message 1!", MINIX_LS_LEVEL_WARN);
	assert( ret == OK ); // We should see a line on in the FileLogger1 file
	
	ret = minix_ls_write_log("FileLogger1", "My message 2!", MINIX_LS_LEVEL_INFO);
	assert( ret == OK ); // We should see a line on in the FileLogger1 file

	ret = minix_ls_write_log("FileLogger1", "[should not be here] My message 3!", MINIX_LS_LEVEL_TRACE);
	assert( ret == OK ); // We should NOT see a line on in the FileLogger1 file

	// Test changing severity on an open logger
	ret = minix_ls_set_logger_level("FileLogger1", MINIX_LS_LEVEL_TRACE);
	assert( ret == LS_ERR_LOGGER_OPEN );

	// Test changing severity on an invalid logger
	ret = minix_ls_set_logger_level("my_log", MINIX_LS_LEVEL_DEBUG);
	assert( ret == LS_ERR_NO_SUCH_LOGGER );

	ret = minix_ls_close_log("FileLogger1");
	assert( ret == OK );

	// Test changing severity on a closed logger
	ret = minix_ls_set_logger_level("FileLogger1", MINIX_LS_LEVEL_TRACE);
	assert( ret == OK );

	ret = minix_ls_start_log("FileLogger1");
	assert( ret == OK );

	ret = minix_ls_write_log("FileLogger1", "My message 3!", MINIX_LS_LEVEL_TRACE);
	assert( ret == OK ); // We should now see a line in the FileLogger1 file because the level has changed

	ret = minix_ls_close_log("FileLogger1");
	assert( ret == OK );

	// Test setting severity to an invalid value
	ret = minix_ls_set_logger_level("FileLogger1", 0xbadf00d);
	assert( ret == -EINVAL );

	// Test writing to a closed log
	ret = minix_ls_write_log("FileLogger2", "My message!", MINIX_LS_LEVEL_WARN);
	assert( ret == LS_ERR_LOGGER_NOT_OPEN );

	// Test writing to a non-existant log
	ret = minix_ls_write_log("my_log", "My message!", MINIX_LS_LEVEL_TRACE);
	assert( ret == LS_ERR_NO_SUCH_LOGGER );

	// Test sending a big logger name
	ret = minix_ls_start_log("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
	assert( ret == -EINVAL );

	// Test sending a big message
	ret = minix_ls_start_log("FileLogger2");
	assert( ret == OK );

	static char big_msg_buf[16384];
	memset(big_msg_buf, 'A', 16383);
	big_msg_buf[16383] = '\0';

	ret = minix_ls_write_log("FileLogger2", big_msg_buf, MINIX_LS_LEVEL_WARN);
	assert( ret == -EINVAL );

	// Test clearing logs
	ret = minix_ls_start_log("ScratchLog1");
	assert( ret == OK );

	ret = minix_ls_start_log("ScratchLog2");
	assert( ret == OK );

	ret = minix_ls_write_log("ScratchLog1", "scratch msg 1", MINIX_LS_LEVEL_WARN);
	assert( ret == OK );

	ret = minix_ls_write_log("ScratchLog2", "scratch msg 2", MINIX_LS_LEVEL_WARN);
	assert( ret == OK );

	// Test clearing logs: when both are open
	ret = minix_ls_clear_logs("ScratchLog1,ScratchLog2");
	assert( ret == LS_ERR_LOGGER_OPEN );

	// Test clearing logs: when one is closed
	ret = minix_ls_close_log("ScratchLog1");
	assert( ret == OK );

	ret = minix_ls_clear_logs("ScratchLog1,ScratchLog2");
	assert( ret == LS_ERR_LOGGER_OPEN );

	// Test clearing logs: when both are closed
	ret = minix_ls_close_log("ScratchLog2");
	assert( ret == OK );
	
	ret = minix_ls_clear_logs("ScratchLog1,ScratchLog2");
	assert( ret == OK );

	// Test clearing nonexistent logs: both nonexistent
	ret = minix_ls_clear_logs("my_Log,my_log");
	assert( ret == LS_ERR_NO_SUCH_LOGGER );

	// Test clearing nonexistent logs: one nonexistent
	ret = minix_ls_clear_logs("ScratchLog1,my_log");
	assert( ret == LS_ERR_NO_SUCH_LOGGER );

	printf("It appears that tests are passing. Have fun!\n");
	return 0;
}
