#include "stdafx.h"
#include "Platform/Text.h"
#include "UI/JewelBank.h"
#include "UI/UIState.h"
#include "Net/Protocol/GameServerProtocol.h"
#include "globals.h"
#include "structs.h"
#include "functions.h"
#include <GL/gl.h>

void __cdecl CloseInventoryRelatedWindows(void);
extern "C" void   __cdecl RenderInventoryInterface(int StartX, int StartY, int Flag);
extern "C" SIZE*  __cdecl RenderCenteredText(int x, int y, const char* text);
extern "C" double __cdecl RenderNumber2D(float, float, int, float, float);
extern "C" void   __cdecl RenderTipText(int sx, int sy, const char* Text);
int __cdecl GetItemCount(int siType, int iLevel);

bool g_bJewelBankWindowOpen = false;

static DWORD s_JewelBankBless = 0;
static DWORD s_JewelBankSoul  = 0;
static DWORD s_JewelBankChaos = 0;
static DWORD s_JewelBankLife  = 0;
static char s_JewelBankStatus[64] = { 0 };
static DWORD s_StatusColor = 0;

#define JB_WindowWidth       ((int)gWindow.GetWidth())
#define JB_WindowHeight      ((int)gWindow.GetHeight())

#define JB_MouseX            ((int)DAT_083a427c)
#define JB_MouseY            ((int)DAT_083a4278)
#define JB_MouseLButtonPush  DAT_083a4124
#define JB_MouseLButtonPop   DAT_083a413c
#define JB_MouseLButton      DAT_083a42c4
#define JB_MouseUpdateTime   DAT_07e11d28
#define JB_MouseUpdateMax    DAT_00559bec

static const int kStartX = 450;
static const int kStartY = 0;

static int RealWidth(int w)
{
    int winW = JB_WindowWidth;
    return w * (winW ? winW : 640) / 640;
}

static bool IsWorkZone(int x, int y, int w, int h)
{
    return JB_MouseX >= x && JB_MouseX <= (x + w) && JB_MouseY >= y && JB_MouseY <= (y + h);
}

static int CenterTextPosY(const char* text, int y)
{
    SIZE sz = { 0, 0 };
    gFont.MeasureText(gFont.GetTextDC(), text, Platform::TextLength(text), &sz);
    int winH = JB_WindowHeight ? JB_WindowHeight : 480;
    return y - ((480 * sz.cy / winH) >> 1);
}

static bool ConsumeClick(void)
{
    if (JB_MouseLButton && JB_MouseLButtonPush) {
        JB_MouseLButtonPush = 0;
        JB_MouseUpdateTime = 0;
        JB_MouseUpdateMax = 6;
        return true;
    }
    return false;
}

