#include "dnie_sounds.h"

#include <notification/notification_messages.h>
#include <stdlib.h>

DnieVerdict dnie_verdict(const DnieAuth* a) {
    if(dnie_verdict_reason(a)) return DnieVerdictFake;
    if(dnie_auth_all_ok(a)) return DnieVerdictOriginal;
    return DnieVerdictUnknown;
}

const char* dnie_verdict_reason(const DnieAuth* a) {
    if(dnie_auth_integrity_failed(a)) return "Datos alterados:\nlos hashes no cuadran";
    if(a->pa.result == DniePaBadSignature) return "Firma del SOD\nfalsa";
    if(a->csca.result == DnieCscaBadSignature) return "Certificado no\nemitido por la Policia";
    if(a->ca.result == DnieCaFailed) return "El chip no tiene la\nclave: posible CLON";
    return NULL;
}

// ---- Melodias ----
// Cada nota: SoundOn + Delay + SoundOff + pausa corta. Se construyen una vez.

typedef struct {
    float freq; // 0 = silencio
    uint16_t ms;
} Note;

// Marcha Real (Espinosa, 1761, dominio publico). Melodia de la partitura para piano de
// Wikimedia Commons ("Marcha Real de España.pdf"), en Do mayor: frase A (con la 2a casilla)
// y frase B, sin repeticiones. Negra = 400 ms. 0 Hz = silencio.
#define Q 400
#define G4_ 392.00f
#define A4_ 440.00f
#define B4_ 493.88f
#define C5_ 523.25f
#define D5_ 587.33f
#define E5_ 659.25f
#define F5_ 698.46f
#define G5_ 783.99f
static const Note ANTHEM[] = {
    // A
    {C5_, Q}, {G4_, Q}, {E5_, Q}, {C5_, Q / 2}, {G5_, Q / 2},
    {F5_, Q / 2}, {E5_, Q / 2}, {D5_, Q / 2}, {C5_, Q / 2},
    {C5_, Q / 2}, {B4_, Q / 2}, {A4_, Q / 2}, {G4_, Q / 2},
    {C5_, Q}, {D5_, Q}, {E5_, Q}, {0, Q / 2}, {G5_, Q / 2},
    {F5_, Q / 2}, {E5_, Q / 2}, {D5_, Q / 2}, {C5_, Q / 2}, {G5_, Q * 2},
    // B
    {G5_, Q}, {E5_, Q * 3 / 4}, {G5_, Q / 4}, {F5_, Q}, {D5_, Q * 3 / 4}, {F5_, Q / 4},
    {E5_, Q}, {C5_, Q * 3 / 4}, {E5_, Q / 4}, {D5_, Q / 2}, {G4_, Q / 2}, {A4_, Q / 2}, {B4_, Q / 2},
    {C5_, Q}, {D5_, Q}, {E5_, Q * 3 / 4}, {F5_, Q / 4}, {G5_, Q * 3 / 4}, {F5_, Q / 4},
    {E5_, Q}, {D5_, Q}, {C5_, Q * 2},
};

// "Wah wah wah waaah": cromatico descendente y grave
static const Note FAKE[] = {
    {196.00f, 300}, {185.00f, 300}, {174.61f, 300}, {164.81f, 1000},
};

#define MAX_NOTES 48
typedef struct {
    NotificationMessage on[MAX_NOTES];
    NotificationMessage wait[MAX_NOTES];
    NotificationMessage gap;
    const NotificationMessage* seq[MAX_NOTES * 5 + 8];
    bool built;
} Melody;

// En el heap y solo cuando hace falta (al acabar la lectura): durante PACE la RAM es justa
static Melody* anthem_m;
static Melody* fake_m;

static void build(Melody* m, const Note* notes, size_t n, bool fake) {
    if(m->built) return;
    if(n > MAX_NOTES) n = MAX_NOTES;
    size_t k = 0;
    m->gap = (NotificationMessage){.type = NotificationMessageTypeDelay, .data.delay.length = 30};
    m->seq[k++] = fake ? &message_red_255 : &message_green_255;
    m->seq[k++] = &message_display_backlight_on;
    m->seq[k++] = &message_vibro_on;
    for(size_t i = 0; i < n; i++) {
        // La cola de notificaciones esta ocupada mientras suena: mantener la pantalla
        // encendida desde la propia melodia
        if(i % 4 == 0) m->seq[k++] = &message_display_backlight_on;
        m->on[i] = (NotificationMessage){
            .type = NotificationMessageTypeSoundOn,
            .data.sound.frequency = notes[i].freq,
            .data.sound.volume = 1.0f};
        m->wait[i] = (NotificationMessage){
            .type = NotificationMessageTypeDelay, .data.delay.length = notes[i].ms - 30};
        if(notes[i].freq > 0) m->seq[k++] = &m->on[i];
        m->seq[k++] = &m->wait[i];
        if(notes[i].freq > 0) m->seq[k++] = &message_sound_off;
        // Vibracion: corta al empezar (como el NFC) o durante todo el fallo
        if(i == 0 && !fake) m->seq[k++] = &message_vibro_off;
        m->seq[k++] = &m->gap;
    }
    if(fake) m->seq[k++] = &message_vibro_off;
    m->seq[k] = NULL;
    m->built = true;
}

static const NotificationSequence* get(Melody** m, const Note* notes, size_t n, bool fake) {
    if(!*m) *m = calloc(1, sizeof(Melody));
    if(!*m) return fake ? &sequence_error : &sequence_success;
    build(*m, notes, n, fake);
    return (const NotificationSequence*)&(*m)->seq;
}

const NotificationSequence* dnie_sound_original(void) {
    return get(&anthem_m, ANTHEM, sizeof(ANTHEM) / sizeof(ANTHEM[0]), false);
}

const NotificationSequence* dnie_sound_fake(void) {
    return get(&fake_m, FAKE, sizeof(FAKE) / sizeof(FAKE[0]), true);
}

void dnie_sounds_free(void) {
    free(anthem_m);
    free(fake_m);
    anthem_m = NULL;
    fake_m = NULL;
}
