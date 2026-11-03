// Prueba en PC de la matematica de PACE contra ICAO 9303-11 G.1 y RFC 4493
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dnie_crypto.h"
#include "dnie_pace_math.h"

static int fails = 0;
static void unhex(const char* h, uint8_t* o) { for(size_t i = 0; h[2 * i]; i++) sscanf(h + 2 * i, "%2hhx", &o[i]); }
static void check(const char* name, const uint8_t* got, const char* exp_hex, size_t n) {
    uint8_t e[128]; unhex(exp_hex, e);
    int ok = memcmp(got, e, n) == 0; if(!ok) fails++;
    printf("%-14s %s\n", name, ok ? "OK" : "FALLA");
    if(!ok) { printf("  got "); for(size_t i = 0; i < n; i++) printf("%02X", got[i]); printf("\n"); }
}
static int rng(void* c, unsigned char* b, size_t n) { (void)c; for(size_t i = 0; i < n; i++) b[i] = rand(); return 0; }
static void pt(const char* x, const char* y, uint8_t out[65]) { out[0] = 4; unhex(x, out + 1); unhex(y, out + 33); }

int main(void) {
    uint8_t mac[16], key[16], msg[16];
    // RFC 4493: CMAC vacio y de 16 bytes
    unhex("2B7E151628AED2A6ABF7158809CF4F3C", key);
    dnie_aes128_cmac(key, NULL, 0, mac); check("CMAC len 0", mac, "BB1D6929E95937287FA37D129B756746", 16);
    unhex("6BC1BEE22E409F96E93D7E117393172A", msg);
    dnie_aes128_cmac(key, msg, 16, mac); check("CMAC len 16", mac, "070A16B46B4D4144F79BDD9DD04A287C", 16);
    uint8_t msg40[40]; unhex("6BC1BEE22E409F96E93D7E117393172AAE2D8A571E03AC9C9EB76FAC45AF8E5130C81C46A35CE411", msg40);
    dnie_aes128_cmac(key, msg40, 40, mac); check("CMAC len 40", mac, "DFA66747DE9AE63030CA32611497C827", 16);

    mbedtls_ecp_group grp; mbedtls_ecp_group_init(&grp);
    if(dnie_ec_load_bp256r1(&grp)) { printf("load FALLA\n"); return 1; }
    mbedtls_ecp_point G_chk; mbedtls_ecp_point_init(&G_chk);
    printf("%-14s %s\n", "G valido", mbedtls_ecp_check_pubkey(&grp, &grp.G) == 0 ? "OK" : (fails++, "FALLA"));

    uint8_t s[16]; unhex("3F00C4D39D153F2B2A214A078D899B22", s);
    mbedtls_mpi sk_map, sk_eph; mbedtls_mpi_init(&sk_map); mbedtls_mpi_init(&sk_eph);
    mbedtls_ecp_point pk_map, pk_map_chip, gt, pk_eph, pk_chip;
    mbedtls_ecp_point_init(&pk_map); mbedtls_ecp_point_init(&pk_map_chip); mbedtls_ecp_point_init(&gt);
    mbedtls_ecp_point_init(&pk_eph); mbedtls_ecp_point_init(&pk_chip);
    uint8_t buf[65];

    // 1. Par de claves del mapeo (nuestro)
    mbedtls_mpi_read_string(&sk_map, 16, "7F4EF07B9EA82FD78AD689B38D0BC78CF21F249D953BC46F4C6E19259C010F99");
    dnie_pace_keypair(&grp, &grp.G, &sk_map, &pk_map, false, rng, NULL);
    dnie_ec_point_write(&grp, &pk_map, buf);
    check("PK_map PCD", buf, "047ACF3EFC982EC45565A4B155129EFBC74650DCBFA6362D896FC70262E0C2CC5E544552DCB6725218799115B55C9BAA6D9F6BC3A9618E70C25AF71777A9C4922D", 65);

    // 2. Clave de mapeo del chip (llega por radio) y G~
    pt("824FBA91C9CBE26BEF53A0EBE7342A3BF178CEA9F45DE0B70AA601651FBA3F57", "30D8C879AAA9C9F73991E61B58F4D52EB87A0A0C709A49DC63719363CCD13C54", buf);
    printf("%-14s %s\n", "read+check pk", dnie_ec_point_read(&grp, &pk_map_chip, buf, 65) == 0 ? "OK" : (fails++, "FALLA"));
    dnie_pace_map_gm(&grp, s, 16, &sk_map, &pk_map_chip, &gt, rng, NULL);
    dnie_ec_point_write(&grp, &gt, buf);
    check("G~", buf, "048CED63C91426D4F0EB1435E7CB1D74A46723A0AF21C89634F65A9AE87A9265E28C879506743F8611AC33645C5B985C80B5F09A0B83407C1B6A4D857AE76FE522", 65);

    // 3. Par efimero sobre G~
    mbedtls_mpi_read_string(&sk_eph, 16, "A73FB703AC1436A18E0CFA5ABB3F7BEC7A070E7A6788486BEE230C4A22762595");
    dnie_pace_keypair(&grp, &gt, &sk_eph, &pk_eph, false, rng, NULL);
    uint8_t pk_pcd[65]; dnie_ec_point_write(&grp, &pk_eph, pk_pcd);
    check("PK PCD", pk_pcd, "042DB7A64C0355044EC9DF190514C625CBA2CEA48754887122F3A5EF0D5EDD301C3556F3B3B186DF10B857B58F6A7EB80F20BA5DC7BE1D43D9BF850149FBB36462", 65);

    // 4. Secreto compartido y claves de sesion
    uint8_t pk_picc[65];
    pt("9E880F842905B8B3181F7AF7CAA9F0EFB743847F44A306D2D28C1D9EC65DF6DB", "7764B22277A2EDDC3C265A9F018F9CB852E111B768B326904B59A0193776F094", pk_picc);
    dnie_ec_point_read(&grp, &pk_chip, pk_picc, 65);
    uint8_t k[32], ksenc[16], ksmac[16];
    dnie_pace_shared_x(&grp, &sk_eph, &pk_chip, k, rng, NULL);
    check("K", k, "28768D20701247DAE81804C9E780EDE582A9996DB4A315020B2733197DB84925", 32);
    dnie_pace_derive_keys(k, ksenc, ksmac);
    check("KSenc", ksenc, "F5F0E35C0D7161EE6724EE513A0D9A7F", 16);
    check("KSmac", ksmac, "FE251C7858B356B24514B3BD5F4297D1", 16);

    // 5. Tokens
    uint8_t oid[10]; unhex("04007F00070202040202", oid);
    uint8_t t[8];
    dnie_pace_token(ksmac, oid, pk_picc, t); check("T_PCD", t, "C2B0BD78D94BA866", 8);
    dnie_pace_token(ksmac, oid, pk_pcd, t);  check("T_PICC", t, "3ABB9674BCE93C08", 8);

    // 6. Robustez: punto fuera de la curva rechazado
    pk_picc[64] ^= 1;
    printf("%-14s %s\n", "punto invalido", dnie_ec_point_read(&grp, &pk_chip, pk_picc, 65) != 0 ? "OK (rechazado)" : (fails++, "FALLA"));

    // 7. Con claves aleatorias el ECDH es simetrico
    mbedtls_mpi a, b; mbedtls_ecp_point A, B; mbedtls_mpi_init(&a); mbedtls_mpi_init(&b); mbedtls_ecp_point_init(&A); mbedtls_ecp_point_init(&B);
    dnie_pace_keypair(&grp, &gt, &a, &A, true, rng, NULL); dnie_pace_keypair(&grp, &gt, &b, &B, true, rng, NULL);
    uint8_t k1[32], k2[32]; dnie_pace_shared_x(&grp, &a, &B, k1, rng, NULL); dnie_pace_shared_x(&grp, &b, &A, k2, rng, NULL);
    printf("%-14s %s\n", "ECDH aleatorio", memcmp(k1, k2, 32) == 0 ? "OK" : (fails++, "FALLA"));

    mbedtls_mpi_free(&sk_map); mbedtls_mpi_free(&sk_eph); mbedtls_mpi_free(&a); mbedtls_mpi_free(&b);
    mbedtls_ecp_point_free(&pk_map); mbedtls_ecp_point_free(&pk_map_chip); mbedtls_ecp_point_free(&gt);
    mbedtls_ecp_point_free(&pk_eph); mbedtls_ecp_point_free(&pk_chip); mbedtls_ecp_point_free(&A);
    mbedtls_ecp_point_free(&B); mbedtls_ecp_point_free(&G_chk);
    mbedtls_ecp_group_free(&grp);
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