static void RenderButton(float x, float y, float w, float h, const char* text, bool enabled = true, bool highlight = false)
{
    GL_ResetState();
    if (!enabled) {
        glColor3f(0.30f, 0.30f, 0.30f);
        GL_DrawTexture(240, x, y, w, h, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
    } else {
        glColor3f(1.0f, 1.0f, 1.0f);
        GL_DrawTexture(240, x, y, w, h, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
        if (IsWorkZone((int)x, (int)y, (int)w, (int)h) || highlight) {
            glColor3f(0.85f, 0.65f, 0.25f);
            GL_SetBlendAdditive();
            GL_DrawTexture(240, x, y, w, h, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
            glColor3f(1.0f, 1.0f, 1.0f);
            GL_ResetState();
        }
    }
    EnableAlphaTest(true);
    m_dwBackColor = Color4b(255, 255, 255, 0);
    m_dwTextColor = enabled ? (DWORD)Color4b(245, 245, 245, 255) : (DWORD)Color4b(120, 120, 120, 255);
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
    RenderText((int)x, CenterTextPosY(text, (int)(y + h / 2.0f)), (char*)text, RealWidth((int)w), 1, nullptr);
    glColor3f(1.0f, 1.0f, 1.0f);
}

#pragma pack(push, 1)
struct PMSG_JEWEL_BANK_ACTION_SEND
{
    BYTE type;      // 0xC1
    BYTE size;      // 10
    BYTE head;      // 0xF8
    BYTE subh;      // 0x21
    BYTE Action;    // 1=Deposit, 2=Withdraw
    BYTE JewelType; // 0..3
    DWORD Count;    // count
};
#pragma pack(pop)

static void SendJewelBankAction(BYTE action, BYTE jewelType, DWORD count)
{
    PMSG_JEWEL_BANK_ACTION_SEND pkt;
    pkt.type = 0xC1;
    pkt.size = sizeof(pkt);
    pkt.head = 0xF8;
    pkt.subh = 0x21;
    pkt.Action = action;
    pkt.JewelType = jewelType;
    pkt.Count = count;
    gNetwork.SendC1((BYTE*)&pkt, sizeof(pkt));
}

extern "C" void __cdecl JewelBank_Close(void)
{
    g_bJewelBankWindowOpen = false;
    InventoryOpened = 0;
    CloseInventoryRelatedWindows();

    BYTE pkt[4] = { 0xC1, 0x04, 0xF8, 0x23 };
    gNetwork.SendC1(pkt, sizeof(pkt));
}

extern "C" void __cdecl JewelBank_ResetCharacter(void)
{
    g_bJewelBankWindowOpen = false;
    s_JewelBankBless = 0;
    s_JewelBankSoul = 0;
    s_JewelBankChaos = 0;
    s_JewelBankLife = 0;
    s_JewelBankStatus[0] = '\0';
}

extern "C" void __cdecl JewelBank_ResetSession(void)
{
    JewelBank_ResetCharacter();
}

extern "C" void __cdecl JewelBank_Recv(BYTE* Msg, int Size)
{
    if (Size < 4) return;
    BYTE subh = Msg[3];

    if (subh == 0x20) // Open / Info
    {
        if (Size >= 20) {
            s_JewelBankBless = *(DWORD*)(Msg + 4);
            s_JewelBankSoul  = *(DWORD*)(Msg + 8);
            s_JewelBankChaos = *(DWORD*)(Msg + 12);
            s_JewelBankLife  = *(DWORD*)(Msg + 16);
            g_bJewelBankWindowOpen = true;
            InventoryOpened = 0;
            CloseInventoryRelatedWindows();
            Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Banco de Joyas conectado", Platform::TruncateText);
            s_StatusColor = (DWORD)Color4b(180, 230, 255, 255);
        }
    }
    else if (subh == 0x22) // Result update
    {
        if (Size >= 27) {
            BYTE result = Msg[4];
            BYTE action = Msg[5];
            BYTE jewel  = Msg[6];
            DWORD count = *(DWORD*)(Msg + 7);
            s_JewelBankBless = *(DWORD*)(Msg + 11);
            s_JewelBankSoul  = *(DWORD*)(Msg + 15);
            s_JewelBankChaos = *(DWORD*)(Msg + 19);
            s_JewelBankLife  = *(DWORD*)(Msg + 23);

            if (result == 0) {
                if (action == 1) {
                    Platform::TextFormatSecure(s_JewelBankStatus, "Depositadas +%u joya(s) con exito", count);
                } else {
                    Platform::TextFormatSecure(s_JewelBankStatus, "Retiradas +%u joya(s) al inventario", count);
                }
                s_StatusColor = (DWORD)Color4b(100, 255, 120, 255);
            } else if (result == 1) {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Saldo insuficiente en el banco.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            } else if (result == 2) {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Inventario lleno: sin espacio libre.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 170, 50, 255);
            } else if (result == 4) {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Operacion en curso... espera.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 255, 100, 255);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }
    }
}

extern "C" void __cdecl RenderJewelBankWindow(void)
{
    if (!g_bJewelBankWindowOpen) return;

    EnableAlphaTest(true);
    glColor3f(1.0f, 1.0f, 1.0f);

    // Panel skeleton frame (x = 450, y = 0, width = 190, height = 433)
    RenderInventoryInterface(kStartX, kStartY, 0);

    // Dark backing for list zone
    GL_ResetState();
    glColor4f(0.08f, 0.08f, 0.10f, 0.90f);
    GL_DrawTexture(240, (float)kStartX + 10, (float)kStartY + 40, 170.0f, 250.0f, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
    glColor3f(1.0f, 1.0f, 1.0f);
    EnableAlphaTest(true);

    // 1. Title Header
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
    m_dwBackColor = Color4b(20, 20, 20, 255);
    m_dwTextColor = (DWORD)Color4b(60, 240, 100, 255);
    int y = kStartY + 12;
    RenderText(kStartX + 20, CenterTextPosY("Banco de Joyas", y), (char*)"Banco de Joyas", RealWidth(150), 1, nullptr);

    // Subtitle
    y = kStartY + 28;
    m_dwBackColor = Color4b(255, 255, 255, 0);
    m_dwTextColor = (DWORD)Color4b(255, 215, 0, 255);
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
    RenderText(kStartX + 15, CenterTextPosY("Deposito y Retiro", y), (char*)"Deposito y Retiro", RealWidth(160), 1, nullptr);

    // 4 Jewels in the order of reference image: Chaos, Bless, Soul, Life
    static const struct {
        BYTE type;
        const char* name;
        int itemIndex;
        DWORD* pBalance;
        DWORD gemColor;
    } jewels[4] = {
        { 2, "Jewel of Chaos", 399, &s_JewelBankChaos, (DWORD)Color4b(255, 160, 50, 255) },
        { 0, "Jewel of Bless", 461, &s_JewelBankBless, (DWORD)Color4b(255, 240, 100, 255) },
        { 1, "Jewel of Soul",  462, &s_JewelBankSoul,  (DWORD)Color4b(120, 210, 255, 255) },
        { 3, "Jewel of Life",  464, &s_JewelBankLife,  (DWORD)Color4b(255, 110, 180, 255) },
    };

    y = kStartY + 48;
    for (int i = 0; i < 4; ++i)
    {
        int invCount = GetItemCount(jewels[i].itemIndex, -1);
        DWORD bankBal = *jewels[i].pBalance;

        // Gem bullet
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
        m_dwTextColor = jewels[i].gemColor;
        RenderText(kStartX + 14, y + 2, (char*)"*", RealWidth(14), 0, nullptr);

        // Jewel Name
        m_dwTextColor = (DWORD)Color4b(235, 235, 235, 255);
        RenderText(kStartX + 26, y + 2, (char*)jewels[i].name, RealWidth(150), 0, nullptr);

        // Balance Counters: "Banco: %u  |  Inv: %d"
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
        char buf[64];
        Platform::TextFormatSecure(buf, "Banco: %u  |  Inv: %d", bankBal, invCount);
        m_dwTextColor = (DWORD)Color4b(190, 205, 220, 255);
        RenderText(kStartX + 20, y + 15, buf, RealWidth(150), 0, nullptr);

        // Action Buttons:
        // [-1] Retirar 1 | [-10] | [+1] Depositar 1 | [+Todo] Depositar todo
        float by = (float)(y + 28);
        RenderButton((float)(kStartX + 16), by, 32, 16, "-1", bankBal >= 1);
        RenderButton((float)(kStartX + 52), by, 36, 16, "-10", bankBal >= 10);
        RenderButton((float)(kStartX + 92), by, 32, 16, "+1", invCount >= 1);
        RenderButton((float)(kStartX + 128), by, 46, 16, "+Todo", invCount >= 1);

        y += 56;
    }

    // Status Message Box
    int sy = kStartY + 276;
    GL_ResetState();
    glColor3f(0.12f, 0.12f, 0.15f);
    GL_DrawTexture(240, (float)kStartX + 12, (float)sy, 166.0f, 26.0f, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
    glColor3f(1.0f, 1.0f, 1.0f);
    EnableAlphaTest(true);

    if (s_JewelBankStatus[0] != '\0') {
        m_dwBackColor = Color4b(255, 255, 255, 0);
        m_dwTextColor = s_StatusColor;
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
        RenderText(kStartX + 16, sy + 7, s_JewelBankStatus, RealWidth(158), 1, nullptr);
    }

    // Bottom Action Button 1: "¡Depositar todas!" (Like Image 4 red arrow)
    RenderButton((float)(kStartX + 22), (float)(kStartY + 312), 146, 26, "Depositar Todo", true);

    // Bottom Action Button 2: "Cerrar Banco"
    RenderButton((float)(kStartX + 22), (float)(kStartY + 346), 146, 24, "Cerrar Banco", true);

    // Close button [X] at bottom left
    const int cx = kStartX + 25, cy = kStartY + 395;
    glColor3f(1.0f, 1.0f, 1.0f);
    GL_DrawTexture(280, (float)cx, (float)cy, 24, 24, 0, 0, 0.75f, 0.75f, 1, 1);
    if (IsWorkZone(cx, cy, 24, 24)) {
        GL_SetBlendAdditive();
        GL_DrawTexture(280, (float)cx, (float)cy, 24, 24, 0, 0, 0.75f, 0.75f, 1, 1);
        GL_ResetState();
    }
}

extern "C" void __cdecl CheckJewelBankWindow(void)
{
    if (!g_bJewelBankWindowOpen) return;

    if (JB_MouseX >= kStartX && JB_MouseX < (kStartX + 190) && JB_MouseY >= 0 && JB_MouseY < 433) {
        UIState::CaptureMouseForUI();
    }

    static const struct {
        BYTE type;
        int itemIndex;
        DWORD* pBalance;
    } jewels[4] = {
        { 2, 399, &s_JewelBankChaos },
        { 0, 461, &s_JewelBankBless },
        { 1, 462, &s_JewelBankSoul  },
        { 3, 464, &s_JewelBankLife  },
    };

    int y = kStartY + 48;
    for (BYTE i = 0; i < 4; ++i)
    {
        int invCount = GetItemCount(jewels[i].itemIndex, -1);
        DWORD bal = *jewels[i].pBalance;
        BYTE jType = jewels[i].type;

        float by = (float)(y + 28);

        // [-1] Withdraw 1
        if (IsWorkZone(kStartX + 16, (int)by, 32, 16) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (bal >= 1) {
                SendJewelBankAction(2, jType, 1);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Saldo insuficiente en el banco.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }

        // [-10] Withdraw 10
        if (IsWorkZone(kStartX + 52, (int)by, 36, 16) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (bal >= 10) {
                SendJewelBankAction(2, jType, 10);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Se requieren al menos 10 joyas.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }

        // [+1] Deposit 1
        if (IsWorkZone(kStartX + 92, (int)by, 32, 16) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (invCount >= 1) {
                SendJewelBankAction(1, jType, 1);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }

        // [+Todo] Deposit All of this jewel
        if (IsWorkZone(kStartX + 128, (int)by, 46, 16) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (invCount >= 1) {
                SendJewelBankAction(1, jType, 0xFFFFFFFF);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }

        y += 56;
    }

    // Bottom Action Button 1: "Depositar Todo"
    if (IsWorkZone(kStartX + 22, kStartY + 312, 146, 26) && ConsumeClick()) {
        PlayBuffer(25, 0, 0);
        bool depositedAny = false;
        for (int i = 0; i < 4; ++i) {
            int count = GetItemCount(jewels[i].itemIndex, -1);
            if (count > 0) {
                SendJewelBankAction(1, jewels[i].type, 0xFFFFFFFF);
                depositedAny = true;
                break;
            }
        }
        if (!depositedAny) {
            Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes joyas para depositar.", Platform::TruncateText);
            s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
        }
    }

    // Bottom Action Button 2: "Cerrar Banco"
    if (IsWorkZone(kStartX + 22, kStartY + 346, 146, 24) && ConsumeClick()) {
        PlayBuffer(25, 0, 0);
        JewelBank_Close();
    }

    // Close button [X] at bottom left
    const int cx = kStartX + 25, cy = kStartY + 395;
    if (IsWorkZone(cx, cy, 24, 24) && ConsumeClick()) {
        PlayBuffer(25, 0, 0);
        JewelBank_Close();
    }
}
