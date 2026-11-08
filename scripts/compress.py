import gzip
import os
import re

def compress_web_assets():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    header_path = os.path.join(base_dir, 'include', 'WebAssets.h')
    out_path = os.path.join(base_dir, 'include', 'WebAssetsGz.h')

    if not os.path.exists(header_path):
        print(f"[Compress] {header_path} not found")
        return

    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract INDEX_HTML
    match = re.search(r'const char INDEX_HTML\[\] PROGMEM = R"rawliteral\((.*?)\)rawliteral";', content, re.DOTALL)
    if not match:
        print("[Compress] INDEX_HTML not found in WebAssets.h")
        return

    html_bytes = match.group(1).encode('utf-8')
    raw_len = len(html_bytes)
    gz_bytes = gzip.compress(html_bytes, 9)
    gz_len = len(gz_bytes)

    # Format as C array
    hex_values = [f"0x{b:02x}" for b in gz_bytes]
    lines = []
    for i in range(0, len(hex_values), 16):
        lines.append("  " + ", ".join(hex_values[i:i+16]))

    joined_lines = ',\n'.join(lines)
    header_content = f"""#pragma once
#include <Arduino.h>

// Automatically generated from include/WebAssets.h (Raw: {raw_len} bytes -> Gz: {gz_len} bytes, {(1 - gz_len/raw_len)*100:.1f}% reduction)
const size_t INDEX_HTML_GZ_LEN = {gz_len};
const uint8_t INDEX_HTML_GZ[] PROGMEM = {{
{joined_lines}
}};
"""
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(header_content)

    print(f"[Compress] Generated WebAssetsGz.h: {raw_len} -> {gz_len} bytes (saved {raw_len - gz_len} bytes, {(1 - gz_len/raw_len)*100:.1f}% reduction)")

try:
    from SCons.Script import Import
    Import("env")
    compress_web_assets()
except Exception:
    pass

if __name__ == '__main__':
    compress_web_assets()
