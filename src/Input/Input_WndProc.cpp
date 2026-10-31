// Input_WndProc.cpp — mensajes de mouse, IME y texto del WndProc.
//
// IDA: WndProc (0x004149D0), segunda pasada del dispatch (mouse/IME) y WM_CHAR.

#include "stdafx.h"
#include "Platform/Text.h"
#include "Render/Camera3D.h"
#include "Input/Input.h"
#include "Platform/IWindow.h"

// Helper de src/UI/Chat_InputTick.cpp — envía una línea de chat
// escrita en InputText[0] (DAT_07db8710 slot 0) por WM_CHAR; la llama el
// handler de Enter en WndProc cuando InputEnable=1 y el buffer no está vacío.
extern "C" void Chat_SendChatLine(const char* text);
void ChatListBox_AddWheel(int notches);
extern "C" BYTE InputTextHide[10];

// Chat_TryAssignMacro -- "/1 texto" guarda una macro en la tecla 1.
static bool Chat_TryAssignMacro(const char* text)
{
    if (!text || text[0] != '/') return false;
    if (text[1] < '0' || text[1] > '9') return false;
    if (text[2] != ' ') return false;

    const char* body = text + 3;
    while (*body == ' ') ++body;
    if (*body == '\0') return false;   // "/1 " solo: no es asignacion

    static const char* const kBlocked[] = {
        "/trade", "/party", "/pt", "/guild", "/guildwar", "/battlesoccer"
    };
    for (int i = 0; i < (int)(sizeof(kBlocked) / sizeof(kBlocked[0])); ++i) {
        if (Platform::TextCompareIgnoreCase(body, kBlocked[i]) == 0)
            return false;
    }

    const int slot = (text[1] == '0') ? 9 : (text[1] - '1');
    char* dst = (char*)MacroText + slot * 0x100;
    memset(dst, 0, 0x100);
    Platform::TextCopy(dst, body, 0x100);
    PlayBuffer(0x19, 0, 0);   // SOUND_CLICK01
    return true;
}

void Input_ResetAllStates()
{
    // Liberar estados sostenidos de mouse
    DAT_083a4124 = 0;      // MouseLButtonPush = 0
    DAT_083a42c4 = 0;      // MouseLButton = 0
    DAT_083a413c = 0;      // MouseLButtonPop = 0
    DAT_083a4299 = 0;      // g_DblClickFlag = 0
    DAT_083a42ac = 0;      // MouseRButton = 0
    MouseRButtonPush = 0;  // MouseRButtonPush = 0
    DAT_055ca038 = 0;      // Enter pressed = 0

    // Liberar estado de auto-ataque o combate trabado
    if (Attacking != -1) {
        Attacking = -1;
    }

    // Cancelar arrastre de cámara 3D si estaba en progreso
    gCamera3D.EndDrag();
    Platform::GetInput().CaptureMouse(false);

    // Limpiar tabla de detección por flanco de teclas (KeyState[256])
    memset(KeyState, 0, sizeof(KeyState));
}

