#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

// CAN: 6 casillas numericas. Arriba/abajo cambia la cifra, izquierda/derecha mueve el cursor,
// OK lee el DNI. Sin teclado de texto: solo se pueden meter cifras y siempre son 6.

typedef struct {
    char digits[DNIE_CAN_LENGTH + 1];
    uint8_t cursor;
} CanModel;

#define BOX_W 17
#define BOX_H 22
#define BOX_GAP 3
#define BOX_Y 16

static void can_draw(Canvas* canvas, void* model) {
    CanModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "CAN del DNI");

    int total = DNIE_CAN_LENGTH * BOX_W + (DNIE_CAN_LENGTH - 1) * BOX_GAP;
    int x0 = (128 - total) / 2;
    canvas_set_font(canvas, FontBigNumbers);
    for(int i = 0; i < DNIE_CAN_LENGTH; i++) {
        int x = x0 + i * (BOX_W + BOX_GAP);
        char d[2] = {m->digits[i], '\0'};
        if(i == m->cursor) {
            canvas_draw_rbox(canvas, x, BOX_Y, BOX_W, BOX_H, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, x + BOX_W / 2, BOX_Y + BOX_H / 2 + 1, AlignCenter, AlignCenter, d);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, x, BOX_Y, BOX_W, BOX_H, 2);
            canvas_draw_str_aligned(canvas, x + BOX_W / 2, BOX_Y + BOX_H / 2 + 1, AlignCenter, AlignCenter, d);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "Arriba/abajo: cifra");
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "OK: leer el DNI");
}

static bool can_input(InputEvent* event, void* context) {
    DnieApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true;
    bool done = false;
    with_view_model(
        app->can_view,
        CanModel * m,
        {
            switch(event->key) {
            case InputKeyUp:
                m->digits[m->cursor] = m->digits[m->cursor] == '9' ? '0' : m->digits[m->cursor] + 1;
                break;
            case InputKeyDown:
                m->digits[m->cursor] = m->digits[m->cursor] == '0' ? '9' : m->digits[m->cursor] - 1;
                break;
            case InputKeyLeft:
                if(m->cursor > 0) m->cursor--;
                break;
            case InputKeyRight:
                if(m->cursor < DNIE_CAN_LENGTH - 1) m->cursor++;
                break;
            case InputKeyOk:
                memcpy(app->can, m->digits, DNIE_CAN_LENGTH);
                app->can[DNIE_CAN_LENGTH] = '\0';
                done = true;
                break;
            default:
                consumed = false; // Atras: lo gestiona el SceneManager
                break;
            }
        },
        true);
    if(done) view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventCanEntered);
    return consumed;
}

View* dnie_can_view_alloc(DnieApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(CanModel));
    view_set_context(view, app);
    view_set_draw_callback(view, can_draw);
    view_set_input_callback(view, can_input);
    return view;
}

void dnie_scene_can_input_on_enter(void* context) {
    DnieApp* app = context;
    // Si ya se metio un CAN antes, lo ofrecemos de nuevo (comodo para repetir la lectura)
    bool have = strlen(app->can) == DNIE_CAN_LENGTH;
    with_view_model(
        app->can_view,
        CanModel * m,
        {
            for(int i = 0; i < DNIE_CAN_LENGTH; i++) m->digits[i] = have ? app->can[i] : '0';
            m->digits[DNIE_CAN_LENGTH] = '\0';
            m->cursor = 0;
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewCan);
}

bool dnie_scene_can_input_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == DnieCustomEventCanEntered) {
        scene_manager_next_scene(app->scene_manager, DnieSceneRead);
        consumed = true;
    }
    return consumed;
}

void dnie_scene_can_input_on_exit(void* context) {
    UNUSED(context);
}
