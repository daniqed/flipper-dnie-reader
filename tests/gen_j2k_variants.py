import io, sys, random
from PIL import Image, ImageDraw, ImageFilter
random.seed(7)
def scene(w, h, mode):
    img = Image.new("RGB", (w, h)); d = ImageDraw.Draw(img)
    for _ in range(60):
        x0, y0 = random.randrange(w), random.randrange(h)
        d.ellipse([x0, y0, x0 + random.randrange(5, w // 2), y0 + random.randrange(5, h // 2)],
                  fill=tuple(random.randrange(256) for _ in range(3)))
    for _ in range(3000):
        d.point((random.randrange(w), random.randrange(h)), fill=tuple(random.randrange(256) for _ in range(3)))
    img = img.filter(ImageFilter.GaussianBlur(1.2))
    return img.convert(mode)
variants = [
    ("comp_alta", 307, 378, "RGB", dict(irreversible=True, num_resolutions=6, codeblock_size=(64, 64), progression="LRCP", mct=1, quality_mode="rates", quality_layers=[60])),
    ("comp_baja", 307, 378, "RGB", dict(irreversible=True, num_resolutions=6, codeblock_size=(64, 64), progression="LRCP", mct=1, quality_mode="rates", quality_layers=[4])),
    ("gris_rpcl", 300, 400, "L",   dict(irreversible=True, num_resolutions=6, codeblock_size=(64, 64), progression="RPCL")),
    ("rlcp_cb32", 252, 334, "RGB", dict(irreversible=True, num_resolutions=5, codeblock_size=(32, 32), progression="RLCP", mct=1)),
    ("grande_cb32", 480, 640, "RGB", dict(irreversible=True, num_resolutions=6, codeblock_size=(32, 32), progression="LRCP", mct=1, quality_mode="rates", quality_layers=[15])),
]
for name, w, h, mode, kw in variants:
    img = scene(w, h, mode); b = io.BytesIO(); img.save(b, format="JPEG2000", **kw); data = b.getvalue()
    open(f"var_{name}.jp2", "wb").write(data)
    nres = kw["num_resolutions"]; nl = nres - 1
    # misma regla que el decodificador: resolucion mas pequena con altura >= 64
    red = 0
    for r in range(nl + 1):
        if -(-h // (1 << (nl - r))) >= 64: red = nl - r; break
    ref = Image.open(io.BytesIO(data)); ref.reduce = red; ref.load()
    if mode == "RGB":
        y = [round(0.299 * r + 0.587 * g + 0.114 * bb) for r, g, bb in ref.get_flattened_data()]
    else:
        y = list(ref.get_flattened_data())
    open(f"var_{name}.gray", "wb").write(bytes(max(0, min(255, v)) for v in y))
    print(f"{name:12s} {len(data):6d} B  reduce={red} -> {ref.size}")
