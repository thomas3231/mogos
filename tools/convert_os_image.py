from pathlib import Path

try:
    from PIL import Image
except ImportError as error:
    raise SystemExit("Install Pillow to regenerate the embedded OS image.") from error


ROOT = Path(__file__).resolve().parents[1]
IMAGE_WIDTH = 160
IMAGE_HEIGHT = 169

source = ROOT / "img" / "os.png"
destination = ROOT / "src" / "kernel" / "os_image.rgb565"

image = Image.open(source).convert("RGB")
image = image.resize((IMAGE_WIDTH, IMAGE_HEIGHT), Image.Resampling.LANCZOS)

pixels = bytearray()
rgb = image.tobytes()
for offset in range(0, len(rgb), 3):
    red, green, blue = rgb[offset : offset + 3]
    rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
    pixels.extend(rgb565.to_bytes(2, "little"))

destination.write_bytes(pixels)
