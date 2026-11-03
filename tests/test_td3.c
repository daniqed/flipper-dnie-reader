#include <stdio.h>
#include <string.h>
#include "dnie_mrz.h"
int main(void) {
    const char* m = "P<UTOERIKSSON<<ANNA<MARIA<<<<<<<<<<<<<<<<<<<"
                    "L898902C36UTO7408122F1204159ZE184226B<<<<<10";
    uint8_t dg1[100] = {0x61, 0x5B, 0x5F, 0x1F, 0x58};
    memcpy(dg1 + 5, m, 88);
    DnieMrz z; int f = 0;
    bool ok = dnie_mrz_parse_dg1(dg1, 93, &z);
    printf("parse %d | %s | %s | doc %s | nac %s | sexo %c | cad %s | pers %s | %s\n", ok, z.surnames, z.given_names, z.doc_number, z.birth, z.sex, z.expiry, z.personal_number, z.nationality);
    f += !ok || strcmp(z.surnames, "ERIKSSON") || strcmp(z.given_names, "ANNA MARIA") || strcmp(z.doc_number, "L898902C3") ||
         strcmp(z.birth, "740812") || z.sex != 'F' || strcmp(z.expiry, "120415") || strcmp(z.personal_number, "ZE184226B") ||
         !dnie_mrz_all_checks_ok(&z);
    printf("checks %d %d %d %d\n", z.check_doc_number, z.check_birth, z.check_expiry, z.check_composite);
    dg1[5 + 44 + 2] = '7'; // tocar el numero de documento
    dnie_mrz_parse_dg1(dg1, 93, &z);
    printf("tras tocar: %d %d\n", z.check_doc_number, z.check_composite);
    f += z.check_doc_number || z.check_composite;
    printf(f ? "FALLO\n" : "Todo OK\n");
    return f;
}
