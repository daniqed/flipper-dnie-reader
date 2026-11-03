// Compara nuestro decodificador de luminancia reducida con OpenJPEG (via Pillow)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "dnie_j2k.h"
static unsigned char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb"); if(!f) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* b = malloc(*n); if(fread(b, 1, *n, f) != *n) { free(b); b = NULL; } fclose(f); return b;
}
int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    const char* jp2 = argc > 1 ? argv[1] : "ref_photo.jp2";
    const char* ref = argc > 2 ? argv[2] : "ref_photo_r2.gray";
    size_t n, rn; unsigned char* data = slurp(jp2, &n); unsigned char* r = slurp(ref, &rn);
    if(!data || !r) { printf("faltan ficheros\n"); return 2; }
    DnieGrayImage img;
    DnieJ2kResult res = dnie_j2k_decode_luma(data, n, 64, &img);
    printf("resultado: %s, %ux%u (reduce %u)\n", dnie_j2k_result_str(res), img.width, img.height, img.reduce);
    if(res != DnieJ2kOk) return 1;
    if(rn != (size_t)img.width * img.height) { printf("tamano distinto a la referencia (%zu)\n", rn); return 1; }
    double se = 0; int maxd = 0; long big = 0;
    for(size_t i = 0; i < rn; i++) { int d = abs((int)img.pixels[i] - (int)r[i]); se += d * d; if(d > maxd) maxd = d; if(d > 4) big++; }
    double mse = se / rn, psnr = mse > 0 ? 10 * log10(255.0 * 255.0 / mse) : 99;
    printf("PSNR vs OpenJPEG: %.2f dB, error max %d, pixeles con error >4: %ld de %zu\n", psnr, maxd, big, rn);
    FILE* f = fopen("/tmp/ours.pgm", "wb"); fprintf(f, "P5 %u %u 255\n", img.width, img.height); fwrite(img.pixels, 1, rn, f); fclose(f);
    f = fopen("/tmp/ref.pgm", "wb"); fprintf(f, "P5 %u %u 255\n", img.width, img.height); fwrite(r, 1, rn, f); fclose(f);
    dnie_gray_free(&img); free(data); free(r);
    return psnr > 40 ? 0 : 1;
}
