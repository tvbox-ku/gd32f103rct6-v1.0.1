#!/usr/bin/env python3
"""Reformat my_font.h: consistent spacing, no duplicate entries, unified style."""
import re, os

SRC = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "src", "my_font.h"))

with open(SRC, "r", encoding="utf-8") as f:
    content = f.read()

# Remove all C-style /* ... */ comments inside data blocks
# (they contain legacy index/comments like /*"欢",0*/)
content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

# Strip trailing whitespace per line
lines = content.split('\n')
lines = [l.rstrip() for l in lines]
content = '\n'.join(lines)

# === Rebuild font_16 array ===
# Extract the font_16 block
m = re.search(r'(const CHINESE_16 font_16\[\] PROGMEM = \{)(.*?)(^\s*\};)', content, re.DOTALL | re.MULTILINE)
if not m:
    print("ERROR: cannot find font_16 array")
    exit(1)

prefix = m.group(1)
body = m.group(2)
suffix = m.group(3)

# Find all strings for character index
char_indices = re.findall(r'"([^"]*)"', body)

# Extract all hex bytes as flat arrays per character
# Pattern: { "X", { 0x00, 0x00, ... } },
entries = re.findall(r'\{\s*"([^"]*)",\s*\{([^}]+)\}\s*\}', body)

# Remove duplicates (keep last occurrence)
seen = {}
for ch, data_str in entries:
    seen[ch] = data_str  # later overwrites earlier

# Build the new body
new_entries = []
for ch, data_str in seen.items():
    # Extract all hex bytes
    hex_vals = re.findall(r'0x[0-9A-Fa-f]+', data_str)
    # Ensure exactly 32 bytes
    if len(hex_vals) > 32:
        hex_vals = hex_vals[:32]
    # Format: 2 lines of 16 bytes each, aligned
    line1 = ', '.join(hex_vals[:16])
    line2 = ', '.join(hex_vals[16:32])
    new_entries.append(f'    {{ "{ch}", {{{line1},\n{line2}}} }}')

new_body = '\n'.join(new_entries)

# Reassemble
new_content = content[:m.start()] + prefix + '\n' + new_body + '\n' + suffix + content[m.end():]

# Fix FONT_COUNT - should be #define not const int
new_content = re.sub(
    r'const int FONT_COUNT = sizeof\(font_16\) / sizeof\(CHINESE_16\);',
    r'#define FONT_COUNT (sizeof(font_16)/sizeof(CHINESE_16))',
    new_content
)

# Remove duplicate #define FONT_COUNT if exists
defs = re.findall(r'#define FONT_COUNT.*', new_content)
if len(defs) > 1:
    # Keep only the last one
    for d in defs[:-1]:
        new_content = new_content.replace(d + '\n', '', 1)

# Clean up extra blank lines (max 1 blank line between blocks)
new_content = re.sub(r'\n{3,}', '\n\n', new_content)

# Ensure #endif has proper blank line before it
new_content = re.sub(r'\n+#endif', '\n\n#endif', new_content)

with open(SRC, "w", encoding="utf-8") as f:
    f.write(new_content)

# Stats
print(f"Written: {SRC}")
print(f"  font_16 chars: {len(seen)} (removed {len(entries)-len(seen)} duplicates)")
print(f"  file size: {len(new_content)} bytes")
