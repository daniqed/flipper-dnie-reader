#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

typedef enum {
    SubmenuIndexRead,
    SubmenuIndexSaved,
    SubmenuIndexDelete,
    SubmenuIndexAbout,
} SubmenuIndex;

static void dnie_scene_start_submenu_callback(void* context, uint32_t index) {
    DnieApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dnie_scene_start_on_enter(void* context) {
    DnieApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "DNIe Reader");
    submenu_add_item(
        submenu, "Leer DNIe", SubmenuIndexRead, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Lecturas guardadas", SubmenuIndexSaved, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Borrar foto (jp2) SD", SubmenuIndexDelete, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Acerca de", SubmenuIndexAbout, dnie_scene_start_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewSubmenu);
}

bool dnie_scene_start_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRead) {
            scene_manager_next_scene(app->scene_manager, DnieSceneCanInput);
            consumed = true;
        } else if(event.event == SubmenuIndexSaved) {
            scene_manager_next_scene(app->scene_manager, DnieSceneSavedList);
            consumed = true;
        } else if(event.event == SubmenuIndexDelete) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_simply_remove(storage, DNIE_PHOTO_J2K_PATH);
            storage_simply_remove(storage, DNIE_PHOTO_JPG_PATH);
            storage_simply_remove(storage, DNIE_DG2_PATH);
            furi_record_close(RECORD_STORAGE);
            notification_message(app->notifications, &sequence_success);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, DnieSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void dnie_scene_start_on_exit(void* context) {
    DnieApp* app = context;
    submenu_reset(app->submenu);
}
