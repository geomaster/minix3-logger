#include "config-parse.h"
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "bufio.h"
#include "mini-printf.h"

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
	int curr_value_offset;

	ls_logger_t current_logger;
} parser_state_t;

parser_state_t* parse_init() {
	parser_state_t *state = (parser_state_t*)malloc(sizeof(parser_state_t));
	if (!state) {
		return NULL;
	}

	state->kind = PARSE_LOGGER_KEYWORD;
	state->line_no = 1;
	state->consume_offset = 0;
	state->curr_value_offset = 0;

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
		LS_LOG_PRINTF(info, "Unexpected character '%s' while consuming literal '%s'", translate_char(ch, chbuf), literal);
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
			res.kind = PARSE_OK; \
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
	if (state->kind == PARSE_OPEN_BRACE && state->consume_offset) {
		return TRUE;
	}

	// We are waiting for the first char of the logger keyword.
	if (state->kind == PARSE_LOGGER_KEYWORD && state->consume_offset > 0) {
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

parser_result_t parse_advance(parser_state_t* state, char ch, char lookahead) {
	if (ch == '\n') {
		/* Just so we can show a reasonable error message. */
		state->line_no++;
		state->char_no = 0;
	}
	state->char_no++;

	parser_result_t res;
	if ((is_white(ch) && is_whitespace_invariant(state)) ||
			(ch == '\n' && is_newline_invariant(state))) {
		set_parse_ok(&res);
		return res;
	}

	switch (state->kind) {
		/* States which consume fixed strings. */
		case PARSE_LOGGER_KEYWORD:
			// Skip newlines if we still haven't seen the start of the logger keyword.
			if (state->consume_offset == 0) {
				set_parse_ok(&res);
				return res;
			}

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
				// TODO: Take note of the logger name
				state->curr_value[state->curr_value_offset] = '\0';
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
			} else if (is_white(ch) || ch == '\n') {
				state->kind = PARSE_CONFIG_OPTION_EQUALS;
				state->curr_value[state->curr_value_offset] = '\0';

				LS_LOG_PRINTF(debug, "Option name = %s", state->curr_value);

				state->consume_offset = 0;
				set_parse_ok(&res);
			} else if (ch == '=') {
				// TODO: Take note of the option name
				state->kind = PARSE_CONFIG_VALUE;
				state->curr_value[state->curr_value_offset] = '\0';

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
				// TODO: Take note of the option value
				state->curr_value[state->curr_value_offset] = '\0';
				state->curr_value_offset = 0;

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

ls_logger_list_t* parse_config_file(const char* filename)
{
	int fd = open(filename, O_RDONLY);
	LS_LOG_PRINTF(info, "Parsing config file '%s'", filename);

	if (fd < 0) {
		LS_LOG_PRINTF(warn, "Failed opening file '%s'", filename);
		goto failure;
	}
	LS_LOG_PRINTF(debug, "Successfully opened '%s'", filename);

	bufio_t* bufio = bufio_init(fd);
	if (!bufio) {
		LS_LOG_PUTS(warn, "Failed to allocate bufio");
		goto failure;
	}
	LS_LOG_PUTS(debug, "Successfully allocated bufio");

	int c = bufio_next_char(bufio);
	parser_state_t* state = parse_init();
	if (!state) {
		LS_LOG_PUTS(warn, "Failed to init parser state");
		goto dealloc_bufio;
	}
	LS_LOG_PUTS(debug, "Successfully initialized parser state");

	parser_result_t res;
	while (c != BUFIO_EOF && c != BUFIO_ERR) {
		res = parse_advance(state, (char)c, '\0');
		switch (res.kind) {
			case PARSE_ERROR:
				LS_LOG_PRINTF(warn, "Parse error on line %d char %d.\n", res.error_line_no, res.error_char_no);
				goto dealloc_bufio;
				break;

			case PARSE_GOT_LOGGER:
				LS_LOG_PUTS(debug, "Got a logger");
				break;

			case PARSE_OK:
				break;
		}
		c = bufio_next_char(bufio);
	}

	bufio_free(bufio);
	return NULL;

dealloc_bufio:
	bufio_free(bufio);

failure:
	return NULL;
}
