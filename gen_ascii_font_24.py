#!/usr/bin/env python3
"""Generate 24x24 ASCII bitmap font data for digits and common symbols."""

from PIL import Image, ImageDraw, ImageFont
import os

# ASCII characters needed: digits 0-9 and common symbols
ASCII_CHARS = "0123456789PaCdSs:-. "

FONT_SIZE_H = 24  # 高度
FONT_SIZE_W = 16  # 宽度
BYTES_PER_ROW = 2  # 16 bits = 2 bytes per row
ROWS = 24

def char_to_bitmap(ch, font, width=16, height=24):
    """Render a character to a 1-bit bitmap (width x height)."""
    img = Image.new('1', (width, height), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (width - tw) // 2 - bbox[0]
    y = (height - th) // 2 - bbox[1]
    draw.text((x, y), ch, fill=1, font=font)
    
    pixels = img.load()
    matrix = []
    for row in range(height):
        row_bytes = []
        for byte_idx in range(BYTES_PER_ROW):
            val = 0
            for bit in range(8):
                col = byte_idx * 8 + bit
                if col < width and pixels[col, row] == 1:
                    val |= (0x01 << bit)
            row_bytes.append(val)
        matrix.append(row_bytes)
    return matrix

def main():
    font_paths = [
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
    ]
    
    font = None
    for fp in font_paths:
        if os.path.exists(fp):
            try:
                font = ImageFont.truetype(fp, FONT_SIZE_H)
                print(f"Using font: {fp}")
                break
            except:
                continue
    
    if font is None:
        print("ERROR: No font found!")
        return
    
    # Generate C code for ASCII 24x16 font
    output = []
    output.append('// 24x16 ASCII字符点阵')
    output.append('struct ASCII_24 {')
    output.append('    char ch;')
    output.append(f'    unsigned char matrix[{ROWS * BYTES_PER_ROW}];')
    output.append('};')
    output.append('')
    output.append(f'const ASCII_24 font_ascii_24[] PROGMEM = {{')
    
    for ch in ASCII_CHARS:
        matrix = char_to_bitmap(ch, font, FONT_SIZE_W, FONT_SIZE_H)
        flat = []
        for row in matrix:
            flat.extend(row)
        
        output.append(f'    {{ \'{ch}\', {{')
        for row_idx in range(ROWS):
            start = row_idx * BYTES_PER_ROW
            row_hex = ",".join(f"0x{flat[j]:02X}" for j in range(start, start + BYTES_PER_ROW))
            comma = "," if row_idx < ROWS - 1 else ""
            output.append(f'        {row_hex}{comma}')
        output.append(f'    }} }},')
    
    output.append('};')
    output.append('')
    output.append(f'const int ASCII_24_COUNT = sizeof(font_ascii_24) / sizeof(ASCII_24);')
    
    # Write to a separate header file
    out_path = os.path.join(os.path.dirname(__file__), 'src', 'ascii_font_24.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output))
    
    print(f"Generated {len(ASCII_CHARS)} ASCII characters (24x16) -> {out_path}")
    print(f"Each char: {ROWS * BYTES_PER_ROW} bytes, total: {len(ASCII_CHARS) * ROWS * BYTES_PER_ROW} bytes")

if __name__ == '__main__':
    main()
