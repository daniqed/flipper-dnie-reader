#include <stdio.h>
#include <string.h>
#include "dnie_sod.h"
#include "dnie_dg14.h"
#include "mrz_vectors.h"
#include "sod_vectors.h"
static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-38s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)
int main(void) {
    static DnieSod sod;
    CHECK("parse SOD", dnie_sod_parse(V_SOD, sizeof(V_SOD), &sod));
    CHECK("  hash LDS = SHA-256", sod.lds_hash == DnieHashSha256);
    CHECK("  3 hashes (DG1, DG2, DG14)", sod.dg_count == 3 && dnie_sod_dg_hash(&sod, 1) && dnie_sod_dg_hash(&sod, 2) && dnie_sod_dg_hash(&sod, 14) && !dnie_sod_dg_hash(&sod, 3));
    uint8_t h[32];
    dnie_hash(DnieHashSha256, V_DG1_ES, sizeof(V_DG1_ES), h);
    CHECK("  hash DG1 coincide", !memcmp(h, dnie_sod_dg_hash(&sod, 1), 32));
    DnieHash st; dnie_hash_start(&st, DnieHashSha256);   // DG2 en streaming, a trozos de 96
    for(size_t i = 0; i < sizeof(V_DG2_SOD); i += 96) dnie_hash_update(&st, V_DG2_SOD + i, sizeof(V_DG2_SOD) - i < 96 ? sizeof(V_DG2_SOD) - i : 96);
    dnie_hash_finish(&st, h);
    CHECK("  hash DG2 (streaming) coincide", !memcmp(h, dnie_sod_dg_hash(&sod, 2), 32));
    dnie_hash(DnieHashSha256, V_DG14, sizeof(V_DG14), h);
    CHECK("  hash DG14 coincide", !memcmp(h, dnie_sod_dg_hash(&sod, 14), 32));
    CHECK("  messageDigest == hash(eContent)", dnie_sod_check_message_digest(&sod));
    CHECK("  firma ECDSA-SHA256", !strcmp(sod.sig_alg, "ECDSA-SHA256") && sod.signature.len >= 64);
    CHECK("  certificado DS y CN", sod.has_cert && !strcmp(sod.ds_subject_cn, "DS DNIe Prueba") && !strcmp(sod.ds_issuer_cn, "CSCA Prueba"));
    CHECK("  signedAttrs localizados", sod.signed_attrs.tag == 0xA0 && sod.signed_attrs.len > 0);
    // Re-etiquetado como SET (0x31) para la firma: debe dar el mismo hash que Python
    { static uint8_t sa[512]; memcpy(sa, sod.signed_attrs.raw, sod.signed_attrs.raw_len); sa[0] = 0x31;
      dnie_hash(DnieHashSha256, sa, sod.signed_attrs.raw_len, h);
      CHECK("  hash(signedAttrs como SET) ok", !memcmp(h, V_SIGNED_ATTRS_SHA256, 32)); }

    static uint8_t bad[sizeof(V_SOD)]; memcpy(bad, V_SOD, sizeof(bad));
    DnieSod s2; dnie_sod_parse(bad, sizeof(bad), &s2);
    // alterar un byte de un hash del LDSSecurityObject -> messageDigest ya no cuadra
    size_t off = (size_t)(sod.econtent.val - V_SOD) + sod.econtent.len - 5; bad[off] ^= 0x55;
    CHECK("SOD alterado -> messageDigest falla", dnie_sod_parse(bad, sizeof(bad), &s2) && !dnie_sod_check_message_digest(&s2));
    CHECK("SOD truncado -> rechazo", !dnie_sod_parse(V_SOD, 100, &s2));

    DnieDg14 d;
    CHECK("parse DG14", dnie_dg14_parse(V_DG14, sizeof(V_DG14), &d));
    CHECK("  CA: ECDH + AES-128, v1, keyId 7", d.info_count == 1 && d.infos[0].ka == 2 && d.infos[0].cipher == 2 && d.infos[0].version == 1 && d.infos[0].has_key_id && d.infos[0].key_id == 7);
    CHECK("  clave CA bp256r1, 65 B, keyId 7", d.key_count == 1 && d.keys[0].curve == DnieCurveBrainpoolP256r1 && d.keys[0].public_key_len == 65 && !memcmp(d.keys[0].public_key, V_CA_PUB, 65) && d.keys[0].key_id == 7);
    CHECK("  PACE y TA detectados", d.has_pace && d.has_ta && !d.has_active_auth && d.total_infos == 4);
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
