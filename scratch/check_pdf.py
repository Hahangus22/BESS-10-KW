import sys
import os

try:
    import pypdf
    print("pypdf available")
except ImportError:
    try:
        import PyPDF2
        print("PyPDF2 available")
    except ImportError:
        print("No pdf reader library")
        sys.exit(1)
