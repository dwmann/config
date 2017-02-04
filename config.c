/*
 * configuration file parser
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"


/*
 * constants
 */

static const int BUFFER_SIZE = 1024;

static const char HANDLER_ERROR[] = "error assigning section.key = value";
static const char MISSING_KEY[] = "a key is required";
static const char UNEXPECTED_EOL[] = "unexpected end of line reading config file";
static const char UNEXPECTED_CHARACTER[] = "unexpected character reading config file";


/*
 * local functions
 */

static const char *parseSection(char *section, char *buffer);

static const char *parseKeyValue(char *sectionKey, char *value, char *buffer, CONFIG_HANDLER_FUNC *handler);


/*
 * parse a configuration
 */
int parseConfig(char *fileName, CONFIG_HANDLER_FUNC *handler) {
	FILE *fp = NULL;
	const char *errorString = NULL;
	int line = 1;
	int rc = 1;
	char *buffer = malloc(BUFFER_SIZE * 5);
	char *section = buffer + BUFFER_SIZE;
	char *sectionKey = section + BUFFER_SIZE;
	char *value = sectionKey + (BUFFER_SIZE * 2);
	/* prepare input file and memory */
	if (buffer == NULL) {
		errorString = "unable to allocate memory to read configuration file";
		goto cleanup;
	}
	if ((fp = fopen(fileName, "r")) == NULL) {
		sprintf(buffer, "unable to open configuration file [%s]", fileName);
		errorString = buffer;
		goto cleanup;
	}
	/* read the input file, one line at a time */
	while (fgets(buffer, BUFFER_SIZE, fp)) {
		char *s = buffer;
		while (isspace(*s)) {
			s++;
		}
		/* section key */
		if (*s == '[') {
			if ((errorString = parseSection(section, s)) != NULL) {
				goto cleanup;
			}
		/* key = value */
		} else if (*s != ';' && *s != 0) {
			strncpy(sectionKey, section, BUFFER_SIZE);
			if ((errorString = parseKeyValue(sectionKey, value, s, handler)) != NULL) {
				goto cleanup;
			}
		}
		line++;
	}
	/* fail on error */
	if (ferror(fp) != 0) {
		sprintf(buffer, "error reading configuration file: %i", errno);
		errorString = buffer;
		goto cleanup;
	}
	rc = 0;
cleanup:
	if (errorString != NULL) {
		fprintf(stderr, "%s at line %i\n", errorString, line);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	if (fp != NULL) {
		fclose(fp);
	}
	return(rc);
}


/*
 * parse a section key
 */
static const char *parseSection(char *section, char *input) {
	int len = 0;
	char haveSection = 0;
	input++;
	while (*input != 0  && len < BUFFER_SIZE - 1) {
		/* on close bracket, finish parsing section key */
		if (*input == ']') {
			*section = 0;
			haveSection = 1;
		/* have a comment before section close, then error */
		} else if (*input == ';') {
			if (haveSection == 0) {
				return(UNEXPECTED_EOL);
			} else {
				return(NULL);
			}
		/* whitespace inside section is ignored */
		} else if (!isspace(*input)) {
			if (haveSection == 0) {
				*section++ = tolower(*input);
				len++;
			} else {
				return(UNEXPECTED_CHARACTER);
			}
		}
		input++;
	}
	if (haveSection == 0) {
		return(UNEXPECTED_EOL);
	}
	return(NULL);
}


/*
 * parse a key = value
 */
static const char *parseKeyValue(char *sectionKey, char *value, char *buffer, CONFIG_HANDLER_FUNC *handler) {
	const char ctlChars[] = "\a\b  \e\f       \n   \r \t \v    ";
	char *env = NULL;
	int keyLen = strlen(sectionKey);
	int valueLen = 0;
	char haveKey = 0;
	char esc = 0;
	/* get key */
	if (keyLen > 0) {
		sectionKey[keyLen++] = '.';
	}
	while (*buffer != 0 && keyLen < (BUFFER_SIZE * 2) - 1) {
		/* all whitespace in key name is ignored */
		if (!isspace(*buffer)) {
			/* stop on = */
			if (*buffer == '=') {
				sectionKey[keyLen] = 0;
				buffer++;
				break;
			/* a comment before = is a syntax error */
			} else if (*buffer == ';') {
				return(UNEXPECTED_CHARACTER);
			/* anything else is part of the key name */
			} else {
				sectionKey[keyLen++] = tolower(*buffer);
				haveKey = 1;
			}
		}
		buffer++;
	}
	if (haveKey != 1) {
		return(MISSING_KEY);
	}
	/* get value */
	while (*buffer != 0 && valueLen < BUFFER_SIZE - 1) {
		if (isspace(*buffer)) {
			if (valueLen > 0 && env == NULL && *buffer != '\n' && *buffer != '\r') {
				value[valueLen++] = *buffer;
			}
		} else {
			/* escape character */
			if (*buffer == '\\') {
				buffer++;
				esc = 1;
			/* environment variable */
			} else if (*buffer == '$' && *(buffer + 1) == '{') {
				buffer += 2;
				env = buffer;
			/* end environment variable name */
			} else if (*buffer == '}' && env != NULL) {
				*buffer = 0;
				char *es = getenv(env);
				if (es != NULL) {	
					strcpy((value + valueLen), es);
					valueLen = strlen(value);
					env = NULL;
				} else {
					*(value + valueLen) = 0;
				}
			/* comment */
			} else if (*buffer == ';') {
				break;
			}
			/* if not parsing an environment variable name... */
			if (env == NULL) {
				char ch = tolower(*buffer);
				/* if we're escaping and the current character is a control character, store that control character */
				if (esc == 1 && (ch == 'a' || ch  == 'b' || ch == 'e' || ch == 'f' || ch == 'n' || ch == 'r' || ch == 't' || ch == 'v')) {
					value[valueLen++] = ctlChars[tolower(*buffer) - 'a'];
				/* otherwise, store next character after escape */
				} else {
					value[valueLen++] = *buffer;
				}
			}
		}
		esc = 0;
		buffer++;
	}
	/* call user handler */
	value[valueLen] = 0;
	if (handler(sectionKey, value)) {
		return(HANDLER_ERROR);
	}
	return(NULL);
}

