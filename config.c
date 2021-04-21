/* config.c
 *
 * Configuration file parser.
 * 
 * MIT License
 *
 * Copyright (c) 2017 Warren Mann (warren@nonvol.io)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
*/

#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"


/* LOCAL CONSTANTS */

/* Character that marks the start of a section declaration. */
static const char CONFIG_CHAR_SECTION_OPEN = '[';

/* Character that marks the end of a section declaration. This may be the same
 * as CONFIG_CHAR_SECTION_OPEN with no ill effects. */
static const char CONFIG_CHAR_SECTION_CLOSE = ']';

/* Character signifying a comment. */
static const char CONFIG_CHAR_COMMENT = ';';

/* Character signifying a newline */
static const char CONFIG_CHAR_NEWLINE = '\n';

/* Character used for assignment */
static const char CONFIG_CHAR_ASSIGNMENT = '=';

/* Character used to delimit values. */
static const char CONFIG_CHAR_VALUE_DELIMITER = '"';

/* Escape character to use within delimited values. */
static const char CONFIG_CHAR_ESCAPE = '\\';

static const char KEY_CHAR_ARRAY[] = {'_', '-'};
static const char SECTION_CHAR_ARRAY[] = {'_', '-'};
static const char IO_EOF = 0;
static const int BUFFER_SIZE = 1024;
static const int MEMORY_FRAGMENT_SIZE = 32;

static const char *error_string[] = {
    [CONFIG_ERROR_NONE] = "No error",
    [CONFIG_ERROR_NO_FILENAME] = "Invalid filename",
    [CONFIG_ERROR_NO_OPEN] = "Unable to open file",
    [CONFIG_ERROR_NO_MEMORY] = "Unable to allocate memory",
    [CONFIG_ERROR_UNEXPECTED_CHARACTER] = "Unexpected character",
    [CONFIG_ERROR_PREMATURE_EOF] = "Unexpected end of file",
    [CONFIG_ERROR_UNEXPECTED_EOL] = "Unexpected end of line",
    [CONFIG_ERROR_COUNT] = "Unknown error"
};


/* LOCAL DATA */

static int file_handle = -1;
static char *io_buffer = NULL;
static char *io_buffer_ptr = NULL;
static char *current_section = NULL;
static int io_buffer_len = 0;
static unsigned long int line = 1;
static config_handler_func_t *assignment_handler;


/* LOCAL FUNCTIONS */

static char io_next(void);
static char io_peek(void);
static int is_key_character(char ch);
static int is_section_character(char ch);
static config_error_t parse_key_character(char **key_ptr);
static config_error_t parse_assignment_operator(void);
static config_error_t parse_delimited_value(char **value_ptr);
static config_error_t parse_raw_value(char **value_ptr);
static config_error_t parse_value(char **value_ptr);
static config_error_t parse_assignment(void);
static config_error_t parse_comment(void);
static config_error_t parse_section(void);
static config_error_t read_file(void);


/* Returns the current character in the input stream and advances the input
 * pointer.
 * 
 * PARAMETERS
 * 
 * 	none
 * 
 * RETURNS
 * 
 * 	Current character in the input stream, or IO_EOF if at the end of the file 
 *  or no file open.
 */
static char io_next(void) {

    char ch = io_peek();

    if (ch != IO_EOF) {
    	io_buffer_len--;
        io_buffer_ptr++;
    }

	return(ch);
}


/* Returns the current character in the input stream without advancing the 
 * input pointer.
 *
 * PARAMETERS
 *
 * 	none
 * 
 * RETURNS
 * 
 *	Current character in the input stream, or IO_EOF if at the end of the file.
 */
static char io_peek(void) {

	if (file_handle == -1 || io_buffer == NULL || io_buffer_ptr == NULL) {
		return(IO_EOF);
	}

	if (io_buffer_len == 0) {
		io_buffer_len = read(file_handle, io_buffer, BUFFER_SIZE);
		if (io_buffer_len <= 0) {
			return(IO_EOF);
		}
		io_buffer_ptr = io_buffer;
	}

	return(*io_buffer_ptr);
}


/* Returns non-zero if the given character is acceptable as a key character.
 * 
 * PARAMETERS
 * 
 *  ch
 *  Character to test.
 * 
 * RETURNS
 * 
 *  0 = Character is not acceptable in a key.
 *  1 = Character is an acceptable key character.
 */
static int is_key_character(char ch) {

    int i;

    /* alphabetic characters are always valid key characters */
    if (isalnum(ch)) {
        return(1);
    }

    /* check for any other key characters */
    for (i = 0; i < sizeof(KEY_CHAR_ARRAY); i++) {
        if (KEY_CHAR_ARRAY[i] == ch) {
            return(1);
        }
    }

    return(0);
}


