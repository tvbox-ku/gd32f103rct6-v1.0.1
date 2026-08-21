#!/usr/bin/env python3
"""Generate 24x24 Chinese bitmap font data from SimSun font."""

from PIL import Image, ImageDraw, ImageFont
import os

# All unique characters from current my_font.h (order matters for grouping)
CHARS = [
    # 标题/公司
    "中","国","谷","子","防","爆",
    # 正压防爆系统
    "正","压","系","统",
    # 菜单
    "启","动","设","置","调","试",
    # 服务电话科技
    "服","务","电","话","科","技",
    # 有限公司危险
    "有","限","公","司","危","险",
    # 正压启动界面
    "进","气","换","倒","计","时",
    "柜","内","力","低","于","开",
    "报","警","消","音","返","回",
    "主","页",
    # 调试界面
    "告","请","确","认","现","场",
    "无","炸","性","体","严","禁",
    "在","所","区","域","使","用",
    "功","能","以","免","发","生",
    "待","机","全",
    # 密码界面
    "第","次","输","入","密","码",
    "加","下","位","对",
    # 其他
    "欢","迎","送","已","温","度","℃",
    "结","束","复","保","存","按",
    "短","长","取","修","改","传",
    "感","器","校","准","参","数",
    "一","选","项","放","弃","上",
    "间","值","精","工","细","造",
    "恢","出","厂","行","排","常",
    "高","定","吹","扫","介","和",
    "控","制","总",
]

# Remove duplicates while preserving order
seen = set()
unique_chars = []
for c in CHARS:
    if c not in seen:
        seen.add(c)
        unique_chars.append(c)

FONT_SIZE = 24
BYTES_PER_ROW = 3  # 24 bits = 3 bytes per row
ROWS = 24

def char_to_bitmap(ch, font, size=24):
    """Render a character to a 1-bit bitmap (24x24)."""
    img = Image.new('1', (size, size), 0)  # black background, 0=white
    draw = ImageDraw.Draw(img)
    # Get text bbox for centering
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    # Center the character
    x = (size - tw) // 2 - bbox[0]
    y = (size - th) // 2 - bbox[1]
    draw.text((x, y), ch, fill=1, font=font)
    
    # Convert to matrix: 1=foreground (white pixel on black = character)
    pixels = img.load()
    matrix = []
    for row in range(size):
        row_bytes = []
        for byte_idx in range(BYTES_PER_ROW):
            val = 0
            for bit in range(8):
                col = byte_idx * 8 + bit
                if col < size and pixels[col, row] == 1:
                    val |= (0x01 << bit)  # LSB first, matching original format
            row_bytes.append(val)
        matrix.append(row_bytes)
    return matrix

def format_bytes(data, comment=""):
    """Format bytes as C hex array content."""
    parts = []
    for i, b in enumerate(data):
        parts.append(f"0x{b:02X}")
    return ",".join(parts)

def main():
    # Try to load SimSun (宋体) font
    font_paths = [
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simsun.ttf",
        "C:/Windows/Fonts/simhei.ttf",  # 黑体 fallback
        "C:/Windows/Fonts/msyh.ttc",    # 微软雅黑 fallback
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
    
    # Generate bitmap data
    output_lines = []
    output_lines.append('#ifndef __MY_FONT_H__')
    output_lines.append('#define __MY_FONT_H__')
    output_lines.append('')
    output_lines.append('#include <Arduino.h>')
    output_lines.append('')
    output_lines.append('// 24x24汉字点阵结构体')
    output_lines.append('struct CHINESE_24 {')
    output_lines.append('    const char* index;')
    output_lines.append(f'    unsigned char matrix[{ROWS * BYTES_PER_ROW}];    // 24行 x 3字节/行 = {ROWS * BYTES_PER_ROW}字节')
    output_lines.append('};')
    output_lines.append('')
    output_lines.append('// 8x16 ASCII字符点阵结构体')
    output_lines.append('struct ASCII_16 {')
    output_lines.append('    char ch;')
    output_lines.append('    unsigned char matrix[16];    // 16行 x 1字节/行 = 16字节')
    output_lines.append('};')
    output_lines.append('')
    output_lines.append(f'const CHINESE_24 font_24[] PROGMEM = {{')
    
    for i, ch in enumerate(unique_chars):
        matrix = char_to_bitmap(ch, font, FONT_SIZE)
        # Flatten matrix
        flat = []
        for row in matrix:
            flat.extend(row)
        
        hex_str = format_bytes(flat)
        output_lines.append(f'    {{ "{ch}", {{')
        # Format as 6 bytes per line (2 characters per line)
        for row_idx in range(ROWS):
            start = row_idx * BYTES_PER_ROW
            row_hex = ",".join(f"0x{flat[j]:02X}" for j in range(start, start + BYTES_PER_ROW))
            comma = "," if row_idx < ROWS - 1 else ""
            output_lines.append(f'        {row_hex}{comma}')
        output_lines.append(f'    }} }},')
    
    output_lines.append('};')
    output_lines.append('')
    output_lines.append(f'const int FONT_COUNT = sizeof(font_24) / sizeof(CHINESE_24);')
    output_lines.append('')
    output_lines.append('// 8x16 ASCII字符点阵')
    output_lines.append('const ASCII_16 ascii_16[] PROGMEM = {')
    output_lines.append('    { \':\', {')
    output_lines.append('        0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,')
    output_lines.append('        0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,')
    output_lines.append('    } },')
    output_lines.append('};')
    output_lines.append('')
    output_lines.append('const int ASCII_FONT_COUNT = sizeof(ascii_16) / sizeof(ASCII_16);')
    output_lines.append('')
    output_lines.append('#endif')
    
    # Write output with UTF-8 encoding (same as main.cpp)
    out_path = os.path.join(os.path.dirname(__file__), 'src', 'my_font.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output_lines))
    
    print(f"Generated {len(unique_chars)} characters -> {out_path}")
    print(f"Each char: {ROWS * BYTES_PER_ROW} bytes, total: {len(unique_chars) * ROWS * BYTES_PER_ROW} bytes")

if __name__ == '__main__':
    main()
