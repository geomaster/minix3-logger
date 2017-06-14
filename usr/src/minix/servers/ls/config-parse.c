#include "config-parse.h"
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include "bufio.h"
#include "mini-printf.h"
#include <assert.h>

#define MAX_VALUE_LEN            2048

typedef enum parser_state_kind_t {
	PARSE_LOGGER_KEYWORD,
	PARSE_LOGGER_NAME,
	PARSE_OPEN_BRACE,
	PARSE_OPEN_BRACE_NEWLINE,
	PARSE_CONFIG_OPTION_NAME,
	PARSE_CONFIG_OPTION_EQUALS,
	PARSE_CONFIG_VALUE
} parser_state_kind_t;

typedef enum parser_result_kind_t {
	PARSE_OK,
	PARSE_GOT_LOGGER,
	PARSE_ERROR
} parser_result_kind_t;

typedef struct parser_result_t {
	parser_result_kind_t kind;
	int error_line_no;
	int error_char_no;
	ls_logger_t* logger;
} parser_result_t;

typedef struct parser_state_t {
	parser_state_kind_t kind;
	int line_no;
	int char_no;

	int consume_offset;

	char curr_value[MAX_VALUE_LEN];
	char option_name[MAX_VALUE_LEN];
	int curr_value_offset;

	int did_set_filename;
	int did_set_append;
	int did_set_type;
	int did_set_format;

	ls_logger_t current_logger;
} parser_state_t;

parser_state_t* parse_init() {
	parser_state_t *state = (parser_state_t*)malloc(sizeof(parser_state_t));
	if (!state) {
		return NULL;
	}

	state->kind = PARSE_LOGGER_KEYWORD;
	state->line_no = 1;
	state->char_no = 1;
	state->consume_offset = 0;
	state->curr_value_offset = 0;
	state->did_set_filename = FALSE;
	state->did_set_append = FALSE;
	state->did_set_type = FALSE;
	state->did_set_format = FALSE;

	memset(&state->current_logger, sizeof(ls_logger_t), 0);

	return state;
}

void parse_destroy(parser_state_t* state) {
	if (state) {
		free(state);
	}
}

// buf needs to be at least 3 bytes long.
char* translate_char(char ch, char* buf) {
	switch (ch) {
	case '\n': strncpy(buf, "\\n", 3); break;
	case '\t': strncpy(buf, "\\t", 3); break;
	case '\r': strncpy(buf, "\\r", 3); break;
	default:
		buf[0] = ch; buf[1] = '\0';
		break;
	}

	return buf;
}

int parse_consumption(parser_state_t* state, const char* literal, char ch, parser_state_kind_t next) {
	if (literal[state->consume_offset] == ch) {
		state->consume_offset++;
		if (state->consume_offset >= strlen(literal)) {
			state->kind = next;
			state->consume_offset = 0;
			state->curr_value_offset = 0;
		}

		return 0;
	} else {
		char chbuf[16];
		LS_LOG_PRINTF(warn, "Unexpected character '%s' while consuming literal '%s'", translate_char(ch, chbuf), literal);
		return -1;
	}
}

int is_white(char ch) {
	return ch == ' ' || ch == '\t';
}

#define TRY_PARSE(x) \
	do { \
		int ret = (x); \
		if (ret < 0) { \
			set_parse_error(&res, state); \
		} else { \
			set_parse_ok(&res); \
		} \
		\
		return res; \
	} while (0)

int is_whitespace_invariant(parser_state_t* state) {
	// We are reading a logger name and we have consumed at least one character
	// of it. Mustn't consume whitespace. The logger name state will stop
	// reading the name when it encounters a whitespace char.
	if (state->kind == PARSE_LOGGER_NAME && state->curr_value_offset > 0) {
		return FALSE;
	}

	// We are reading the `logger` keyword. Similarly.
	if (state->kind == PARSE_LOGGER_KEYWORD && state->consume_offset > 0) {
		return FALSE;
	}

	// We have started reading a config option.
	if (state->kind == PARSE_CONFIG_OPTION_NAME && state->curr_value_offset > 0) {
		return FALSE;
	}

	// We have started reading a config value. This is terminated only by a newline,
	// and all whitespace has to be included.
	if (state->kind == PARSE_CONFIG_VALUE && state->curr_value_offset > 0) {
		return FALSE;
	}

	// In all other cases, whitespace needs to be skipped.
	return TRUE;
}

