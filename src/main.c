#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = readline(PROMPT)) != NULL) {
        char* p = cmdline;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') {
            free(cmdline);
            continue;
        }

        if (cmdline[0] == '!') {
            char* endptr;
            long n = strtol(cmdline + 1, &endptr, 10);
            if (cmdline[1] == '\0' || *endptr != '\0' || n < 1 || n > history_count) {
                fprintf(stderr, "No such command in history: %s\n", cmdline);
                free(cmdline);
                continue;
            }
            char* expanded = strdup(history[n - 1]);
            free(cmdline);
            cmdline = expanded;
            printf("%s\n", cmdline);
        }

        add_history(cmdline);
        add_to_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0) execute(arglist);

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
