# -*- coding: utf-8 -*-
"""
正压防爆控制系统 说明书界面图生成脚本
按 main.cpp 中的真实布局(480x320横屏)等比复刻各界面, 2倍分辨率输出
"""
import os
from PIL import Image, ImageDraw, ImageFont

SC = 2
W, H = 480 * SC, 320 * SC
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "images")
os.makedirs(OUT, exist_ok=True)
LOGO = r"E:\gd32f103rct6-618-3.5\logo.png"

# ===== 颜色 (与 TFT_eSPI 定义一致) =====
WHITE  = (255, 255, 255)
BLACK  = (0, 0, 0)
BLUE   = (0, 0, 255)
RED    = (255, 0, 0)
GREEN  = (0, 180, 0)      # 略降亮度便于纸面阅读
YELLOW = (255, 215, 0)
CYAN   = (0, 200, 200)
DGREY  = (126, 126, 126)
SGREY  = (57, 60, 66)     # COLOR_SPACEGREY 0x39E8

FONT = r"C:\Windows\Fonts\msyh.ttc"
FONTB = r"C:\Windows\Fonts\msyhbd.ttc"
f24  = ImageFont.truetype(FONT, 21 * SC)   # 对应24px点阵字
f24b = ImageFont.truetype(FONTB, 21 * SC)
f32  = ImageFont.truetype(FONTB, 29 * SC)  # 对应32px标题字

def adv(s):
    """按固件字宽模型估算文本宽度: 中文26px, ASCII 12px"""
    w = 0
    for ch in s:
        w += 26 if ord(ch) > 0x2E7F else 12
    return w * SC

def text(d, s, x, y, color, font=None):
    """逐字符绘制, 模拟固件点阵字间距"""
    font = font or f24
    cx = x * SC
    for ch in s:
        d.text((cx, y * SC), ch, font=font, fill=color, anchor="la")
        cx += (26 if ord(ch) > 0x2E7F else 12) * SC

def text_c(d, s, cx, y, color, font=None):
    w = adv(s)
    text(d, s, (cx * SC - w / 2) / SC, y, color, font)

def rect(d, x, y, w, h, color):
    d.rectangle([x * SC, y * SC, (x + w) * SC, (y + h) * SC], fill=color)

def rect_o(d, x, y, w, h, color, width=2):
    d.rectangle([x * SC, y * SC, (x + w) * SC, (y + h) * SC], outline=color, width=width)

BTN_GAP, BTN_W, BTN_H, BTN_Y = 10, 146, 32, 286

def btn(d, idx, label, bg, fg):
    x = BTN_GAP + idx * (BTN_W + BTN_GAP)
    rect(d, x, BTN_Y, BTN_W, BTN_H, bg)
    text_c(d, label, x + BTN_W / 2, BTN_Y + 5, fg)

def btn_x(d, x, w, label, bg, fg):
    rect(d, x, BTN_Y, w, BTN_H, bg)
    text_c(d, label, x + w / 2, BTN_Y + 5, fg)

def top_bar(d, inlet=False, exhaust=False, power=False, alarm=False, mute=False):
    sw, sh, gap = 62, 28, 2
    x = 2
    labels = []
    for i, on in enumerate([inlet, exhaust, power]):
        rect(d, x, 2, sw, sh, WHITE)
        if on:
            c = GREEN if i == 2 else SGREY
            rect(d, x, 2, sw, sh, c)
            text(d, ["进气", "排气", "送电"][i], x + 10, 5, WHITE)
        x += sw + gap
    rect(d, x, 2, sw, sh, WHITE)
    if mute:
        rect(d, x, 2, sw, sh, YELLOW)
        text(d, "消音", x + 10, 5, BLACK)
    elif alarm:
        rect(d, x, 2, sw, sh, RED)
        text(d, "警报", x + 10, 5, WHITE)

def digits(d, s, x, y, color, sel=-1, selcolor=YELLOW, advpx=12):
    cx = x
    for i, ch in enumerate(s):
        c = selcolor if i == sel else color
        d.text((cx * SC, y * SC), ch, font=f24, fill=c, anchor="la")
        cx += advpx
    return cx

