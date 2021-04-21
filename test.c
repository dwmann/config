/* test.c
 * 
 * Configuration file parser demo.
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

#include <stdio.h>

#include "config.h"


static config_error_t handle_assignment(char *section, char *key, char *value);


int main(int ac, char **av) {

    config_error_t rc;

    printf("parsing config file...\n");
    rc = config_parse("test.ini", handle_assignment);

    if (rc != CONFIG_ERROR_NONE) {
        printf("Error parsing configuration: %s on line %lu\n", 
            config_get_error_string(rc), config_get_line_number());
    } else {
        printf("All done!\n");
    }
}


static config_error_t handle_assignment(char *section, char *key, char *value) {

    if (section != NULL) {
        printf("%s.%s = \"%s\"\n", section, key, value);
    } else {
        printf("%s = \"%s\"\n", key, value);
    }
    return(CONFIG_ERROR_NONE);
}
