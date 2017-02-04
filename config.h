/*
 * configuration management
 */

#ifndef CONFIG_H
#define CONFIG_H


/*
 * data types
 */

typedef int (CONFIG_HANDLER_FUNC)(char *key, char *value);


/*
 * global functions
 */

extern int parseConfig(char *fileName, CONFIG_HANDLER_FUNC *handler);


#endif
