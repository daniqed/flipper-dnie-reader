#include <stdio.h>
#include <string.h>
#include "dnie_record.h"
int main(void) {
    static DnieRecord a, b; static uint8_t buf[DNIE_RECORD_MAX + 64];
    a.kind = DnieRecordAlbum; a.saved_at = 1793750000;
    strcpy(a.given_names, "ANNA MARIA"); strcpy(a.surnames, "ERIKSSON"); strcpy(a.birth, "19740812"); strcpy(a.issuer, "USA");
    size_t n = dnie_record_pack(&a, buf, sizeof(buf));
    int ok = n && dnie_record_unpack(buf, n, &b) && !strcmp(b.issuer, "USA") && !strcmp(b.birth, "19740812") && !strcmp(b.surnames, "ERIKSSON");
    strcpy(a.issuer, "D");
    n = dnie_record_pack(&a, buf, sizeof(buf));
    ok = ok && n && dnie_record_unpack(buf, n, &b) && !strcmp(b.issuer, "D");
    printf(ok ? "Album con pais: OK\n" : "FALLO\n");
    return !ok;
}
