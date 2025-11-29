#!/usr/bin/env python3
"""
Verify icon conversion by comparing PNG with generated C array
"""

import sys
from PIL import Image

def parse_c_array(h_file_path, var_name):
    """Extract pixel data from C array in .h file"""
    with open(h_file_path, 'r') as f:
        content = f.read()
    
    # Find the array definition
    array_start = content.find(f'uint8_t {var_name}_map[] = {{')
    if array_start == -1:
        print(f"Error: Could not find {var_name}_map in file")
        return None
    
    # Find the closing brace
    array_end = content.find('};', array_start)
    array_section = content[array_start:array_end]
    
    # Extract hex values
    import re
    hex_values = re.findall(r'0x([0-9a-fA-F]{2})', array_section)
    
    # Skip header (first 8 bytes: format + width + height)
    pixel_data = [int(h, 16) for h in hex_values[8:]]
    
    return pixel_data

def rgb565a8_to_rgba(data, width, height):
    """Convert RGB565A8 byte array to RGBA pixels"""
    pixels = []
    idx = 0
    for y in range(height):
        for x in range(width):
            # RGB565 (little-endian) + A8
            rgb565_low = data[idx]
            rgb565_high = data[idx + 1]
            alpha = data[idx + 2]
            idx += 3
            
            # Combine RGB565
            rgb565 = (rgb565_high << 8) | rgb565_low
            
            # Extract RGB
            r5 = (rgb565 >> 11) & 0x1F
            g6 = (rgb565 >> 5) & 0x3F
            b5 = rgb565 & 0x1F
            
            # Convert to 8-bit
            r = (r5 << 3) | (r5 >> 2)
            g = (g6 << 2) | (g6 >> 4)
            b = (b5 << 3) | (b5 >> 2)
            
            pixels.append((r, g, b, alpha))
    
    return pixels

def compare_images(png_path, h_file_path, var_name):
    """Compare PNG with C array representation"""
    print(f"Comparing: {png_path}")
    print(f"Variable: {var_name}")
    print()
    
    # Load PNG
    img = Image.open(png_path).convert('RGBA')
    png_pixels = list(img.getdata())
    width, height = img.size
    
    print(f"PNG size: {width}x{height}")
    
    # Parse C array
    c_array_data = parse_c_array(h_file_path, var_name)
    if not c_array_data:
        return
    
    # Convert RGB565A8 to RGBA
    c_array_pixels = rgb565a8_to_rgba(c_array_data, width, height)
    
    print(f"C array pixels: {len(c_array_pixels)}")
    print()
    
    # Compare pixel by pixel
    differences = []
    for idx, (png_pixel, c_pixel) in enumerate(zip(png_pixels, c_array_pixels)):
        x = idx % width
        y = idx // width
        
        png_r, png_g, png_b, png_a = png_pixel
        c_r, c_g, c_b, c_a = c_pixel
        
        # Check for significant differences (accounting for RGB565 quantization)
        r_diff = abs(png_r - c_r)
        g_diff = abs(png_g - c_g)
        b_diff = abs(png_b - c_b)
        a_diff = abs(png_a - c_a)
        
        # Allow small differences due to RGB565 conversion
        if r_diff > 8 or g_diff > 4 or b_diff > 8 or a_diff > 1:
            differences.append({
                'pos': (x, y),
                'png': png_pixel,
                'c_array': c_pixel,
                'diff': (r_diff, g_diff, b_diff, a_diff)
            })
    
    if differences:
        print(f"⚠️  Found {len(differences)} significant pixel differences:")
        print()
        
        # Show first 20 differences
        for i, diff in enumerate(differences[:20]):
            x, y = diff['pos']
            png_r, png_g, png_b, png_a = diff['png']
            c_r, c_g, c_b, c_a = diff['c_array']
            r_diff, g_diff, b_diff, a_diff = diff['diff']
            
            print(f"  Pixel ({x:2d}, {y:2d}):")
            print(f"    PNG:     RGBA({png_r:3d}, {png_g:3d}, {png_b:3d}, {png_a:3d})")
            print(f"    C array: RGBA({c_r:3d}, {c_g:3d}, {c_b:3d}, {c_a:3d})")
            print(f"    Diff:    ({r_diff:3d}, {g_diff:3d}, {b_diff:3d}, {a_diff:3d})")
            print()
        
        if len(differences) > 20:
            print(f"  ... and {len(differences) - 20} more differences")
    else:
        print("✓ No significant pixel differences found")
        print("  (Small differences due to RGB565 quantization are expected)")

if __name__ == '__main__':
    png_path = '/home/jtielens/dev/esp32-template-wifi/ui-assets/icons/call_end_48dp_FFFFFF_FILL0_wght400_GRAD0_opsz48.png'
    h_file_path = '/home/jtielens/dev/esp32-template-wifi/src/app/ui/icons.h'
    var_name = 'icon_call_end_48dp_ffffff_fill0_wght400_grad0_opsz48'
    
    compare_images(png_path, h_file_path, var_name)
