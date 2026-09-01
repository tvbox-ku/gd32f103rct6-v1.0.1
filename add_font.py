import re
from PIL import Image, ImageDraw, ImageFont

missing = ['欠', '断', '护']
fontPath = r'C:\Windows\Fonts\Deng.ttf'
pilFont = ImageFont.truetype(fontPath, 24)

results = []
for ch in missing:
    img = Image.new('1', (24, 24), 0)
    draw = ImageDraw.Draw(img)
    bbox = pilFont.getbbox(ch)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    ox = (24 - w) // 2 - bbox[0]
    oy = (24 - h) // 2 - bbox[1]
    draw.text((ox, oy), ch, fill=1, font=pilFont)
    matrix = []
    for row in range(24):
        for byteCol in range(3):
            val = 0
            for bit in range(8):
                col = byteCol * 8 + bit
                if col < 24 and img.getpixel((col, row)):
                    val |= (1 << bit)
            matrix.append(val)
    results.append((ch, matrix))

with open('src/my_font.h', encoding='utf-8') as f:
    content = f.read()

# 找到 font_24 数组的 }; 位置
font24_start = content.find('const CHINESE_24 font_24[]')
font24_end = content.find('};', font24_start)

new_entries = ''
for ch, matrix in results:
    new_entries += f'    {{ "{ch}", {{\n'
    line = ''
    for i, v in enumerate(matrix):
        line += f'0x{v:02X},'
        if (i + 1) % 16 == 0:
            new_entries += line + '\n'
            line = ''
    if line:
        new_entries += line + '\n'
    new_entries += '    } },\n'

new_content = content[:font24_end] + new_entries + content[font24_end:]
with open('src/my_font.h', 'w', encoding='utf-8') as f:
    f.write(new_content)

print(f'已添加 {len(results)} 个字: {"".join(missing)}')
