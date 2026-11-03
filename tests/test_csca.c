#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dnie_csca.h"
#include "csca_vectors.h"
static int fails;
static void check(const char* what, const uint8_t* der, size_t len, DnieCscaResult want, const char* name) {
    DerIt it = der_it(der, len); DerTlv cert;
    DnieCscaInfo info;
    DnieCscaResult r = der_expect(&it, 0x30, &cert) ? dnie_csca_verify_cert(&cert, &info) : DnieCscaFormat;
    int ok = r == want && (!name || (info.csca_name && strcmp(info.csca_name, name) == 0));
    printf("%s %s: %s (%s)\n", ok ? "OK  " : "FAIL", what, dnie_csca_result_str(r), info.csca_name ? info.csca_name : "-");
    if(!ok) fails++;
}
int main(void) {
    check("enlace 4 firmado por 3", CERT_LINK_4_BY_3, sizeof(CERT_LINK_4_BY_3), DnieCscaOk, "CSCA SPAIN 3");
    check("autofirmado 2", CERT_SELF_2, sizeof(CERT_SELF_2), DnieCscaOk, "CSCA SPAIN 2");
    check("autofirmado 4", CERT_SELF_4, sizeof(CERT_SELF_4), DnieCscaOk, "CSCA SPAIN 4");
    uint8_t* t = malloc(sizeof(CERT_LINK_4_BY_3)); memcpy(t, CERT_LINK_4_BY_3, sizeof(CERT_LINK_4_BY_3));
    t[300] ^= 1; // tocar el tbsCertificate (clave publica)
    check("enlace manipulado", t, sizeof(CERT_LINK_4_BY_3), DnieCscaBadSignature, "CSCA SPAIN 3");
    memcpy(t, CERT_LINK_4_BY_3, sizeof(CERT_LINK_4_BY_3));
    // cambiar el numero de serie del emisor (3 -> 9) en el campo issuer: nombre desconocido
    for(size_t i = 0; i + 6 < sizeof(CERT_LINK_4_BY_3); i++)
        if(!memcmp(t + i, "\x06\x03\x55\x04\x05\x13\x01" "3", 8)) { t[i + 7] = '9'; break; }
    check("emisor desconocido", t, sizeof(CERT_LINK_4_BY_3), DnieCscaUnknownIssuer, NULL);
    free(t);
    printf(fails ? "%d FALLOS\n" : "Todo OK\n", fails);
    return fails != 0;
}
