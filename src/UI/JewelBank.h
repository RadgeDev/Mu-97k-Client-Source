#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void __cdecl JewelBank_ResetCharacter(void);
void __cdecl JewelBank_ResetSession(void);
void __cdecl JewelBank_Recv(BYTE* Msg, int Size);
void __cdecl RenderJewelBankWindow(void);
void __cdecl RenderJewelBankItems3D(void);
void __cdecl CheckJewelBankWindow(void);
void __cdecl JewelBank_Open(void);
void __cdecl JewelBank_Close(void);
void __cdecl JewelBank_Toggle(void);

#ifdef __cplusplus
}
#endif

extern bool g_bJewelBankWindowOpen;
