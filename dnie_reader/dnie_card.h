#pragma once

#include <furi.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#define DNIE_CARD_ACCESS_MAX 512

typedef enum {
    DnieCardOk,
    DnieCardErrorTransport, // fallo de radio / tarjeta retirada
    DnieCardErrorSelect, // el chip rechazo el SELECT de EF.CardAccess
    DnieCardErrorRead, // READ BINARY devolvio un SW distinto de 9000
    DnieCardErrorFormat, // no empieza por SET (0x31) o longitud invalida
    DnieCardErrorTooBig, // mas grande que nuestro buffer
} DnieCardResult;

// Lee EF.CardAccess (FID 011C). Solo se puede llamar dentro del callback del poller.
DnieCardResult dnie_card_read_card_access(
    Iso14443_4bPoller* poller,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    uint16_t* last_sw);

const char* dnie_card_result_str(DnieCardResult result);

// Envia una APDU dentro del callback del poller. En rx quedan datos + SW1SW2;
// en sw se devuelve el status word. false = error de transporte.
bool dnie_card_apdu(
    Iso14443_4bPoller* poller,
    const uint8_t* apdu,
    size_t apdu_len,
    BitBuffer* tx,
    BitBuffer* rx,
    uint16_t* sw);
