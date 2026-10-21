#include <stdio.h>   /* PART 1: DATA AND LEXER SETUP */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 4096
#define MAX_TEXT 128
#define MAX_VARS 128
#define MAX_CODE 512

typedef struct {
    char text[MAX_TEXT];
    int line;
} Token;

Token tokens[MAX_TOKENS];
int token_count, pos, errors;

char variables[MAX_VARS][MAX_TEXT];
int var_count;

char code[MAX_CODE][MAX_TEXT * 2];
int code_count, temp_id, label_id;

void add_token(const char *s, int line) {
    if (token_count >= MAX_TOKENS) {
        return;
    }

    snprintf(tokens[token_count].text, MAX_TEXT, "%s", s);
    tokens[token_count].line = line;
    token_count++;
}

int is_keyword(const char *s) {
    const char *words[] = {
        "int", "if", "else", "while", "for", "printf", "return"
    };

    for (int i = 0; i < 7; i++) {
        if (!strcmp(s, words[i])) {
            return 1;
        }
    }

    return 0;
}

void lex(const char *src) {
    int i = 0;
    int line = 1;

    while (src[i]) {
        if (src[i] == '\n') {
            line++;
            i++;
            continue;
        }

        if (isspace((unsigned char)src[i])) {
            i++;
            continue;
        }

        char word[MAX_TEXT];
        int n = 0;

        if (src[i] == '/' && src[i + 1] == '/') {
            while (src[i] && src[i] != '\n') {
                i++;
            }
            continue;
        }

        if (src[i] == '"') { /* PART 2: RECOGNIZE SOURCE TOKENS */
            word[n++] = src[i++];

            while (src[i] && src[i] != '"' && n < MAX_TEXT - 2) {
                if (src[i] == '\\' && src[i + 1]) {
                    word[n++] = src[i++];
                }
                word[n++] = src[i++];
            }

            if (src[i] == '"') {
                word[n++] = src[i++];
            }

            word[n] = 0;
            add_token(word, line);
            continue;
        }

        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            while (isalnum((unsigned char)src[i]) || src[i] == '_') {
                if (n < MAX_TEXT - 1) {
                    word[n++] = src[i];
                }
                i++;
            }

            word[n] = 0;
            add_token(word, line);
            continue;
        }

        if (isdigit((unsigned char)src[i])) {
            while (isdigit((unsigned char)src[i])) {
                if (n < MAX_TEXT - 1) {
                    word[n++] = src[i];
                }
                i++;
            }

            word[n] = 0;
            add_token(word, line);
            continue;
        }

        if ((src[i] == '+' && src[i + 1] == '+') ||
            (src[i] == '-' && src[i + 1] == '-') ||
            (src[i] == '=' && src[i + 1] == '=') ||
            (src[i] == '!' && src[i + 1] == '=') ||
            (src[i] == '<' && src[i + 1] == '=') ||
            (src[i] == '>' && src[i + 1] == '=') ||
            (src[i] == '&' && src[i + 1] == '&') ||
            (src[i] == '|' && src[i + 1] == '|')) {
            word[0] = src[i];
            word[1] = src[i + 1];
            word[2] = 0;
            i += 2;
            add_token(word, line);
            continue;
        }

        if (strchr("+-*/%=<>!;(),{}", src[i])) {
            word[0] = src[i++];
            word[1] = 0;
            add_token(word, line);
            continue;
        }

        fprintf(stderr,
                "Lexical error at line %d: unexpected character '%c'\n",
                line, src[i]);
        errors++;
        i++;
    }

    add_token("<END>", line);
}

/* PART 3: SYMBOL TABLE AND EXPRESSION HELPERS */
int is_number(const char *s) {
    return isdigit((unsigned char)s[0]);
}

int is_declared(const char *s) {
    for (int i = 0; i < var_count; i++) {
        if (!strcmp(variables[i], s)) {
            return 1;
        }
    }

    return 0;
}