int is_newline_invariant(parser_state_t* state) {
	// We are waiting for the opening brace after the logger name.
	if (state->kind == PARSE_OPEN_BRACE && state->consume_offset == 0) {
		return TRUE;
	}

	// We are waiting for the first char of the logger keyword.
	if (state->kind == PARSE_LOGGER_KEYWORD && state->consume_offset == 0) {
		return TRUE;
	}

	// We are waiting for an option name. This just means an empty line, which we
	// allow.
	if (state->kind == PARSE_CONFIG_OPTION_NAME && state->curr_value_offset == 0) {
		return TRUE;
	}

	return FALSE;
}

int is_allowed_in_logger_name(char ch) {
	return
		(ch >= 'A' && ch <= 'Z') ||
		(ch >= 'a' && ch <= 'z') ||
		(ch >= '0' && ch <= '9') ||
		(ch == '_');
}

int is_allowed_in_config_option_name(char ch) {
	return
		(ch >= 'a' && ch <= 'z') ||
		(ch >= '0' && ch <= '9');
}

void set_parse_error(parser_result_t* res, parser_state_t* state) {
	res->kind = PARSE_ERROR;
	res->error_line_no = state->line_no;
	res->error_char_no = state->char_no;
}

void set_parse_ok(parser_result_t* res) {
	res->kind = PARSE_OK;
}

int set_logger_dest_type(const char* dest_type, ls_logger_t* logger) {
	if (strcmp(dest_type, "file") == 0) {
		logger->dest_type = LS_DESTINATION_FILE;
	} else if (strcmp(dest_type, "stdout") == 0) {
		logger->dest_type = LS_DESTINATION_STDOUT;
	} else if (strcmp(dest_type, "stderr") == 0) {
		logger->dest_type = LS_DESTINATION_STDERR;
	} else {
		LS_LOG_PRINTF(warn, "Invalid logger destination '%s' for logger '%s'", dest_type, logger->name);
		LS_LOG_PUTS  (warn, "    (expected one of 'file', 'stdout', 'stderr')");
		return -1;
	}

	return 0;
}

int set_logger_severity(const char* severity, ls_logger_t* logger) {
	if (strcmp(severity, "trace") == 0) {
		logger->severity = LS_SEV_TRACE;
	} else if (strcmp(severity, "debug") == 0) {
		logger->severity = LS_SEV_DEBUG;
	} else if (strcmp(severity, "info") == 0) {
		logger->severity = LS_SEV_INFO;
	} else if (strcmp(severity, "warn") == 0) {
		logger->severity = LS_SEV_WARN;
	} else {
		LS_LOG_PRINTF(warn, "Invalid logger severity '%s' for logger '%s'", severity, logger->name);
		LS_LOG_PUTS  (warn, "    (expected one of 'trace', 'debug', 'info', 'warn')");
		return -1;
	}

	return 0;
}

int set_logger_append(const char* append, ls_logger_t* logger) {
	if (strcmp(append, "true") == 0) {
		logger->append = TRUE;
	} else if (strcmp(append, "false") == 0) {
		logger->append = FALSE;
	} else {
		LS_LOG_PRINTF(warn, "Invalid append value '%s' for logger '%s'", append, logger->name);
		LS_LOG_PUTS  (warn, "    (expected 'true' or 'false')");
		return -1;
	}

	return 0;
}

const char* trim(const char* str) {
	while (*str && is_white(*str)) {
		str++;
	}

	return str;
}

int set_logger_option(const char* option_name, const char* option_value, ls_logger_t* logger, parser_state_t* state) {
	option_name = trim(option_name);
	option_value = trim(option_value);

	if (strcmp(option_name, "destination") == 0) {
		state->did_set_type = TRUE;
		return set_logger_dest_type(option_value, logger);
	} else if (strcmp(option_name, "severity") == 0) {
		return set_logger_severity(option_value, logger);
	} else if (strcmp(option_name, "format") == 0) {
		state->did_set_format = TRUE;
		size_t len = strlen(option_value);
		if (len > LS_MAX_LOGGER_FORMAT_LEN - 1) {
			LS_LOG_PRINTF(warn, "Logger format string has length %d, which is longer than maximum allowed (%d)", (int)len, LS_MAX_LOGGER_FORMAT_LEN - 1);

			return -1;
		}

		memcpy(logger->format, option_value, len + 1);
		logger->format[LS_MAX_LOGGER_FORMAT_LEN - 1] = '\0';
	} else if (strcmp(option_name, "filename") == 0) {
		state->did_set_filename = TRUE;
		size_t len = strlen(option_value);
		if (len > LS_MAX_LOGGER_LOGFILE_PATH_LEN - 1) {
			LS_LOG_PRINTF(warn, "Logger destination filename string has length %d, which is longer than maximum allowed (%d)", (int)len, LS_MAX_LOGGER_LOGFILE_PATH_LEN - 1);

			return -1;
		}

		memcpy(logger->dest_filename, option_value, len + 1);
		logger->dest_filename[LS_MAX_LOGGER_LOGFILE_PATH_LEN - 1] = '\0';
	} else if (strcmp(option_name, "append") == 0) {
		state->did_set_append = TRUE;
		return set_logger_append(option_value, logger);
	} else {
		LS_LOG_PRINTF(warn, "Invalid option name '%s' for logger '%s'", option_name, logger->name);
		LS_LOG_PUTS  (warn, "    expected one of 'destination', 'filename', 'severity',");
		LS_LOG_PUTS  (warn, "                    'format', 'append'");

		return -1;
	}

	return 0;
}