def cursor(d, x, y):
    """数字下方的黄色三角光标"""
    d.polygon([(x * SC, (y) * SC), ((x + 6) * SC, y * SC), ((x + 3) * SC, (y - 4) * SC)], fill=YELLOW)

def save(img, name):
    p = os.path.join(OUT, name)
    img.save(p)
    print("OK", p)

def new(bg=WHITE):
    img = Image.new("RGB", (W, H), bg)
    return img, ImageDraw.Draw(img)

# ============ 01 开机界面 ============
img, d = new(WHITE)
logo = Image.open(LOGO).convert("RGBA").resize((180 * SC, 147 * SC))
img.paste(logo, (150 * SC, 8 * SC), logo)
text(d, "谷子防爆电气有限公司", 60, 168, BLUE, f32)
rect_o(d, 90, 230, 300, 14, DGREY)
rect(d, 92, 232, int(296 * 0.66), 10, BLUE)
text(d, "版本号", 150, 250, DGREY)
text(d, "V1.0.1", 230, 250, DGREY)
save(img, "01_开机界面.png")

# ============ 02 系统主界面 ============
img, d = new(WHITE)
rect(d, 0, 268, 480, 3, BLUE)
text(d, "欢迎使用正压防爆系统", 60, 80, BLUE, f32)
text(d, "服务电话:13023456789", 100, 130, BLACK)
text(d, "谷子防爆电气有限公司", 60, 180, BLUE, f32)
btn(d, 0, "正压启动", DGREY, BLACK)
btn(d, 1, "系统设置", DGREY, BLACK)
btn(d, 2, "系统调试", DGREY, BLACK)
save(img, "02_系统主界面.png")

# ============ 03 主界面-报警状态 ============
img, d = new(WHITE)
rect(d, 0, 268, 480, 3, BLUE)
text(d, "欢迎使用正压防爆系统", 60, 80, BLUE, f32)
text(d, "服务电话:13023456789", 100, 130, BLACK)
text(d, "谷子防爆电气有限公司", 60, 180, BLUE, f32)
top_bar(d, inlet=True, alarm=True)
btn(d, 0, "取消报警", RED, WHITE)
btn(d, 1, "系统设置", DGREY, BLACK)
btn(d, 2, "系统调试", DGREY, BLACK)
save(img, "03_主界面_报警状态.png")

# ============ 04 正压启动-换气倒计时 ============
img, d = new(WHITE)
top_bar(d, inlet=True, exhaust=True)
text(d, "换气倒计时:", 20, 70, BLACK); text(d, "0900s", 200, 72, BLACK)
text(d, "柜内压力:", 20, 110, BLACK); text(d, "0150Pa", 200, 112, BLACK)
rect(d, 20, 150, 440, 38, GREEN)
text_c(d, "柜内压力正常", 240, 155, WHITE)
text(d, "换气开启压力:", 20, 190, BLACK); text(d, "0150", 200, 190, BLACK); text(d, "Pa", 250, 190, BLACK)
text(d, "总换气时间:", 20, 230, BLACK); text(d, "0900", 200, 230, BLACK); text(d, "s", 250, 230, BLACK)
btn_x(d, BTN_GAP, BTN_W, "取消警报", DGREY, BLACK)
btn_x(d, BTN_GAP + 2 * (BTN_W + BTN_GAP), BTN_W, "返回主页", DGREY, WHITE)
save(img, "04_正压启动_换气倒计时.png")

# ============ 05 正压启动-正常运行 ============
img, d = new(WHITE)
top_bar(d, power=True)
text(d, "柜内温度:", 20, 70, BLACK); text(d, "26℃", 200, 72, BLACK)
text(d, "柜内压力:", 20, 110, BLACK); text(d, "0250Pa", 200, 112, BLACK)
rect(d, 20, 150, 440, 38, GREEN)
text_c(d, "系统运行中", 240, 155, WHITE)
text(d, "压力正常值:", 20, 190, BLACK); text(d, "0150 ~ 0300Pa", 200, 190, BLACK)
btn_x(d, BTN_GAP, BTN_W, "取消警报", DGREY, BLACK)
btn_x(d, BTN_GAP + 2 * (BTN_W + BTN_GAP), BTN_W, "返回主页", DGREY, WHITE)
save(img, "05_正压启动_正常运行.png")

