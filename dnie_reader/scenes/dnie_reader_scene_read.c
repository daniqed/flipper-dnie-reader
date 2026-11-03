#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

/*
 * Lectura del DNIe:
 *  A. Activar el chip (ISO14443-4B) y leer EF.CardAccess (publico).
 *  B. PACE completo con el CAN -> KSenc/KSmac (canal seguro).
 *  C. Secure Messaging: leer DG1 (MRZ) y parsearlo.
 *  D. EF.SOD + DG14: hashes firmados y datos de Chip Authentication.
 *  E. DG2 (foto) a la SD con su hash; al terminar se extrae la imagen.
 *  F. Chip Authentication: el chip demuestra que tiene la clave privada de DG14.
 */

static const char* const DNIE_STEP_NAMES[] = {
    "Leyendo EF.CardAccess",
    "1/4 Nonce cifrado",
    "2/4 Mapeo (ECC)",
    "3/4 ECDH (ECC)",
    "4/4 Tokens",
    "Leyendo DG1 (cifrado)",
    "Comprobando SOD/DG14",
    "Foto (DG2)",
    "Autenticando chip",
};
#define DNIE_READ_STEPS 8
#define DNIE_STEP_PHOTO 7 // muestra el porcentaje

static void dnie_scene_read_draw(DnieApp* app) {
    Widget* widget = app->widget;
    uint8_t step = app->pace_progress;
    if(step > DNIE_READ_STEPS) step = DNIE_READ_STEPS;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, step == 0 ? "Acerca el DNIe" : (step <= DNIE_PACE_STEPS ? "PACE en curso" : "Canal seguro OK"));
    widget_add_string_multiline_element(
        widget,
        64,
        18,
        AlignCenter,
        AlignTop,
        FontSecondary,
        step ? "No muevas el DNI,\npuede tardar unos\nsegundos" : "a la parte trasera\ndel Flipper y no\nlo muevas");
    if(step == DNIE_STEP_PHOTO) {
        char line[32];
        snprintf(line, sizeof(line), "%s: %u%%", DNIE_STEP_NAMES[step], app->photo_percent);
        widget_add_string_element(widget, 64, 54, AlignCenter, AlignTop, FontSecondary, line);
    } else {
        widget_add_string_element(
            widget, 64, 54, AlignCenter, AlignTop, FontSecondary, DNIE_STEP_NAMES[step]);
    }
}

// Hilo del poller -> GUI: solo mediante custom events
static void dnie_scene_read_progress(uint8_t step, void* context) {
    DnieApp* app = context;
    app->pace_progress = step;
    view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventProgress);
}

static void dnie_scene_read_photo_progress(uint8_t percent, void* context) {
    DnieApp* app = context;
    app->photo_percent = percent;
    view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventProgress);
}

// OJO: este callback corre en el hilo del poller, no en el de la GUI.
// Solo hablamos con la GUI mediante custom events.
static NfcCommand dnie_scene_read_poller_callback(NfcGenericEvent event, void* context) {
    DnieApp* app = context;
    furi_assert(event.protocol == NfcProtocolIso14443_4b);

    const Iso14443_4bPollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso14443_4bPollerEventTypeReady) {
        return NfcCommandContinue; // aun no hay DNI en el campo: seguimos buscando
    }

    bool ok = false;
    FURI_LOG_I(
        "DnieRead",
        "Inicio: heap libre %u B, bloque max %u B",
        memmgr_get_free_heap(),
        memmgr_heap_get_max_free_block());
    do {
        // Paso A: EF.CardAccess (publico)
        app->read_result = dnie_card_read_card_access(
            event.instance,
            app->card_access,
            sizeof(app->card_access),
            &app->card_access_len,
            &app->last_sw);
        if(app->read_result != DnieCardOk) break;

        app->pace_info_count = dnie_pace_info_parse(
            app->card_access, app->card_access_len, app->pace_infos, DNIE_PACE_INFO_MAX);

        // Paso B: PACE completo
        if(!dnie_pace_choose(app->pace_infos, app->pace_info_count, &app->pace.info)) {
            app->pace_result = DniePaceErrorUnsupported;
            break;
        }
        app->pace_result = dnie_pace_establish(
            event.instance, app->can, &app->pace, dnie_scene_read_progress, app);
        if(app->pace_result != DniePaceOk) break;

        // Paso C: DG1 dentro del canal seguro
        dnie_scene_read_progress(DNIE_PACE_STEPS + 1, app);
        DnieSm sm;
        dnie_sm_init(&sm, app->pace.ks_enc, app->pace.ks_mac);
        // Las claves de sesion ya estan en sm: fuera de app->pace
        memset(app->pace.ks_enc, 0, sizeof(app->pace.ks_enc));
        memset(app->pace.ks_mac, 0, sizeof(app->pace.ks_mac));

        if(dnie_lds_select_app(event.instance, &sm, &app->lds) == DnieLdsOk) {
            dnie_lds_read_dg1(
                event.instance, &sm, app->dg1, sizeof(app->dg1), &app->dg1_len, &app->lds);
        }
        if(app->lds.result != DnieLdsOk) {
            dnie_sm_clear(&sm);
            break;
        }
        app->mrz_ok = dnie_mrz_parse_dg1(app->dg1, app->dg1_len, &app->mrz);
        ok = true; // con DG1 ya tenemos lectura valida; la foto es un extra

        // Paso D: autenticidad (SOD con los hashes firmados + DG14)
        dnie_scene_read_progress(DNIE_PACE_STEPS + 2, app);
        dnie_auth_read(event.instance, &sm, app->dg1, app->dg1_len, &app->auth);

        // Paso E: foto (DG2) volcada a la SD, hasheandola por el camino
        app->photo_percent = 0;
        dnie_scene_read_progress(DNIE_PACE_STEPS + 3, app);
        DnieHash dg2_hash;
        bool hashing = dnie_auth_dg2_hash_start(&app->auth, &dg2_hash);
        DnieLdsResult pr = dnie_photo_read_dg2(
            event.instance,
            &sm,
            &app->photo,
            dnie_scene_read_photo_progress,
            app,
            hashing ? &dg2_hash : NULL);
        if(hashing) dnie_auth_dg2_hash_done(&app->auth, &dg2_hash, pr == DnieLdsOk);

        // Paso F: Chip Authentication (al final: cambia el canal a 3DES con claves nuevas)
        if(app->auth.dg14_ok && pr != DnieLdsErrorSm && pr != DnieLdsErrorTransport) {
            dnie_scene_read_progress(DNIE_PACE_STEPS + 4, app);
            dnie_ca_run(event.instance, &sm, &app->auth.dg14, app->dg1, &app->auth.ca);
            FURI_LOG_I("DnieRead", "Chip Auth: %d (SW %04X)", app->auth.ca.result, app->auth.ca.sw);
        }
        dnie_sm_clear(&sm);
        dnie_auth_compact(&app->auth); // fuera SOD y DG14: liberamos memoria para la foto
    } while(false);
    dnie_auth_compact(&app->auth); // tambien si algo fallo a medias
    FURI_LOG_I(
        "DnieRead",
        "Heap libre %u B, bloque max %u B",
        memmgr_get_free_heap(),
        memmgr_heap_get_max_free_block());

    view_dispatcher_send_custom_event(
        app->view_dispatcher, ok ? DnieCustomEventReadOk : DnieCustomEventReadError);
    return NfcCommandStop;
}

