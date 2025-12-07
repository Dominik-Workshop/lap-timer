from PIL import Image

def generate_c_array(image_path, array_name):
    # Open image and convert to 1-bit monochrome (dithered)
    img = Image.open(image_path).convert('1')
    
    # Resize to target dimensions
    width, height = 128, 64
    img = img.resize((width, height))
    
    # Generate C code
    print(f"const uint8_t {array_name}[] = {{")
    
    # Process image row by row (Horizontal Raster Scan)
    for y in range(height):
        row_hex = []
        for x in range(0, width, 8):
            byte_val = 0
            for bit in range(8):
                if x + bit < width:
                    # Get pixel (0=black, 1=white in PIL usually, but OLEDs usually want 1=lit)
                    # Adjust logic below depending on if your display is '0=ON' or '1=ON'
                    # Assuming 1 = Pixel ON (White on Black)
                    pixel = 1 if img.getpixel((x + bit, y)) > 0 else 0
                    if pixel:
                        byte_val |= (1 << (7 - bit)) # MSB first
            row_hex.append(f"0x{byte_val:02X}")
        
        print(f"    {', '.join(row_hex)},")
        
    print("};")

# Usage
# Make sure you have 'Pillow' installed: pip install Pillow
# Place a file named 'logo.png' in this directory
try:
    generate_c_array("logo.png", "lap_timer_logo_128_64")
except FileNotFoundError:
    print("Error: Please save your graffiti image as 'logo.png' first.")
