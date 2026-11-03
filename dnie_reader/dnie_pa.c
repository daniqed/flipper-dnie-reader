#include "dnie_pa.h"

#include <stdlib.h>
#include <string.h>
#include <mbedtls/bignum.h>

static const uint8_t OID_RSA_ENCRYPTION[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
static const uint8_t OID_RSA_PREFIX[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01};

// DigestInfo DER (sin el hash) de cada algoritmo, con parametros NULL (forma habitual)
static const uint8_t DI_SHA1[] = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x0E,
                                  0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14};
static const uint8_t DI_SHA224[] = {0x30, 0x2D, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                                    0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1C};
static const uint8_t DI_SHA256[] = {0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                                    0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

static const uint8_t* digest_info(DnieHashAlg alg, size_t* len) {
    switch(alg) {
    case DnieHashSha1:
        *len = sizeof(DI_SHA1);
        return DI_SHA1;
    case DnieHashSha224:
        *len = sizeof(DI_SHA224);
        return DI_SHA224;
    case DnieHashSha256:
        *len = sizeof(DI_SHA256);
        return DI_SHA256;
    default:
        return NULL;
    }
}

// Comprueba el DigestInfo de EM, aceptando tambien la variante sin NULL (30 xx 30 yy 06 OID 04 ..)
static bool digest_info_matches(DnieHashAlg alg, const uint8_t* di, size_t len, const uint8_t* hash) {
    size_t ref_len;
    const uint8_t* ref = digest_info(alg, &ref_len);
    size_t hl = dnie_hash_len(alg);
    if(!ref) return false;
    if(len == ref_len + hl && memcmp(di, ref, ref_len) == 0 && memcmp(di + ref_len, hash, hl) == 0)
        return true;
    // Sin "05 00": longitudes 2 menos en las dos SEQUENCE y el OID igual
    size_t oid_len = ref[5];
    size_t alt_len = ref_len - 2;
    if(len != alt_len + hl) return false;
    if(di[0] != 0x30 || di[1] != ref[1] - 2 || di[2] != 0x30 || di[3] != ref[3] - 2) return false;
    if(memcmp(di + 4, ref + 4, 2 + oid_len) != 0) return false;
    if(di[6 + oid_len] != 0x04 || di[7 + oid_len] != hl) return false;
    return memcmp(di + 8 + oid_len, hash, hl) == 0;
}

DniePaResult dnie_pa_rsa_pkcs1_verify(
    const uint8_t* n,
    size_t n_len,
    const uint8_t* e,
    size_t e_len,
    DnieHashAlg alg,
    const uint8_t* hash,
    const uint8_t* sig,
    size_t sig_len) {
    // Quitar ceros a la izquierda del modulo (INTEGER positivo)
    while(n_len > 0 && n[0] == 0) {
        n++;
        n_len--;
    }
    if(n_len < 64 || n_len > 512 || sig_len > n_len + 1) return DniePaFormat;

    mbedtls_mpi N, E, S, M;
    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&E);
    mbedtls_mpi_init(&S);
    mbedtls_mpi_init(&M);
    uint8_t* em = malloc(n_len);
    DniePaResult r = DniePaBadSignature;

    do {
        if(!em || mbedtls_mpi_read_binary(&N, n, n_len) != 0 ||
           mbedtls_mpi_read_binary(&E, e, e_len) != 0 ||
           mbedtls_mpi_read_binary(&S, sig, sig_len) != 0) {
            r = DniePaFormat;
            break;
        }
        if(mbedtls_mpi_cmp_mpi(&S, &N) >= 0) break; // firma fuera de rango
        // M = S^E mod N (operacion publica: solo usa la clave publica)
        if(mbedtls_mpi_exp_mod(&M, &S, &E, &N, NULL) != 0) {
            r = DniePaFormat;
            break;
        }
        if(mbedtls_mpi_write_binary(&M, em, n_len) != 0) break;

        // EM = 00 01 FF..FF 00 DigestInfo  (al menos 8 bytes FF)
        if(em[0] != 0x00 || em[1] != 0x01) break;
        size_t i = 2;
        while(i < n_len && em[i] == 0xFF) i++;
        if(i - 2 < 8 || i >= n_len || em[i] != 0x00) break;
        i++;
        if(digest_info_matches(alg, em + i, n_len - i, hash)) r = DniePaOk;
    } while(0);

    if(em) {
        memset(em, 0, n_len);
        free(em);
    }
    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&E);
    mbedtls_mpi_free(&S);
    mbedtls_mpi_free(&M);
    return r;
}