void declare_var(const char *s, int line) {
    if (is_declared(s)) {
        fprintf(stderr,
                "Semantic error at line %d: '%s' is already declared\n",
                line, s);
        errors++;
    } else if (var_count < MAX_VARS) {
        snprintf(variables[var_count], MAX_TEXT, "%s", s);
        var_count++;
    }
}

void use_var(const char *s, int line) {
    if (!is_declared(s)) {
        fprintf(stderr,
                "Semantic error at line %d: '%s' is not declared\n",
                line, s);
        errors++;
    }
}

const char *peek(void) {
    return tokens[pos].text;
}

int accept(const char *s) {
    if (!strcmp(peek(), s)) {
        pos++;
        return 1;
    }

    return 0;
}

void expect(const char *s) {
    if (!accept(s)) {
        fprintf(stderr,
                "Syntax error at line %d: expected '%s', found '%s'\n",
                tokens[pos].line, s, peek());
        errors++;

        if (strcmp(peek(), "<END>")) {
            pos++;
        }
    }
}

void emit(const char *s) {
    if (code_count < MAX_CODE) {
        snprintf(code[code_count], MAX_TEXT * 2, "%s", s);
        code_count++;
    }
}

char *new_temp(void) {
    static char names[MAX_CODE][24];
    static int count;

    if (count >= MAX_CODE) {
        return names[MAX_CODE - 1];
    }

    snprintf(names[count], sizeof(names[count]), "t%d", ++temp_id);
    return names[count++];
}

char *new_label(void) {
    static char names[MAX_CODE][24];
    static int count;

    if (count >= MAX_CODE) {
        return names[MAX_CODE - 1];
    }

    snprintf(names[count], sizeof(names[count]), "L%d", ++label_id);
    return names[count++];
}

char *expression(void);

char *primary(void) {
    static char value[4][MAX_TEXT];
    static int slot;

    slot = (slot + 1) % 4;

    if (accept("(")) {
        char *v = expression();
        snprintf(value[slot], MAX_TEXT, "%s", v);
        expect(")");
        return value[slot];
    }

    if (is_number(peek())) {
        snprintf(value[slot], MAX_TEXT, "%s", peek());
        pos++;
        return value[slot];
    }

    if (isalpha((unsigned char)peek()[0]) || peek()[0] == '_') {
        snprintf(value[slot], MAX_TEXT, "%s", peek());
        use_var(peek(), tokens[pos].line);
        pos++;
        return value[slot];
    }

    fprintf(stderr,
            "Syntax error at line %d: expected a value, found '%s'\n",
            tokens[pos].line, peek());
    errors++;

    if (strcmp(peek(), "<END>")) {
        pos++;
    }

    strcpy(value[slot], "0");
    return value[slot];
}

char *term(void) {
    char *left = primary();

    while (!strcmp(peek(), "*") || !strcmp(peek(), "/") ||
           !strcmp(peek(), "%")) {
        char op[MAX_TEXT];
        strcpy(op, peek());
        pos++;

        char *right = primary();
        char *t = new_temp();
        char line[MAX_TEXT * 2];

        snprintf(line, sizeof(line), "%s = %s %s %s", t, left, op, right);
        emit(line);
        left = t;
    }

    return left;
}

/* PART 4: ADDITION, COMPARISONS, AND BASIC STATEMENTS */
char *additive(void) {
    char *left = term();

    while (!strcmp(peek(), "+") || !strcmp(peek(), "-")) {
        char op[MAX_TEXT];
        strcpy(op, peek());
        pos++;

        char *right = term();
        char *t = new_temp();
        char line[MAX_TEXT * 2];

        snprintf(line, sizeof(line), "%s = %s %s %s", t, left, op, right);
        emit(line);
        left = t;
    }

    return left;
}

