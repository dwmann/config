/* config.h
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

#ifndef CONFIG_H
#define CONFIG_H


/* DATA TYPES */

/* Error codes returned by the library.
 *
 * CONFIG_ERROR_NONE
 * No error.
 * 
 * CONFIG_ERROR_NO_FILENAME
 * NULL was passed in as a filename parameter.
 * 
 * CONFIG_ERROR_NO_OPEN
 * Unable to open the specified file.
 * 
 * CONFIG_ERROR_NO_MEMORY
 * Unable to allocate memory.
 * 
 * CONFIG_ERROR_UNEXPECTED_CHARACTER
 * An unexpected character appeared in the input stream.
 * 
 * CONFIG_ERROR_PREMATURE_EOF
 * The input stream ended before an object could be completely parsed.
 * 
 * CONFIG_ERROR_UNEXPECTED_EOL
 * The line ended before a delimited value was completed with a delimiter.
 */
typedef enum config_error_t {
    CONFIG_ERROR_NONE,
    CONFIG_ERROR_NO_FILENAME,
    CONFIG_ERROR_NO_OPEN,
    CONFIG_ERROR_NO_MEMORY,
    CONFIG_ERROR_UNEXPECTED_CHARACTER,
    CONFIG_ERROR_PREMATURE_EOF,
    CONFIG_ERROR_UNEXPECTED_EOL,
    CONFIG_ERROR_COUNT
} config_error_t;

/* Callback function supplied by user for processing key = value pairs as they
 * are parsed. 
 *
 * PARAMETERS
 * 
 *  s
 *  Will contain a pointer to the null-terminated current section.
 * 
 *  k
 *  Will contain a pointer to the null-terminated parsed key name.
 * 
 *  v
 *  Will contain a pointer to the null-terminated value assigned to the key.
 * 
 * RETURNS
 * 
 *  Should return a config_error_t code on any errors or CONFIG_ERROR_NONE.
 */
typedef config_error_t (config_handler_func_t)(char *s, char *k, char *v);


/* GLOBAL FUNCTIONS */

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
extern config_error_t config_parse(char *file_name, config_handler_func_t *handler);

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
extern unsigned long int config_get_line_number(void);

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
extern const char *config_get_error_string(config_error_t error);


#endif /* CONFIG_H */
