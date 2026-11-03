#pragma once

// Cadena de confianza: comprueba que el certificado del Document Signer (el que firma el SOD)
// lo emitio la CSCA de Espana ("CSCA SPAIN", Direccion General de la Policia).
// Las claves publicas de la CSCA van dentro de la app (sacadas de la Master List del BSI).

#include "dnie_der.h"
#include "dnie_hash.h"

typedef struct {
    const char* name;
    uint16_t valid_until; // año
    const uint8_t* subject; // Name DER completo
    size_t subject_len;
    const uint8_t* n;
    size_t n_len;
    uint32_t e;
} DnieCscaKey;

typedef enum {
    DnieCscaNotDone,
    DnieCscaOk,
    DnieCscaBadSignature, // emisor conocido pero la firma no cuadra: certificado falso
    DnieCscaUnknownIssuer, // no lo firma ninguna CSCA SPAIN que conozcamos
    DnieCscaUnsupported, // algoritmo no implementado
    DnieCscaFormat,
    DnieCscaNoMemory,
} DnieCscaResult;

typedef struct {
    DnieCscaResult result;
    const char* csca_name; // CSCA que lo valida (o la que coincide en nombre)
} DnieCscaInfo;

// cert = Certificate DER completo (SEQUENCE). Comprueba su firma con las CSCA conocidas.
DnieCscaResult dnie_csca_verify_cert(const DerTlv* cert, DnieCscaInfo* info);

const char* dnie_csca_result_str(DnieCscaResult r);