void dnie_scene_read_on_enter(void* context) {
    DnieApp* app = context;
    app->card_access_len = 0;
    app->last_sw = 0;
    app->pace_info_count = 0;
    app->dg1_len = 0;
    app->mrz_ok = false;
    memset(app->dg1, 0, sizeof(app->dg1));
    memset(&app->mrz, 0, sizeof(app->mrz));
    memset(&app->lds, 0, sizeof(app->lds));
    memset(&app->photo, 0, sizeof(app->photo));
    app->photo_decode_tried = false;
    app->viewing_saved = false;
    app->viewing_album = false;
    dnie_auth_reset(&app->auth);
    app->photo_decode_result = DnieJ2kErrorFormat;
    memset(&app->pix, 0, sizeof(app->pix));
    app->photo.lds.result = DnieLdsErrorTransport; // "no leida" hasta que se lea
    app->read_result = DnieCardErrorTransport;
    app->pace_result = DniePaceErrorTransport;
    memset(&app->pace, 0, sizeof(app->pace));

    app->pace_progress = 0;
    dnie_scene_read_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewWidget);

    // LED cian parpadeando mientras buscamos, como la app NFC oficial
    notification_message(app->notifications, &sequence_blink_start_cyan);

    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4b);
    nfc_poller_start(app->poller, dnie_scene_read_poller_callback, app);
}

bool dnie_scene_read_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DnieCustomEventProgress) {
            dnie_scene_read_draw(app);
            consumed = true;
        } else if(event.event == DnieCustomEventReadOk || event.event == DnieCustomEventReadError) {
            // Extraer la foto de DG2 (acceso a SD: aqui, en el hilo de la GUI)
            if(app->photo.lds.result == DnieLdsOk && app->photo.dg2_len > 0) {
                dnie_photo_extract(&app->photo);
            }
            notification_message(app->notifications, &sequence_blink_stop);
            // Exito: pitido + vibracion + LED verde. Error: sonido de error + LED rojo
            notification_message(
                app->notifications,
                event.event == DnieCustomEventReadOk ? &sequence_success : &sequence_error);
            // Con datos validos, el carnet; si no, la pantalla de detalles con el error
            bool card = event.event == DnieCustomEventReadOk && app->mrz_ok;
            app->card_hint_pending = card; // en el carnet: "Arriba: guardar"
            scene_manager_next_scene(app->scene_manager, card ? DnieSceneCard : DnieSceneResult);
            consumed = true;
        }
    }

    return consumed;
}

void dnie_scene_read_on_exit(void* context) {
    DnieApp* app = context;

    if(app->poller) {
        nfc_poller_stop(app->poller); // espera a que termine el hilo del poller
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
    notification_message(app->notifications, &sequence_blink_stop); // por si salimos con Atras
    widget_reset(app->widget);
}
