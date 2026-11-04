#pragma once

#include <notification/notification.h>
#include "dnie_auth.h"

typedef enum {
    DnieVerdictUnknown, // no se pudo comprobar todo (sin SOD, sin memoria...)
    DnieVerdictOriginal, // hashes + firma + CSCA + chip
    DnieVerdictGenuine, // hashes + firma + CSCA, pero el documento no tiene Chip Authentication
    DnieVerdictFake, // algo NO cuadra: alterado, firma falsa o clon
} DnieVerdict;

DnieVerdict dnie_verdict(const DnieAuth* a);

// Motivo corto para la pantalla de aviso (NULL si no es Fake)
const char* dnie_verdict_reason(const DnieAuth* a);

// Documento autentico: LED verde y el himno del pais emisor (fanfarria si no lo tenemos).
// No autentico: LED rojo, vibracion larga y sonido grave. La musica va en un hilo propio.
void dnie_sound_original(NotificationApp* n, const char* issuer);
void dnie_sound_fake(NotificationApp* n);

// Corta la musica al instante (al salir de la app o empezar otra lectura)
void dnie_sound_stop(void);
