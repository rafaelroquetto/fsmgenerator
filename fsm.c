#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>

struct transition
{
    int source_state;
    int target_state;

    const char *token;
};

struct state_machine
{
    char **tokens;
    int nstates;
    int istate;
    int *fstates;
    int fs_count;

    struct transition **transitions;
    int t_capacity;
    int t_used;
};

static void
panic(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "FATAL: ");

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);

    exit(1);
}

static void
sm_initialize(struct state_machine *sm)
{
    bzero(sm, sizeof (*sm));
    sm->t_capacity = 16;
}

static struct state_machine *
sm_make(void)
{
    struct state_machine *sm;

    sm = malloc(sizeof *sm);

    sm_initialize(sm);

    return sm;
}

static void
sm_add_transition(struct state_machine *sm, struct transition *t)
{
    if (sm->transitions == NULL) {
        sm->transitions = (struct transition **) calloc(sm->t_capacity, sizeof (t));
        bzero(sm->transitions, sm->t_capacity * sizeof (t));
    }

    if (sm->t_used == (sm->t_capacity - 2)) {
        sm->t_capacity <<= 1;

        struct transition **ptr = (struct transition **)
            realloc(sm->transitions, sm->t_capacity * sizeof (t));

        if (ptr == NULL)
            panic("Not enough memory.");

        sm->transitions = ptr;
    }

    sm->transitions[sm->t_used] = t;
    sm->t_used++;
}

static void
sm_free_transitions(struct state_machine *sm)
{
    int i;

    for (i = 0; i < sm->t_used; ++i)
        free(*(sm->transitions + i));

    free(sm->transitions);

    sm->t_used = 0;
    sm->t_capacity = 16;
}

static inline int
sm_has_state(struct state_machine *sm, int state)
{
    return (state < sm->nstates);
}

static int
sm_is_final_state(struct state_machine *sm, int state)
{
    int i;

    for (i = 0; i < sm->fs_count; ++i) {
        if (*(sm->fstates + i) == state)
            return 1;
    }

    return 0;
}

static struct transition *
sm_make_transition(char *token, int source_state, int target_state)
{
    struct transition *t;

    t = (struct transition *) malloc(sizeof *t);
    t->token = token;
    t->source_state = source_state;
    t->target_state = target_state;

    return t;
}

static char *
read_line(void)
{
    static char buf[256];
    int nbytes;

    bzero(buf, sizeof buf);

    do {
        nbytes = read(STDIN_FILENO, buf, sizeof buf);
    } while (nbytes == -1);

    buf[nbytes - 1] = '\0';

    return buf;
}

static int
ask(const char *question)
{
    printf("%s: ", question);

    fflush(stdout);

    char *line = NULL;

    do {
        line = read_line();
    } while (!isdigit(line[0]));

    return strtol(line, NULL, 10);
}

static char **
tokenize(char *str)
{
    static size_t CAPACITY = 8;

    char **tokens;
    char *token;
    int ntokens;


    ntokens = 0;
    token = NULL;

    token = strtok(str, " ");

    if (token == NULL)
        return NULL;

    tokens = (char **) calloc(CAPACITY, sizeof (char *));
    bzero(tokens, CAPACITY * sizeof (char *));

    tokens[ntokens++] = strdup(token);

    while ((token = strtok(NULL, " ")) != NULL) {
        tokens[ntokens++] = strdup(token);

        if (ntokens == (CAPACITY - 2)) {
            CAPACITY <<= 1;
            char **ptr = (char **) realloc(tokens, CAPACITY * sizeof (char *));

            if (ptr == NULL)
                panic("Not enough memory.");

            tokens = ptr;
        }
    }

    tokens[ntokens] = NULL;

    fflush(stdout);
    return tokens;
}

static void
free_tokens(char **tokens)
{
    char **ptr;

    ptr = tokens;

    while (*ptr) {
        free(*ptr);
        ptr++;
    }

    free(tokens);
}

static size_t
tokens_size(char **tokens)
{
    size_t size = 0;

    while (*tokens)
        size++, tokens++;

    return size;
}

static char **
read_tokens(void)
{
    char *line;

    printf("Enter tokens: ");

    fflush(stdout);

    line = read_line();

    return tokenize(line);
}

static int
read_final_states_helper(int **states)
{
    char *line;
    char **tokens;
    int nstates;
    int i;

    printf("Final states: ");

    fflush(stdout);

    line = read_line();
    tokens = tokenize(line);

    if (tokens == NULL)
        panic("Invalid input");

    nstates = tokens_size(tokens);

    *states = calloc(nstates, sizeof (int));
    bzero(*states, nstates * sizeof (int));

    for (i = 0; i < nstates; ++i)
        (*states)[i] = strtol(tokens[i], NULL, 10);

    free_tokens(tokens);

    return nstates;
}

static int
check_fs_range(int *final_states, int nstates, int smstates)
{
    int success;
    int i;

    success = 1;

    for (i = 0; i < nstates; ++i) {
        if (final_states[i] >= smstates) {
            printf("Final state '%d' is out of range\n", final_states[i]);
            success = 0;
        }
    }

    return success;
}

