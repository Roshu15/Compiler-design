"""Local web page for the C compiler visualizer. Uses only Python's standard library."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
PROGRAM = ROOT / ("mini_compiler.exe" if sys.platform == "win32" else "mini_compiler")
MARKERS = [
    "=== 1. LEXICAL ANALYSIS: TOKENS ===",
    "=== 2. SYNTAX + SEMANTIC ANALYSIS ===",
    "=== 3. SYMBOL TABLE ===",
    "=== 4. INTERMEDIATE CODE (THREE-ADDRESS STYLE) ===",
    "=== 5. PROGRAM OUTPUT ===",
]


def split_phases(stdout: str, stderr: str) -> dict:
    parts = stdout.splitlines()
    phases = {"tokens": "", "checks": "", "symbols": "", "ir": "", "program_output": ""}
    current = None
    mapping = dict(zip(MARKERS, phases))
    for line in parts:
        if line in mapping:
            current = mapping[line]
        elif current:
            phases[current] += line + "\n"
    if stderr.strip():
        phases["checks"] = (phases["checks"].rstrip() + "\n\n" + stderr.strip() + "\n").lstrip()
    return {key: value.strip() for key, value in phases.items()}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_POST(self):
        if self.path != "/compile":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length > 100_000:
                self.send_error(413, "Source file is too large")
                return
            request = json.loads(self.rfile.read(length).decode("utf-8"))
            source = request.get("source", "")
            if not isinstance(source, str):
                raise ValueError("source must be text")
            if not PROGRAM.is_file():
                raise FileNotFoundError(f"Compiler executable not found: {PROGRAM.name}. Build compiler.c first.")
            result = subprocess.run(
                [str(PROGRAM)], input=source, text=True, capture_output=True,
                cwd=ROOT, timeout=5, encoding="utf-8", errors="replace",
            )
            data = split_phases(result.stdout, result.stderr)
            data["success"] = result.returncode == 0
            if result.returncode < 0:
                data["checks"] += f"\nCompiler stopped with exit code {result.returncode}."
            self.send_json(200, data)
        except subprocess.TimeoutExpired:
            self.send_json(408, {"success": False, "checks": "Compilation timed out."})
        except FileNotFoundError as error:
            self.send_json(503, {"success": False, "checks": str(error)})
        except (ValueError, json.JSONDecodeError) as error:
            self.send_json(400, {"success": False, "checks": f"Invalid request: {error}"})
        except Exception as error:
            self.send_json(500, {"success": False, "checks": f"Server error: {error}"})

    def send_json(self, status_code, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        print("[web] " + format % args)


if __name__ == "__main__":
    address = ("127.0.0.1", 8765)
    print(f"Mini C Compiler is ready at http://{address[0]}:{address[1]}")
    print("Keep this terminal open while using the page. Press Ctrl+C to stop.")
    try:
        ThreadingHTTPServer(address, Handler).serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
