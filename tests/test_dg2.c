#include <stdio.h>
#include <string.h>
#include "dnie_dg2.h"
#include "dg2_vectors.h"
static int fails = 0;
#define CHECK(name, cond) do { int _ok = (cond); if(!_ok) fails++; printf("%-34s %s\n", name, _ok ? "OK" : "FALLA"); } while(0)
#define CASE(N, comps, lv, cbw, xf, prog) do { \
    DnieDg2Info d; DnieJ2kInfo j; \
    CHECK(#N ": encuentra imagen", dnie_dg2_find_image(V_##N, DNIE_DG2_HEAD_LEN, V_##N##_TOTAL, &d)); \
    CHECK(#N ": offset/longitud", d.image_offset == V_##N##_IMG_OFF && d.image_len == V_##N##_IMG_LEN); \
    CHECK(#N ": tipo JPEG2000 y tamano", d.image_type == 1 && d.width == V_##N##_W && d.height == V_##N##_H); \
    CHECK(#N ": cabecera J2K", dnie_j2k_parse_header(V_##N + d.image_offset, sizeof(V_##N) - d.image_offset, &j)); \
    CHECK(#N ": contenedor", j.jp2_container == V_##N##_JP2); \
    CHECK(#N ": SIZ", j.width == V_##N##_W && j.height == V_##N##_H && j.components == comps && j.depth == 8); \
    CHECK(#N ": COD", j.levels == lv && j.cb_w == cbw && j.transform == xf && j.progression == prog); \
    printf("   -> %ux%u comp=%u niv=%u cb=%ux%u xf=%u prog=%s capas=%u qcd=%u tiles=%ux%u\n", j.width, j.height, j.components, j.levels, 1u<<j.cb_w, 1u<<j.cb_h, j.transform, dnie_j2k_progression_str(j.progression), j.layers, j.qcd_style, j.tile_w, j.tile_h); \
} while(0)
int main(void) {
    CASE(JP2_RGB, 3, 5, 6, 0, 0);   // irreversible 9/7, 6 resoluciones = 5 niveles, CB 64
    CASE(J2K_GRAY, 1, 4, 5, 1, 2);  // reversible 5/3, 5 resoluciones = 4 niveles, CB 32, RPCL
    DnieDg2Info d;
    CHECK("DG2 truncado -> rechazo", !dnie_dg2_find_image(V_JP2_RGB, 20, V_JP2_RGB_TOTAL, &d));
    uint8_t bad[sizeof(V_JP2_RGB)]; memcpy(bad, V_JP2_RGB, sizeof(bad)); bad[0] = 0x76;
    CHECK("tag incorrecto -> rechazo", !dnie_dg2_find_image(bad, DNIE_DG2_HEAD_LEN, V_JP2_RGB_TOTAL, &d));
    CHECK("total incoherente -> rechazo", !dnie_dg2_find_image(V_JP2_RGB, DNIE_DG2_HEAD_LEN, 100, &d));
    printf("\n%s (%d fallos)\n", fails ? "HAY FALLOS" : "TODO OK", fails);
    return fails != 0;
}