# ============ 06 正压启动-压力报警 ============
img, d = new(WHITE)
top_bar(d, inlet=True, alarm=True)
text(d, "柜内温度:", 20, 70, BLACK); text(d, "26℃", 200, 72, BLACK)
text(d, "柜内压力:", 20, 110, BLACK); text(d, "0060Pa", 200, 112, BLACK)
rect(d, 20, 150, 440, 38, RED)
text_c(d, "柜内压力低", 240, 155, WHITE)
text(d, "压力正常值:", 20, 190, BLACK); text(d, "0150 ~ 0300Pa", 200, 190, BLACK)
btn_x(d, BTN_GAP, BTN_W, "取消警报", RED, BLACK)
btn_x(d, BTN_GAP + 2 * (BTN_W + BTN_GAP), BTN_W, "返回主页", DGREY, WHITE)
save(img, "06_正压启动_压力报警.png")

# ============ 07 密码输入界面 ============
img, d = new(BLACK)
text(d, "请输入密码进入设置", 5, 5, YELLOW)
text(d, "输入次数：3", 30, 60, CYAN)
text(d, "输入密码:", 30, 100, WHITE)
digits(d, "000", 150, 100, WHITE, sel=0)
cursor(d, 150 + 4, 128)
btn(d, 0, "值加1", DGREY, WHITE)
btn(d, 1, "下一位", DGREY, WHITE)
btn(d, 2, "确认", DGREY, WHITE)
save(img, "07_密码输入界面.png")

# ============ 08 系统设置菜单 ============
img, d = new(WHITE)
text(d, "系统设置", 5, 5, BLACK)
items = ["修改密码", "校准传感器", "设置参数", "恢复出厂"]
ys = [60, 100, 140, 180]
for i, (it, y) in enumerate(zip(items, ys)):
    if i == 0:
        rect(d, 20, y - 5, 160, 30, SGREY)
        d.polygon([(8 * SC, (y + 5) * SC), (8 * SC, (y + 15) * SC), (16 * SC, (y + 10) * SC)], fill=DGREY)
        text(d, it, 30, y, WHITE)
    else:
        text(d, it, 30, y, BLACK)
btn(d, 0, "返回主页", DGREY, BLACK)
btn(d, 1, "下一选项", DGREY, BLACK)
btn(d, 2, "确认", DGREY, BLACK)
save(img, "08_系统设置菜单.png")

def edit_btns(d):
    """两行三列编辑按钮: 值加 / 下一位+放弃 / 下一项+保存"""
    r1, r2 = BTN_Y - BTN_H - BTN_GAP, BTN_Y
    bw = (480 - 2 * BTN_GAP) // 3 - BTN_GAP
    c0, c1, c2 = BTN_GAP, BTN_GAP + (480 - 2 * BTN_GAP) // 3, BTN_GAP + 2 * (480 - 2 * BTN_GAP) // 3
    def b(x, y, label):
        rect(d, x, y, bw, BTN_H, DGREY)
        text_c(d, label, x + bw / 2, y + 6, WHITE)
    b(c0, r2, "值加")
    b(c1, r1, "下一位"); b(c1, r2, "放弃")
    b(c2, r1, "下一项"); b(c2, r2, "保存")

# ============ 09 修改密码 ============
img, d = new(BLACK)
text(d, "修改密码", 5, 5, YELLOW)
text(d, "当前密码:555", 30, 60, CYAN)
text(d, "输入密码:", 30, 100, WHITE)
digits(d, "000", 150, 100, WHITE, sel=0)
cursor(d, 150 + 4, 128)
edit_btns(d)
save(img, "09_修改密码.png")

# ============ 10 校准传感器 ============
img, d = new(BLACK)
text(d, "校准传感器", 5, 5, YELLOW)
text(d, "校准温度", 30, 80, WHITE)
digits(d, "0026", 200, 80, CYAN, sel=0, advpx=14)
cursor(d, 200 + 4, 108)
text(d, "℃", 262, 80, WHITE)
text(d, "校准压力", 30, 140, WHITE)
digits(d, "0000", 200, 140, CYAN, advpx=14)
text(d, "Pa", 262, 140, WHITE)
edit_btns(d)
save(img, "10_校准传感器.png")