char *expression(void) {
    char *left = additive();

    while (!strcmp(peek(), "==") || !strcmp(peek(), "!=") ||
           !strcmp(peek(), "<") || !strcmp(peek(), ">") ||
           !strcmp(peek(), "<=") || !strcmp(peek(), ">=")) {
        char op[MAX_TEXT];
        strcpy(op, peek());
        pos++;

        char *right = additive();
        char *t = new_temp();
        char line[MAX_TEXT * 2];

        snprintf(line, sizeof(line), "%s = %s %s %s", t, left, op, right);
        emit(line);
        left = t;
    }

    return left;
}

void statement(void);

void block(void) {
    expect("{");

    while (strcmp(peek(), "}") && strcmp(peek(), "<END>")) {
        statement();
    }

    expect("}");
}

void statement(void) {
    if (!strcmp(peek(), "int") && !strcmp(tokens[pos + 1].text, "main")) {
        pos++;
    }

    if (!strcmp(peek(), "main")) {
        pos++;
        expect("(");
        expect(")");

        if (!strcmp(peek(), "{")) {
            block();
        } else {
            fprintf(stderr,
                    "Syntax error at line %d: expected main function body\n",
                    tokens[pos].line);
            errors++;
        }

        return;
    }

    if (accept("int")) {
        char name[MAX_TEXT];
        snprintf(name, sizeof(name), "%s", peek());
        int line = tokens[pos].line;

        if (!(isalpha((unsigned char)peek()[0]) || peek()[0] == '_')) {
            expect("identifier");
            return;
        }

        pos++;
        declare_var(name, line);

        if (accept("=")) {
            char *v = expression();
            char out[MAX_TEXT * 2];

            snprintf(out, sizeof(out), "%s = %s", name, v);
            emit(out);
        }

        expect(";");
        return;
    }

    /* PART 5: IF, WHILE, AND FOR CONTROL FLOW */
    if (accept("if")) {
        expect("(");
        char *condition = expression();
        expect(")");

        char *otherwise = new_label();
        char *done = new_label();
        char out[MAX_TEXT * 2];

        snprintf(out, sizeof(out), "ifFalse %s goto %s", condition, otherwise);
        emit(out);
        statement();

        if (accept("else")) {
            snprintf(out, sizeof(out), "goto %s", done);
            emit(out);

            snprintf(out, sizeof(out), "%s:", otherwise);
            emit(out);
            statement();

            snprintf(out, sizeof(out), "%s:", done);
            emit(out);
        } else {
            snprintf(out, sizeof(out), "%s:", otherwise);
            emit(out);
        }

        return;
    }

    if (accept("while")) {
        char *start = new_label();
        char *done = new_label();
        char out[MAX_TEXT * 2];

        snprintf(out, sizeof(out), "%s:", start);
        emit(out);

        expect("(");
        char *condition = expression();
        expect(")");

        snprintf(out, sizeof(out), "ifFalse %s goto %s", condition, done);
        emit(out);
        statement();

        snprintf(out, sizeof(out), "goto %s", start);
        emit(out);
        snprintf(out, sizeof(out), "%s:", done);
        emit(out);
        return;
    }

    if (accept("for")) {
        expect("(");

        if (!strcmp(peek(), "int")) {
            statement();
        } else if (!accept(";")) {
            statement();
        }

        char *start = new_label();
        char *done = new_label();
        char out[MAX_TEXT * 2];

        snprintf(out, sizeof(out), "%s:", start);
        emit(out);

        if (accept(";")) {
            emit("ifFalse 0 goto loop_end");
        } else {
            char *condition = expression();
            expect(";");
            snprintf(out, sizeof(out), "ifFalse %s goto %s", condition, done);
            emit(out);
        }

        /* Save the increment; it is emitted after the loop body. */
        int inc_start = pos;
        while (strcmp(peek(), ")") && strcmp(peek(), "<END>")) {
            pos++;
        }
        int inc_end = pos;
        expect(")");

        statement();

        int saved_pos = pos;
        pos = inc_start;

        if (inc_start < inc_end) {
            char var[MAX_TEXT];
            snprintf(var, sizeof(var), "%s", peek());
            use_var(var, tokens[pos].line);
            pos++;

            if (accept("++")) {
                char x[MAX_TEXT * 2];
                snprintf(x, sizeof(x), "%s = %s + 1", var, var);
                emit(x);
            } else if (accept("--")) {
                char x[MAX_TEXT * 2];
                snprintf(x, sizeof(x), "%s = %s - 1", var, var);
                emit(x);
            } else if (accept("=")) {
                char *v = expression();
                char x[MAX_TEXT * 2];
                snprintf(x, sizeof(x), "%s = %s", var, v);
                emit(x);
            } else {
                fprintf(stderr, "Note: unsupported for-loop increment\n");
            }
        }

        pos = saved_pos;
        snprintf(out, sizeof(out), "goto %s", start);
        emit(out);
        snprintf(out, sizeof(out), "%s:", done);
        emit(out);
        return;
    }

    /* PART 6: PRINT, RETURN, ASSIGNMENTS, AND PHASE DISPLAY */
    if (accept("printf")) {
        expect("(");

        if (peek()[0] == '"') {
            char format[MAX_TEXT];
            snprintf(format, sizeof(format), "%s", peek());
            pos++;

            if (accept(",")) {
                char *v = expression();
                char value[MAX_TEXT];
                snprintf(value, sizeof(value), "%s", v);
                char *placeholder = strstr(format, "%d");

                if (placeholder) {
                    char prefix[MAX_TEXT];
                    char suffix[MAX_TEXT];
                    size_t prefix_length = (size_t)(placeholder - format);

                    memcpy(prefix, format, prefix_length);
                    prefix[prefix_length] = '"';
                    prefix[prefix_length + 1] = 0;
                    snprintf(suffix, sizeof(suffix), "\"%s", placeholder + 2);

                    char out[MAX_TEXT * 2];
                    snprintf(out, sizeof(out), "printstr %s", prefix);
                    emit(out);
                    snprintf(out, sizeof(out), "print %s", value);
                    emit(out);

                    if (strcmp(suffix, "\"\"")) {
                        snprintf(out, sizeof(out), "printstr %s", suffix);
                        emit(out);
                    }
                } else {
                    char out[MAX_TEXT * 2];
                    snprintf(out, sizeof(out), "printstr %s", format);
                    emit(out);
                }
            } else {
                char out[MAX_TEXT * 2];
                snprintf(out, sizeof(out), "printstr %s", format);
                emit(out);
            }
        } else {
            char *v = expression();
            char value[MAX_TEXT];
            snprintf(value, sizeof(value), "%s", v);
            char out[MAX_TEXT * 2];
            snprintf(out, sizeof(out), "print %s", value);
            emit(out);
        }

        expect(")");
        expect(";");
        return;
    }

    if (accept("return")) {
        if (!strcmp(peek(), ";")) {
            emit("return");
        } else {
            char *v = expression();
            char out[MAX_TEXT * 2];
            snprintf(out, sizeof(out), "return %s", v);
            emit(out);
        }

        expect(";");
        return;
    }

    if (!strcmp(peek(), "{")) {
        block();
        return;
    }

    if (isalpha((unsigned char)peek()[0]) || peek()[0] == '_') {
        char name[MAX_TEXT];
        snprintf(name, sizeof(name), "%s", peek());
        int line = tokens[pos].line;

        pos++;
        use_var(name, line);

        if (accept("=")) {
            char *v = expression();
            char out[MAX_TEXT * 2];
            snprintf(out, sizeof(out), "%s = %s", name, v);
            emit(out);
        } else if (accept("++")) {
            char out[MAX_TEXT * 2];
            snprintf(out, sizeof(out), "%s = %s + 1", name, name);
            emit(out);
        } else if (accept("--")) {
            char out[MAX_TEXT * 2];
            snprintf(out, sizeof(out), "%s = %s - 1", name, name);
            emit(out);
        } else {
            fprintf(stderr, "Syntax error at line %d: expected assignment\n", line);
            errors++;
        }

        expect(";");
        return;
    }

    fprintf(stderr, "Syntax error at line %d near '%s'\n", tokens[pos].line, peek());
    errors++;

    if (strcmp(peek(), "<END>")) {
        pos++;
    }
}

