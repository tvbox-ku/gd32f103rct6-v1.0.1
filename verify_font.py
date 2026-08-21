#!/usr/bin/env python3
"""Verify generated font data by rendering characters to images."""

from PIL import Image, ImageDraw, ImageFont
import re
import os

def parse_font_data(filepath):
    """Parse the generated my_font.h and extract bitmap data."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find all character entries
    pattern = r'\{ "(.)", \{(.*?)\} \}'
    matches = re.findall(pattern, content, re.DOTALL)
    
    chars = {}
    for ch, data_str in matches:
        # Parse hex bytes
        bytes_list = [int(x.strip(), 16) for x in data_str.split(',') if x.strip()]
        chars[ch] = bytes_list
    
    return chars

def render_char(bitmap_data, size=24):
    """Render bitmap data to an image."""
    bytes_per_row = (size + 7) // 8
    img = Image.new('1', (size, size), 0)  # Black background
    pixels = img.load()
    
    for row in range(size):
        for col in range(size):
            byte_idx = row * bytes_per_row + col // 8
            if byte_idx < len(bitmap_data):
                if bitmap_data[byte_idx] & (0x01 << (col % 8)):
                    pixels[col, row] = 1  # White pixel
    
    return img

def main():
    chars = parse_font_data('src/my_font.h')
    print(f"Found {len(chars)} characters in font data")
    
    # Render a few test characters
    test_chars = ['中', '国', '正', '压', '防', '爆']
    
    # Create a comparison image: original SimSun vs generated bitmap
    font = ImageFont.truetype("C:/Windows/Fonts/simsun.ttc", 24)
    
    for ch in test_chars:
        if ch in chars:
            # Generated bitmap
            gen_img = render_char(chars[ch], 24)
            gen_img_scaled = gen_img.resize((96, 96), Image.NEAREST)
            gen_img_scaled.save(f'verify_{ch}_generated.png')
            
            # Original SimSun rendering
            orig_img = Image.new('1', (24, 24), 0)
            draw = ImageDraw.Draw(orig_img)
            bbox = draw.textbbox((0, 0), ch, font=font)
            x = (24 - (bbox[2] - bbox[0])) // 2 - bbox[0]
            y = (24 - (bbox[3] - bbox[1])) // 2 - bbox[1]
            draw.text((x, y), ch, fill=1, font=font)
            orig_img_scaled = orig_img.resize((96, 96), Image.NEAREST)
            orig_img_scaled.save(f'verify_{ch}_original.png')
            
            print(f"Character '{ch}': bitmap data has {sum(1 for b in chars[ch] for bit in range(8) if b & (1 << bit))} pixels set")
        else:
            print(f"Character '{ch}' NOT FOUND in font data!")
    
    # Also create a side-by-side comparison
    if test_chars:
        found = [c for c in test_chars if c in chars]
        if found:
            width = 96 * len(found) * 2 + 10 * (len(found) - 1)
            comparison = Image.new('1', (width, 96), 0)
            
            for i, ch in enumerate(found):
                # Original
                orig = Image.open(f'verify_{ch}_original.png')
                comparison.paste(orig, (i * 192, 0))
                # Generated
                gen = Image.open(f'verify_{ch}_generated.png')
                comparison.paste(gen, (i * 192 + 96, 0))
            
            comparison.save('verify_comparison.png')
            print(f"\nComparison image saved: verify_comparison.png")

if __name__ == '__main__':
    main()
