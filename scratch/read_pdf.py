import sys
import pypdf

pdf_path = r"c:\Users\LENOVO\Documents\PlatformIO\Projects\Layar BESS 10Kw\src\AMETHYST Series AT Command Manual v1.1.pdf"
output_path = r"c:\Users\LENOVO\Documents\PlatformIO\Projects\Layar BESS 10Kw\scratch\pdf_text.txt"

reader = pypdf.PdfReader(pdf_path)
num_pages = len(reader.pages)

with open(output_path, "w", encoding="utf-8") as f:
    f.write(f"Total pages: {num_pages}\n")
    for page_num in range(num_pages):
        f.write(f"\n--- Page {page_num + 1} ---\n")
        page = reader.pages[page_num]
        f.write(page.extract_text() or "")
        
print("Extraction completed successfully.")
