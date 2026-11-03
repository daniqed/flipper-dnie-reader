#include "../dnie_reader.h"
#include "../dnie_sounds.h"
#include "dnie_reader_scene.h"

// Aviso a pantalla completa cuando el documento NO supera las comprobaciones.
// Derecha: ver el carnet igualmente. Atras/izquierda: volver al menu.

static void warning_button(GuiButtonType button, InputType type, void* context) {
    DnieApp* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        button == GuiButtonTypeRight ? DnieCustomEventWarningShow : DnieCustomEventWarningExit);
}

void dnie_scene_warning_on_enter(void* context) {
    DnieApp* app = context;
    Widget* w = app->widget;
    widget_reset(w);
    widget_add_frame_element(w, 0, 0, 128, 51, 3);
    widget_add_string_element(w, 64, 3, AlignCenter, AlignTop, FontPrimary, "!! NO AUTENTICO !!");
    const char* reason = dnie_verdict_reason(&app->auth);
    widget_add_string_multiline_element(
        w, 64, 17, AlignCenter, AlignTop, FontSecondary, reason ? reason : "Comprobacion fallida");
    widget_add_string_element(
        w, 64, 40, AlignCenter, AlignTop, FontSecondary, "No te fies de este doc.");
    widget_add_button_element(w, GuiButtonTypeLeft, "Salir", warning_button, app);
    widget_add_button_element(w, GuiButtonTypeRight, "Ver", warning_button, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewWidget);
}

bool dnie_scene_warning_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DnieCustomEventWarningShow) {
            app->card_hint_pending = true;
            scene_manager_next_scene(app->scene_manager, DnieSceneCard);
            return true;
        }
        if(event.event == DnieCustomEventWarningExit) {
            return scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DnieSceneStart);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, DnieSceneStart);
    }
    return false;
}

void dnie_scene_warning_on_exit(void* context) {
    DnieApp* app = context;
    widget_reset(app->widget);
}