typedef struct {
    char name[MAX_TEXT];
    int value;
} RuntimeValue;

int get_runtime_value(const char *name, RuntimeValue values[], int *count) {
    if (isdigit((unsigned char)name[0])) {
        return (int)strtol(name, NULL, 10);
    }

    for (int i = 0; i < *count; i++) {
        if (!strcmp(values[i].name, name)) {
            return values[i].value;
        }
    }

    if (*count < MAX_CODE + MAX_VARS) {
        snprintf(values[*count].name, MAX_TEXT, "%s", name);
        values[*count].value = 0;
        (*count)++;
    }

    return 0;
}

void set_runtime_value(const char *name, int value,
                       RuntimeValue values[], int *count) {
    for (int i = 0; i < *count; i++) {
        if (!strcmp(values[i].name, name)) {
            values[i].value = value;
            return;
        }
    }

    if (*count < MAX_CODE + MAX_VARS) {
        snprintf(values[*count].name, MAX_TEXT, "%s", name);
        values[*count].value = value;
        (*count)++;
    }
}

void append_output(char output[], size_t capacity, size_t *used,
                   const char *text) {
    while (*text && *used + 1 < capacity) {
        output[(*used)++] = *text++;
    }
    output[*used] = 0;
}

void append_string_literal(char output[], size_t capacity, size_t *used,
                           const char *literal) {
    if (*literal != '"') {
        return;
    }

    literal++;
    while (*literal && *literal != '"' && *used + 1 < capacity) {
        char ch = *literal++;

        if (ch == '\\' && *literal) {
            ch = *literal++;
            if (ch == 'n') {
                ch = '\n';
            } else if (ch == 't') {
                ch = '\t';
            }
        }

        output[(*used)++] = ch;
    }

    output[*used] = 0;
}

