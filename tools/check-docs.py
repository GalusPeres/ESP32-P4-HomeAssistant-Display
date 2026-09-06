"""Check generated documentation links and the mirrored installer release."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import hashlib
import json
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "site").resolve()


class Page(HTMLParser):
    def __init__(self, path):
        super().__init__()
        self.ids = set()
        self.links = []
        self.feed(path.read_text(encoding="utf-8"))

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        if tag in ("a", "img", "script", "source", "link"):
            value = attrs.get("href") if tag in ("a", "link") else attrs.get("src")
            if value:
                self.links.append(value)


pages = {path.resolve(): Page(path) for path in root.rglob("*.html")}
errors = []
if not pages:
    errors.append(f"No generated HTML pages in {root}")
for path, page in pages.items():
    for href in page.links:
        url = urlsplit(href)
        if url.scheme or url.netloc:
            continue
        target = ((root / unquote(url.path).lstrip("/")) if url.path.startswith("/")
                  else (path.parent / unquote(url.path)) if url.path else path)
        if target.is_dir():
            target /= "index.html"
        target = target.resolve()
        if not target.exists():
            errors.append(f"{path.relative_to(root)}: missing {href}")
        elif url.fragment and target in pages and unquote(url.fragment) not in pages[target].ids:
            errors.append(f"{path.relative_to(root)}: missing anchor {href}")

manifest = root / "firmware/latest/release.json"
if not manifest.exists():
    errors.append("Missing installer release.json; prepare the release assets before publication.")
else:
    release = json.loads(manifest.read_text(encoding="utf-8"))
    for device in release["devices"]:
        for mode in ("update", "factory"):
            asset = device[mode]
            path = root / "firmware/latest" / asset["file"]
            if not path.is_file() or path.stat().st_size != asset["size"]:
                errors.append(f"Missing or incomplete release asset: {asset['file']}")
            elif hashlib.sha256(path.read_bytes()).hexdigest() != asset["sha256"]:
                errors.append(f"Release asset digest mismatch: {asset['file']}")

if errors:
    print("\n".join(errors))
    raise SystemExit(1)
print(f"PASS: {len(pages)} pages; local links, anchors and assets; "
      f"{len(release['devices'])} verified firmware pairs ({release['tag']}).")
