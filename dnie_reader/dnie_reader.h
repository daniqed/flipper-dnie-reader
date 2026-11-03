#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <notification/notification_messages.h>

#include "dnie_card.h"
#include "dnie_pace_info.h"
#include "dnie_pace.h"
#include "dnie_sm.h"
#include "dnie_lds.h"
#include "dnie_mrz.h"
#include "dnie_photo.h"
#include "dnie_j2k.h"
#include "dnie_pixelart.h"
#include "dnie_store.h"
#include "dnie_auth.h"

#define DNIE_CAN_LENGTH 6 // El CAN del DNIe son 6 digitos impresos en el anverso
#define DNIE_CAN_BUFFER_SIZE (DNIE_CAN_LENGTH + 1)

// Identificadores de cada View registrada en el ViewDispatcher
typedef enum {
    DnieViewSubmenu,
    DnieViewWidget,
    DnieViewCard, // carnet con la foto en pixel art
    DnieViewCan, // CAN: 6 casillas numericas
} DnieView;

// Eventos personalizados que las escenas se mandan entre si
typedef enum {
    DnieCustomEventCanEntered = 100,
    DnieCustomEventReadOk,
    DnieCustomEventReadError,
    DnieCustomEventProgress, // el poller avanza de paso en PACE
    DnieCustomEventCardDetails, // OK en el carnet -> detalles
    DnieCustomEventCardDeleted, // lectura guardada borrada desde el carnet
    DnieCustomEventCardSave, // arriba en el carnet -> menu de guardar
} DnieCustomEvent;

// Contexto global de la app: lo comparten todas las escenas
typedef struct {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    View* can_view;
    Widget* widget;
    NotificationApp* notifications; // LED, vibracion y sonido

    char can[DNIE_CAN_BUFFER_SIZE]; // CAN introducido por el usuario

    // NFC
    Nfc* nfc;
    NfcPoller* poller; // solo vive mientras estamos en la escena Read

    // Resultado de la ultima lectura
    DnieCardResult read_result; // lectura de EF.CardAccess
    DniePaceResult pace_result; // PACE (solo si EF.CardAccess fue bien)
    DniePaceInfo pace_infos[DNIE_PACE_INFO_MAX];
    size_t pace_info_count;
    DniePaceState pace;
    volatile uint8_t pace_progress; // paso actual (lo escribe el hilo del poller)

    // Datos del titular (DG1). Se borran al salir de la app.
    DnieLdsStatus lds;
    uint8_t dg1[DNIE_DG1_MAX];
    size_t dg1_len;
    DnieMrz mrz;
    bool mrz_ok;

    // Autenticidad (SOD + DG14)
    DnieAuth auth;

    // Foto (DG2)
    DniePhoto photo;
    volatile uint8_t photo_percent;

    // Pixel art (se calcula al abrir el carnet)
    View* card_view;
    bool photo_decode_tried;
    DnieJ2kResult photo_decode_result;
    uint16_t photo_decoded_w, photo_decoded_h; // resolucion reducida decodificada
    uint8_t photo_reduce;
    DniePixelSource* pix_src; // gris escalado (malloc, ~4 KB)
    DniePixelArt pix;
    DnieDither dither;

    // Lecturas guardadas
    bool viewing_saved; // el carnet muestra una lectura guardada, no una recien leida
    uint32_t saved_at;
    char saved_path[128];
    DnieSavedEntry* saved_entries; // malloc, DNIE_STORE_MAX
    size_t saved_count;

    // Aviso temporal en el carnet ("Guardado", "Borrado"...)
    const char* toast;
    FuriTimer* toast_timer;
    const char* pending_toast; // lo deja el menu de guardar para el carnet
    bool card_hint_pending; // mostrar "Arriba: guardar" al llegar al carnet

    // Lectura de album abierta (solo nombre, foto y cumple)
    bool viewing_album;
    char album_given[DNIE_RECORD_NAME_MAX];
    char album_surnames[DNIE_RECORD_NAME_MAX];
    char album_birth[9]; // AAAAMMDD

    uint16_t last_sw;
    uint8_t card_access[DNIE_CARD_ACCESS_MAX];
    size_t card_access_len;
} DnieApp;

// Vista del CAN (escena can_input)
View* dnie_can_view_alloc(DnieApp* app);

// Fecha de nacimiento de la MRZ (AAMMDD) -> "AAAAMMDD" deduciendo el siglo con el reloj
void dnie_birth_full(const char* yymmdd, char out[9]);
