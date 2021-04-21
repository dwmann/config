# config
This is a c module for parsing an ini style configuration file.  To use the library, include config.h in your program and call the function `config_parse()` with the full path and name of the file to parse and a callback function to call whenever a key = value assignment is parsed. The function will return an error code, defined in config.h. The default in the makefile is to use clang for compilation and linking. To use gcc, do something like `cc=gcc ld=gcc make`.

Configuration files can have optional sections that are defined inside section delimiter characters (default is '[' and ']'). See the "CONSTANTS" section of config.c for default delimiter, comment and assignment characters.

`[section name]`

If no section is defined, the callback function will receive a NULL in the section argument. Section names are a little less restricted than keys and can contain whitespace.

Assignments are made in the configuration using the assignment operator (default is '='). Each assignment must have a key and a value.

`key = value`

Values can be delimited by enclosing them in the delimiter character (default '"').

`key = "delimited value"`

If a value is not delimited, there are some restrictions on what it can contain. It can't have whitespace in it, comments or newlines.

Keys also have these limitations and cannot be delimited.

The callback function supplied to the config_parse function will be called with the currently active section name (if any, otherwise NULL), the current key and the value assigned to that key. The current line number can be retrieved using the `config_get_line_number()` function. Line numbers start at 1. The section, key and value passed to the callback are allocated and freed by the config module, internally. If any of those need to be preserved, the application code will have to allocate an appropriate sized block and copy the values from the pointers passed to the handler function into the application's allocated memory as the pointers will be invalid as soon as the handler function returns. The callback function can return its own error code (a code greater than `CONFIG_ERROR_COUNT`) to indicate a problem particular to the application. 

The module also supplies a function `config_get_error_string()` that can be used to get descriptive text for any error returned by the `config_parse()` function. If the error code passed to this function is >= `CONFIG_ERROR_COUNT`, the text returned will be `"Unknown error"`.

See the included test.c and test.ini for example usage. Run make to build the example program from test.c and run it from the command line to see how the library behaves with the supplied test.ini.

There are no limits on line lengths in the config file, nor length limits for sections, keys or values.

warren@nonvol.io