// Saca n y e de la clave publica RSA del certificado
static bool cert_rsa_key(const DerTlv* cert, DerTlv* n, DerTlv* e) {
    DerIt c = der_enter(cert);
    DerTlv tbs, f, spki, alg, oid, bits;
    if(!der_expect(&c, 0x30, &tbs)) return false;
    DerIt t = der_enter(&tbs);
    if(!der_next(&t, &f)) return false;
    if(f.tag == 0xA0 && !der_next(&t, &f)) return false; // version -> serial
    // sigAlg, issuer, validity, subject, SPKI
    for(int k = 0; k < 4; k++)
        if(!der_next(&t, &f)) return false;
    if(!der_expect(&t, 0x30, &spki)) return false;
    DerIt s = der_enter(&spki);
    if(!der_expect(&s, 0x30, &alg) || !der_expect(&s, 0x03, &bits)) return false;
    DerIt a = der_enter(&alg);
    if(!der_expect(&a, 0x06, &oid) || !der_oid_is(&oid, OID_RSA_ENCRYPTION, sizeof(OID_RSA_ENCRYPTION)))
        return false;
    if(bits.len < 2 || bits.val[0] != 0) return false;
    DerIt k = der_it(bits.val + 1, bits.len - 1);
    DerTlv seq;
    if(!der_expect(&k, 0x30, &seq)) return false;
    DerIt ks = der_enter(&seq);
    return der_expect(&ks, 0x02, n) && der_expect(&ks, 0x02, e);
}

// Hash para la firma: del OID de firma (sha256WithRSA...) o, si es rsaEncryption a secas,
// del digestAlgorithm del SignerInfo
static DnieHashAlg signature_hash(const DnieSod* sod, bool* is_rsa) {
    *is_rsa = false;
    const DerTlv* oid = &sod->sig_alg_oid;
    if(oid->tag != 0x06) return DnieHashUnknown;
    if(der_oid_is(oid, OID_RSA_ENCRYPTION, sizeof(OID_RSA_ENCRYPTION))) {
        *is_rsa = true;
        return sod->signer_hash;
    }
    if(oid->len == sizeof(OID_RSA_PREFIX) + 1 &&
       memcmp(oid->val, OID_RSA_PREFIX, sizeof(OID_RSA_PREFIX)) == 0) {
        *is_rsa = true;
        switch(oid->val[sizeof(OID_RSA_PREFIX)]) {
        case 5:
            return DnieHashSha1;
        case 11:
            return DnieHashSha256;
        case 14:
            return DnieHashSha224;
        default:
            return DnieHashUnknown; // PSS (10), SHA-384/512...
        }
    }
    return DnieHashUnknown;
}

DniePaResult dnie_pa_verify_sod(const DnieSod* sod, DniePaInfo* info) {
    memset(info, 0, sizeof(*info));
    if(!sod->has_cert) return info->result = DniePaNoCert;
    if(!sod->signed_attrs.raw || sod->signed_attrs.raw_len < 2) return info->result = DniePaFormat;

    bool is_rsa;
    DnieHashAlg alg = signature_hash(sod, &is_rsa);
    info->hash = alg;
    if(!is_rsa || !dnie_hash_supported(alg)) return info->result = DniePaUnsupported;

    DerTlv n, e;
    if(!cert_rsa_key(&sod->cert, &n, &e)) return info->result = DniePaUnsupported;
    size_t nl = n.len;
    const uint8_t* nv = n.val;
    while(nl > 0 && *nv == 0) {
        nv++;
        nl--;
    }
    info->key_bits = (uint16_t)(nl * 8);

    // Lo firmado son los signedAttrs con su etiqueta [0] cambiada por SET (0x31)
    uint8_t h[DNIE_HASH_MAX];
    DnieHash hs;
    const uint8_t set_tag = 0x31;
    if(!dnie_hash_start(&hs, alg)) return info->result = DniePaUnsupported;
    dnie_hash_update(&hs, &set_tag, 1);
    dnie_hash_update(&hs, sod->signed_attrs.raw + 1, sod->signed_attrs.raw_len - 1);
    if(!dnie_hash_finish(&hs, h)) return info->result = DniePaFormat;

    info->result = dnie_pa_rsa_pkcs1_verify(
        n.val, n.len, e.val, e.len, alg, h, sod->signature.val, sod->signature.len);
    return info->result;
}

const char* dnie_pa_result_str(DniePaResult r) {
    switch(r) {
    case DniePaOk:
        return "OK";
    case DniePaBadSignature:
        return "NO VALIDA";
    case DniePaNoCert:
        return "sin certificado";
    case DniePaUnsupported:
        return "alg. no soportado";
    case DniePaFormat:
        return "mal formada";
    case DniePaNoMemory:
        return "sin memoria";
    default:
        return "no comprobada";
    }
}