/* Returns non-zero if the given character is acceptable as a section 
 * character.
 * 
 * PARAMETERS
 * 
 *  ch
 *  Character to test.
 * 
 * RETURNS
 * 
 *  0 = Character is not acceptable in a section.
 *  1 = Character is an acceptable section character.
 */
static int is_section_character(char ch) {

    int i;

    /* alphabetic characters are always valid section characters */
    if (isalnum(ch)) {
        return(1);
    }

    /* check for any other section characters */
    for (i = 0; i < sizeof(SECTION_CHAR_ARRAY); i++) {
        if (SECTION_CHAR_ARRAY[i] == ch) {
            return(1);
        }
    }

    return(0);
}


/* Parse a key from the input stream.
 * 
 * PARAMETERS
 * 
 *  key_ptr
 *  Pointer to a char pointer where the pointer to the buffer containing the 
 *  key will be stored.
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_key(char **key_ptr) {

    char *temp_ptr;
    char *key = NULL;
    unsigned long int key_total = MEMORY_FRAGMENT_SIZE;
    unsigned long int key_now = 0;
    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;

    /* Allocate buffer to parse the key name into */
    key = malloc(MEMORY_FRAGMENT_SIZE);
    if (key == NULL) {
        rc = CONFIG_ERROR_NO_MEMORY;
        goto term;
    }
    key_total = MEMORY_FRAGMENT_SIZE;

    /* Parse the key name */
    while (1) {
        ch = io_peek();
        /* EOF is unexpected here */
        if (ch == IO_EOF) {
            rc = CONFIG_ERROR_PREMATURE_EOF;
            goto term;
        /* An assignment operator is terminal for the key name */
        } else if (ch == CONFIG_CHAR_ASSIGNMENT) {
            break;
        /* Comments are not expected here */
        } else if (ch == CONFIG_CHAR_COMMENT) {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            goto term;
        /* Newline is okay, need to increment line number */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            line++;
            io_next();
            break;
        /* Whitespace terminates the key name */
        } else if (isspace(ch)) {
            io_next();
            break;
        /* Check that the character is valid in a key name */
        } else if (is_key_character(ch)) {
            io_next();
            if (key_now == key_total) {
                key_total += MEMORY_FRAGMENT_SIZE;
                temp_ptr = realloc(key, key_total);
                if (temp_ptr == NULL) {
                    rc = CONFIG_ERROR_NO_MEMORY;
                    goto term;
                } else {
                    key = temp_ptr;
                }
            }
            key[key_now++] = ch;
        /* Any other character is invalid */
        } else {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            goto term;
        }
    }

    /* NULL-terminate the key */
    if (key_now == key_total) {
        key_total++;
        temp_ptr = realloc(key, key_total);
        if (temp_ptr == NULL) {
            rc = CONFIG_ERROR_NO_MEMORY;
            goto term;
        } else {
            key = temp_ptr;
        }
    }
    key[key_now++] = 0;

term:

    if (rc != CONFIG_ERROR_NONE) {
        if (key != NULL) {
            free(key);
        }
        key = NULL;
    }

    *key_ptr = key;

    return(rc);
}


/* Verifies that the assignment operator appears as expected after the key
 * and any separating whitespace. Any other characters are invalid.
 * 
 * PARAMETERS
 * 
 *  none
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_assignment_operator(void) {

    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;

    /* Look for an assignment operator. The only other character allowed is 
     * whitespace. */
    while (1) {
        ch = io_peek();
        /* EOF is premature here and invalid */
        if (ch == IO_EOF) {
            rc = CONFIG_ERROR_PREMATURE_EOF;
            break;
        /* The assignment operator has been found, return success */
        } else if (ch == CONFIG_CHAR_ASSIGNMENT) {
            io_next();
            break;
        /* Newline is treated as whitespace, but need to increment line 
         * counter */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            line++;
            io_next();
        /* Whitespace is okay and is just skipped */
        } else if (isspace(ch)) {
            io_next();
        } else {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            break;
        }
    }

    return(rc);
}


