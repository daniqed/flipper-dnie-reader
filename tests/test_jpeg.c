#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "dnie_jpeg.h"
static size_t rd(void* ctx, uint8_t* buf, size_t max) { return fread(buf, 1, max, (FILE*)ctx); }
static int fails;
static void one(const char* name, int expect_ok) {
    char p[64]; snprintf(p, sizeof p, "%s.jpg", name);
    FILE* f = fopen(p, "rb"); DnieGrayImage g;
    DnieJ2kResult r = dnie_jpeg_decode_dc(rd, f, &g); fclose(f);
    if(!expect_ok) { printf("%s %s: resultado %d (no soportado esperado)\n", r == DnieJ2kErrorUnsupported ? "OK  " : "FAIL", name, r); fails += r != DnieJ2kErrorUnsupported; return; }
    snprintf(p, sizeof p, "%s.gray", name);
    f = fopen(p, "rb"); uint8_t hd[4]; fread(hd, 1, 4, f);
    int w = hd[0] | hd[1] << 8, h = hd[2] | hd[3] << 8;
    uint8_t* ref = malloc(w * h); fread(ref, 1, w * h, f); fclose(f);
    if(r != DnieJ2kOk || g.width != w || g.height != h) { printf("FAIL %s: r=%d %ux%u vs %dx%d\n", name, r, g.width, g.height, w, h); fails++; return; }
    double se = 0; int maxd = 0;
    for(int i = 0; i < w * h; i++) { int d = abs(g.pixels[i] - ref[i]); se += d * d; if(d > maxd) maxd = d; }
    double psnr = se ? 10 * log10(255.0 * 255 * w * h / se) : 99;
    printf("%s %s %dx%d: PSNR %.1f dB, max dif %d\n", psnr > 40 ? "OK  " : "FAIL", name, w, h, psnr, maxd);
    fails += psnr <= 40; free(ref); dnie_gray_free(&g);
}
int main(void) { one("j420", 1); one("j444", 1); one("j422rst", 1); one("jgray", 1); one("jprog", 0);
    printf(fails ? "%d FALLOS\n" : "Todo OK\n", fails); return fails != 0; }
