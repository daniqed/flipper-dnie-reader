#include "../dnie_reader.h"
#include "dnie_reader_scene.h"
#include "../dnie_sounds.h"

typedef enum {
    SubmenuIndexRead,
    SubmenuIndexPassport,
    SubmenuIndexIdCan,
    SubmenuIndexIdMrz,
    SubmenuIndexSaved,
    SubmenuIndexDelete,
    SubmenuIndexSettings,
    SubmenuIndexAbout,
} SubmenuIndex;

static void dnie_scene_start_submenu_callback(void* context, uint32_t index) {
    DnieApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dnie_scene_start_on_enter(void* context) {
    DnieApp* app = context;
    dnie_sound_stop(); // por si venimos de un carnet con el himno sonando
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "IDentity");
    submenu_add_item(
        submenu, "Spanish ID", SubmenuIndexRead, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Passport", SubmenuIndexPassport, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Other ID (with CAN)", SubmenuIndexIdCan, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Other ID (with MRZ)", SubmenuIndexIdMrz, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Saved readings", SubmenuIndexSaved, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Delete photos from SD", SubmenuIndexDelete, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Anthem volume", SubmenuIndexSettings, dnie_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, dnie_scene_start_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewSubmenu);
}

bool dnie_scene_start_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRead || event.event == SubmenuIndexPassport ||
           event.event == SubmenuIndexIdCan || event.event == SubmenuIndexIdMrz) {
            app->passport = event.event == SubmenuIndexPassport || event.event == SubmenuIndexIdMrz;
            app->foreign_id = event.event == SubmenuIndexIdCan || event.event == SubmenuIndexIdMrz;
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
        } else if(event.event == SubmenuIndexSettings) {
            scene_manager_next_scene(app->scene_manager, DnieSceneSettings);
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
