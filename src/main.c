#include "shell.h"

static char* trim_ws(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
    return s;
}

static int is_valid_var_name(const char* s, size_t len) {
    if (len == 0) return 0;
    if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return 0;
    for (size_t i = 1; i < len; i++) {
        if (!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') ||
              (s[i] >= '0' && s[i] <= '9') || s[i] == '_')) return 0;
    }
    return 1;
}

static int run_single_command(char* segment) {
    segment = trim_ws(segment);
    if (*segment == '\0') return 0;

    char* expanded = expand_vars(segment);
    char* cmd = trim_ws(expanded);
    size_t len = strlen(cmd);
    if (len == 0) {
        free(expanded);
        return 0;
    }

    char* eq = strchr(cmd, '=');
    if (eq != NULL && is_valid_var_name(cmd, (size_t)(eq - cmd))) {
        char var_name[256];
        size_t nlen = (size_t)(eq - cmd);
        if (nlen >= sizeof(var_name)) nlen = sizeof(var_name) - 1;
        strncpy(var_name, cmd, nlen);
        var_name[nlen] = '\0';

        char* val = eq + 1;
        size_t vlen = strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen - 1] == '"') ||
                          (val[0] == '\'' && val[vlen - 1] == '\''))) {
            val[vlen - 1] = '\0';
            val++;
        }
        set_shell_var(var_name, val);
        free(expanded);
        return 0;
    }

    int is_bg = 0;
    if (cmd[len - 1] == '&') {
        is_bg = 1;
        cmd[--len] = '\0';
        cmd = trim_ws(cmd);
        len = strlen(cmd);
    }
    if (len == 0) {
        free(expanded);
        return 0;
    }

    int status = 0;
    char* raw_copy = strdup(cmd);
    char** arglist = tokenize(cmd);
    if (arglist != NULL) {
        int b = handle_builtin(arglist);
        if (b == -1) {
            status = execute_bg(arglist, is_bg, raw_copy);
        } else {
            status = b;
        }
        for (int i = 0; arglist[i] != NULL; i++) {
            free(arglist[i]);
        }
        free(arglist);
    }
    free(raw_copy);
    free(expanded);
    return status;
}

static int has_complete_if_blocks(const char* text) {
    int depth = 0;
    char quote = 0;
    const char* p = text;

    while (*p != '\0') {
        if (*p == '"' || *p == '\'') {
            if (quote == 0) quote = *p;
            else if (quote == *p) quote = 0;
            p++;
            continue;
        }
        if (quote == 0 && (p == text || *(p - 1) == ' ' || *(p - 1) == '\t' || *(p - 1) == '\n' || *(p - 1) == ';')) {
            if (strncmp(p, "if", 2) == 0 && (p[2] == ' ' || p[2] == '\t' || p[2] == '\n' || p[2] == '\0')) {
                depth++;
                p += 2;
                continue;
            }
            if (strncmp(p, "fi", 2) == 0 && (p[2] == ' ' || p[2] == '\t' || p[2] == '\n' || p[2] == ';' || p[2] == '\0')) {
                depth--;
                p += 2;
                continue;
            }
        }
        p++;
    }
    return depth <= 0;
}

static int execute_script(char* script) {
    char* copy = strdup(script);
    char* stmts[256];
    int n_stmts = 0;

    char* start = copy;
    char quote = 0;
    for (char* cur = copy; ; cur++) {
        if (*cur == '"' || *cur == '\'') {
            if (quote == 0) quote = *cur;
            else if (quote == *cur) quote = 0;
        } else if (((*cur == ';' || *cur == '\n') && quote == 0) || *cur == '\0') {
            char saved = *cur;
            *cur = '\0';
            char* trimmed = trim_ws(start);
            if (*trimmed != '\0' && n_stmts < 256) {
                stmts[n_stmts++] = trimmed;
            }
            if (saved == '\0') break;
            start = cur + 1;
        }
    }

    int last_status = 0;
    for (int i = 0; i < n_stmts; i++) {
        char* s = stmts[i];
        if (strncmp(s, "if", 2) == 0 && (s[2] == ' ' || s[2] == '\t')) {
            char* cond_cmd = trim_ws(s + 3);
            char then_buf[MAX_LEN * 4] = "";
            char else_buf[MAX_LEN * 4] = "";
            int state = 0; // 0 = before then, 1 = then branch, 2 = else branch
            int depth = 1;

            while (++i < n_stmts && depth > 0) {
                char* line = stmts[i];
                if (depth == 1) {
                    if (strcmp(line, "then") == 0) {
                        state = 1;
                        continue;
                    } else if (strncmp(line, "then", 4) == 0 && (line[4] == ' ' || line[4] == '\t')) {
                        state = 1;
                        line = trim_ws(line + 5);
                    } else if (strcmp(line, "else") == 0) {
                        state = 2;
                        continue;
                    } else if (strncmp(line, "else", 4) == 0 && (line[4] == ' ' || line[4] == '\t')) {
                        state = 2;
                        line = trim_ws(line + 5);
                    } else if (strcmp(line, "fi") == 0) {
                        depth = 0;
                        break;
                    }
                }

                if (strncmp(line, "if", 2) == 0 && (line[2] == ' ' || line[2] == '\t')) {
                    depth++;
                } else if (strcmp(line, "fi") == 0) {
                    depth--;
                    if (depth == 0) break;
                }

                if (state == 1) {
                    strncat(then_buf, line, sizeof(then_buf) - strlen(then_buf) - 2);
                    strcat(then_buf, "\n");
                } else if (state == 2) {
                    strncat(else_buf, line, sizeof(else_buf) - strlen(else_buf) - 2);
                    strcat(else_buf, "\n");
                }
            }

            if (depth != 0 || state == 0) {
                fprintf(stderr, "Syntax error: malformed if-then-else-fi block\n");
                free(copy);
                return 1;
            }

            if (execute_script(cond_cmd) == 0) {
                last_status = execute_script(then_buf);
            } else {
                last_status = execute_script(else_buf);
            }
        } else {
            last_status = run_single_command(s);
        }
    }

    free(copy);
    return last_status;
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

        while (!has_complete_if_blocks(cmdline)) {
            char* cont = readline("> ");
            if (cont == NULL) {
                fprintf(stderr, "Syntax error: unexpected EOF while looking for 'fi'\n");
                free(cmdline);
                cmdline = NULL;
                break;
            }
            size_t new_len = strlen(cmdline) + strlen(cont) + 2;
            char* joined = malloc(new_len);
            sprintf(joined, "%s\n%s", cmdline, cont);
            free(cmdline);
            free(cont);
            cmdline = joined;
        }
        if (cmdline == NULL) break;

        add_history(cmdline);
        add_to_history(cmdline);
        execute_script(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