parser_result_t parse_advance(parser_state_t* state, char ch, char lookahead) {
	if (ch == '\n') {
		/* Just so we can show a reasonable error message. */
		state->line_no++;
		state->char_no = 0;
	} else {
		state->char_no++;
	}

	parser_result_t res;
	if ((is_white(ch) && is_whitespace_invariant(state)) ||
			(ch == '\n' && is_newline_invariant(state))) {
		set_parse_ok(&res);
		return res;
	}

	switch (state->kind) {
		/* States which consume fixed strings. */
		case PARSE_LOGGER_KEYWORD:
			state->did_set_filename = FALSE;
			state->did_set_append = FALSE;
			state->did_set_type = FALSE;
			state->did_set_format = FALSE;
			TRY_PARSE(parse_consumption(state, "logger", ch, PARSE_LOGGER_NAME));
			break;

		case PARSE_OPEN_BRACE:
			TRY_PARSE(parse_consumption(state, "{", ch, PARSE_OPEN_BRACE_NEWLINE));
			break;

		case PARSE_OPEN_BRACE_NEWLINE:
			TRY_PARSE(parse_consumption(state, "\n", ch, PARSE_CONFIG_OPTION_NAME));
			break;

		case PARSE_CONFIG_OPTION_EQUALS:
			TRY_PARSE(parse_consumption(state, "=", ch, PARSE_CONFIG_VALUE));
			break;

		/* States which consume values and put them into the buffer. */
		case PARSE_LOGGER_NAME:
			if (is_allowed_in_logger_name(ch) &&
					state->curr_value_offset < LS_MAX_LOGGER_NAME_LEN - 1) {

				state->curr_value[state->curr_value_offset++] = ch;
				set_parse_ok(&res);
			} else if (is_white(ch) || ch == '\n') {
				memset(&state->current_logger, 0, sizeof(ls_logger_t));

				state->curr_value[state->curr_value_offset] = '\0';
				assert(state->curr_value_offset + 1 <= LS_MAX_LOGGER_NAME_LEN);
				memcpy(state->current_logger.name, state->curr_value, state->curr_value_offset + 1);
				LS_LOG_PRINTF(debug, "Logger name = %s", state->curr_value);

				state->kind = PARSE_OPEN_BRACE;
				state->consume_offset = 0;

				set_parse_ok(&res);
			} else {
				char chbuf[16];
				LS_LOG_PRINTF(warn, "Unexpected char '%s' in logger name", translate_char(ch, chbuf));
				set_parse_error(&res, state);
			}

			return res;
			break;

		case PARSE_CONFIG_OPTION_NAME:
			if (is_allowed_in_config_option_name(ch) &&
					state->curr_value_offset < MAX_VALUE_LEN - 1) {

				state->curr_value[state->curr_value_offset++] = ch;
				set_parse_ok(&res);
			} else if (ch == '}') {
				// TODO: Spit out a new logger
				res.kind = PARSE_GOT_LOGGER;
				state->kind = PARSE_LOGGER_KEYWORD;
				state->consume_offset = 0;
			} else if ((ch == '=' || is_white(ch)) && state->curr_value_offset > 0) {
				state->kind = (ch == '=' ? PARSE_CONFIG_VALUE : PARSE_CONFIG_OPTION_EQUALS);
				state->curr_value[state->curr_value_offset] = '\0';
				assert(state->curr_value_offset + 1 <= MAX_VALUE_LEN);
				memcpy(state->option_name, state->curr_value, state->curr_value_offset + 1);

				LS_LOG_PRINTF(debug, "Option name = %s", state->curr_value);

				state->consume_offset = 0;
				state->curr_value_offset = 0;
				set_parse_ok(&res);
			} else {
				char chbuf[16];
				LS_LOG_PRINTF(warn, "Unexpected char '%s' in option name", translate_char(ch, chbuf));
				set_parse_error(&res, state);
			}

			return res;
			break;

		case PARSE_CONFIG_VALUE:
			if (ch == '\n') {
				state->kind = PARSE_CONFIG_OPTION_NAME;
				state->curr_value[state->curr_value_offset] = '\0';
				if (set_logger_option(state->option_name, state->curr_value, &state->current_logger, state) != 0) {
					set_parse_error(&res, state);
					return res;
				}

				state->curr_value_offset = 0;
				state->consume_offset = 0;

				LS_LOG_PRINTF(debug, "Option value = %s", state->curr_value);
				set_parse_ok(&res);
			} else if (state->curr_value_offset < MAX_VALUE_LEN - 1) {
				state->curr_value[state->curr_value_offset++] = ch;
				set_parse_ok(&res);
			} else {
				char chbuf[16];
				LS_LOG_PRINTF(warn, "Unexpected char '%s' in option value", translate_char(ch, chbuf));
				set_parse_error(&res, state);
			}

			return res;
			break;
	}
}

