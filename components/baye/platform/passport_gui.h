#ifndef PASSPORT_GUI_H
#define PASSPORT_GUI_H

#include "baye/compa.h"
#include "baye/comm.h"
#include "inc/dictsys.h"

#ifdef __cplusplus
extern "C" {
#endif

U8 GuiInit(void);
U8 GuiPushMsg(PtrMsg pMsg);
U8 GuiGetMsg(PtrMsg pMsg);
U16 GuiGetKbdState(void);
void GuiSetKbdState(U16 state);
void GuiSetInputFilter(U8 filter);
void GuiSetKbdType(U8 type);
U8 GuiTranslateMsg(PtrMsg pMsg);
FAR U8 GuiMsgBox(U8* strMsg, U16 nTimeout);
FAR U8 GuiQueryBox(U8 sel, U8 infoType, U8 *infoData);

// Helper to push a key directly
void bayeSendKey(int key);

#ifdef __cplusplus
}
#endif

#endif // PASSPORT_GUI_H
