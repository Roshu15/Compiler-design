"""Create a simple, printable PDF from this project's PROJECT_GUIDE.md."""

from pathlib import Path
import re
import textwrap

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "PROJECT_GUIDE.md"
OUTPUT = ROOT / "Compiler_Visualizer_Six_Person_Guide.pdf"
PAGE_W, PAGE_H = 612, 792
LEFT, RIGHT, TOP, BOTTOM = 58, 554, 724, 54


def clean_inline(text):
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    return text.replace("**", "").replace("`", "").replace("*", "")


def pdf_text(text):
    raw = text.encode("cp1252", errors="replace")
    return b"(" + raw.replace(b"\\", b"\\\\").replace(b"(", b"\\(").replace(b")", b"\\)") + b")"


class Document:
    def __init__(self):
        self.pages = []
        self.commands = []
        self.y = TOP
        self.page()

    def page(self):
        if self.commands:
            self.pages.append(self.commands)
        self.commands = []
        self.y = TOP

    def ensure(self, height):
        if self.y - height < BOTTOM:
            self.page()

    def line(self, text, *, size=10, font="F1", color=(.17, .2, .26), x=LEFT, leading=None):
        leading = leading or size * 1.45
        self.ensure(leading)
        r, g, b = color
        command = (f"BT /{font} {size} Tf {r:.3f} {g:.3f} {b:.3f} rg "
                   f"1 0 0 1 {x:.1f} {self.y:.1f} Tm ").encode("ascii")
        self.commands.append(command + pdf_text(text) + b" Tj ET\n")
        self.y -= leading

    def gap(self, amount=6):
        self.y -= amount
        if self.y < BOTTOM:
            self.page()

    def paragraph(self, text, *, size=10, indent=0, color=(.17, .2, .26), font="F1"):
        max_chars = max(35, int((RIGHT - LEFT - indent) / (size * .51)))
        lines = textwrap.wrap(text, width=max_chars, break_long_words=True, break_on_hyphens=False) or [""]
        for line in lines:
            self.line(line, size=size, font=font, color=color, x=LEFT + indent)

    def heading(self, text, level):
        self.gap(8 if level < 3 else 4)
        size = {1: 23, 2: 16, 3: 12}.get(level, 11)
        color = (.08, .35, .42) if level == 1 else (.18, .28, .43)
        self.paragraph(clean_inline(text), size=size, color=color)
        self.gap(4)

    def finish(self):
        if self.commands:
            self.pages.append(self.commands)
        return self.pages


def parse_markdown(markdown):
    doc = Document()
    lines = markdown.splitlines()
    in_code = False
    paragraph_lines = []
    quote_lines = []

    def flush_paragraph():
        if paragraph_lines:
            doc.paragraph(clean_inline(" ".join(paragraph_lines)))
            doc.gap(5)
            paragraph_lines.clear()

    def flush_quote():
        if quote_lines:
            quote = clean_inline(" ".join(quote_lines))
            max_chars = 82
            for part in textwrap.wrap(quote, width=max_chars, break_long_words=True):
                doc.line(part, size=10, color=(.22, .31, .40), x=LEFT + 15)
            doc.gap(5)
            quote_lines.clear()

    for raw in lines:
        stripped = raw.strip()
        if stripped.startswith("```"):
            flush_paragraph(); flush_quote()
            in_code = not in_code
            doc.gap(2)
            continue
        if in_code:
            code_lines = textwrap.wrap(raw, width=100, replace_whitespace=False, drop_whitespace=False) or [""]
            for code_line in code_lines:
                doc.line(code_line, size=8.2, font="F2", color=(.12, .32, .39), leading=11, x=LEFT + 9)
            continue
        if re.match(r"## Part \d+ ", stripped):
            flush_paragraph(); flush_quote()
            doc.page()
        if not stripped:
            flush_paragraph(); flush_quote(); doc.gap(3); continue
        if stripped.startswith("#"):
            flush_paragraph(); flush_quote()
            count = len(stripped) - len(stripped.lstrip("#"))
            doc.heading(stripped[count:].strip(), min(count, 3))
            continue
        if stripped.startswith(">"):
            flush_paragraph()
            quote_lines.append(stripped[1:].strip())
            continue
        if stripped.startswith("|"):
            flush_paragraph(); flush_quote()
            if re.fullmatch(r"[|:\- ]+", stripped):
                continue
            cells = [clean_inline(c.strip()) for c in stripped.strip("|").split("|")]
            doc.paragraph("   |   ".join(cells), size=8.8, font="F1")
            doc.gap(1)
            continue
        if stripped in {"---", "***", "___"}:
            flush_paragraph(); flush_quote(); doc.gap(7); continue
        if stripped.startswith("- ") or re.match(r"\d+\.\s", stripped):
            flush_paragraph(); flush_quote()
            item = re.sub(r"^(?:-\s+|\d+\.\s+)", "- ", stripped)
            doc.paragraph(clean_inline(item), indent=10)
            continue
        flush_quote()
        paragraph_lines.append(stripped)

    flush_paragraph(); flush_quote()
    return doc.finish()