static inline void
read_nstates(struct state_machine *sm)
{
    sm->nstates = ask("Number of states");
}

static void
read_initial_state(struct state_machine *sm)
{
    int state;

    for (;;) {
        state = ask("Initial state");

        if (sm_has_state(sm, state))
            break;

        printf("Initial state is invalid.\n");
    }

    sm->istate = state;
}

static void
read_final_states(struct state_machine *sm)
{
    int *final_states;
    int nstates;

    final_states = NULL;

    do {
        if (final_states != NULL)
            free(final_states);

        nstates = read_final_states_helper(&final_states);
    } while (!check_fs_range(final_states, nstates, sm->nstates));

    sm->fstates = final_states;
    sm->fs_count = nstates;
}

static void
sm_free(struct state_machine *sm)
{
    sm_free_transitions(sm);
    free_tokens(sm->tokens);
    free(sm->fstates);
    free(sm);
}

static void
print_tokens(char **tokens)
{
    printf("[ ");

    while (*tokens) {
        printf("%s ", *tokens);
        ++tokens;
    }

    printf("]\n");
}

static char *
find_token(char *token, char **tokens)
{
    while (*tokens) {
        if (strcmp(token, *tokens) == 0)
            return *tokens;

        ++tokens;
    }

    return NULL;
}

static char *
read_token(struct state_machine *sm)
{
    char *line;
    char *token;

    for (;;) {
        printf("Enter a token (or null to advance to next state): ");
        fflush(stdout);

        line = read_line();

        if (strcmp(line, "") == 0)
            break;

        token = find_token(line, sm->tokens);

        if (token != NULL)
            return token;

        printf("Invalid token entered.\n");
    }

    return NULL;
}

static int
read_target_state(struct state_machine *sm)
{
    int target_state;

    target_state = -1;

    do {
        if (target_state != -1)
            printf("Invalid state: %d\n", target_state);

        target_state = ask("Target state");
    } while (!sm_has_state(sm, target_state));

    return target_state;
}

static void
setup_transition(struct state_machine *sm, int state)
{
    char *token;
    int target_state;

    printf("\n\nTransitions for the state 'E%d'\n", state);
    printf("Available tokens: ");
    print_tokens(sm->tokens);

    for (;;) {
        token = read_token(sm);

        if (token == NULL)
            break;

        target_state = read_target_state(sm);

        sm_add_transition(sm, sm_make_transition(token, state, target_state));
    }
}

static void
setup_transitions(struct state_machine *sm)
{
    int i;

    for (i = 0; i < sm->nstates; ++i)
        setup_transition(sm, i);
}

static inline void
write_string(FILE *stream, const char *string)
{
    fwrite(string, strlen(string), sizeof (char), stream);
}

static void
write_goto_preamble(FILE *stream)
{
    static const char header[] =
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <unistd.h>\n"
        "#include <string.h>\n"
        "\n"
        "static char *\n"
        "read_line(void)\n"
        "{\n"
        "    static char buf[256];\n"
        "    int nbytes;\n"
        "\n"
        "    memset(buf, 0, sizeof (buf));\n"
        "\n"
        "    do {\n"
        "        nbytes = read(STDIN_FILENO, buf, sizeof buf);\n"
        "    } while (nbytes == -1);\n"
        "\n"
        "    buf[nbytes - 1] = '\\0';\n"
        "\n"
        "    return buf;\n"
        "}\n"
        "\n"
        "\n"
        "int main(int argc, char *argv[])\n"
        "{\n"
        "    char *line = read_line();\n"
        "\n"
        "    char *token = NULL;\n\n";


    write_string(stream, header);
}

static void
write_goto_epilogue(FILE *stream)
{
    static const char epilogue[] =
        "\n"
        "    REJECT:\n"
        "        printf(\"Input rejected!\\n\");\n"
        "        goto END;\n"
        "\n"
        "\n"
        "    ACCEPT:\n"
        "        printf(\"Input accepted!\\n\");\n"
        "        goto END;\n"
        "\n\n"
        "    END:\n"
        "        return 0;\n"
        "}\n";


    write_string(stream, epilogue);
}

static void
write_goto_state(struct state_machine *sm, int state, FILE *stream)
{
    static const char GOTO_REJECT[] = "        goto REJECT;\n\n\n";

    struct transition *transition;
    int i;

    fprintf(stream, "    E%d:\n", state);


    fprintf(stream, "        token = strtok(%s, \" \");\n\n",
            (sm->istate  == state) ? "line" : "NULL");

    if (sm->istate == state)
        fprintf(stream, "    line = NULL;\n");

    if (sm_is_final_state(sm, state)) {
        fprintf(stream, "        /* this is a final state */\n");
        fprintf(stream, "        printf(\"Entered accepting state E%d\\n\");\n\n", state);
    }

    fprintf(stream, "        if (token == NULL)\n");

    if (sm_is_final_state(sm, state))
        fprintf(stream, "            goto ACCEPT;\n\n");
    else
        fprintf(stream, "            goto REJECT;\n\n");

    for (i = 0; i < sm->t_used; ++i) {
        transition = *(sm->transitions + i);

        if (transition->source_state != state)
            continue;

        fprintf(stream, "        if (strcmp(\"%s\", token) == 0)\n", transition->token);
        fprintf(stream, "            goto E%d;\n", transition->target_state);
    }

    fprintf(stream, "\n\n");

    write_string(stream, GOTO_REJECT);
}

