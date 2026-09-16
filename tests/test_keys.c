#include <assert.h>
#include <stdio.h>
#include "passport_input.h"
#include "passport_gui.h"
#include "baye/comm.h"
#include "inc/keytable.h"

int main(void) {
    printf("--- Running test_keys ---\n");

    GuiInit();

    MsgType msg;

    // 1. UP CLICK -> CHAR_UP
    passport_input_on_button(BSP_BTN_UP, BSP_BTN_CLICK);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_UP);

    // 2. UP LONG -> CHAR_LEFT
    passport_input_on_button(BSP_BTN_UP, BSP_BTN_LONG);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_LEFT);

    // 3. DOWN CLICK -> CHAR_DOWN
    passport_input_on_button(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_DOWN);

    // 4. DOWN LONG -> CHAR_RIGHT
    passport_input_on_button(BSP_BTN_DOWN, BSP_BTN_LONG);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_RIGHT);

    // 5. OK CLICK -> CHAR_ENTER
    passport_input_on_button(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_ENTER);

    // 6. OK LONG -> CHAR_EXIT
    passport_input_on_button(BSP_BTN_OK, BSP_BTN_LONG);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_EXIT);

    // 7. OK DOUBLE -> CHAR_HELP
    passport_input_on_button(BSP_BTN_OK, BSP_BTN_DOUBLE);
    assert(GuiGetMsg(&msg) == 1);
    assert(msg.type == VM_CHAR_FUN);
    assert(msg.param == CHAR_HELP);

    // 8. No pending messages
    assert(GuiGetMsg(&msg) == 0);

    printf("test_keys: PASS\n");
    return 0;
}
