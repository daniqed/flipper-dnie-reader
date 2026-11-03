# Verificacion independiente del ejemplo ICAO 9303-11 (apendice D): KDF 3DES y Secure Messaging 3DES
import hashlib
from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES
from cryptography.hazmat.primitives.ciphers import Cipher, modes
H = bytes.fromhex
def parity(k): return bytes((b & 0xFE) | (bin(b >> 1).count("1") % 2 == 0) for b in k)
def kdf(seed, c): return parity(hashlib.sha1(seed + c.to_bytes(4, "big")).digest()[:16])
def des(k8, b, enc=True):  # DES simple = 3DES con K1=K2=K3
    c = Cipher(TripleDES(k8 * 3), modes.ECB()); x = c.encryptor() if enc else c.decryptor(); return x.update(b) + x.finalize()
def tdes_cbc(k16, b, enc=True):
    c = Cipher(TripleDES(k16), modes.CBC(bytes(8))); x = c.encryptor() if enc else c.decryptor(); return x.update(b) + x.finalize()
def pad(b): b += b"\x80"; return b + b"\x00" * ((-len(b)) % 8)
def retail_mac(k16, data):  # ISO 9797-1 MAC alg. 3 sobre datos ya rellenos
    k1, k2 = k16[:8], k16[8:]; y = bytes(8)
    for i in range(0, len(data), 8): y = des(k1, bytes(a ^ b for a, b in zip(y, data[i:i+8])))
    return des(k1, des(k2, y, False))
def ok(name, got, exp): print(f"{name:26s}", "OK" if got == exp else f"FALLA {got.hex().upper()}")

# Derivacion de claves BAC (D.2)
seed = H("239AB9CB282DAF66231DC5A4DF6BFBAE")
ok("Kenc", kdf(seed, 1), H("AB94FDECF2674FDFB9B391F85D7F76F2"))
ok("Kmac", kdf(seed, 2), H("7962D9ECE03D1ACD4C76089DCE131543"))
# Secure Messaging (D.4)
ksenc, ksmac = H("979EC13B1CBFE9DCD01AB0FED307EAE5"), H("F1CB1F1FB5ADF208806B89DC579DC1F8")
ssc = int("887022120C06C226", 16)
def wrap(hdr, data=b"", le=None):
    global ssc; ssc += 1
    h = bytes([hdr[0] | 0x0C]) + hdr[1:]
    do87 = (bytes([0x87, len(pad(data)) + 1, 1]) + tdes_cbc(ksenc, pad(data))) if data else b""
    do97 = bytes([0x97, 1, le]) if le is not None else b""
    cc = retail_mac(ksmac, pad(ssc.to_bytes(8, "big") + pad(h) + do87 + do97))
    body = do87 + do97 + b"\x8e\x08" + cc
    return h + bytes([len(body)]) + body + b"\x00"
def check_resp(resp):
    global ssc; ssc += 1
    do = resp[:-12]; mac = resp[-10:-2]
    return retail_mac(ksmac, pad(ssc.to_bytes(8, "big") + do)) == mac
ok("SELECT EF.COM protegido", wrap(H("00A4020C"), H("011E")), H("0CA4020C158709016375432908C044F68E08BF8B92D635FF24F800"))
print(f"{'MAC respuesta SELECT':26s}", "OK" if check_resp(H("990290008E08FA855A5D4C50A8ED9000")) else "FALLA")
ok("READ BINARY protegido", wrap(H("00B00000"), le=4), H("0CB000000D9701048E08ED6705417E96BA5500"))
r = H("8709019FF0EC34F9922651990290008E08AD55CC17140B2DED9000")
print(f"{'MAC respuesta READ':26s}", "OK" if check_resp(r) else "FALLA")
ok("datos descifrados", tdes_cbc(ksenc, r[3:11], False)[:4], H("60145F01"))
