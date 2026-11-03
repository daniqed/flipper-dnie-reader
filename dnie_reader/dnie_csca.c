#include "dnie_csca.h"
#include "dnie_pa.h"

#include <string.h>

#include "dnie_csca_keys.inc"

#define CSCA_COUNT (sizeof(DNIE_CSCA_KEYS) / sizeof(DNIE_CSCA_KEYS[0]))

static const uint8_t OID_RSA_PREFIX[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01};

static DnieHashAlg sig_hash(const DerTlv* alg) {
    DerIt a = der_enter(alg);
    DerTlv oid;
    if(!der_expect(&a, 0x06, &oid)) return DnieHashUnknown;
    if(oid.len != sizeof(OID_RSA_PREFIX) + 1 ||
       memcmp(oid.val, OID_RSA_PREFIX, sizeof(OID_RSA_PREFIX)) != 0)
        return DnieHashUnknown;
    switch(oid.val[sizeof(OID_RSA_PREFIX)]) {
    case 5:
        return DnieHashSha1;
    case 11:
        return DnieHashSha256;
    case 14:
        return DnieHashSha224;
    default:
        return DnieHashUnknown; // PSS, SHA-384/512
    }
}

static DnieCscaResult try_key(
    const DnieCscaKey* k,
    DnieHashAlg alg,
    const uint8_t* h,
    const DerTlv* sig) {
    uint8_t e[4] = {k->e >> 24, k->e >> 16, k->e >> 8, k->e};
    DniePaResult r = dnie_pa_rsa_pkcs1_verify(
        k->n, k->n_len, e, sizeof(e), alg, h, sig->val + 1, sig->len - 1);
    if(r == DniePaOk) return DnieCscaOk;
    if(r == DniePaBadSignature) return DnieCscaBadSignature;
    return DnieCscaFormat;
}

DnieCscaResult dnie_csca_verify_cert(const DerTlv* cert, DnieCscaInfo* info) {
    memset(info, 0, sizeof(*info));
    DerIt c = der_enter(cert);
    DerTlv tbs, alg, sig, f, issuer;
    if(!der_expect(&c, 0x30, &tbs) || !der_expect(&c, 0x30, &alg) ||
       !der_expect(&c, 0x03, &sig) || sig.len < 2 || sig.val[0] != 0)
        return info->result = DnieCscaFormat;

    DerIt t = der_enter(&tbs);
    if(!der_next(&t, &f)) return info->result = DnieCscaFormat;
    if(f.tag == 0xA0 && !der_next(&t, &f)) return info->result = DnieCscaFormat; // -> serial
    if(!der_next(&t, &f) || !der_expect(&t, 0x30, &issuer)) return info->result = DnieCscaFormat;

    DnieHashAlg ha = sig_hash(&alg);
    if(!dnie_hash_supported(ha)) return info->result = DnieCscaUnsupported;

    uint8_t h[DNIE_HASH_MAX];
    DnieHash hs;
    if(!dnie_hash_start(&hs, ha)) return info->result = DnieCscaUnsupported;
    dnie_hash_update(&hs, tbs.raw, tbs.raw_len);
    if(!dnie_hash_finish(&hs, h)) return info->result = DnieCscaFormat;

    // Primero las CSCA cuyo nombre coincide exactamente con el emisor
    DnieCscaResult res = DnieCscaUnknownIssuer;
    for(size_t i = 0; i < CSCA_COUNT; i++) {
        const DnieCscaKey* k = &DNIE_CSCA_KEYS[i];
        if(issuer.raw_len != k->subject_len || memcmp(issuer.raw, k->subject, k->subject_len) != 0)
            continue;
        info->csca_name = k->name;
        res = try_key(k, ha, h, &sig);
        if(res == DnieCscaOk) return info->result = res;
    }
    if(res == DnieCscaUnknownIssuer) {
        // El nombre no coincide byte a byte (codificacion distinta): probar todas
        for(size_t i = 0; i < CSCA_COUNT; i++) {
            if(try_key(&DNIE_CSCA_KEYS[i], ha, h, &sig) == DnieCscaOk) {
                info->csca_name = DNIE_CSCA_KEYS[i].name;
                return info->result = DnieCscaOk;
            }
        }
    }
    return info->result = res;
}

const char* dnie_csca_result_str(DnieCscaResult r) {
    switch(r) {
    case DnieCscaOk:
        return "OK";
    case DnieCscaBadSignature:
        return "FALSA";
    case DnieCscaUnknownIssuer:
        return "emisor desconocido";
    case DnieCscaUnsupported:
        return "alg. no soportado";
    case DnieCscaFormat:
        return "mal formado";
    case DnieCscaNoMemory:
        return "sin memoria";
    default:
        return "no comprobada";
    }
}
