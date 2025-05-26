#include "interface.h"

void print_info(const char *msg) {
    printf(BOLD CYAN "%s\n" RESET, msg);
}

void print_error(const char *msg) {
    printf(BOLD RED "[ERREUR] %s\n" RESET, msg);
}

void print_success(const char *msg) {
    printf(BOLD GREEN "[SUCCÈS] %s\n" RESET, msg);
}

void print_channel_msg(const char *channel, const char *user, const char *msg) {
    printf(BOLD MAGENTA "[#%s] " RESET BLUE "<%s>" RESET " %s\n", channel, user, msg);
}
