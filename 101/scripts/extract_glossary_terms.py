#!/usr/bin/env python3
"""Extract lessons/glossary.md into lessons/assets/terms-data.js for the
hover-glossary popup (terms-popup.js). One-off/regenerate-on-demand script,
not part of the mkdocs build — re-run this manually after editing glossary.md.

Parses the confirmed glossary format: a line `**Term**` or `**Term (Alias)**`
followed by a definition paragraph, until the next `**`-prefixed line or a
`---` separator. Two special cases:
  - `**Term — see also** *X*, *Y*.` appendix lines (Term already has its own
    full entry above) are skipped — the canonical entry already covers it.
  - `**Term — see** *Target*.` standalone redirects (Term has no entry of
    its own anywhere) are kept as a short "See Target." entry.
A short, code-like parenthetical (no internal whitespace, 2-8 chars, at
least one uppercase letter — e.g. "ASan", "FFD", "WASM") is registered as an
extra searchable label on the same entry, since lessons mostly use the short
form in running prose, not the spelled-out name.

Does NOT catch: parentheticals that are themselves multi-word disambiguators
(e.g. "(effect)", "(call stack)") — intentionally excluded so generic English
words never become accidental hover targets.
"""
import json
import re
import sys
from pathlib import Path

GLOSSARY = Path(__file__).parent.parent / "lessons" / "glossary.md"
OUT = Path(__file__).parent.parent / "lessons" / "assets" / "terms-data.js"

TERM_LINE = re.compile(r"^\*\*(.+?)\*\*(.*)$")
ALIAS_OK = re.compile(r"^[A-Za-z][A-Za-z0-9]{1,7}$")


def slug(label):
    return re.sub(r"[^a-z0-9]+", "", label.lower())


def alias_from_paren(paren_text):
    if not ALIAS_OK.match(paren_text):
        return None
    if not any(c.isupper() for c in paren_text):
        return None
    return paren_text


def main():
    lines = GLOSSARY.read_text().splitlines()
    entries = []  # list of {labels: [...], body: str}
    known_slugs = set()

    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        m = TERM_LINE.match(line)
        if not m:
            i += 1
            continue

        head, rest = m.group(1).strip(), m.group(2).strip()

        if " — see also" in head:
            # Appendix note on a term that already has its own full entry above —
            # always skip, regardless of parse order (no known_slugs dependency).
            i += 1
            continue

        if " — see" in head:
            base_term = head.split(" — see")[0].strip()
            target = re.sub(r"^\*|\*\.?$|\.$", "", rest).strip("* .")
            entries.append({"labels": [base_term], "body": "See " + target + "."})
            known_slugs.add(slug(base_term))
            i += 1
            continue

        primary = head
        aliases = []
        paren_match = re.search(r"\(([^)]*)\)\s*$", head)
        if paren_match:
            primary = head[: paren_match.start()].strip()
            alias = alias_from_paren(paren_match.group(1).strip())
            if alias:
                aliases.append(alias)

        body_lines = []
        i += 1
        while i < n and lines[i].strip() not in ("---",) and not TERM_LINE.match(lines[i]):
            if lines[i].strip():
                body_lines.append(lines[i].strip())
            i += 1
        body = " ".join(body_lines).strip()
        if not body:
            continue

        labels = [primary] + aliases
        for lbl in labels:
            known_slugs.add(slug(lbl))
        entries.append({"labels": labels, "body": body})

    js = "window.CF_TERMS = " + json.dumps(entries, indent=None, ensure_ascii=False) + ";\n"
    OUT.write_text(js)
    print(f"Wrote {len(entries)} entries ({sum(len(e['labels']) for e in entries)} labels) to {OUT}")


if __name__ == "__main__":
    sys.exit(main())
