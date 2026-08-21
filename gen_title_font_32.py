#!/usr/bin/env python3
"""Generate 32x32 Chinese bitmap font data for title characters."""

from PIL import Image, ImageDraw, ImageFont
import os

# Title characters: 欢迎使用正压防爆系统
TITLE_CHARS = ["欢", "迎", "使", "用", "正", "压", "防", "爆", "系", "统"]

FONT_SIZE = 32
BYTES_PER_ROW = 4  # 32 bits = 4 bytes per row
ROWS = 32

def char_to_bitmap(ch, font, size=32):
    """Render a character to a 1-bit bitmap (32x32)."""
    img = Image.new('1', (size, size), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (size - tw) // 2 - bbox[0]
    y = (size - th) // 2 - bbox[1]
    draw.text((x, y), ch, fill=1, font=font)
    
    pixels = img.load()
    matrix = []
    for row in range(size):
        row_bytes = []
        for byte_idx in range(BYTES_PER_ROW):
            val = 0
            for bit in range(8):
                col = byte_idx * 8 + bit
                if col < size and pixels[col, row] == 1:
                    val |= (0x01 << bit)
            row_bytes.append(val)
        matrix.append(row_bytes)
    return matrix

def main():
    font_paths = [
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/msyh.ttc",
    ]
    
    font = None
    for fp in font_paths:
        if os.path.exists(fp):
            try:
                font = ImageFont.truetype(fp, FONT_SIZE)
                print(f"Using font: {fp}")
                break
            except:
                continue
    
    if font is None:
        print("ERROR: No Chinese font found!")
        return
    
    # Generate C code for 32x32 font
    output = []
    output.append('// 32x32标题字体点阵')
    output.append('struct CHINESE_32 {')
    output.append('    const char* index;')
    output.append(f'    unsigned char matrix[{ROWS * BYTES_PER_ROW}];')
    output.append('};')
    output.append('')
    output.append(f'const CHINESE_32 font_title_32[] PROGMEM = {{')
    
    for ch in TITLE_CHARS:
        matrix = char_to_bitmap(ch, font, FONT_SIZE)
        flat = []
        for row in matrix:
            flat.extend(row)
        
        output.append(f'    {{ "{ch}", {{')
        for row_idx in range(ROWS):
            start = row_idx * BYTES_PER_ROW
            row_hex = ",".join(f"0x{flat[j]:02X}" for j in range(start, start + BYTES_PER_ROW))
            comma = "," if row_idx < ROWS - 1 else ""
            output.append(f'        {row_hex}{comma}')
        output.append(f'    }} }},')
    
    output.append('};')
    output.append('')
    output.append(f'const int FONT_TITLE_COUNT = sizeof(font_title_32) / sizeof(CHINESE_32);')
    
    # Write to a separate header file
    out_path = os.path.join(os.path.dirname(__file__), 'src', 'title_font_32.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output))
    
    print(f"Generated {len(TITLE_CHARS)} title characters (32x32) -> {out_path}")
    print(f"Each char: {ROWS * BYTES_PER_ROW} bytes, total: {len(TITLE_CHARS) * ROWS * BYTES_PER_ROW} bytes")

if __name__ == '__main__':
    main()
