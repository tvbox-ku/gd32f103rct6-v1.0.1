#!/usr/bin/env python3
"""Generate 20x20 and 18x18 Chinese font bitmaps.
Strategy: grayscale at 30×, LANCZOS downsample, 40% threshold, 1px dilation.
Font: SimSun (宋体)
"""
from PIL import Image, ImageDraw, ImageFont
import os, re

OUTPUT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "src", "my_font.h"))
FONT_PATH = r"C:\Windows\Fonts\simsun.ttc"
S = 30

chars_20 = [
    "欢", "迎", "使", "用",
    "谷", "子", "防", "爆",
    "正", "压", "控", "制",
    "系", "统", "设", "置",
    "调", "试", "修", "改",
    "密", "码", "校", "准",
    "传", "感", "器", "参",
    "数", "恢", "复", "出",
    "厂", "返", "回", "下",
    "一", "选", "项", "确",
    "认", "服", "务", "电",
    "话", "柜", "气", "启",
    "动", "主", "页", "危",
    "险", "区", "域", "严",
    "禁", "在", "现", "场",
    "导", "致", "爆", "炸",
    "请", "安", "全", "换",
    "倒", "计", "时", "力",
    "常", "异", "界", "于",
    "间",
]
chars_18 = chars_20 + ["低", "于", "上", "限", "高", "报", "警", "声", "音", "取", "消", "状", "态"]

def render(ch, font, target_w, target_h, thresh=0.40):
    """Grayscale at S×, LANCZOS downsample, threshold, dilate 1px."""
    big_w = target_w * S
    big_h = target_h * S
    img = Image.new("L", (big_w, big_h), 255)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    cw = bbox[2] - bbox[0]
    ch_h = bbox[3] - bbox[1]
    ox = (big_w - cw) // 2 - bbox[0]
    oy = (big_h - ch_h) // 2 - bbox[1]
    draw.text((ox, oy), ch, font=font, fill=0)

    small = img.resize((target_w, target_h), Image.LANCZOS)
    pixels = list(small.getdata())

    # Threshold: pixel < thresh*255 → black
    bits = [1 if p < (thresh * 255) else 0 for p in pixels]

    # 1-pixel dilation to reconnect thin strokes
    b2d = [bits[r * target_w:(r+1) * target_w] for r in range(target_h)]
    dil = [[0]*target_w for _ in range(target_h)]
    for r in range(target_h):
        for c in range(target_w):
            if b2d[r][c]:
                for dr in (-1,0,1):
                    for dc in (-1,0,1):
                        nr, nc = r+dr, c+dc
                        if 0 <= nr < target_h and 0 <= nc < target_w:
                            dil[nr][nc] = 1

    bpr = (target_w + 7) // 8
    res = []
    for r in range(target_h):
        for bc in range(bpr):
            v = 0
            for bit in range(8):
                c = bc*8 + bit
                if c < target_w and dil[r][c]:
                    v |= (1 << bit)
            res.append(v)
    return res

def arr_block(chars, name, sz, wd, font, thresh):
    lines = [f"const {name} font_{sz}[] PROGMEM = {{"]
    for ch in chars:
        data = render(ch, font, wd, sz, thresh)
        h = ["0x%02X" % b for b in data]
        lines.append('    { "' + ch + '", {')
        for i in range(0, len(h), 8):
            lines.append("        " + ", ".join(h[i:i+8]) + ",")
        lines.append("    } },")
    lines.append("};")
    return "\n".join(lines)

print("Loading SimSun font...")
f30 = ImageFont.truetype(FONT_PATH, int(20 * S * 1.1))
f18 = ImageFont.truetype(FONT_PATH, int(18 * S * 1.1))

print("Generating font_20...")
b20 = arr_block(chars_20, "CHINESE_20", 20, 20, f30, 0.40)
print("Generating font_18...")
b18 = arr_block(chars_18, "CHINESE_18", 18, 18, f18, 0.40)

H = """\
#ifndef __MY_FONT_H__
#define __MY_FONT_H__
#include <Arduino.h>
#include <TFT_eSPI.h>
extern TFT_eSPI tft;

// ====== 16x16 (向下兼容) ======
struct CHINESE_16 { const char* index; unsigned char matrix[32]; };
const CHINESE_16 font_16[] PROGMEM = {
    { "欢", { 0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x08,0x00,0x00,0x08,0x00,0x80,0x09,0x00,0xFC, 0x04,0x01,0x80,0xFC,0x01,0x80,0x84,0x00,0x50,0x52,0x00,0x50,0x12,0x00,0x60,0x09 } },
    { "迎", { 0x00,0x00,0x00,0x08,0x08,0x00,0x10,0x0C,0x00,0x20,0x13,0x03,0x20,0xF1,0x01,0x00, 0x11,0x01,0x00,0x11,0x01,0x70,0x11,0x01,0x2C,0x11,0x01,0x20,0x1D,0x01,0x10,0x13 } },
    { "使", { 0x00,0x00,0x00,0x00,0x08,0x00,0x40,0x08,0x00,0x40,0x08,0x00,0x20,0x88,0x01,0x90, 0x7F,0x00,0x10,0x08,0x00,0x18,0xF8,0x00,0x14,0x4F,0x00,0x14,0x49,0x00,0x10,0x79 } },
    { "用", { 0x00,0x00,0x00,0x00,0x80,0x00,0x20,0xFC,0x00,0xE0,0x83,0x00,0x20,0x82,0x00,0x20, 0x82,0x00,0x20,0xBE,0x00,0xE0,0x83,0x00,0x20,0x82,0x00,0x10,0x82,0x00,0x90,0x9F } },
};
#define FONT_16_COUNT (sizeof(font_16)/sizeof(CHINESE_16))
#define FONT_COUNT FONT_16_COUNT

// ====== 20x20 大标题 (宋体) ======
struct CHINESE_20 { const char* index; unsigned char matrix[60]; };
"""