# ============ 11 设置参数 ============
img, d = new(BLACK)
text(d, "设置参数", 5, 5, YELLOW)
labels = ["压力下限", "压力下下限", "压力上限", "压力上上限", "温度上限", "换气时间"]
vals = ["0150", "0080", "0300", "0400", "0080", "0900"]
for i, (lb, v) in enumerate(zip(labels, vals)):
    y = 32 + i * 34
    if i == 0:
        rect(d, 8, y - 2, 270, 32, SGREY)
        d.polygon([(10 * SC, (y + 5) * SC), (10 * SC, (y + 15) * SC), (18 * SC, (y + 10) * SC)], fill=DGREY)
        text(d, lb, 24, y, WHITE)
        digits(d, v, 200, y, CYAN, sel=0, advpx=14)
        cursor(d, 200 + 4, y + 28)
    else:
        text(d, lb, 24, y, WHITE)
        digits(d, v, 200, y, CYAN, advpx=14)
edit_btns(d)
save(img, "11_设置参数.png")

# ============ 12 系统调试-安全警示 ============
img, d = new(WHITE)
text(d, "系统调试", 5, 5, BLACK)
text(d, "危险区域严禁在现场使用", 60, 80, RED, f32)
text(d, "可能导致爆炸请确认安全", 60, 120, RED, f32)
btn(d, 0, "返回主页", DGREY, BLACK)
btn(d, 2, "确认调试", DGREY, BLACK)
save(img, "12_系统调试_安全警示.png")

# ============ 13 调试-送电确认 ============
img, d = new(WHITE)
rect(d, 480 - 95, 3, 70, 30, RED)
text(d, "送电", 480 - 85, 7, WHITE)
text(d, "系统已送电 请确认安全", 60, 80, RED, f32)
text(d, "压力:", 30, 200, BLACK); text(d, "0250", 112, 200, BLACK); text(d, "Pa", 170, 200, BLACK)
text(d, "温度:", 230, 200, BLACK); text(d, "26", 310, 200, BLACK); text(d, "℃", 350, 200, BLACK)
btn(d, 2, "调试结束", DGREY, WHITE)
save(img, "13_调试_送电确认.png")

# ============ 14 面板布局示意图 ============
img = Image.new("RGB", (W, H), (245, 247, 250))
d = ImageDraw.Draw(img)
d.rounded_rectangle([20 * SC, 30 * SC, 460 * SC, 290 * SC], radius=14 * SC, outline=(70, 80, 90), width=3 * SC, fill=(232, 236, 240))
text(d, "正压防爆控制系统 面板示意图", 120, 2, (50, 60, 70), f24b)
# 屏幕
d.rectangle([45 * SC, 70 * SC, 285 * SC, 230 * SC], fill=(20, 28, 40), outline=(70, 80, 90), width=2 * SC)
text_c(d, "3.5寸彩色显示屏", 165, 132, WHITE, f24b)
text_c(d, "480 × 320", 165, 162, (170, 180, 195))
# 按键
keys = [("按键1", "数值加 / 消音", 90), ("按键2", "下一位 / 下一选项", 150), ("按键3", "确认 / 保存", 210)]
for name, desc, y in keys:
    cx = 350
    d.ellipse([(cx - 16) * SC, (y - 16) * SC, (cx + 16) * SC, (y + 16) * SC], fill=(90, 100, 110), outline=(50, 60, 70), width=2 * SC)
    text_c(d, name, cx, y - 10, WHITE, f24b)
    text(d, desc, cx + 24, y - 10, (40, 50, 60))
# 蜂鸣器
d.ellipse([(320) * SC, (245) * SC, (344) * SC, (269) * SC], fill=(200, 205, 210), outline=(70, 80, 90), width=2 * SC)
text(d, "蜂鸣器", 352, 248, (40, 50, 60))
save(img, "14_面板布局示意图.png")

print("全部界面图生成完毕:", len(os.listdir(OUT)), "个文件")
