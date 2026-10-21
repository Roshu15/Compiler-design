# Mini C Compiler Visualizer

A small educational compiler written in C, with a browser frontend. It accepts a
simple subset of C and displays each compiler phase in its own panel:

1. Lexical analysis: splits the source into tokens.
2. Syntax and semantic analysis: checks the supported statement forms and
   verifies that variables are declared before use.
3. Symbol table: lists declared integer variables.
4. Intermediate code: displays simple three-address instructions and labels.

## Build and open the frontend

You need GCC (MinGW on Windows) and Python 3. The compiler is written in C;
Python runs a small local web server to connect the page to the compiler.

On Windows, open PowerShell in this project folder and run:

```powershell
gcc .\compiler.c -o .\mini_compiler.exe
python .\server.py
```

Leave that terminal open. Open this address in your browser:

```powershell
http://127.0.0.1:8765
```

Click **Load sample** to fill the editor, then click **Compile and view phases**.
The tokens, syntax and semantic checks, symbol table, and intermediate code
appear in separate panels. You can edit the source in the page and compile it
again. To stop the local server, press **Ctrl+C** in PowerShell.

To run the compiler directly in the terminal instead, use:

```powershell
cmd /c "mini_compiler.exe < sample.c"
```

On Linux or macOS, build with `gcc -std=c99 compiler.c -o mini_compiler`, start
the page with `python3 server.py`, and open `http://127.0.0.1:8765`.

## Supported language

- `int` declarations and assignments
- Integer arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- `if` and `else`
- `while` and `for` loops
- `printf` with a string or a simple value
- `//` single-line comments
- Braces for statement blocks

This is intentionally a teaching subset, not a full C compiler. Functions,
arrays, pointers, floating-point values, and general C format strings are not
implemented. The only function wrapper supported is `int main() { ... }`;
other function declarations are outside the supported grammar.

For the detailed source-order explanation and build walkthrough, see
[PROJECT_GUIDE.md](PROJECT_GUIDE.md).

The six-speaker printable version is
[Compiler_Visualizer_Six_Person_Guide.pdf](Compiler_Visualizer_Six_Person_Guide.pdf).
The six assigned code sections are marked with `PART 1` through `PART 6`
comments in `compiler.c`.
