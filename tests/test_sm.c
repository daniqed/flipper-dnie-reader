// Prueba de Secure Messaging contra una implementacion Python independiente (sm_vectors.h)
#include <stdio.h>
#include <string.h>
#include "dnie_sm.h"
#include "dnie_crypto.h"
#include "sm_vectors.h"

static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-28s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)

static void dump(const char* n, const uint8_t* b, size_t l) { printf("  %s: ", n); for(size_t i = 0; i < l; i++) printf("%02X", b[i]); printf("\n"); }

int main(void) {
    const uint8_t ksenc[16] = {0xF5,0xF0,0xE3,0x5C,0x0D,0x71,0x61,0xEE,0x67,0x24,0xEE,0x51,0x3A,0x0D,0x9A,0x7F};
    const uint8_t ksmac[16] = {0xFE,0x25,0x1C,0x78,0x58,0xB3,0x56,0xB2,0x45,0x14,0xB3,0xBD,0x5F,0x42,0x97,0xD1};
    DnieSm sm; dnie_sm_init(&sm, ksenc, ksmac);
    uint8_t apdu[300], data[300]; size_t n, dlen; uint16_t sw;

    // SELECT AID eMRTD
    const uint8_t h_sel_app[4] = {0x00, 0xA4, 0x04, 0x0C}, aid[] = {0xA0,0x00,0x00,0x02,0x47,0x10,0x01};
    n = dnie_sm_wrap(&sm, h_sel_app, aid, sizeof(aid), -1, apdu, sizeof(apdu));
    CHECK("wrap SELECT AID", n == sizeof(V_A1) && !memcmp(apdu, V_A1, n));
    if(n != sizeof(V_A1) || memcmp(apdu, V_A1, n)) { dump("C ", apdu, n); dump("Py", V_A1, sizeof(V_A1)); }
    CHECK("unwrap resp SELECT AID", dnie_sm_unwrap(&sm, V_R1, sizeof(V_R1), data, sizeof(data), &dlen, &sw) == DnieSmOk && sw == 0x9000 && dlen == 0);

    // SELECT DG1
    const uint8_t h_sel_ef[4] = {0x00, 0xA4, 0x02, 0x0C}, fid[] = {0x01, 0x01};
    n = dnie_sm_wrap(&sm, h_sel_ef, fid, sizeof(fid), -1, apdu, sizeof(apdu));
    CHECK("wrap SELECT DG1", n == sizeof(V_A2) && !memcmp(apdu, V_A2, n));
    CHECK("unwrap resp SELECT DG1", dnie_sm_unwrap(&sm, V_R2, sizeof(V_R2), data, sizeof(data), &dlen, &sw) == DnieSmOk && sw == 0x9000);

    // READ BINARY 4 bytes
    const uint8_t h_rb0[4] = {0x00, 0xB0, 0x00, 0x00};
    n = dnie_sm_wrap(&sm, h_rb0, NULL, 0, 4, apdu, sizeof(apdu));
    CHECK("wrap READ BINARY (Le=4)", n == sizeof(V_A3) && !memcmp(apdu, V_A3, n));
    CHECK("unwrap 4 bytes de DG1", dnie_sm_unwrap(&sm, V_R3, sizeof(V_R3), data, sizeof(data), &dlen, &sw) == DnieSmOk && dlen == 4 && !memcmp(data, V_DG1, 4));

    // READ BINARY resto
    const uint8_t h_rb4[4] = {0x00, 0xB0, 0x00, 0x04};
    n = dnie_sm_wrap(&sm, h_rb4, NULL, 0, sizeof(V_DG1) - 4, apdu, sizeof(apdu));
    CHECK("wrap READ BINARY (resto)", n == sizeof(V_A4) && !memcmp(apdu, V_A4, n));
    DnieSm before = sm;
    CHECK("unwrap resto de DG1", dnie_sm_unwrap(&sm, V_R4, sizeof(V_R4), data, sizeof(data), &dlen, &sw) == DnieSmOk && dlen == sizeof(V_DG1) - 4 && !memcmp(data, V_DG1 + 4, dlen));

    // Respuesta manipulada: un bit cambiado en el criptograma -> MAC incorrecto
    uint8_t bad[sizeof(V_R4)]; memcpy(bad, V_R4, sizeof(bad)); bad[10] ^= 0x01;
    DnieSm tmp = before;
    CHECK("respuesta manipulada -> MAC", dnie_sm_unwrap(&tmp, bad, sizeof(bad), data, sizeof(data), &dlen, &sw) == DnieSmErrorMac);
    // SSC desincronizado -> MAC incorrecto
    tmp = before; tmp.ssc[15] ^= 1;
    CHECK("SSC desincronizado -> MAC", dnie_sm_unwrap(&tmp, V_R4, sizeof(V_R4), data, sizeof(data), &dlen, &sw) == DnieSmErrorMac);

    // Error protegido 6A82 (fichero no encontrado) tras otro comando
    n = dnie_sm_wrap(&sm, h_sel_ef, fid, sizeof(fid), -1, apdu, sizeof(apdu));
    CHECK("error protegido 6A82", dnie_sm_unwrap(&sm, V_R5, sizeof(V_R5), data, sizeof(data), &dlen, &sw) == DnieSmOk && sw == 0x6A82);

    // SW plano (error de SM del chip)
    const uint8_t plain[] = {0x69, 0x88};
    CHECK("SW plano 6988", dnie_sm_unwrap(&sm, plain, 2, data, sizeof(data), &dlen, &sw) == DnieSmErrorPlainStatus && sw == 0x6988);

    // ===== 3DES: ejemplo resuelto de ICAO 9303-11, apendice D =====
    {
        uint8_t k[16], exp[16];
        const uint8_t seed[16] = {0x23,0x9A,0xB9,0xCB,0x28,0x2D,0xAF,0x66,0x23,0x1D,0xC5,0xA4,0xDF,0x6B,0xFB,0xAE};
        dnie_kdf_3des(seed, 16, 1, k);
        const uint8_t kenc[16] = {0xAB,0x94,0xFD,0xEC,0xF2,0x67,0x4F,0xDF,0xB9,0xB3,0x91,0xF8,0x5D,0x7F,0x76,0xF2};
        CHECK("3DES: KDF Kenc (con paridad)", !memcmp(k, kenc, 16));
        dnie_kdf_3des(seed, 16, 2, k);
        const uint8_t kmac[16] = {0x79,0x62,0xD9,0xEC,0xE0,0x3D,0x1A,0xCD,0x4C,0x76,0x08,0x9D,0xCE,0x13,0x15,0x43};
        CHECK("3DES: KDF Kmac (con paridad)", !memcmp(k, kmac, 16));
        (void)exp;

        const uint8_t ksenc[16] = {0x97,0x9E,0xC1,0x3B,0x1C,0xBF,0xE9,0xDC,0xD0,0x1A,0xB0,0xFE,0xD3,0x07,0xEA,0xE5};
        const uint8_t ksmac[16] = {0xF1,0xCB,0x1F,0x1F,0xB5,0xAD,0xF2,0x08,0x80,0x6B,0x89,0xDC,0x57,0x9D,0xC1,0xF8};
        const uint8_t ssc[8] = {0x88,0x70,0x22,0x12,0x0C,0x06,0xC2,0x26};
        DnieSm s3; dnie_sm_init_3des(&s3, ksenc, ksmac, ssc);
        const uint8_t sel[4] = {0x00,0xA4,0x02,0x0C}, ef_com[2] = {0x01,0x1E};
        const uint8_t sel_exp[] = {0x0C,0xA4,0x02,0x0C,0x15,0x87,0x09,0x01,0x63,0x75,0x43,0x29,0x08,0xC0,0x44,0xF6,0x8E,0x08,0xBF,0x8B,0x92,0xD6,0x35,0xFF,0x24,0xF8,0x00};
        n = dnie_sm_wrap(&s3, sel, ef_com, 2, -1, apdu, sizeof(apdu));
        CHECK("3DES: SELECT EF.COM (ICAO D.4)", n == sizeof(sel_exp) && !memcmp(apdu, sel_exp, n));
        const uint8_t r1[] = {0x99,0x02,0x90,0x00,0x8E,0x08,0xFA,0x85,0x5A,0x5D,0x4C,0x50,0xA8,0xED,0x90,0x00};
        CHECK("3DES: respuesta SELECT", dnie_sm_unwrap(&s3, r1, sizeof(r1), data, sizeof(data), &dlen, &sw) == DnieSmOk && sw == 0x9000);
        const uint8_t rb[4] = {0x00,0xB0,0x00,0x00};
        const uint8_t rb_exp[] = {0x0C,0xB0,0x00,0x00,0x0D,0x97,0x01,0x04,0x8E,0x08,0xED,0x67,0x05,0x41,0x7E,0x96,0xBA,0x55,0x00};
        n = dnie_sm_wrap(&s3, rb, NULL, 0, 4, apdu, sizeof(apdu));
        CHECK("3DES: READ BINARY (ICAO D.4)", n == sizeof(rb_exp) && !memcmp(apdu, rb_exp, n));
        const uint8_t r2[] = {0x87,0x09,0x01,0x9F,0xF0,0xEC,0x34,0xF9,0x92,0x26,0x51,0x99,0x02,0x90,0x00,0x8E,0x08,0xAD,0x55,0xCC,0x17,0x14,0x0B,0x2D,0xED,0x90,0x00};
        const uint8_t plain_exp[4] = {0x60,0x14,0x5F,0x01};
        CHECK("3DES: respuesta READ descifrada", dnie_sm_unwrap(&s3, r2, sizeof(r2), data, sizeof(data), &dlen, &sw) == DnieSmOk && dlen == 4 && !memcmp(data, plain_exp, 4));
        DnieSm s4; dnie_sm_init_3des(&s4, ksenc, ksmac, ssc);
        n = dnie_sm_wrap(&s4, sel, ef_com, 2, -1, apdu, sizeof(apdu));
        uint8_t bad3[sizeof(r1)]; memcpy(bad3, r1, sizeof(r1)); bad3[8] ^= 1;
        CHECK("3DES: MAC manipulado -> rechazo", dnie_sm_unwrap(&s4, bad3, sizeof(bad3), data, sizeof(data), &dlen, &sw) == DnieSmErrorMac);
    }

    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
