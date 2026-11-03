# SOD + DG14 sinteticos con la estructura de ICAO 9303 / TR-03110 para probar los parsers
import hashlib, datetime
from asn1crypto import cms, core, algos, x509 as ax509, keys as akeys
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

def tlv(tag, value):
    n = len(value)
    l = bytes([n]) if n < 0x80 else (b"\x81" + bytes([n]) if n < 0x100 else b"\x82" + n.to_bytes(2, "big"))
    return (tag.to_bytes(2, "big") if tag > 0xFF else bytes([tag])) + l + value

# --- DG1 (espécimen estilo DNIe del test de MRZ) y "DG2" cualquiera
mrz = open("mrz_vectors.h").read().split("{")[1].split("}")[0]
dg1 = bytes(int(x, 16) for x in mrz.replace(" ", "").split(","))
dg2 = bytes(range(256)) * 40

# --- DG14: ChipAuthenticationInfo + ChipAuthenticationPublicKeyInfo (bp256r1 por OID y por id) + PACE + TA
ca_key = ec.generate_private_key(ec.BrainpoolP256R1())
ca_pub = ca_key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
OID = lambda dotted: core.ObjectIdentifier(dotted).dump()
INT = lambda v: core.Integer(v).dump()
SEQ = lambda *xs: tlv(0x30, b"".join(xs))
spki_named = SEQ(SEQ(OID("1.2.840.10045.2.1"), OID("1.3.36.3.3.2.8.1.1.7")), tlv(0x03, b"\x00" + ca_pub))
infos = [
    SEQ(OID("0.4.0.127.0.7.2.2.3.2.2"), INT(1), INT(7)),          # id-CA-ECDH-AES-CBC-CMAC-128, v1, keyId 7
    SEQ(OID("0.4.0.127.0.7.2.2.1.2"), spki_named, INT(7)),        # id-PK-ECDH
    SEQ(OID("0.4.0.127.0.7.2.2.4.2.2"), INT(2), INT(13)),         # PACE
    SEQ(OID("0.4.0.127.0.7.2.2.2"), INT(1)),                      # TA
]
dg14 = tlv(0x6E, tlv(0x31, b"".join(sorted(infos))))

# --- LDSSecurityObject
hashes_dg = [(1, dg1), (2, dg2), (14, dg14)]
lds = SEQ(INT(0), SEQ(OID("2.16.840.1.101.3.4.2.1")),
          SEQ(*[SEQ(INT(n), tlv(0x04, hashlib.sha256(d).digest())) for n, d in hashes_dg]))

# --- Certificado del Document Signer (autofirmado para la prueba)
ds_key = ec.generate_private_key(ec.BrainpoolP256R1())
name = lambda cn: x509.Name([x509.NameAttribute(NameOID.COUNTRY_NAME, "ES"), x509.NameAttribute(NameOID.COMMON_NAME, cn)])
now = datetime.datetime(2026, 1, 1)
cert = (x509.CertificateBuilder().subject_name(name("DS DNIe Prueba")).issuer_name(name("CSCA Prueba"))
        .public_key(ds_key.public_key()).serial_number(1234).not_valid_before(now)
        .not_valid_after(now + datetime.timedelta(days=3650)).sign(ds_key, hashes.SHA256()))
cert_der = cert.public_bytes(serialization.Encoding.DER)

# --- SignedData con atributos firmados (contentType + messageDigest)
signed_attrs = cms.CMSAttributes([
    cms.CMSAttribute({"type": "content_type", "values": ["2.23.136.1.1.1"]}),
    cms.CMSAttribute({"type": "message_digest", "values": [hashlib.sha256(lds).digest()]}),
])
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
sig = ds_key.sign(signed_attrs.dump(), ec.ECDSA(hashes.SHA256()))
signer = cms.SignerInfo({
    "version": "v1",
    "sid": cms.SignerIdentifier({"issuer_and_serial_number": cms.IssuerAndSerialNumber({
        "issuer": ax509.Certificate.load(cert_der).issuer, "serial_number": 1234})}),
    "digest_algorithm": algos.DigestAlgorithm({"algorithm": "sha256"}),
    "signed_attrs": signed_attrs,
    "signature_algorithm": algos.SignedDigestAlgorithm({"algorithm": "sha256_ecdsa"}),
    "signature": sig,
})
sd = cms.SignedData({
    "version": "v3",
    "digest_algorithms": [algos.DigestAlgorithm({"algorithm": "sha256"})],
    "encap_content_info": {"content_type": "2.23.136.1.1.1", "content": core.ParsableOctetString(lds)},
    "certificates": [ax509.Certificate.load(cert_der)],
    "signer_infos": [signer],
})
sod = tlv(0x77, cms.ContentInfo({"content_type": "signed_data", "content": sd}).dump())

