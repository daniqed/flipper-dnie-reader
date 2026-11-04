#include <stdio.h>
#include <string.h>
#include "dnie_record.h"
int main(void) {
    static DnieRecord a, b; static uint8_t buf[DNIE_RECORD_MAX + 64]; int ok = 1;
    a.kind = DnieRecordAlbum; a.saved_at = 1793750000;
    strcpy(a.given_names, "ANNA"); strcpy(a.surnames, "ERIKSSON"); strcpy(a.birth, "19740812"); strcpy(a.issuer, "USA");
    size_t n = dnie_record_pack(&a, buf, sizeof(buf));
    ok &= n && dnie_record_unpack(buf, n, &b) && b.alias[0] == 0 && !strcmp(b.issuer, "USA");
    strcpy(a.alias, "Juan clase");
    n = dnie_record_pack(&a, buf, sizeof(buf));
    ok &= n && dnie_record_unpack(buf, n, &b) && !strcmp(b.alias, "Juan clase") && !strcmp(b.surnames, "ERIKSSON");
    static DnieRecord f; f.kind = DnieRecordFull; f.dg1_len = 10; memset(f.dg1, 'A', 10); strcpy(f.alias, "Mama");
    f.has_photo = 1; f.photo.width = 4; f.photo.height = 4;
    n = dnie_record_pack(&f, buf, sizeof(buf));
    ok &= n && dnie_record_unpack(buf, n, &b) && !strcmp(b.alias, "Mama") && b.dg1_len == 10 && b.has_photo && b.photo.width == 4;
    // Alias maximo
    memset(a.alias, 'X', DNIE_RECORD_ALIAS_MAX - 1); a.alias[DNIE_RECORD_ALIAS_MAX - 1] = 0;
    n = dnie_record_pack(&a, buf, sizeof(buf));
    ok &= n && dnie_record_unpack(buf, n, &b) && strlen(b.alias) == DNIE_RECORD_ALIAS_MAX - 1;
    printf(ok ? "Alias en lecturas guardadas: OK\n" : "FALLO\n");
    return !ok;
}
