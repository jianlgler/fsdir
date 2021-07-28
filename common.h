#include <errno.h>
#include <stdio.h>
#include <string.h>
// macros for handling errors
#define handle_error_en(msg, en)    do { errno = en; perror((const char *)(msg)); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg)           do { perror((const char *)(msg)); exit(EXIT_FAILURE); } while (0)