/* Parses the value part of an assignment when the value is enclosed in 
 * delimiters (default "").
 *
 * PARAMETERS
 * 
 *  value_ptr
 *  Pointer to a char pointer where the address of the buffer containing the
 *  parsed, null-terminated value will be stored.
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_delimited_value(char **value_ptr) {

    char *temp_ptr;
    char *value = NULL;
    unsigned long int value_total = MEMORY_FRAGMENT_SIZE;
    unsigned long int value_now = 0;
    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;
    char escape_flag = 0;

    /* Skip past the delimiter read in caller. */
    io_next();

    /* Allocate buffer to parse the value into. */
    value = malloc(MEMORY_FRAGMENT_SIZE);
    if (value == NULL) {
        rc = CONFIG_ERROR_NO_MEMORY;
        goto term;
    }
    value_total = MEMORY_FRAGMENT_SIZE;

    /* Parse the value. */
    while (1) {
        ch = io_peek();
        /* EOF is invalid here. */
        if (ch == IO_EOF) {
            rc = CONFIG_ERROR_PREMATURE_EOF;
            goto term;
        /* Newline is invalid here. */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            rc = CONFIG_ERROR_UNEXPECTED_EOL;
            goto term;
        /* Anything else is copied to the value */
        } else {
            io_next();
            if (value_now == value_total) {
                value_total += MEMORY_FRAGMENT_SIZE;
                temp_ptr = realloc(value, value_total);
                if (temp_ptr == NULL) {
                    rc = CONFIG_ERROR_NO_MEMORY;
                    goto term;
                } else {
                    value = temp_ptr;
                }
            }
            /* Some escape sequences are recognized and substituted here. */
            if (escape_flag == 1) {
                escape_flag = 0;
                if (ch == 'a' || ch == 'A') {
                    ch = '\a';
                } else if (ch == 'b' || ch == 'B') {
                    ch = '\b';
                } else if (ch == 'f' || ch == 'F') {
                    ch = '\f';
                } else if (ch == 'n' || ch == 'N') {
                    ch = '\n';
                } else if (ch == 'r' || ch == 'R') {
                    ch = '\r';
                } else if (ch == 't' || ch == 'T') {
                    ch = '\t';
                } else if (ch == 'v' || ch == 'V') {
                    ch = '\v';
                }
            /* If not itself escaped, the esc character flags the next character as
            * escaped in delimited values. */
            } else if (ch == CONFIG_CHAR_ESCAPE) {
                escape_flag = 1;
            /* If not escaped, a value delimiter completes parsing of the value. */
            }  else if (ch == CONFIG_CHAR_VALUE_DELIMITER) {
                break;
            }
            if (escape_flag == 0) {
                value[value_now++] = ch;
            }
        }
    }

    /* NULL-terminate the value */
    if (value_now == value_total) {
        value_total++;
        temp_ptr = realloc(value, value_total);
        if (temp_ptr == NULL) {
            rc = CONFIG_ERROR_NO_MEMORY;
            goto term;
        } else {
            value = temp_ptr;
        }
    }
    value[value_now++] = 0;

term:

    if (rc != CONFIG_ERROR_NONE) {
        if (value != NULL) {
            free(value);
        }
        value = NULL;
    }

    *value_ptr = value;

    return(rc);
}


/* Parses the value part of an assignment when the value is not enclosed in
 * delimiters.
 *
 * PARAMETERS
 * 
 *  value_ptr
 *  Pointer to a char pointer where the address of the buffer containing the
 *  parsed, null-terminated value will be stored.
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_raw_value(char **value_ptr) {

    char *temp_ptr;
    char *value = NULL;
    unsigned long int value_total = MEMORY_FRAGMENT_SIZE;
    unsigned long int value_now = 0;
    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;

    /* Allocate buffer to parse the key name into */
    value = malloc(MEMORY_FRAGMENT_SIZE);
    if (value == NULL) {
        rc = CONFIG_ERROR_NO_MEMORY;
        goto term;
    }
    value_total = MEMORY_FRAGMENT_SIZE;

    /* Parse the value */
    while (1) {
        ch = io_peek();
        /* EOF is valid here and marks the end of the value. */
        if (ch == IO_EOF) {
            break;
        /* A newline marks the end of the value. */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            break;
        /* Whitespace ends the value. */
        }  else if (isspace(ch)) {
            break;
        /* A comment character ends the value if we aren't using a delimiter */
        } else if (ch == CONFIG_CHAR_COMMENT) {
            break;
        /* Anything else is copied to the value */
        } else {
            io_next();
            if (value_now == value_total) {
                value_total += MEMORY_FRAGMENT_SIZE;
                temp_ptr = realloc(value, value_total);
                if (temp_ptr == NULL) {
                    rc = CONFIG_ERROR_NO_MEMORY;
                    goto term;
                } else {
                    value = temp_ptr;
                }
            }
            value[value_now++] = ch;
        }
    }

    /* NULL-terminate the value */
    if (value_now == value_total) {
        value_total++;
        temp_ptr = realloc(value, value_total);
        if (temp_ptr == NULL) {
            rc = CONFIG_ERROR_NO_MEMORY;
            goto term;
        } else {
            value = temp_ptr;
        }
    }
    value[value_now++] = 0;

