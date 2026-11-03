# Implementacion independiente de SM AES (ICAO 9303-11 9.8) para generar vectores de prueba
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.cmac import CMAC
KSenc = bytes.fromhex("F5F0E35C0D7161EE6724EE513A0D9A7F")
KSmac = bytes.fromhex("FE251C7858B356B24514B3BD5F4297D1")
def pad(b): b = b + b"\x80"; return b + b"\x00" * ((-len(b)) % 16)
def ecb(k, b): e = Cipher(algorithms.AES(k), modes.ECB()).encryptor(); return e.update(b) + e.finalize()
def cbc(k, iv, b, enc=True):
    c = Cipher(algorithms.AES(k), modes.CBC(iv)); x = c.encryptor() if enc else c.decryptor(); return x.update(b) + x.finalize()
def mac(ssc, m): c = CMAC(algorithms.AES(KSmac)); c.update(pad(ssc.to_bytes(16, "big") + m)); return c.finalize()[:8]
def wrap(ssc, hdr, data=b"", le=None):
    ssc += 1; h = bytes([hdr[0] | 0x0C]) + hdr[1:]
    do87 = b""
    if data:
        iv = ecb(KSenc, ssc.to_bytes(16, "big")); ct = cbc(KSenc, iv, pad(data))
        do87 = bytes([0x87, len(ct) + 1, 1]) + ct
    do97 = bytes([0x97, 1, le & 0xFF]) if le is not None else b""
    cc = mac(ssc, pad(h) + do87 + do97)
    body = do87 + do97 + b"\x8e\x08" + cc
    return ssc, h + bytes([len(body)]) + body + b"\x00"
def card_resp(ssc, data, sw):
    ssc += 1; do87 = b""
    if data:
        iv = ecb(KSenc, ssc.to_bytes(16, "big")); ct = cbc(KSenc, iv, pad(data))
        do87 = bytes([0x87, len(ct) + 1, 1]) + ct
    do99 = b"\x99\x02" + sw.to_bytes(2, "big")
    cc = mac(ssc, do87 + do99)
    return ssc, do87 + do99 + b"\x8e\x08" + cc + b"\x90\x00"
C = lambda b: ", ".join(f"0x{x:02X}" for x in b)
out = []
ssc = 0
ssc, a1 = wrap(ssc, bytes.fromhex("00A4040C"), bytes.fromhex("A0000002471001"))   # SELECT AID
ssc, r1 = card_resp(ssc, b"", 0x9000)
ssc, a2 = wrap(ssc, bytes.fromhex("00A4020C"), bytes.fromhex("0101"))             # SELECT DG1
ssc, r2 = card_resp(ssc, b"", 0x9000)
ssc, a3 = wrap(ssc, bytes.fromhex("00B00000"), b"", 4)                           # READ BINARY 4
dg1 = bytes.fromhex("615D5F1F5A") + b"I<UTOD231458907<<<<<<<<<<<<<<<7408122F1204159UTO<<<<<<<<<<<6ERIKSSON<<ANNA<MARIA<<<<<<<<<<"
ssc, r3 = card_resp(ssc, dg1[:4], 0x9000)
ssc, a4 = wrap(ssc, bytes.fromhex("00B00004"), b"", len(dg1) - 4)                   # READ resto
ssc, r4 = card_resp(ssc, dg1[4:], 0x9000)
ssc, r5_err = card_resp(ssc + 1, b"", 0x6A82)                                    # error protegido (tras un wrap)
for n, v in [("A1", a1), ("R1", r1), ("A2", a2), ("R2", r2), ("A3", a3), ("R3", r3), ("A4", a4), ("R4", r4), ("R5", r5_err), ("DG1", dg1)]:
    out.append(f"static const uint8_t V_{n}[] = {{{C(v)}}};")
open("/home/claude/dnie_tests/sm_vectors.h", "w").write("\n".join(out) + "\n")
print("vectores generados; DG1 =", len(dg1), "bytes")