int is_logger_valid(parser_state_t* state) {
	ls_logger_t* l = &state->current_logger;

	if (!state->did_set_format) {
		LS_LOG_PRINTF(warn, "Logger '%s' has no format option, but it is required", l->name);
		return FALSE;
	}

	if (!state->did_set_type) {
		LS_LOG_PRINTF(warn, "Logger '%s' has no destination option, but it is required", l->name);
		return FALSE;
	}

	if (state->did_set_filename && l->dest_type != LS_DESTINATION_FILE) {
		LS_LOG_PRINTF(warn, "Logger '%s' has a filename option, but its destination is not a file", l->name);
		return FALSE;
	}

	if (state->did_set_append && l->dest_type != LS_DESTINATION_FILE) {
		LS_LOG_PRINTF(warn, "Logger '%s' has an append option, but its destination is not a file", l->name);
		return FALSE;
	}

	if (l->dest_type == LS_DESTINATION_FILE && !state->did_set_filename) {
		LS_LOG_PRINTF(warn, "Logger '%s' has no filename option, but its destination is a file", l->name);
		return FALSE;
	}

	return TRUE;
}

ls_logger_list_t* parse_config_file(const char* filename)
{
	ls_logger_list_t *l, *nxt;

	int fd = open(filename, O_RDONLY);
	LS_LOG_PRINTF(info, "Parsing config file '%s'", filename);

	if (fd < 0) {
		LS_LOG_PRINTF(warn, "Failed opening file '%s': %d", filename, fd);
		goto failure;
	}
	LS_LOG_PRINTF(debug, "Successfully opened '%s'", filename);

	bufio_t* bufio = bufio_init(fd);
	if (!bufio) {
		LS_LOG_PUTS(warn, "Failed to allocate bufio");
		goto failure;
	}
	LS_LOG_PUTS(debug, "Successfully allocated bufio");

	parser_state_t* state = parse_init();
	if (!state) {
		LS_LOG_PUTS(warn, "Failed to init parser state");
		goto dealloc_bufio;
	}
	LS_LOG_PUTS(debug, "Successfully initialized parser state");

	ls_logger_list_t *head = NULL, *last = NULL;
	int nloggers = 0;

	parser_result_t res;
	int c = bufio_next_char(bufio);
	while (c != BUFIO_EOF && c != BUFIO_ERR) {
		res = parse_advance(state, (char)c, '\0');
		switch (res.kind) {
			case PARSE_ERROR:
				LS_LOG_PRINTF(warn, "Parse error on line %d char %d.\n", res.error_line_no, res.error_char_no);
				goto dealloc_bufio;
				break;

			case PARSE_GOT_LOGGER:
				LS_LOG_PUTS(debug, "Got a logger");
				if (!is_logger_valid(state)) {
					goto dealloc_loggers;
				}

				ls_logger_list_t* new = malloc(sizeof(ls_logger_list_t));
				if (!new) {
					LS_LOG_PUTS(warn, "Failed to allocate memory");
					goto dealloc_loggers;
				}

				new->logger = state->current_logger;
				if (!head) {
					head = new;
				}
				if (last) {
					last->tail = new;
				}
				last = new;
				nloggers++;

				break;

			case PARSE_OK:
				break;
		}
		c = bufio_next_char(bufio);
	}

	LS_LOG_PRINTF(info, "Successfully parsed config file and registered %d loggers", nloggers);

	return head;

	close(fd);
	bufio_free(bufio);
	return NULL;

dealloc_loggers:
	for (l = head; l; l = nxt) {
		nxt = l->tail;
		free(l);
	}

dealloc_bufio:
	bufio_free(bufio);

failure:
	return NULL;
}
