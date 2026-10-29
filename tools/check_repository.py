#!/usr/bin/env python3
"""Check tracked source hygiene and repository-relative Markdown links."""
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

root = Path(__file__).resolve().parents[1]
files = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode().split("\0")
errors = []
for name in filter(None, files):
    path = root / name
    if not path.exists():
        continue  # Allows running during staged file moves.
    if path.suffix in {".o", ".a", ".bin", ".elf", ".zip"} or name == "include/config.h":
        errors.append(f"{name}: generated/private file must not be tracked")
    if path.suffix in {".c", ".cpp", ".h"}:
        source = path.read_text()
        if re.search(r'(?:WIFI_SSID|WIFI_PASSWORD|AP_PASSWORD)\s+"[^"\n]+"', source):
            errors.append(f"{name}: network credential must not be committed")
    if path.suffix == ".md":
        for target in re.findall(r"\[[^\]]*\]\(([^\s)]+)(?:\s+[^)]*)?\)", path.read_text()):
            parsed = urlsplit(target.strip("<>"))
            if parsed.scheme or target.startswith(("#", "//")):
                continue
            destination = (path.parent / unquote(parsed.path)).resolve()
            if not destination.exists():
                errors.append(f"{name}: missing local link {target}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Repository hygiene and local documentation links passed.")
