#include <stdio.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include "dnie_record.h"
#include "mrz_vectors.h"
static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-38s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)
static void* memmem_(const void* h, size_t hl, const void* n, size_t nl) { for(size_t i = 0; i + nl <= hl; i++) if(!memcmp((const char*)h + i, n, nl)) return (void*)((const char*)h + i); return NULL; }
int main(void) {
    static DnieRecord a, b; static uint8_t buf[DNIE_RECORD_MAX];
    a.kind = DnieRecordFull; a.saved_at = 1793721600u; a.dg1_len = sizeof(V_DG1_ES); memcpy(a.dg1, V_DG1_ES, sizeof(V_DG1_ES));
    a.has_photo = true; a.photo.width = 52; a.photo.height = 64;
    for(int i = 0; i < 52 * 64; i++) a.photo.gray[i] = (uint8_t)(i * 7);
    size_t n = dnie_record_pack(&a, buf, sizeof(buf));
    CHECK("completo: pack", n > 0 && n % 16 == 0);
    CHECK("completo: ida y vuelta", dnie_record_unpack(buf, n, &b) && b.kind == DnieRecordFull && b.saved_at == a.saved_at && b.dg1_len == a.dg1_len && !memcmp(b.dg1, a.dg1, a.dg1_len));
    CHECK("  foto", b.has_photo && b.photo.width == 52 && !memcmp(b.photo.gray, a.photo.gray, 52 * 64));
    buf[20] ^= 1;
    CHECK("byte alterado -> rechazo (SHA)", !dnie_record_unpack(buf, n, &b));
    buf[20] ^= 1; buf[n - 1] = 0;
    CHECK("relleno roto -> rechazo", !dnie_record_unpack(buf, n, &b));

    // Album: solo nombre, apellidos, cumple y foto
    static DnieRecord al; al.kind = DnieRecordAlbum; al.saved_at = 1793721601u;
    strcpy(al.given_names, "FULANO MENGANO"); strcpy(al.surnames, "PRUEBA EJEMPLO"); memcpy(al.birth, "19950101", 8);
    al.has_photo = true; al.photo = a.photo;
    n = dnie_record_pack(&al, buf, sizeof(buf));
    CHECK("album: ida y vuelta", n && dnie_record_unpack(buf, n, &b) && b.kind == DnieRecordAlbum && !strcmp(b.given_names, "FULANO MENGANO") && !strcmp(b.surnames, "PRUEBA EJEMPLO") && !memcmp(b.birth, "19950101", 8) && b.has_photo && b.dg1_len == 0);
    CHECK("album: NO contiene DNI ni MRZ", !memmem_(buf, n, "12345678Z", 9) && !memmem_(buf, n, "IDESP", 5) && !memmem_(buf, n, "CAA123456", 9));
    CHECK("album: mas pequeno que completo", n < dnie_record_pack(&a, (uint8_t[DNIE_RECORD_MAX]){0}, DNIE_RECORD_MAX) + 1);
    strcpy(al.given_names, "NOMBRE MUY LARGO QUE NO CABE EN");   // 31 chars: se recorta a 30
    n = dnie_record_pack(&al, buf, sizeof(buf));
    CHECK("album: nombre largo recortado", n && dnie_record_unpack(buf, n, &b) && strlen(b.given_names) == 30);

    // Formato antiguo (v1, guardado con la v0.9): debe seguir leyendose como completo
    size_t i = 0; uint8_t v1[DNIE_RECORD_MAX];
    memcpy(v1, "DNIE", 4); i = 4; v1[i++] = 1;
    v1[i++] = 0x6A; v1[i++] = 0xEA; v1[i++] = 0x6C; v1[i++] = 0x00;
    v1[i++] = sizeof(V_DG1_ES); memcpy(&v1[i], V_DG1_ES, sizeof(V_DG1_ES)); i += sizeof(V_DG1_ES);
    v1[i++] = 0; v1[i++] = 0;
    mbedtls_sha256(v1, i, &v1[i], 0); i += 32;
    size_t padded = (i / 16 + 1) * 16; memset(&v1[i], (int)(padded - i), padded - i);
    CHECK("v1 (v0.9) sigue leyendose", dnie_record_unpack(v1, padded, &b) && b.kind == DnieRecordFull && b.dg1_len == sizeof(V_DG1_ES) && !b.has_photo && b.saved_at == 0x6AEA6C00u);

    CHECK("tipo desconocido -> 0", (al.kind = 7, dnie_record_pack(&al, buf, sizeof(buf)) == 0));
    CHECK("longitud corta -> rechazo", !dnie_record_unpack(buf, 32, &b));
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
