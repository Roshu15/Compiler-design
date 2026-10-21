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
    static char names[2][24];
    static int slot;

    slot = (slot + 1) % 2;
    snprintf(names[slot], 24, "t%d", ++temp_id);
    return names[slot];
}

char *new_label(void) {
    static char names[2][24];
    static int slot;

    slot = (slot + 1) % 2;
    snprintf(names[slot], 24, "L%d", ++label_id);
    return names[slot];
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
            pos++;

            if (accept(",")) {
                char *v = expression();
                char out[MAX_TEXT * 2];
                snprintf(out, sizeof(out), "print %s", v);
                emit(out);
            } else {
                emit("print string");
            }
        } else {
            char *v = expression();
            char out[MAX_TEXT * 2];
            snprintf(out, sizeof(out), "print %s", v);
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
    for (int i = 0; i < token_count - 1; i++) {
        printf("%-12s%s", tokens[i].text, (i % 6 == 5) ? "\n" : " ");
    }
    puts("\n");

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

    return errors ? 1 : 0;
}
