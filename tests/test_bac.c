// BAC con el ejemplo resuelto de ICAO 9303-11 apendice D.3 (chip simulado)
#include <stdio.h>
#include <stdlib.h>
#include "dnie_bac.h"

static int fails;
static void hex(const char* h, uint8_t* o) { for(size_t i = 0; h[2 * i]; i++) sscanf(h + 2 * i, "%2hhx", &o[i]); }
#define CHECK(c, msg) do { printf("%s %s\n", (c) ? "OK  " : "FAIL", msg); if(!(c)) fails++; } while(0)

BitBuffer* bit_buffer_alloc(size_t c) { (void)c; return calloc(1, sizeof(BitBuffer)); }
void bit_buffer_free(BitBuffer* b) { free(b); }
void bit_buffer_reset(BitBuffer* b) { b->n = 0; }
void bit_buffer_append_bytes(BitBuffer* b, const uint8_t* d, size_t n) { memcpy(b->d + b->n, d, n); b->n += n; }
size_t bit_buffer_get_size_bytes(const BitBuffer* b) { return b->n; }
uint8_t bit_buffer_get_byte(const BitBuffer* b, size_t i) { return b->d[i]; }
const uint8_t* bit_buffer_get_data(const BitBuffer* b) { return b->d; }

// RND.IFD y K.IFD del ejemplo, en el orden en que los pide dnie_bac
static int rnd_calls;
void furi_hal_random_fill_buf(uint8_t* buf, uint32_t len) {
    hex(rnd_calls++ == 0 ? "781723860C06C226" : "0B795240CB7049B01C19B33E32804F0B", buf); (void)len;
}
static uint8_t sent_ea[40];
static Iso14443_4bError chip(BitBuffer* tx, BitBuffer* rx) {
    uint8_t r[64]; size_t n;
    if(tx->d[1] == 0x84) { hex("4608F919887022129000", r); n = 10; }
    else { memcpy(sent_ea, tx->d + 5, 40);
        hex("46B9342A41396CD7386BF5803104D7CEDC122B9132139BAF2EEDC94EE178534F2F2D235D074D74499000", r); n = 42; }
    bit_buffer_reset(rx); bit_buffer_append_bytes(rx, r, n); return Iso14443_4bErrorNone;
}
Iso14443_4bError iso14443_4b_poller_send_block(void* p, BitBuffer* tx, BitBuffer* rx) { (void)p; return chip(tx, rx); }
Iso14443_4aError iso14443_4a_poller_send_block(void* p, BitBuffer* tx, BitBuffer* rx) { (void)p; return (Iso14443_4aError)chip(tx, rx); }

int main(void) {
    char info[25];
    dnie_bac_mrz_info("L898902C", "690806", "940623", info);
    CHECK(strcmp(info, "L898902C<369080619406236") == 0, "MRZ information");
    DnieTransport t = {NfcProtocolIso14443_4a, NULL};
    DnieSm sm; uint16_t sw;
    DnieBacResult r = dnie_bac_establish(&t, info, &sm, &sw);
    CHECK(r == DnieBacOk, "BAC completo");
    uint8_t e[40]; hex("72C29C2371CC9BDB65B779B8E8D37B29ECC154AA56A8799FAE2F498F76ED92F25F1448EEA8AD90A7", e);
    CHECK(memcmp(sent_ea, e, 40) == 0, "E_IFD || M_IFD");
    uint8_t ke[16], km[16], ssc[8];
    hex("979EC13B1CBFE9DCD01AB0FED307EAE5", ke); hex("F1CB1F1FB5ADF208806B89DC579DC1F8", km); hex("887022120C06C226", ssc);
    CHECK(memcmp(sm.ks_enc, ke, 16) == 0 && memcmp(sm.ks_mac, km, 16) == 0, "KSenc / KSmac");
    CHECK(memcmp(sm.ssc, ssc, 8) == 0, "SSC");
    DniePacePassword pw; dnie_bac_pace_password(info, &pw);
    CHECK(pw.ref == DNIE_PACE_PW_MRZ && pw.len == 20, "contrasena PACE-MRZ");
    printf(fails ? "%d FALLOS\n" : "Todo OK\n", fails);
    return fails != 0;
}