term:

    if (rc != CONFIG_ERROR_NONE) {
        if (value != NULL) {
            free(value);
        }
        value = NULL;
    }

    *value_ptr = value;

    return(rc);
}


/* Parses the value part of an assignment.
 *
 * PARAMETERS
 * 
 *  value_ptr
 *  Pointer to a char pointer where the address of the buffer containing the
 *  parsed, null-terminated value will be stored.
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_value(char **value_ptr) {

    char ch;

    /* Skip leading whitespace */
    while (1) {
        ch = io_peek();
        if (ch == IO_EOF) {
            return CONFIG_ERROR_PREMATURE_EOF;
        } else if (ch == CONFIG_CHAR_COMMENT) {
            return CONFIG_ERROR_UNEXPECTED_CHARACTER;
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            line++;
            ch = io_next();
        } else if (isspace(ch)) {
            ch = io_next();
        } else {
            break;
        }
    }

    /* Call appropriate parser, based on if a delimiter was encountered. */
    if (ch == CONFIG_CHAR_VALUE_DELIMITER) {
        return parse_delimited_value(value_ptr);
    } else {
        return parse_raw_value(value_ptr);
    }
}


/* Parses an assignment. Once the key and value are determined, the callback 
 * function is invoked with them as parameters.
 * 
 * PARAMETERS
 * 
 *  none
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_assignment(void) {

    config_error_t rc;
    char *key = NULL;
    char *value = NULL;

    /* Parse the key */
    rc = parse_key(&key);
    if (rc != CONFIG_ERROR_NONE) {
        goto term;
    }

    /* Parse the assignment operator */
    rc = parse_assignment_operator();
    if (rc != CONFIG_ERROR_NONE) {
        goto term;
    }

    /* Parse the value to assign to the key */
    rc = parse_value(&value);
    if (rc != CONFIG_ERROR_NONE) {
        goto term;
    }

    /* Invoke the caller's assignment handler callback with the parsed data */
    rc = assignment_handler(current_section, key, value);

term:

    if (key != NULL) {
        free(key);
    }

    if (value != NULL) {
        free(value);
    }

    return(rc);
}


/* Parses a comment. The results are discarded and this merely looks for 
 * correct syntax to the end of line.
 * 
 * PARAMETERS
 * 
 *  none
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_comment(void) {

    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;

    while (1) {
        ch = io_peek();
        /* EOF is fine here, just bail out */
        if (ch == IO_EOF) {
            break;
        /* Newline marks the end of the comment, let the parent loop parse it 
         * and handle incrementing the line number or whatever else it wants
         * to do (in other words, no io_next() here) */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            break;
        }
        io_next();
    }

    return(rc);
}


/* Parses a section declaration. The section is used by parse_assignment to
 * determine the current section in which a key belongs.
 * 
 * PARAMETERS
 * 
 *  none
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t parse_section(void) {

    char *temp_ptr;
    char *section = NULL;
    unsigned long int section_total = 0;
    unsigned long int section_now = 0;
    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;

    /* Allocate a buffer to parse the section string into */
    section = malloc(MEMORY_FRAGMENT_SIZE);
    if (section == NULL) {
        rc = CONFIG_ERROR_NO_MEMORY;
        goto term;
    }
    section_total = MEMORY_FRAGMENT_SIZE;

    while (1) {
        ch = io_peek();
        /* EOF is not expected here, abort with an error */
        if (ch == IO_EOF) {
            rc = CONFIG_ERROR_PREMATURE_EOF;
            break;
        /* The section declaration termination character */
        } else if (ch == CONFIG_CHAR_SECTION_CLOSE) {
            io_next();
            break;
        /* Comment characters are unexpected */
        } else if (ch == CONFIG_CHAR_COMMENT) {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            goto term;
        /* Newlines are unexpected */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            goto term;
        /* Check for valid section characters */
        } else if (is_section_character(ch) || isspace(ch)) {
            /* If our current section buffer is too small, realloc larger */
            if (section_now == section_total) {
                section_total += MEMORY_FRAGMENT_SIZE;
                temp_ptr = realloc(section, section_total);
                if (temp_ptr == NULL) {
                    rc = CONFIG_ERROR_NO_MEMORY;
                    goto term;
                }
                section = temp_ptr;
            }
            /* Append the character to the current section string */
            section[section_now++] = ch;
            io_next();
        /* Any other character is an error */
        } else {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            goto term;
        }
    }

