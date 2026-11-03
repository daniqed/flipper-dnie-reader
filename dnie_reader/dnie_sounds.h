#pragma once

#include <notification/notification.h>
#include "dnie_auth.h"

typedef enum {
    DnieVerdictUnknown, // no se pudo comprobar todo (sin SOD, sin memoria...)
    DnieVerdictOriginal, // hashes + firma + CSCA + chip
    DnieVerdictFake, // algo NO cuadra: alterado, firma falsa o clon
} DnieVerdict;

DnieVerdict dnie_verdict(const DnieAuth* a);

// Motivo corto para la pantalla de aviso (NULL si no es Fake)
const char* dnie_verdict_reason(const DnieAuth* a);

// Marcha Real (LED verde) / sonido grave descendente + vibracion larga (LED rojo)
const NotificationSequence* dnie_sound_original(void);
const NotificationSequence* dnie_sound_fake(void);

// Al salir de la app (despues de que la notificacion haya terminado de sonar)
void dnie_sounds_free(void);
