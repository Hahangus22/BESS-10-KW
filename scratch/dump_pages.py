import sys
from pypdf import PdfReader

reader = PdfReader("src/AMETHYST Series AT Command Manual v1.0.pdf")
with open("scratch/dump.txt", "w", encoding="utf-8") as f:
    for i, page in enumerate(reader.pages):
        f.write(f"=== PAGE {i+1} ===\n")
        f.write(page.extract_text())
        f.write("\n\n")
print("Dumped successfully to scratch/dump.txt")
