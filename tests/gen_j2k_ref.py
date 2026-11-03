# Imagen de prueba tipo "retrato" codificada con los parametros del DNIe, y la referencia
# de OpenJPEG (Pillow) a resolucion reducida para comparar con nuestro decodificador.
import io, random, sys
from PIL import Image, ImageDraw, ImageFilter
random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
W, H = 307, 378
img = Image.new("RGB", (W, H), (200, 205, 215))
d = ImageDraw.Draw(img)
for y in range(H):  # fondo con degradado
    d.line([(0, y), (W, y)], fill=(180 + y // 8, 190 + y // 10, 210 - y // 12))
d.ellipse([60, 260, 250, 470], fill=(40, 50, 90))            # hombros
d.rectangle([125, 220, 185, 290], fill=(220, 175, 150))      # cuello
d.ellipse([80, 70, 230, 260], fill=(225, 180, 155))          # cara
d.ellipse([75, 40, 235, 150], fill=(60, 40, 30))             # pelo
d.ellipse([110, 140, 140, 160], fill=(255, 255, 255)); d.ellipse([120, 145, 132, 157], fill=(30, 30, 30))
d.ellipse([170, 140, 200, 160], fill=(255, 255, 255)); d.ellipse([180, 145, 192, 157], fill=(30, 30, 30))
d.polygon([(155, 160), (145, 200), (165, 200)], fill=(200, 150, 130))
d.arc([125, 200, 185, 235], 20, 160, fill=(150, 60, 60), width=4)
for _ in range(400):  # textura
    x, y = random.randrange(W), random.randrange(H)
    c = random.randrange(-30, 30)
    d.point((x, y), fill=tuple(max(0, min(255, v + c)) for v in img.getpixel((x, y))))
img = img.filter(ImageFilter.GaussianBlur(0.8))
buf = io.BytesIO()
img.save(buf, format="JPEG2000", irreversible=True, num_resolutions=6, codeblock_size=(64, 64),
         progression="LRCP", mct=1, tile_size=(640, 480), quality_mode="rates", quality_layers=[20])
j2k = buf.getvalue()
open("ref_photo.jp2", "wb").write(j2k)
ref = Image.open(io.BytesIO(j2k)); ref.reduce = 2; ref.load()
y = [round(0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in ref.getdata()]
open("ref_photo_r2.gray", "wb").write(bytes(max(0, min(255, v)) for v in y))
print("jp2", len(j2k), "B; referencia reducida", ref.size)
img.convert("L").save("ref_photo_full.png")
