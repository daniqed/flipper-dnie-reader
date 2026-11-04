// Cadena CSCA de otros paises: cada certificado CSCA (autofirmado o enlace) de los paises
// principales se verifica contra csca.bin. Certificados reales de la Master List del BSI.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dnie_csca.h"
static size_t rd(void* ctx, uint8_t* b, size_t n) { return fread(b, 1, n, (FILE*)ctx); }
int main(void) {
    FILE* certs = fopen("top_certs.bin", "rb");
    uint8_t hdr[2]; static uint8_t der[4096];
    int ok = 0, unsup = 0, bad = 0, total = 0, tamper_fail = 0, pss_ok = 0;
    while(fread(hdr, 1, 2, certs) == 2) {
        size_t len = hdr[0] << 8 | hdr[1]; fread(der, 1, len, certs);
        DerIt it = der_it(der, len); DerTlv cert; der_expect(&it, 0x30, &cert);
        FILE* f = fopen("csca.bin", "rb"); DnieCscaInfo info;
        DnieCscaResult r = dnie_csca_verify_cert(&cert, rd, f, &info); fclose(f);
        total++;
        if(r == DnieCscaOk) {
            ok++;
            if(memmem(der, len, "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0A", 11)) pss_ok++;
            // Manipular un byte del tbs: tiene que fallar
            der[len / 2] ^= 1;
            it = der_it(der, len); der_expect(&it, 0x30, &cert);
            f = fopen("csca.bin", "rb");
            DnieCscaResult r2 = dnie_csca_verify_cert(&cert, rd, f, &info); fclose(f);
            if(r2 == DnieCscaOk) tamper_fail++;
        } else if(r == DnieCscaUnsupported) unsup++;
        else { bad++; printf("  resultado %s (%s)\n", dnie_csca_result_str(r), info.csca_name); }
    }
    printf("%d certificados: %d OK (%d con RSA-PSS), %d alg. no soportado (ECDSA/SHA-512), %d otros\n", total, ok, pss_ok, unsup, bad);
    printf("manipulados aceptados: %d\n", tamper_fail);
    int fail = ok < 30 || pss_ok < 5 || tamper_fail;
    printf(fail ? "FALLO\n" : "Todo OK\n");
    return fail;
}
