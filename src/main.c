#include "shell.h"

static void run_single_command(char* segment) {
    while (*segment == ' ' || *segment == '\t') segment++;
    size_t len = strlen(segment);
    while (len > 0 && (segment[len - 1] == ' ' || segment[len - 1] == '\t')) {
        segment[--len] = '\0';
    }
    if (len == 0) return;

    int is_bg = 0;
    if (segment[len - 1] == '&') {
        is_bg = 1;
        segment[--len] = '\0';
        while (len > 0 && (segment[len - 1] == ' ' || segment[len - 1] == '\t')) {
            segment[--len] = '\0';
        }
    }
    if (len == 0) return;

    char* raw_copy = strdup(segment);
    char** arglist = tokenize(segment);
    if (arglist != NULL) {
        if (handle_builtin(arglist) == 0) {
            execute_bg(arglist, is_bg, raw_copy);
        }
        for (int i = 0; arglist[i] != NULL; i++) {
            free(arglist[i]);
        }
        free(arglist);
    }
    free(raw_copy);
}

int main() {
    char* cmdline;

    while (1) {
        reap_zombies();
        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

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

        char* seg_start = cmdline;
        char quote = 0;
        for (char* cur = cmdline; ; cur++) {
            if (*cur == '"' || *cur == '\'') {
                if (quote == 0) quote = *cur;
                else if (quote == *cur) quote = 0;
            } else if ((*cur == ';' && quote == 0) || *cur == '\0') {
                char saved = *cur;
                *cur = '\0';
                run_single_command(seg_start);
                if (saved == '\0') break;
                seg_start = cur + 1;
            }
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
