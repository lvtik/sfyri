#include "ui.h"

int main(void) {
    spawn_window();
    main_ui_loop();
    destroy_ui();
    return 0;
}