def build_pdf(pages):
    objects = [None] * (5 + 2 * len(pages))
    objects[1] = b"<< /Type /Catalog /Pages 2 0 R >>"
    page_ids = [5 + i * 2 for i in range(len(pages))]
    objects[2] = (b"<< /Type /Pages /Kids [" + b" ".join(f"{n} 0 R".encode("ascii") for n in page_ids)
                  + f"] /Count {len(pages)} >>".encode("ascii"))
    objects[3] = b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"
    objects[4] = b"<< /Type /Font /Subtype /Type1 /BaseFont /Courier /Encoding /WinAnsiEncoding >>"
    total = len(pages)
    for index, page_commands in enumerate(pages):
        page_id = 5 + index * 2
        content_id = page_id + 1
        page_commands = list(page_commands)
        page_commands.insert(0, b"0.80 0.84 0.89 RG 58 750 m 554 750 l S\n")
        page_commands.insert(0, b"BT /F1 8 Tf 0.35 0.42 0.51 rg 1 0 0 1 58 762 Tm (MINI C COMPILER VISUALIZER  |  SIX-PERSON GUIDE) Tj ET\n")
        page_commands.append(b"0.80 0.84 0.89 RG 58 42 m 554 42 l S\n")
        footer = f"Page {index + 1} of {total}".encode("ascii")
        page_commands.append(b"BT /F1 8 Tf 0.35 0.42 0.51 rg 1 0 0 1 480 27 Tm (" + footer + b") Tj ET\n")
        stream = b"".join(page_commands)
        objects[page_id] = (f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {PAGE_W} {PAGE_H}] "
                            f"/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents {content_id} 0 R >>").encode("ascii")
        objects[content_id] = b"<< /Length " + str(len(stream)).encode("ascii") + b" >>\nstream\n" + stream + b"endstream"

    output = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for number, obj in enumerate(objects[1:], start=1):
        offsets.append(len(output))
        output.extend(f"{number} 0 obj\n".encode("ascii"))
        output.extend(obj)
        output.extend(b"\nendobj\n")
    xref_offset = len(output)
    output.extend(f"xref\n0 {len(objects)}\n".encode("ascii"))
    output.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        output.extend(f"{offset:010d} 00000 n \n".encode("ascii"))
    output.extend(f"trailer\n<< /Size {len(objects)} /Root 1 0 R >>\nstartxref\n{xref_offset}\n%%EOF\n".encode("ascii"))
    return bytes(output)


if __name__ == "__main__":
    pages = parse_markdown(SOURCE.read_text(encoding="utf-8"))
    OUTPUT.write_bytes(build_pdf(pages))
    print(f"Created {OUTPUT.name} ({len(pages)} pages)")
