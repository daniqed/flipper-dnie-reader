#include <stdio.h>
#include <string.h>
#include "dnie_mrz.h"
#include "sm_vectors.h"
#include "mrz_vectors.h"
static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-30s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)
int main(void) {
    DnieMrz m;
    CHECK("parse especimen ICAO", dnie_mrz_parse_dg1(V_DG1, sizeof(V_DG1), &m));
    CHECK("  apellidos", !strcmp(m.surnames, "ERIKSSON"));
    CHECK("  nombre", !strcmp(m.given_names, "ANNA MARIA"));
    CHECK("  n documento", !strcmp(m.doc_number, "D23145890"));
    CHECK("  nacimiento/sexo/cad", !strcmp(m.birth, "740812") && m.sex == 'F' && !strcmp(m.expiry, "120415"));
    CHECK("  nacionalidad", !strcmp(m.nationality, "UTO"));
    CHECK("  4 digitos de control", dnie_mrz_all_checks_ok(&m));

    CHECK("parse MRZ estilo DNIe", dnie_mrz_parse_dg1(V_DG1_ES, sizeof(V_DG1_ES), &m));
    CHECK("  DNI (opcional linea 1)", !strcmp(m.personal_number, "12345678Z"));
    CHECK("  soporte", !strcmp(m.doc_number, "CAA123456"));
    CHECK("  dos apellidos", !strcmp(m.surnames, "PRUEBA EJEMPLO"));
    CHECK("  nombre compuesto", !strcmp(m.given_names, "FULANO MENGANO"));
    CHECK("  digitos de control", dnie_mrz_all_checks_ok(&m));

    uint8_t bad[sizeof(V_DG1_ES)]; memcpy(bad, V_DG1_ES, sizeof(bad));
    bad[5 + 30 + 1] = '6'; // cambiar un digito de la fecha de nacimiento
    dnie_mrz_parse_dg1(bad, sizeof(bad), &m);
    CHECK("MRZ alterada -> control falla", !m.check_birth && !m.check_composite && m.check_expiry);
    bad[5] = 'i';
    CHECK("caracter ilegal -> rechazo", !dnie_mrz_parse_dg1(bad, sizeof(bad), &m));
    CHECK("DG1 truncado -> rechazo", !dnie_mrz_parse_dg1(V_DG1, 50, &m));
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
