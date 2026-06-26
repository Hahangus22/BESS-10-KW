import zipfile
import xml.etree.ElementTree as ET
import os

docx_path = r"c:\Users\LENOVO\Documents\PlatformIO\Projects\Layar BESS 10Kw jual\Pin Out.docx"
output_path = r"c:\Users\LENOVO\Documents\PlatformIO\Projects\Layar BESS 10Kw jual\scratch\pin_out_text.txt"

if not os.path.exists(docx_path):
    print(f"Error: {docx_path} not found.")
else:
    try:
        with zipfile.ZipFile(docx_path) as docx:
            xml_content = docx.read('word/document.xml')
            root = ET.fromstring(xml_content)
            
            # NS map for docx XML tags
            ns = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
            
            text_runs = []
            for paragraph in root.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}p'):
                p_text = []
                for run in paragraph.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t'):
                    if run.text:
                        p_text.append(run.text)
                if p_text:
                    text_runs.append("".join(p_text))
            
            with open(output_path, "w", encoding="utf-8") as f:
                f.write("\n".join(text_runs))
            print(f"Successfully extracted text to {output_path}")
    except Exception as e:
        print(f"Error: {e}")
