#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL;
    }

    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        memset(arglist[i], 0, ARGLEN);
    }

    char* cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0') break;

        if (*cp == '<' || *cp == '>' || *cp == '|') {
            arglist[argnum][0] = *cp;
            arglist[argnum][1] = '\0';
            argnum++;
            cp++;
            continue;
        }

        int pos = 0;
        char quote = 0;
        while (*cp != '\0' && pos < ARGLEN - 1) {
            if (quote) {
                if (*cp == quote) {
                    quote = 0;
                } else {
                    arglist[argnum][pos++] = *cp;
                }
            } else {
                if (*cp == '"' || *cp == '\'') {
                    quote = *cp;
                } else if (*cp == ' ' || *cp == '\t' || *cp == '<' || *cp == '>' || *cp == '|') {
                    break;
                } else {
                    arglist[argnum][pos++] = *cp;
                }
            }
            cp++;
        }
        arglist[argnum][pos] = '\0';
        argnum++;
    }

    if (argnum == 0) {
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

char* history[HISTORY_SIZE];
int history_count = 0;

void add_to_history(const char* cmd) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)
            history[i - 1] = history[i];
        history_count--;
    }
    history[history_count++] = strdup(cmd);
}

void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

typedef struct ShellVar {
    char* name;
    char* value;
    struct ShellVar* next;
} ShellVar;

static ShellVar* var_head = NULL;

void set_shell_var(const char* name, const char* value) {
    for (ShellVar* cur = var_head; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) {
            free(cur->value);
            cur->value = strdup(value);
            return;
        }
    }
    ShellVar* node = (ShellVar*)malloc(sizeof(ShellVar));
    node->name = strdup(name);
    node->value = strdup(value);
    node->next = var_head;
    var_head = node;
}

const char* get_shell_var(const char* name) {
    for (ShellVar* cur = var_head; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) {
            return cur->value;
        }
    }
    const char* env_val = getenv(name);
    return env_val ? env_val : "";
}

void print_shell_vars(void) {
    for (ShellVar* cur = var_head; cur != NULL; cur = cur->next) {
        printf("%s=%s\n", cur->name, cur->value);
    }
}

char* expand_vars(const char* input) {
    size_t cap = strlen(input) * 2 + 64;
    char* out = (char*)malloc(cap);
    size_t out_len = 0;
    char quote = 0;

    for (size_t i = 0; input[i] != '\0'; ) {
        if (input[i] == '\'' || input[i] == '"') {
            if (quote == 0) quote = input[i];
            else if (quote == input[i]) quote = 0;
            if (out_len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[out_len++] = input[i++];
            continue;
        }

        if (input[i] == '$' && quote != '\'') {
            size_t start = i + 1;
            int braced = 0;
            if (input[start] == '{') {
                braced = 1;
                start++;
            }
            if ((input[start] >= 'A' && input[start] <= 'Z') ||
                (input[start] >= 'a' && input[start] <= 'z') ||
                input[start] == '_') {
                size_t end = start;
                while ((input[end] >= 'A' && input[end] <= 'Z') ||
                       (input[end] >= 'a' && input[end] <= 'z') ||
                       (input[end] >= '0' && input[end] <= '9') ||
                       input[end] == '_') {
                    end++;
                }
                if (!braced || input[end] == '}') {
                    char var_name[256];
                    size_t nlen = end - start;
                    if (nlen >= sizeof(var_name)) nlen = sizeof(var_name) - 1;
                    strncpy(var_name, input + start, nlen);
                    var_name[nlen] = '\0';

                    const char* val = get_shell_var(var_name);
                    size_t vlen = strlen(val);
                    while (out_len + vlen + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
                    memcpy(out + out_len, val, vlen);
                    out_len += vlen;

                    i = braced ? end + 1 : end;
                    continue;
                }
            }
        }

        if (out_len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
        out[out_len++] = input[i++];
    }

    out[out_len] = '\0';
    return out;
}
