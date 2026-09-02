"""
生成20号字体点阵字库：
  - 汉字 20x20 (3字节/行 x 20行 = 60字节/字)
  - ASCII 20x14 (2字节/行 x 20行 = 40字节/字)
输出: src/font_20.h, src/ascii_font_20.h
"""
from PIL import Image, ImageDraw, ImageFont

fontPath = r'C:\Windows\Fonts\Deng.ttf'

# ====== 汉字 20x20 ======
chinese_chars = ['运', '行', '时', '间', '天']
pilFontCN = ImageFont.truetype(fontPath, 20)

cn_results = []
for ch in chinese_chars:
    img = Image.new('1', (20, 20), 0)
    draw = ImageDraw.Draw(img)
    bbox = pilFontCN.getbbox(ch)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    ox = (20 - w) // 2 - bbox[0]
    oy = (20 - h) // 2 - bbox[1]
    draw.text((ox, oy), ch, fill=1, font=pilFontCN)
    matrix = []
    for row in range(20):
        for byteCol in range(3):
            val = 0
            for bit in range(8):
                col = byteCol * 8 + bit
                if col < 20 and img.getpixel((col, row)):
                    val |= (1 << bit)
            matrix.append(val)
    cn_results.append((ch, matrix))

with open('src/font_20.h', 'w', encoding='utf-8') as f:
    f.write('#ifndef __FONT_20_H__\n')
    f.write('#define __FONT_20_H__\n\n')
    f.write('#include <Arduino.h>\n\n')
    f.write('// 20x20汉字点阵结构体\n')
    f.write('struct CHINESE_20 {\n')
    f.write('    const char* index;\n')
    f.write('    unsigned char matrix[60];    // 20行 x 3字节/行 = 60字节\n')
    f.write('};\n\n')
    f.write('#define FONT_20_COUNT %d\n\n' % len(cn_results))
    f.write('const CHINESE_20 font_20[] PROGMEM = {\n')
    for ch, matrix in cn_results:
        f.write('    { "%s", {\n' % ch)
        line = ''
        for i, v in enumerate(matrix):
            line += '0x%02X,' % v
            if (i + 1) % 16 == 0:
                f.write(line + '\n')
                line = ''
        if line:
            f.write(line + '\n')
        f.write('    } },\n')
    f.write('};\n\n')
    f.write('#endif\n')
print('font_20.h: %d 个汉字' % len(cn_results))

# ====== ASCII 20x14 ======
ascii_chars = '0123456789:'
pilFontASC = ImageFont.truetype(fontPath, 14)

asc_results = []
for ch in ascii_chars:
    img = Image.new('1', (14, 20), 0)
    draw = ImageDraw.Draw(img)
    bbox = pilFontASC.getbbox(ch)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    ox = (14 - w) // 2 - bbox[0]
    oy = (20 - h) // 2 - bbox[1]
    draw.text((ox, oy), ch, fill=1, font=pilFontASC)
    matrix = []
    for row in range(20):
        for byteCol in range(2):
            val = 0
            for bit in range(8):
                col = byteCol * 8 + bit
                if col < 14 and img.getpixel((col, row)):
                    val |= (1 << bit)
            matrix.append(val)
    asc_results.append((ch, matrix))

with open('src/ascii_font_20.h', 'w', encoding='utf-8') as f:
    f.write('#ifndef __ASCII_FONT_20_H__\n')
    f.write('#define __ASCII_FONT_20_H__\n\n')
    f.write('// 20x14 ASCII字符点阵\n')
    f.write('struct ASCII_20 {\n')
    f.write('    char ch;\n')
    f.write('    unsigned char matrix[40];    // 20行 x 2字节/行 = 40字节\n')
    f.write('};\n\n')
    f.write('#define ASCII_20_COUNT %d\n\n' % len(asc_results))
    f.write('const ASCII_20 font_ascii_20[] PROGMEM = {\n')
    for ch, matrix in asc_results:
        f.write("    { '%s', {\n" % ch)
        line = ''
        for i, v in enumerate(matrix):
            line += '0x%02X,' % v
            if (i + 1) % 16 == 0:
                f.write(line + '\n')
                line = ''
        if line:
            f.write(line + '\n')
        f.write('    } },\n')
    f.write('};\n\n')
    f.write('#endif\n')
print('ascii_font_20.h: %d 个ASCII字符' % len(asc_results))
