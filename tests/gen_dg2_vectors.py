# Genera DG2 sinteticos (TLV + ISO 19794-5 + JPEG2000 de Pillow) para probar dnie_dg2.c
import io, struct
from PIL import Image
def tl(tag, value):
    t = tag.to_bytes(2 if tag > 0xFF else 1, "big"); n = len(value)
    l = bytes([n]) if n < 0x80 else (b"\x81" + bytes([n]) if n < 0x100 else b"\x82" + n.to_bytes(2, "big"))
    return t + l + value
def make_dg2(img_bytes, w, h, img_type=1, npoints=0):
    pts = b"".join(b"\x01\x05" + struct.pack(">HH", 10 * i, 20 * i) + b"\x00\x00" for i in range(npoints))
    info = bytes([1, img_type]) + struct.pack(">HH", w, h) + bytes([1, 2]) + b"\x00\x00\x00\x00"
    rec_len = 20 + len(pts) + len(info) + len(img_bytes)
    frh = struct.pack(">IH", rec_len, npoints) + bytes(14)
    fac = b"FAC\x00" + b"010\x00" + struct.pack(">IH", 14 + rec_len, 1)
    bdb = fac + frh + pts + info + img_bytes
    bht = tl(0xA1, tl(0x80, b"\x01\x01") + tl(0x81, b"\x02") + tl(0x87, b"\x01\x01") + tl(0x88, b"\x00\x08"))
    return tl(0x75, tl(0x7F61, tl(0x02, b"\x01") + tl(0x7F60, bht + tl(0x5F2E, bdb)))), bdb
def j2k(w, h, mode, **kw):
    img = Image.new(mode, (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            v = (x * 3 + y * 5) % 256
            px[x, y] = (v, 255 - v, (x * y) % 256) if mode == "RGB" else v
    b = io.BytesIO(); img.save(b, **kw); return b.getvalue()
cases = [
    ("JP2_RGB",  307, 378, "RGB", dict(format="JPEG2000", irreversible=True, num_resolutions=6, quality_mode="rates", quality_layers=[20]), 0, True),
    ("J2K_GRAY", 240, 320, "L",   dict(format="JPEG2000", no_jp2=True, num_resolutions=5, codeblock_size=(32, 32), progression="RPCL"), 3, False),
]
out = []
for name, w, h, mode, kw, npts, jp2 in cases:
    img = j2k(w, h, mode, **kw)
    dg2, bdb = make_dg2(img, w, h, 1, npts)
    off = dg2.index(img)
    out.append(f"static const uint8_t V_{name}[] = {{{', '.join(f'0x{b:02X}' for b in dg2[:600])}}};")
    out.append(f"#define V_{name}_TOTAL {len(dg2)}\n#define V_{name}_IMG_OFF {off}\n#define V_{name}_IMG_LEN {len(img)}")
    out.append(f"#define V_{name}_W {w}\n#define V_{name}_H {h}\n#define V_{name}_JP2 {int(jp2)}")
    print(name, "DG2", len(dg2), "B, imagen en", off, "len", len(img))
open("dg2_vectors.h", "w").write("\n".join(out) + "\n")
