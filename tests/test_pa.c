#include <stdio.h>
#include <string.h>
#include "dnie_pa.h"
#include "sod_vectors.h"
static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-40s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)
int main(void) {
    static DnieSod sod; DniePaInfo pi;
    CHECK("SOD RSA (SHA-1, como el DNIe): parse", dnie_sod_parse(V_SOD_RSA_SHA1, sizeof(V_SOD_RSA_SHA1), &sod));
    CHECK("  alg: RSA (PKCS#1), hash LDS SHA-1", !strcmp(sod.sig_alg, "RSA (PKCS#1)") && sod.lds_hash == DnieHashSha1);
    CHECK("  firma VALIDA, 2048 bits", dnie_pa_verify_sod(&sod, &pi) == DniePaOk && pi.key_bits == 2048 && pi.hash == DnieHashSha1);
    CHECK("  messageDigest OK", dnie_sod_check_message_digest(&sod));
    CHECK("SOD RSA-SHA256: firma VALIDA", dnie_sod_parse(V_SOD_RSA_SHA256, sizeof(V_SOD_RSA_SHA256), &sod) && dnie_pa_verify_sod(&sod, &pi) == DniePaOk && pi.hash == DnieHashSha256);
    CHECK("firma alterada -> NO VALIDA", dnie_sod_parse(V_SOD_RSA_BAD, sizeof(V_SOD_RSA_BAD), &sod) && dnie_pa_verify_sod(&sod, &pi) == DniePaBadSignature);
    // signedAttrs alterados (un bit del messageDigest firmado) -> la firma ya no vale
    static uint8_t t[sizeof(V_SOD_RSA_SHA1)]; memcpy(t, V_SOD_RSA_SHA1, sizeof(t));
    dnie_sod_parse(t, sizeof(t), &sod);
    size_t off = (size_t)(sod.signed_attrs.val - t) + sod.signed_attrs.len - 3; t[off] ^= 0x01;
    CHECK("signedAttrs alterados -> NO VALIDA", dnie_sod_parse(t, sizeof(t), &sod) && dnie_pa_verify_sod(&sod, &pi) == DniePaBadSignature);
    CHECK("SOD ECDSA -> no soportado (no falso OK)", dnie_sod_parse(V_SOD, sizeof(V_SOD), &sod) && dnie_pa_verify_sod(&sod, &pi) == DniePaUnsupported);
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