term:

    /* On error, clean up allocated resources, since we'll abort */
    if (rc != CONFIG_ERROR_NONE) {
        if (section != NULL) {
            free(section);
        }
    /* On success, make the parsed section the current active section */
    } else {
        if (section_now == section_total) {
            section_total++;
            temp_ptr = realloc(section, section_total);
            if (temp_ptr == NULL) {
                free(section);
                rc = CONFIG_ERROR_NO_MEMORY;
            }
        }
        if (rc == CONFIG_ERROR_NONE) {
            section[section_now] = 0;
            /* Free any current active section first */
            if (current_section != NULL) {
                free(current_section);
            }
            current_section = section;
        }
    }

    return(rc);
}


/* Read data from the input file and parse it.
 * 
 * PARAMETERS
 * 
 *  none
 * 
 * RETURNS
 * 
 *  Error code defined in config.h
 */
static config_error_t read_file(void) {

    config_error_t rc = CONFIG_ERROR_NONE;
    char ch;

    while (1) {
        /* get next character in input, without advancing input pointer */
        ch = io_peek();
        /* on eof (or error), we're done */
        if (ch == IO_EOF) {
            break;
        /* check for section declaration opening character */
        } else if (ch == CONFIG_CHAR_SECTION_OPEN) {
            ch = io_next();
            rc = parse_section();
            if (rc != CONFIG_ERROR_NONE) {
                break;
            }
        /* check for comment character */
        } else if (ch == CONFIG_CHAR_COMMENT) {
            ch = io_next();
            rc = parse_comment();
            if (rc != CONFIG_ERROR_NONE) {
                break;
            }
        /* check for newline */
        } else if (ch == CONFIG_CHAR_NEWLINE) {
            line++;
            ch = io_next();
        /* check for key name */
        } else if (is_key_character(ch)) {
            rc = parse_assignment();
            if (rc != CONFIG_ERROR_NONE) {
                break;
            }
        /* skip whitespace */
        } else if (isspace(ch)) {
            ch = io_next();
        /* anything else is problematic */
        } else {
            rc = CONFIG_ERROR_UNEXPECTED_CHARACTER;
            break;
        }
    }
    
    return(rc);
}


/* Returns the current parser line number of the configuration file.  Starts 
 * at 1.
 * 
 * PARAMETERS
 * 
 *  none
 * 
 * RETURNS
 * 
 *  Line number of the configuration file currently being parsed.
 */
unsigned long int config_get_line_number(void)
{
    return(line);
}


/* Returns the descriptive string corresponding to an input error code.
 *
 * PARAMETERS
 * 
 *  error
 *  The error code for which the descriptive string is returned.
 * 
 * RETURNS
 * 
 *  Pointer to the NULL-terminated string containing the descriptive error
 *  text.
 */
const char *config_get_error_string(config_error_t error) {

    if (error < CONFIG_ERROR_COUNT) {
        return(error_string[error]);
    } else {
        return(error_string[CONFIG_ERROR_COUNT]);
    }
}


/* Parse an .ini style configuration file.
 * 
 * PARAMETERS
 * 
 * 	file_name
 *  Fully-qualified path/name of the .ini file to parse.
 * 
 * 	handler
 *  Callback function to invoke when a key = value pair is parsed.
 * 
 * RETURNS
 * 
 *	Error code defined in config.h
 */
config_error_t config_parse(char *file_name, config_handler_func_t *handler) {

	config_error_t rc = CONFIG_ERROR_NONE;

    /* open the input config file */
	if (file_name == NULL || strlen(file_name) == 0) {
		rc = CONFIG_ERROR_NO_FILENAME;
		goto cleanup;
	}
	if ((file_handle = open(file_name, O_RDONLY)) < 0) {
		rc = CONFIG_ERROR_NO_OPEN;
		goto cleanup;
	}

	/* prepare input buffer */
	io_buffer = io_buffer_ptr = malloc(BUFFER_SIZE);
	if (io_buffer == NULL) {
		rc = CONFIG_ERROR_NO_MEMORY;
		goto cleanup;
	}
    io_buffer_len = 0;
    line = 1;

    assignment_handler = handler;
    current_section = NULL;

    /* read the file and parse it */
    rc = read_file();

cleanup:

	if (io_buffer != NULL) {
		free(io_buffer);
	}
    io_buffer = io_buffer_ptr = NULL;
    io_buffer_len = 0;

	if (file_handle != -1) {
		close(file_handle);
        file_handle = -1;
	}

    if (current_section != NULL) {
        free(current_section);
        current_section = NULL;
    }

    assignment_handler = NULL;

	return(rc);
}