void Input_OnEvent(const Platform::InputEvent& event)
{
    if (!Platform::GetWindow().IsActive())
        return;

    // Si la ventana no está activa o está minimizada, no procesar eventos de entrada
    if (!Platform::GetWindow().IsActive())
        return;

    switch (event.type)
    {
    case Platform::InputEventType::MouseMove:     // 0x200
        {
            DWORD sw = gWindow.GetWidth() ? gWindow.GetWidth() : 640;
            DWORD sh = gWindow.GetHeight() ? gWindow.GetHeight() : 480;
            DAT_083a427c = ((DWORD)event.x * 640) / sw;
            DAT_083a4278 = ((DWORD)event.y * 480) / sh;
        }
        gCamera3D.Drag(event.x, event.y);
        break;

    case Platform::InputEventType::LeftDown:   // 0x201
        DAT_083a413c = 0;                    // cancelar "dialog close" anterior
        if (DAT_083a42c4 == 0) {
            DAT_083a4124 = 1;                // g_ClickFlag = 1
        }
        DAT_083a42c4 = 1;                    // g_DblClickPending = 1
        DAT_055ca03c = DAT_083a427c;         // snapshot posición al down
        DAT_055ca040 = DAT_083a4278;
        break;

    case Platform::InputEventType::LeftUp:     // 0x202
        DAT_083a4124 = 0;                    // g_ClickFlag = 0
        // Si no hubo drag desde el down, marcar "dialog close / click confirmado"
        if (DAT_055ca03c == DAT_083a427c && DAT_055ca040 == DAT_083a4278) {
            DAT_083a413c = 1;
        } else {
            DAT_083a413c = 0;                // fue drag
        }
        DAT_083a42c4 = 0;                    // clear pending
        DAT_055ca03c = DAT_083a427c;
        DAT_055ca040 = DAT_083a4278;
        break;

    case Platform::InputEventType::LeftDoubleClick: // 0x203
        DAT_083a4299 = 1;                    // g_DblClickFlag
        break;

    case Platform::InputEventType::RightDown:   // 0x204
        if (DAT_083a42ac == 0) {
            MouseRButtonPush = 1;                // MouseRButtonPush
        }
        DAT_083a42ac = 1;                    // MouseRButton
        break;

    case Platform::InputEventType::RightUp:     // 0x205
        MouseRButtonPush = 0;
        DAT_083a42ac = 0;
        break;

    case Platform::InputEventType::Wheel:    // 0x20A
        if (!gCamera3D.Wheel(event.wheel))
            ChatListBox_AddWheel(event.wheel / 120);
        break;

    case Platform::InputEventType::MiddleDown:
        gCamera3D.BeginDrag(event.x, event.y);
        Platform::GetInput().CaptureMouse(true);
        break;
    case Platform::InputEventType::MiddleUp:
        gCamera3D.EndDrag();
        Platform::GetInput().CaptureMouse(false);
        break;

    case Platform::InputEventType::ImeStart:  // 0x10D
        DAT_055ca019 = 1;
        break;
    case Platform::InputEventType::ImeEnd:    // 0x10E
        DAT_055ca019 = 0;
        break;

    case Platform::InputEventType::Text:          // 0x102
    {
        DWORD  slot = DAT_07e11d78 & 0x0F;
        if (slot > 9) slot = 0;
        DWORD* lens = (DWORD*)DAT_07d780a8;
        int    len  = (int)lens[slot];
        char*  buf  = (char*)DAT_07db8710 + slot * 0x100;

        if (!DAT_055ca019) {
            *((char*)&DAT_07e11cec + slot * 4) = 0;
        }

        if (event.character == 8) {           // Backspace
            if (len > 0) {
                lens[slot] = (DWORD)(len - 1);
                buf[len - 1] = 0;
            }
            break;
        }
        if (event.character == 9) {           // Tab — rotate active slot
            if (DAT_00559c84 && InputNumber > 1) {
                DAT_07e11d78 = (DAT_07e11d78 + 1) % InputNumber;
                PlayBuffer(0x19, 0, 0);   // click sfx
            }
            break;
        }
        if (event.character == 0xD) {         // Enter
            DAT_055ca038 = 1;

            if (GoldInputEnable) {
                DAT_07e11d78 = 0;
                char* goldBuf = (char*)DAT_07db8710;
                goldBuf[0xFF] = 0;
                int gold = 0;
                for (const char* digit = goldBuf; *digit; ++digit) {
                    if (digit - goldBuf >= 10 || *digit < '0' || *digit > '9' ||
                        gold > (2000000000 - (*digit - '0')) / 10) {
                        gold = 2000000001;
                        break;
                    }
                    gold = gold * 10 + (*digit - '0');
                }
                InputGold = gold;
                memset(goldBuf, 0, 0x100);
                lens[0] = 0;
                DAT_00559c84 = 0;
                break;
            }

            const bool guildDeleteCodeDialog =
                DAT_083a7c24 == 126 || DAT_083a7c24 == 152;
            if (SceneFlag == 5 && GuildInputEnable == 0 && !guildDeleteCodeDialog) {
                bool empty = (lens[slot] == 0);
                if (empty) {
                    if (DAT_00559c84) {
                        DAT_00559c84 = 0;
                    } else {
                        _InputTextMaxArr[0] = 42;
                        _InputTextMaxArr[1] = 10;
                        InputTextHide[0] = 0;
                        InputNumber = 2;
                        DAT_00559c84 = 1;
                        DAT_07e11d78 = 0;
                    }
                    PlayBuffer(0x19, 0, 0);
                } else if (DAT_00559c84) {
                    char* line = (char*)DAT_07db8710 + slot * 0x100;
                    if (Chat_TryAssignMacro(line)) {
                        memset(line, 0, 0x100);
                        lens[slot] = 0;
                        DAT_00559c84 = 0;
                        break;
                    }
                    if (((const char*)&DAT_07db8810)[0] == '\0') {
                        WORD helper = *(WORD*)(DAT_07abf5d8 + 0x2b8);
                        if ((helper != 818 && helper != 819) || *(char*)(DAT_07abf5d8 + 0x34e))
                            CheckChatText(line);
                    }
                    Chat_SendChatLine(line);

                    memset(line, 0, 0x100);
                    lens[slot] = 0;
                    DAT_00559c84 = 0;
                    PlayBuffer(0x19, 0, 0);
                }
            }
            break;
        }

        const bool guildDeleteCodeDialog =
            DAT_083a7c24 == 126 || DAT_083a7c24 == 152;
        if (DAT_00559c84 || GoldInputEnable || GuildInputEnable || guildDeleteCodeDialog) {
            BYTE c = (BYTE)event.character;
            if (GoldInputEnable && (c < '0' || c > '9')) break;
            if (c >= 0x20 && c != 0x7F) {
                int maxLen = _InputTextMaxArr[slot];
                if (maxLen <= 0) maxLen = 10;
                if (len < maxLen) {
                    buf[len]   = (char)c;
                    lens[slot] = (DWORD)(len + 1);
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