int find_label(const char *name, char labels[][MAX_TEXT],
               int positions[], int count) {
    for (int i = 0; i < count; i++) {
        if (!strcmp(labels[i], name)) {
            return positions[i];
        }
    }

    return -1;
}

int calculate(int left, const char *op, int right) {
    if (!strcmp(op, "+")) return left + right;
    if (!strcmp(op, "-")) return left - right;
    if (!strcmp(op, "*")) return left * right;
    if (!strcmp(op, "/")) return right ? left / right : 0;
    if (!strcmp(op, "%")) return right ? left % right : 0;
    if (!strcmp(op, "==")) return left == right;
    if (!strcmp(op, "!=")) return left != right;
    if (!strcmp(op, "<")) return left < right;
    if (!strcmp(op, ">")) return left > right;
    if (!strcmp(op, "<=")) return left <= right;
    if (!strcmp(op, ">=")) return left >= right;
    return 0;
}

void run_intermediate(char output[], size_t capacity) {
    char labels[MAX_CODE][MAX_TEXT];
    int label_positions[MAX_CODE];
    int label_count = 0;
    RuntimeValue values[MAX_CODE + MAX_VARS];
    int value_count = 0;
    size_t output_length = 0;
    int pc = 0;
    int steps = 0;

    output[0] = 0;

    for (int i = 0; i < code_count; i++) {
        char *colon = strchr(code[i], ':');
        if (colon && colon[1] == 0 && label_count < MAX_CODE) {
            size_t name_length = (size_t)(colon - code[i]);
            memcpy(labels[label_count], code[i], name_length);
            labels[label_count][name_length] = 0;
            label_positions[label_count] = i;
            label_count++;
        }
    }

    while (pc < code_count && steps++ < 100000) {
        char *line = code[pc];
        char *colon = strchr(line, ':');

        if (colon && colon[1] == 0) {
            pc++;
            continue;
        }

        if (!strncmp(line, "goto ", 5)) {
            char target[MAX_TEXT];
            sscanf(line + 5, "%127s", target);
            int target_pc = find_label(target, labels, label_positions, label_count);
            pc = target_pc >= 0 ? target_pc : pc + 1;
            continue;
        }

        if (!strncmp(line, "ifFalse ", 8)) {
            char condition[MAX_TEXT];
            char target[MAX_TEXT];
            sscanf(line, "ifFalse %127s goto %127s", condition, target);

            if (!get_runtime_value(condition, values, &value_count)) {
                int target_pc = find_label(target, labels, label_positions, label_count);
                pc = target_pc >= 0 ? target_pc : pc + 1;
            } else {
                pc++;
            }
            continue;
        }

        if (!strncmp(line, "printstr ", 9)) {
            append_string_literal(output, capacity, &output_length, line + 9);
            pc++;
            continue;
        }

        if (!strncmp(line, "print ", 6)) {
            char value[MAX_TEXT];
            char rendered[32];
            sscanf(line + 6, "%127s", value);
            snprintf(rendered, sizeof(rendered), "%d",
                     get_runtime_value(value, values, &value_count));
            append_output(output, capacity, &output_length, rendered);
            pc++;
            continue;
        }

        if (!strncmp(line, "return", 6)) {
            break;
        }

        char target[MAX_TEXT];
        char left[MAX_TEXT];
        char op[8];
        char right[MAX_TEXT];
        int parts = sscanf(line, "%127s = %127s %7s %127s",
                           target, left, op, right);

        if (parts == 4) {
            int left_value = get_runtime_value(left, values, &value_count);
            int right_value = get_runtime_value(right, values, &value_count);
            set_runtime_value(target, calculate(left_value, op, right_value),
                              values, &value_count);
        } else if (parts == 2) {
            set_runtime_value(target, get_runtime_value(left, values, &value_count),
                              values, &value_count);
        }

        pc++;
    }

    if (steps >= 100000) {
        append_output(output, capacity, &output_length,
                      "\nStopped: execution step limit reached.");
    }
}