static void
write_output_goto(struct state_machine *sm, const char *path)
{
    FILE *out;
    int i;

    if ((out = fopen(path, "w")) == NULL)
        panic("Cannot open output file for writing: %s", strerror(errno));

    write_goto_preamble(out);

    fprintf(out, "    goto E%d;\n\n", sm->istate);

    for (i = 0; i < sm->nstates; ++i)
        write_goto_state(sm, i, out);

    write_goto_epilogue(out);

    fclose(out);
}

static void
write_func_preamble(FILE *stream, struct state_machine *sm)
{
    static const char header1[] =
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <unistd.h>\n"
        "#include <string.h>\n"
        "\n";

    int i;

    write_string(stream, header1);

    for (i = 0; i < sm->nstates; ++i)
        fprintf(stream, "static void e%d(const char *token);\n", i);

    static const char header2[] =
        "static char *\n"
        "read_line(void)\n"
        "{\n"
        "    static char buf[256];\n"
        "    int nbytes;\n"
        "\n"
        "    memset(buf, 0, sizeof (buf));\n"
        "\n"
        "    do {\n"
        "        nbytes = read(STDIN_FILENO, buf, sizeof buf);\n"
        "    } while (nbytes == -1);\n"
        "\n"
        "    buf[nbytes - 1] = '\\0';\n"
        "\n"
        "    return buf;\n"
        "}\n"
        "\n"
        "\n"
        "static void\n"
        "reject(void)\n"
        "{\n"
        "    printf(\"Input rejected!\\n\");\n"
        "    exit(EXIT_FAILURE);\n"
        "}\n"
        "\n"
        "\n"
        "static void\n"
        "accept(void)\n"
        "{\n"
        "    printf(\"Input accepted!\\n\");\n"
        "    exit(EXIT_SUCCESS);\n"
        "}\n"
        "\n"
        "\n"
        "int main(int argc, char *argv[])\n"
        "{\n"
        "    char *line = read_line();\n"
        "\n";
    write_string(stream, header2);

    fprintf(stream, "    e%d(strtok(line, \" \"));\n", sm->istate);

    static const char footer[] =
        "    return 0;\n"
        "}\n\n";

    write_string(stream, footer);
}

static void
write_func_state(struct state_machine *sm, int state, FILE *stream)
{
    struct transition *transition;
    int i;

    fprintf(stream, "static void e%d(const char *token)\n{\n", state);

    if (sm_is_final_state(sm, state)) {
        fprintf(stream, "    /* this is a final state */\n");
        fprintf(stream, "    printf(\"Entered accepting state E%d\\n\");\n\n", state);
    }

    fprintf(stream, "    if (token == NULL)\n");

    if (sm_is_final_state(sm, state))
        fprintf(stream, "        accept();\n\n");
    else
        fprintf(stream, "        reject();\n\n");

    for (i = 0; i < sm->t_used; ++i) {
        transition = *(sm->transitions + i);

        if (transition->source_state != state)
            continue;

        fprintf(stream, "    if (strcmp(\"%s\", token) == 0)\n", transition->token);
        fprintf(stream, "        e%d(strtok(NULL, \" \"));\n",
                transition->target_state);
    }

    fprintf(stream, "\n\n    reject();\n}\n\n");
}

static void
write_output_func(struct state_machine *sm, const char *path)
{
    FILE *out;
    int i;

    if ((out = fopen(path, "w")) == NULL)
        panic("Cannot open output file for writing: %s", strerror(errno));

    write_func_preamble(out, sm);

    for (i = 0; i < sm->nstates; ++i)
        write_func_state(sm, i, out);

    fclose(out);
}

static int
is_goto(int argc, char *argv[])
{
    int opt;
    int gt;

    gt = 1;

    while ((opt = getopt(argc, argv, "fg")) != -1) {
        switch (opt) {
        case 'f':
            gt = 0;
            break;
        case 'g':
            gt = 1;
            break;
        default:
            panic("Usage: %s [-g|f]", argv[0]);
        }
    }

    return gt;
}

int main(int argc, char *argv[])
{
    struct state_machine *sm;
    char *filename;

    int nstates;
    int i;
    int use_goto;

    use_goto = is_goto(argc, argv);

    sm = sm_make();

    sm->tokens = read_tokens();

    if (sm->tokens == NULL)
        panic("Invalid token input");

    read_nstates(sm);
    read_initial_state(sm);
    read_final_states(sm);

    setup_transitions(sm);

    printf("Output filename: ");
    fflush(stdout);

    filename = read_line();

    if (use_goto)
        write_output_goto(sm, filename);
    else
        write_output_func(sm, filename);

    sm_free(sm);

    return 0;
}
