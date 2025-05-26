#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdio.h>

#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"
#define UNDERLINE   "\033[4m"

void print_info(const char *msg);
void print_error(const char *msg);
void print_success(const char *msg);
void print_channel_msg(const char *channel, const char *user, const char *msg);

#endif