int main(void) {
    char source[20000];
    size_t used = 0;
    int ch;

    while ((ch = getchar()) != EOF && used < sizeof(source) - 1) {
        source[used++] = (char)ch;
    }
    source[used] = 0;

    lex(source);

    puts("=== 1. LEXICAL ANALYSIS: TOKENS ===");
    printf("%-6s %-24s %s\n", "No.", "Token", "Line");
    printf("-----------------------------------\n");
    for (int i = 0; i < token_count - 1; i++) {
        printf("%-6d %-24s %d\n", i + 1, tokens[i].text, tokens[i].line);
    }
    putchar('\n');

    puts("=== 2. SYNTAX + SEMANTIC ANALYSIS ===");
    pos = 0;
    while (strcmp(peek(), "<END>")) {
        statement();
    }

    if (errors) {
        printf("Compilation found %d error(s). Fix them to generate intermediate code.\n\n",
               errors);
    } else {
        puts("Syntax is valid. Variables are declared before use.\n");
    }

    puts("=== 3. SYMBOL TABLE ===");
    if (!var_count) {
        puts("(empty)");
    }
    for (int i = 0; i < var_count; i++) {
        printf("%d. %s : int\n", i + 1, variables[i]);
    }

    puts("\n=== 4. INTERMEDIATE CODE (THREE-ADDRESS STYLE) ===");
    if (errors) {
        puts("Not generated because the source contains errors.");
    } else {
        for (int i = 0; i < code_count; i++) {
            printf("%2d: %s\n", i + 1, code[i]);
        }
    }

    puts("=== 5. PROGRAM OUTPUT ===");
    if (errors) {
        puts("Not run because the source contains errors.");
    } else {
        char output[4096];
        run_intermediate(output, sizeof(output));
        puts(output[0] ? output : "(no output)");
    }

    return errors ? 1 : 0;
}