F20 = """
#define FONT_20_COUNT (sizeof(font_20)/sizeof(CHINESE_20))

// ====== 18x18 正文 (宋体) ======
struct CHINESE_18 { const char* index; unsigned char matrix[54]; };
"""

F18 = """
#define FONT_18_COUNT (sizeof(font_18)/sizeof(CHINESE_18))

// ========== 绘制函数 ==========
#ifdef __cplusplus
extern "C" {
#endif
void drawChineseChar(const char*, int, int, uint16_t, float, bool);
void drawMixedString(const char*, int, int, uint16_t, float, bool);
#ifdef __cplusplus
}
#endif

static inline void drawChineseChar20(const char* ch, int x, int y, uint16_t color) {
    for (int i = 0; i < FONT_20_COUNT; i++) {
        if (strcmp(font_20[i].index, ch) == 0) {
            for (int r = 0; r < 20; r++)
                for (int c = 0; c < 20; c++)
                    if (font_20[i].matrix[r*3+c/8] & (0x01<<(c%8)))
                        tft.drawPixel(x+c, y+r, color);
            return;
        }
    }
}

static inline void drawTitleString(const char* str, int x, int y, uint16_t color) {
    int cx = x;
    for (int i = 0; str[i]; ) {
        unsigned char c = (unsigned char)str[i];
        if (c >= 0xE0 && str[i+1] && str[i+2]) {
            char ch[4] = { str[i], str[i+1], str[i+2], 0 };
            drawChineseChar20(ch, cx, y, color); cx += 22; i += 3;
        } else if (c < 0x80) {
            tft.setTextColor(color); tft.setTextSize(1);
            tft.setCursor(cx, y+3); tft.print(str[i]); cx += 10; i++;
        } else i++;
    }
}

static inline void drawChineseChar18(const char* ch, int x, int y, uint16_t color) {
    for (int i = 0; i < FONT_18_COUNT; i++) {
        if (strcmp(font_18[i].index, ch) == 0) {
            for (int r = 0; r < 18; r++)
                for (int c = 0; c < 18; c++)
                    if (font_18[i].matrix[r*3+c/8] & (0x01<<(c%8)))
                        tft.drawPixel(x+c, y+r, color);
            return;
        }
    }
}

static inline void drawContentString(const char* str, int x, int y, uint16_t color) {
    int cx = x;
    for (int i = 0; str[i]; ) {
        unsigned char c = (unsigned char)str[i];
        if (c >= 0xE0 && str[i+1] && str[i+2]) {
            char ch[4] = { str[i], str[i+1], str[i+2], 0 };
            drawChineseChar18(ch, cx, y, color); cx += 20; i += 3;
        } else if (c < 0x80) {
            tft.setTextColor(color); tft.setTextSize(1);
            tft.setCursor(cx, y+2); tft.print(str[i]); cx += 9; i++;
        } else i++;
    }
}
#endif
"""

final = H + b20 + F20 + b18 + F18

with open(OUTPUT, "w", encoding="utf-8") as f:
    f.write(final)

# Verify
print("\n-- Verify --")
for name, sn, sz, wd, chs in [
    ("font20", "CHINESE_20", 20, 20, chars_20),
    ("font18", "CHINESE_18", 18, 18, chars_18),
]:
    p = re.compile(r'%s font_%d\[\] PROGMEM = \{(.*?)\};' % (sn, sz), re.DOTALL)
    m = p.search(final)
    ents = re.findall(r'\{\s*"([^"]+)",\s*\{\s*([^}]+)\s*\}\s*\}', m.group(1))
    ok = sum(1 for i in range(min(len(ents), len(chs))) if ents[i][0] == chs[i])
    nnz = []
    for i in range(len(ents)):
        hx = re.findall(r'0x([0-9A-Fa-f]+)', ents[i][1])
        bd = [int(v, 16) for v in hx]
        nnz.append(sum(1 for b in bd if b != 0))
    pitch = wd + 2
    strip = Image.new("1", (len(ents) * pitch, sz), 1)
    for i in range(len(ents)):
        hx = re.findall(r'0x([0-9A-Fa-f]+)', ents[i][1])
        bd = [int(v, 16) for v in hx]
        for r in range(sz):
            for c in range(wd):
                idx = r * ((wd + 7) // 8) + c // 8
                if idx < len(bd) and bd[idx] & (1 << (c % 8)):
                    strip.putpixel((i * pitch + c, r), 0)
    strip.resize((len(ents) * pitch * 4, sz * 4), Image.NEAREST).save(f"verify_{name}.png")
    avg = sum(nnz) / len(nnz)
    print(f"  {name}: {ok}/{len(chs)} OK, avg {avg:.1f}/{len(bd)} non-zero")

print(f"\nWritten: {OUTPUT}")
