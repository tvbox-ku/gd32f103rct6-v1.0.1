#!/usr/bin/env python3
"""Verify 32x32 title font data by rendering to images."""

from PIL import Image
import re

def parse_font_data(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    pattern = r'\{ "(.)", \{(.*?)\} \}'
    matches = re.findall(pattern, content, re.DOTALL)
    
    chars = {}
    for ch, data_str in matches:
        bytes_list = [int(x.strip(), 16) for x in data_str.split(',') if x.strip()]
        chars[ch] = bytes_list
    
    return chars

def render_char(bitmap_data, size=32):
    bytes_per_row = (size + 7) // 8
    img = Image.new('1', (size, size), 0)
    pixels = img.load()
    
    for row in range(size):
        for col in range(size):
            byte_idx = row * bytes_per_row + col // 8
            if byte_idx < len(bitmap_data):
                if bitmap_data[byte_idx] & (0x01 << (col % 8)):
                    pixels[col, row] = 1
    
    return img

def main():
    chars = parse_font_data('src/title_font_32.h')
    print(f"Found {len(chars)} title characters")
    
    # Render all title characters side by side
    title = "欢迎使用正压防爆系统"
    width = 34 * len(title)
    height = 32
    combined = Image.new('1', (width, height), 0)
    
    for i, ch in enumerate(title):
        if ch in chars:
            char_img = render_char(chars[ch], 32)
            combined.paste(char_img, (i * 34, 0))
            print(f"'{ch}': {sum(1 for b in chars[ch] for bit in range(8) if b & (1 << bit))} pixels")
        else:
            print(f"'{ch}': NOT FOUND!")
    
    combined_scaled = combined.resize((width * 3, height * 3), Image.NEAREST)
    combined_scaled.save('verify_title_32.png')
    print(f"\nTitle preview saved: verify_title_32.png")

if __name__ == '__main__':
    main()