C = lambda b: ", ".join(f"0x{x:02X}" for x in b)
out = [f"static const uint8_t V_SOD[] = {{{C(sod)}}};",
       f"static const uint8_t V_DG14[] = {{{C(dg14)}}};",
       f"static const uint8_t V_DG2_SOD[] = {{{C(dg2)}}};",
       f"static const uint8_t V_CA_PUB[] = {{{C(ca_pub)}}};",
       f"static const uint8_t V_DS_PUB[] = {{{C(ds_key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint))}}};",
       f"static const uint8_t V_SIGNED_ATTRS_SHA256[] = {{{C(hashlib.sha256(signed_attrs.dump()).digest())}}};"]

# ---------- Variante RSA (como el DNIe: LDS con SHA-1, firma rsaEncryption PKCS#1 v1.5) ----------
from cryptography.hazmat.primitives.asymmetric import rsa, padding
rsa_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
lds1 = SEQ(INT(0), SEQ(OID("1.3.14.3.2.26"), b"\x05\x00"),
           SEQ(*[SEQ(INT(n), tlv(0x04, hashlib.sha1(d).digest())) for n, d in hashes_dg]))
rcert = (x509.CertificateBuilder().subject_name(name("DS n-eID PRUEBA 2")).issuer_name(name("CSCA PRUEBA"))
         .public_key(rsa_key.public_key()).serial_number(77).not_valid_before(now)
         .not_valid_after(now + datetime.timedelta(days=3650)).sign(rsa_key, hashes.SHA256()))
rcert_der = rcert.public_bytes(serialization.Encoding.DER)
def rsa_sod(digest, hashname, sigalg, tamper=False):
    hfun = getattr(hashlib, hashname)
    attrs = cms.CMSAttributes([
        cms.CMSAttribute({"type": "content_type", "values": ["2.23.136.1.1.1"]}),
        cms.CMSAttribute({"type": "message_digest", "values": [hfun(lds1).digest()]}),
    ])
    hcls = {"sha1": hashes.SHA1(), "sha256": hashes.SHA256()}[hashname]
    sig = rsa_key.sign(attrs.dump(), padding.PKCS1v15(), hcls)
    if tamper: sig = sig[:-1] + bytes([sig[-1] ^ 1])
    si = cms.SignerInfo({"version": "v1",
        "sid": cms.SignerIdentifier({"issuer_and_serial_number": cms.IssuerAndSerialNumber({
            "issuer": ax509.Certificate.load(rcert_der).issuer, "serial_number": 77})}),
        "digest_algorithm": algos.DigestAlgorithm({"algorithm": digest}),
        "signed_attrs": attrs,
        "signature_algorithm": algos.SignedDigestAlgorithm({"algorithm": sigalg}),
        "signature": sig})
    sdr = cms.SignedData({"version": "v3", "digest_algorithms": [algos.DigestAlgorithm({"algorithm": digest})],
        "encap_content_info": {"content_type": "2.23.136.1.1.1", "content": core.ParsableOctetString(lds1)},
        "certificates": [ax509.Certificate.load(rcert_der)], "signer_infos": [si]})
    return tlv(0x77, cms.ContentInfo({"content_type": "signed_data", "content": sdr}).dump())
out.append(f"static const uint8_t V_SOD_RSA_SHA1[] = {{{C(rsa_sod('sha1', 'sha1', 'rsassa_pkcs1v15'))}}};")
out.append(f"static const uint8_t V_SOD_RSA_SHA256[] = {{{C(rsa_sod('sha256', 'sha256', 'sha256_rsa'))}}};")
out.append(f"static const uint8_t V_SOD_RSA_BAD[] = {{{C(rsa_sod('sha1', 'sha1', 'rsassa_pkcs1v15', tamper=True))}}};")
open("sod_vectors.h", "w").write("\n".join(out) + "\n")
print("SOD", len(sod), "B, DG14", len(dg14), "B, firma", len(sig), "B")
