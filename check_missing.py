import re

with open('src/main.cpp', encoding='utf-8') as f:
    code = f.read()

code_no_comments = re.sub(r'//.*', '', code)
code_no_comments = re.sub(r'/\*.*?\*/', '', code_no_comments, flags=re.DOTALL)

all_cn = set()
for m in re.finditer(r'"([^"]*[\u4e00-\u9fff][^"]*)"', code_no_comments):
    for ch in m.group(1):
        if '\u4e00' <= ch <= '\u9fff':
            all_cn.add(ch)

with open('src/my_font.h', encoding='utf-8') as f:
    font24 = set(re.findall(r'\{\s*"(.)"', f.read()))

with open('src/title_font_32.h', encoding='utf-8') as f:
    font32 = set(re.findall(r'\{\s*"(.)"', f.read()))

need_24 = all_cn - font32
missing24 = need_24 - font24
print(f'需要font_24: {len(need_24)}, 缺失: {len(missing24)}')
if missing24:
    print(f'缺失字: {"".join(sorted(missing24))}')
