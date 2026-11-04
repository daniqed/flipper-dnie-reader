# Genera JPEGs sinteticos y su referencia a 1/8 con libjpeg (Pillow draft = escala DCT 1/8)
import sys
from PIL import Image, ImageDraw, ImageFilter
import random
random.seed(1)
def face(w, h):
    im = Image.new('RGB', (w, h), (200, 210, 220))
    d = ImageDraw.Draw(im)
    d.ellipse((w*0.2, h*0.15, w*0.8, h*0.8), fill=(225, 180, 150))
    d.ellipse((w*0.33, h*0.38, w*0.43, h*0.45), fill=(40, 30, 30))
    d.ellipse((w*0.57, h*0.38, w*0.67, h*0.45), fill=(40, 30, 30))
    d.rectangle((w*0.4, h*0.62, w*0.6, h*0.66), fill=(150, 60, 60))
    for _ in range(300):
        x, y = random.randrange(w), random.randrange(h)
        d.point((x, y), fill=(random.randrange(256),)*3)
    return im.filter(ImageFilter.GaussianBlur(1))
cases = [('j420', 413, 531, dict(subsampling=2)), ('j444', 300, 400, dict(subsampling=0)),
         ('j422rst', 257, 333, dict(subsampling=1, restart_marker_blocks=3)), ('jgray', 240, 320, {})]
for name, w, h, kw in cases:
    im = face(w, h)
    if name == 'jgray': im = im.convert('L')
    im.save(name + '.jpg', quality=85, optimize=(name == 'j444'), **kw)
    ref = Image.open(name + '.jpg'); ref.draft('L', (w//8, h//8)); ref = ref.convert('L')
    open(name + '.gray', 'wb').write(bytes([ref.width & 255, ref.width >> 8, ref.height & 255, ref.height >> 8]) + ref.tobytes())
    print(name, ref.size)
im = face(200, 200); im.save('jprog.jpg', progressive=True)
