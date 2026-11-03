// Chip Authentication: el "terminal" (nosotros) y un chip simulado llegan a las mismas claves
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dnie_pace_math.h"
#include "dnie_crypto.h"
#include "dnie_sm.h"
static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-42s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)
static int rng(void* c, unsigned char* b, size_t n) { (void)c; for(size_t i = 0; i < n; i++) b[i] = rand(); return 0; }
int main(void) {
    mbedtls_ecp_group g; mbedtls_ecp_group_init(&g); dnie_ec_load_bp256r1(&g);
    mbedtls_mpi skc, skt, skx; mbedtls_ecp_point pkc, pkt, pkx;
    mbedtls_mpi_init(&skc); mbedtls_mpi_init(&skt); mbedtls_mpi_init(&skx);
    mbedtls_ecp_point_init(&pkc); mbedtls_ecp_point_init(&pkt); mbedtls_ecp_point_init(&pkx);
    dnie_pace_keypair(&g, &g.G, &skc, &pkc, true, rng, NULL);   // clave estatica del chip (DG14)
    dnie_pace_keypair(&g, &g.G, &skt, &pkt, true, rng, NULL);   // efimera del terminal
    uint8_t buf[65], kt[32], kc[32], kx[32];
    CHECK("PK del chip codificable (65 B)", dnie_ec_point_write(&g, &pkc, buf) == 0 && buf[0] == 4);
    mbedtls_ecp_point back; mbedtls_ecp_point_init(&back);
    CHECK("PK del chip valida en la curva", dnie_ec_point_read(&g, &back, buf, 65) == 0);
    dnie_pace_shared_x(&g, &skt, &pkc, kt, rng, NULL);         // terminal: sk_t * PK_chip
    dnie_pace_shared_x(&g, &skc, &pkt, kc, rng, NULL);         // chip: sk_chip * PK_t
    CHECK("secreto K igual en ambos lados", !memcmp(kt, kc, 32));
    uint8_t te[16], tm[16], ce[16], cm[16];
    dnie_kdf_3des(kt, 32, 1, te); dnie_kdf_3des(kt, 32, 2, tm);
    dnie_kdf_3des(kc, 32, 1, ce); dnie_kdf_3des(kc, 32, 2, cm);
    CHECK("KSenc/KSmac 3DES iguales", !memcmp(te, ce, 16) && !memcmp(tm, cm, 16));
    // Un "clon" sin la clave privada usa otra: el MAC de su respuesta no puede cuadrar
    dnie_pace_keypair(&g, &g.G, &skx, &pkx, true, rng, NULL);
    dnie_pace_shared_x(&g, &skx, &pkt, kx, rng, NULL);
    CHECK("clon: secreto distinto", memcmp(kx, kt, 32) != 0);
    // Respuesta "99 02 90 00 8E 08 MAC 90 00" calculada por el clon con sus claves
    uint8_t xe[16], xm[16]; dnie_kdf_3des(kx, 32, 1, xe); dnie_kdf_3des(kx, 32, 2, xm);
    DnieSm term; dnie_sm_init_3des(&term, te, tm, NULL);
    uint8_t apdu[64]; const uint8_t sel[4] = {0, 0xA4, 2, 0x0C}, fid[2] = {1, 1};
    dnie_sm_wrap(&term, sel, fid, 2, -1, apdu, sizeof(apdu));       // SSC=1
    uint8_t mac_in[16] = {0,0,0,0,0,0,0,2, 0x99,0x02,0x90,0x00, 0x80,0,0,0}; // SSC=2 || DO99 || relleno
    uint8_t resp[16] = {0x99,0x02,0x90,0x00,0x8E,0x08}; resp[14] = 0x90; resp[15] = 0x00;
    dnie_retail_mac(xm, mac_in, 16, &resp[6]);
    uint8_t out[16]; size_t ol; uint16_t sw;
    CHECK("clon: el canal nuevo lo rechaza (MAC)", dnie_sm_unwrap(&term, resp, 16, out, sizeof(out), &ol, &sw) == DnieSmErrorMac);
    // El chip autentico, con las claves buenas, si pasa
    DnieSm term2; dnie_sm_init_3des(&term2, te, tm, NULL);
    dnie_sm_wrap(&term2, sel, fid, 2, -1, apdu, sizeof(apdu));
    dnie_retail_mac(cm, mac_in, 16, &resp[6]);
    CHECK("chip autentico: el canal nuevo funciona", dnie_sm_unwrap(&term2, resp, 16, out, sizeof(out), &ol, &sw) == DnieSmOk && sw == 0x9000);
    mbedtls_mpi_free(&skc); mbedtls_mpi_free(&skt); mbedtls_mpi_free(&skx);
    mbedtls_ecp_point_free(&pkc); mbedtls_ecp_point_free(&pkt); mbedtls_ecp_point_free(&pkx); mbedtls_ecp_point_free(&back);
    mbedtls_ecp_group_free(&g);
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
