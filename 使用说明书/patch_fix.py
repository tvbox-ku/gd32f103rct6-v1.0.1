# -*- coding: utf-8 -*-
import re
p = r"E:\gd32f103rct6-618-3.5\使用说明书\gen_manual.py"
s = open(p, encoding="utf-8").read()
s = re.sub(r"fig\('([^']+)', '图\d+  ([^']*)'\)", r"fig('\1', '\2')", s)
anchor = "para('系统启动运行后，主界面顶部显示「进气 / 排气 / 送电 / 警报」状态指示条，可实时查看各路输出状态。', size=10)"
extra = (anchor + "\n"
         "fig('03_主界面_报警状态.png', '主界面（报警状态）')\n"
         "para('当系统处于报警状态时，主界面顶部状态条红色闪烁显示「警报」，底部第一个菜单变为红色「取消报警」；短按按键1可消音，顶部转为黄色「消音」。', size=10)")
assert anchor in s
s = s.replace(anchor, extra)
open(p, "w", encoding="utf-8").write(s)
print("patched")
