# Pruebas en PC de la matematica de PACE

Comprueba CMAC (RFC 4493) y PACE ECDH-GM sobre brainpoolP256r1 contra el
ejemplo de ICAO Doc 9303 parte 11, apendice G.1.

Necesitas mbedtls 3.6.2 compilado para tu PC (libmbedcrypto) y sus cabeceras:

    gcc -Wall -fsanitize=address,undefined -I../dnie_reader -I<mbedtls>/include \
        test_pace_math.c ../dnie_reader/dnie_pace_math.c ../dnie_reader/dnie_ec.c \
        ../dnie_reader/dnie_crypto.c <mbedtls>/build/library/libmbedcrypto.a -o test_pace_math
    ./test_pace_math

## Secure Messaging y MRZ

`gen_sm_vectors.py` es una segunda implementacion de SM AES escrita en Python
(con la libreria `cryptography`) que simula al chip y genera `sm_vectors.h`
y `mrz_vectors.h`. `test_sm.c` y `test_mrz.c` comprueban que el C produce y
acepta exactamente los mismos bytes.

    pip install cryptography && python3 gen_sm_vectors.py
    gcc -I../dnie_reader -I. -I<mbedtls>/include test_sm.c ../dnie_reader/dnie_sm.c \
        ../dnie_reader/dnie_crypto.c <mbedtls>/build/library/libmbedcrypto.a -o test_sm && ./test_sm
    gcc -I../dnie_reader -I. test_mrz.c ../dnie_reader/dnie_mrz.c -o test_mrz && ./test_mrz

## DG2 (foto)

`gen_dg2_vectors.py` crea DG2 sinteticos con fotos JPEG2000 reales (Pillow) y
`test_dg2.c` comprueba que se encuentra la imagen y se lee su cabecera:

    python3 gen_dg2_vectors.py
    gcc -I../dnie_reader -I. test_dg2.c ../dnie_reader/dnie_dg2.c -o test_dg2 && ./test_dg2

## Decodificador JPEG2000 y pixel art

`gen_j2k_ref.py` crea una foto de prueba con los parametros del DNIe (307x378, 9/7,
5 niveles, bloques 64x64, 1 capa, LRCP, MCT) y la referencia de OpenJPEG (Pillow)
a 1/4 de resolucion. `gen_j2k_variants.py` crea variantes (compresion, gris, RPCL,
RLCP, bloques 32, imagen grande). `test_j2k.c` compara la luminancia con OpenJPEG:

    python3 gen_j2k_ref.py && python3 gen_j2k_variants.py
    gcc -O1 -I../dnie_reader test_j2k.c ../dnie_reader/dnie_j2k.c ../dnie_reader/dnie_dg2.c -lm -o test_j2k
    ./test_j2k                                   # foto tipo DNIe
    ./test_j2k var_comp_baja.jp2 var_comp_baja.gray   # etc.

## Autenticidad (SOD y DG14)

`gen_sod_vectors.py` construye un EF.SOD realista (CMS SignedData con certificado DS
brainpoolP256r1, LDSSecurityObject SHA-256 y firma ECDSA sobre signedAttrs) y un DG14
con ChipAuthenticationInfo/PublicKeyInfo. Necesita `pip install asn1crypto cryptography`.

    python3 gen_sod_vectors.py
    gcc -I../dnie_reader -I. -I<mbedtls>/include test_sod.c ../dnie_reader/dnie_sod.c \
        ../dnie_reader/dnie_dg14.c ../dnie_reader/dnie_der.c ../dnie_reader/dnie_hash.c \
        <mbedtls>/build/library/libmbedcrypto.a -o test_sod && ./test_sod

## Deteccion de clones (v0.12)

- `ref_sm3des.py`: comprueba en Python, de forma independiente, el ejemplo resuelto de
  ICAO 9303-11 apendice D (KDF 3DES con paridad, SELECT/READ protegidos, MAC retail).
  `test_sm.c` exige que el C reproduzca esos mismos bytes.
- `test_pa.c`: firma RSA PKCS#1 v1.5 del SOD (como el DNIe: SHA-1 + rsaEncryption),
  SHA-256, firma alterada y atributos firmados alterados. Vectores en `gen_sod_vectors.py`.
- `test_ca.c`: Chip Authentication con un chip simulado: mismas claves en ambos lados y un
  "clon" sin la clave privada no supera el MAC del canal nuevo.
