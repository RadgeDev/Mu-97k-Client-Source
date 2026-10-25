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
static DWORD s_JewelBankCreation = 0;
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

static void RenderArrowButton(float x, float y, float w, float h, bool isUp, bool enabled)
{
    GL_ResetState();
    bool hovered = IsWorkZone((int)x, (int)y, (int)w, (int)h);

    // Button body
    if (!enabled) {
        glColor4f(0.12f, 0.12f, 0.14f, 0.70f);
        GL_DrawRect(x, y, w, h);
        glColor4f(0.25f, 0.25f, 0.28f, 0.50f);
    } else if (hovered) {
        glColor4f(0.22f, 0.25f, 0.32f, 0.95f);
        GL_DrawRect(x, y, w, h);
        glColor4f(0.90f, 0.75f, 0.30f, 0.90f);
    } else {
        glColor4f(0.16f, 0.17f, 0.20f, 0.90f);
        GL_DrawRect(x, y, w, h);
        glColor4f(0.40f, 0.45f, 0.55f, 0.60f);
    }

    // 1px Border
    GL_DrawRect(x, y, w, 1.0f);
    GL_DrawRect(x, y + h - 1.0f, w, 1.0f);
    GL_DrawRect(x, y, 1.0f, h);
    GL_DrawRect(x + w - 1.0f, y, 1.0f, h);

    // Vector Triangle Icon
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    if (!enabled) {
        glColor4f(0.35f, 0.35f, 0.38f, 0.60f);
    } else if (hovered) {
        if (isUp) {
            glColor4f(0.40f, 1.00f, 0.50f, 1.0f); // Bright Green
        } else {
            glColor4f(1.00f, 0.75f, 0.30f, 1.0f); // Bright Gold/Orange
        }
    } else {
        if (isUp) {
            glColor4f(0.30f, 0.85f, 0.40f, 0.9f); // Green
        } else {
            glColor4f(0.90f, 0.60f, 0.25f, 0.9f); // Gold/Orange
        }
    }

    if (isUp) {
        glVertex2f(cx, cy - 3.5f);
        glVertex2f(cx - 3.5f, cy + 2.5f);
        glVertex2f(cx + 3.5f, cy + 2.5f);
    } else {
        glVertex2f(cx - 3.5f, cy - 2.5f);
        glVertex2f(cx + 3.5f, cy - 2.5f);
        glVertex2f(cx, cy + 3.5f);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    EnableAlphaTest(true);
}

#pragma pack(push, 1)
struct PMSG_JEWEL_BANK_ACTION_SEND
{
    BYTE type;      // 0xC1
    BYTE size;      // 10
    BYTE head;      // 0xF8
    BYTE subh;      // 0x21
    BYTE Action;    // 1=Deposit, 2=Withdraw
    BYTE JewelType; // 0..4
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
    s_JewelBankCreation = 0;
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
        if (Size >= 24) {
            s_JewelBankBless    = *(DWORD*)(Msg + 4);
            s_JewelBankSoul     = *(DWORD*)(Msg + 8);
            s_JewelBankChaos    = *(DWORD*)(Msg + 12);
            s_JewelBankLife     = *(DWORD*)(Msg + 16);
            s_JewelBankCreation = *(DWORD*)(Msg + 20);
            g_bJewelBankWindowOpen = true;
            InventoryOpened = 0;
            CloseInventoryRelatedWindows();
            Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Banco de Joyas conectado", Platform::TruncateText);
            s_StatusColor = (DWORD)Color4b(180, 230, 255, 255);
        }
    }
    else if (subh == 0x22) // Result update
    {
        if (Size >= 31) {
            BYTE result = Msg[4];
            BYTE action = Msg[5];
            BYTE jewel  = Msg[6];
            DWORD count = *(DWORD*)(Msg + 7);
            s_JewelBankBless    = *(DWORD*)(Msg + 11);
            s_JewelBankSoul     = *(DWORD*)(Msg + 15);
            s_JewelBankChaos    = *(DWORD*)(Msg + 19);
            s_JewelBankLife     = *(DWORD*)(Msg + 23);
            s_JewelBankCreation = *(DWORD*)(Msg + 27);

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

// 5 Jewels in exact order requested by user:
// 1. Jewel of Chaos
// 2. Jewel of Bless
// 3. Jewel of Soul
// 4. Jewel of Life
// 5. Creation
struct JewelBankDef {
    BYTE type;
    const char* name;
    int itemIndex;
    DWORD* pBalance;
    unsigned char r, g, b;
};

static const JewelBankDef s_Jewels[5] = {
    { 2, "Jewel of Chaos", 399, &s_JewelBankChaos,    255, 205, 30  }, // 🟡 Yellow/Gold
    { 0, "Jewel of Bless", 461, &s_JewelBankBless,    200, 110, 255 }, // 🟣 Purple
    { 1, "Jewel of Soul",  462, &s_JewelBankSoul,     255, 75,  75  }, // 🔴 Red
    { 3, "Jewel of Life",  464, &s_JewelBankLife,     210, 130, 50  }, // 🟫 Brown/Amber
    { 4, "Creation",       470, &s_JewelBankCreation, 195, 150, 85  }, // 🟫 Warm Gold/Brown
};

extern "C" void __cdecl RenderJewelBankWindow(void)
{
    if (!g_bJewelBankWindowOpen) return;

    EnableAlphaTest(true);
    glColor3f(1.0f, 1.0f, 1.0f);

    // Panel skeleton frame (x = 450, y = 0, width = 190, height = 433)
    RenderInventoryInterface(kStartX, kStartY, 0);

    // Dark slate background for clean table zone
    GL_ResetState();
    glColor4f(0.06f, 0.07f, 0.09f, 0.92f);
    GL_DrawTexture(240, (float)kStartX + 10, (float)kStartY + 38, 170.0f, 260.0f, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
    glColor3f(1.0f, 1.0f, 1.0f);
    EnableAlphaTest(true);

    // 1. Title Header: "Banco de Joyas"
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
    m_dwBackColor = Color4b(20, 20, 20, 255);
    m_dwTextColor = (DWORD)Color4b(255, 230, 140, 255);
    int y = kStartY + 12;
    RenderText(kStartX + 15, CenterTextPosY("Banco de Joyas", y), (char*)"Banco de Joyas", RealWidth(160), 1, nullptr);

    // Subtitle: "Deposito de joyas"
    y = kStartY + 26;
    m_dwBackColor = Color4b(255, 255, 255, 0);
    m_dwTextColor = (DWORD)Color4b(180, 220, 255, 255);
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
    RenderText(kStartX + 15, CenterTextPosY("Deposito de joyas", y), (char*)"Deposito de joyas", RealWidth(160), 1, nullptr);

    // Column Labels: Joya | Banco/Inv | Accion
    y = kStartY + 39;
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
    m_dwTextColor = (DWORD)Color4b(130, 145, 165, 255);
    RenderText(kStartX + 25, y, (char*)"Joya", RealWidth(50), 0, nullptr);
    RenderText(kStartX + 96, y, (char*)"Banco/Inv", RealWidth(42), 1, nullptr);
    RenderText(kStartX + 140, y, (char*)"Accion", RealWidth(42), 1, nullptr);

    // Top table separator line ====================
    GL_ResetState();
    glColor4f(0.50f, 0.55f, 0.65f, 0.85f);
    GL_DrawRect((float)(kStartX + 12), (float)(kStartY + 51), 166.0f, 1.5f);
    EnableAlphaTest(true);

    char tipBuf[64] = { 0 };
    int tipX = 0, tipY = 0;

    int rowY = kStartY + 54;
    for (int i = 0; i < 5; ++i)
    {
        int invCount = GetItemCount(s_Jewels[i].itemIndex, -1);
        DWORD bankBal = *s_Jewels[i].pBalance;

        // 1. Jewel pip (colored gem pip)
        GL_ResetState();
        glColor4ub(s_Jewels[i].r, s_Jewels[i].g, s_Jewels[i].b, 255);
        GL_DrawRect((float)(kStartX + 15), (float)(rowY + 9), 6.0f, 6.0f);
        // Pip specular highlight
        glColor4ub(255, 255, 255, 200);
        GL_DrawRect((float)(kStartX + 16), (float)(rowY + 10), 2.0f, 2.0f);
        EnableAlphaTest(true);

        // 2. Jewel Name
        m_dwBackColor = Color4b(255, 255, 255, 0);
        m_dwTextColor = (DWORD)Color4b(235, 235, 240, 255);
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
        RenderText(kStartX + 25, rowY + 6, (char*)s_Jewels[i].name, RealWidth(72), 0, nullptr);

        // 3. Count: "0/0" (Banco/Inv)
        char countBuf[32];
        Platform::TextFormatSecure(countBuf, "%u/%d", bankBal, invCount);
        if (bankBal > 0 || invCount > 0) {
            m_dwTextColor = (DWORD)Color4b(255, 225, 120, 255);
        } else {
            m_dwTextColor = (DWORD)Color4b(145, 155, 170, 255);
        }
        RenderText(kStartX + 96, rowY + 6, countBuf, RealWidth(42), 1, nullptr);

        // 4. Action Buttons: [ ▲ ] and [ ▼ ]
        float btnDepositX = (float)(kStartX + 140);
        float btnWithdrawX = (float)(kStartX + 162);
        float btnY = (float)(rowY + 6);

        RenderArrowButton(btnDepositX, btnY, 19.0f, 18.0f, true, invCount >= 1);
        RenderArrowButton(btnWithdrawX, btnY, 19.0f, 18.0f, false, bankBal >= 1);

        // Check hover for tooltips
        if (IsWorkZone((int)btnDepositX, (int)btnY, 19, 18)) {
            Platform::TextCopySecure(tipBuf, sizeof(tipBuf), "Depositar 1 (Shift: Todo)", Platform::TruncateText);
            tipX = kStartX + 50;
            tipY = rowY - 12;
        } else if (IsWorkZone((int)btnWithdrawX, (int)btnY, 19, 18)) {
            Platform::TextCopySecure(tipBuf, sizeof(tipBuf), "Retirar 1 (Shift: 10)", Platform::TruncateText);
            tipX = kStartX + 70;
            tipY = rowY - 12;
        }

        // Horizontal row divider line --------------------
        GL_ResetState();
        if (i < 4) {
            glColor4f(0.25f, 0.28f, 0.35f, 0.60f);
            GL_DrawRect((float)(kStartX + 14), (float)(rowY + 31), 162.0f, 1.0f);
        }
        EnableAlphaTest(true);

        rowY += 32;
    }

    // Bottom table separator line ====================
    GL_ResetState();
    glColor4f(0.50f, 0.55f, 0.65f, 0.85f);
    GL_DrawRect((float)(kStartX + 12), (float)(rowY + 1), 166.0f, 1.5f);
    EnableAlphaTest(true);

    // Status Message Box
    int sy = rowY + 7;
    GL_ResetState();
    glColor3f(0.10f, 0.11f, 0.14f);
    GL_DrawTexture(240, (float)kStartX + 12, (float)sy, 166.0f, 24.0f, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
    glColor3f(1.0f, 1.0f, 1.0f);
    EnableAlphaTest(true);

    if (s_JewelBankStatus[0] != '\0') {
        m_dwBackColor = Color4b(255, 255, 255, 0);
        m_dwTextColor = s_StatusColor;
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
        RenderText(kStartX + 15, sy + 6, s_JewelBankStatus, RealWidth(160), 1, nullptr);
    }

    // Bottom Action Button 1: "Depositar Todo"
    RenderButton((float)(kStartX + 20), (float)(sy + 30), 150.0f, 24.0f, "Depositar Todo", true);

    // Bottom Action Button 2: "Cerrar"
    RenderButton((float)(kStartX + 20), (float)(sy + 58), 150.0f, 24.0f, "Cerrar", true);

    // Close button [X] at bottom left
    const int cx = kStartX + 25, cy = kStartY + 395;
    glColor3f(1.0f, 1.0f, 1.0f);
    GL_DrawTexture(280, (float)cx, (float)cy, 24, 24, 0, 0, 0.75f, 0.75f, 1, 1);
    if (IsWorkZone(cx, cy, 24, 24)) {
        GL_SetBlendAdditive();
        GL_DrawTexture(280, (float)cx, (float)cy, 24, 24, 0, 0, 0.75f, 0.75f, 1, 1);
        GL_ResetState();
    }

    // Floating Tooltip (drawn on top)
    if (tipBuf[0] != '\0') {
        RenderTipText(tipX, tipY, tipBuf);
    }
}

extern "C" void __cdecl CheckJewelBankWindow(void)
{
    if (!g_bJewelBankWindowOpen) return;

    if (JB_MouseX >= kStartX && JB_MouseX < (kStartX + 190) && JB_MouseY >= 0 && JB_MouseY < 433) {
        UIState::CaptureMouseForUI();
    }

    int rowY = kStartY + 54;
    for (BYTE i = 0; i < 5; ++i)
    {
        int invCount = GetItemCount(s_Jewels[i].itemIndex, -1);
        DWORD bal = *s_Jewels[i].pBalance;
        BYTE jType = s_Jewels[i].type;

        float btnDepositX = (float)(kStartX + 140);
        float btnWithdrawX = (float)(kStartX + 162);
        float btnY = (float)(rowY + 6);

        // [ ▲ ] Deposit
        if (IsWorkZone((int)btnDepositX, (int)btnY, 19, 18) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (invCount >= 1) {
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                DWORD count = shift ? 0xFFFFFFFF : 1;
                SendJewelBankAction(1, jType, count);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }

        // [ ▼ ] Withdraw
        if (IsWorkZone((int)btnWithdrawX, (int)btnY, 19, 18) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (bal >= 1) {
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                DWORD count = shift ? 10 : 1;
                SendJewelBankAction(2, jType, count);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Saldo insuficiente en el banco.", Platform::TruncateText);
                s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
            }
        }

        rowY += 32;
    }

    int sy = rowY + 7;

    // Bottom Action Button 1: "Depositar Todo"
    if (IsWorkZone(kStartX + 20, sy + 30, 150, 24) && ConsumeClick()) {
        PlayBuffer(25, 0, 0);
        bool depositedAny = false;
        for (int i = 0; i < 5; ++i) {
            int count = GetItemCount(s_Jewels[i].itemIndex, -1);
            if (count > 0) {
                SendJewelBankAction(1, s_Jewels[i].type, 0xFFFFFFFF);
                depositedAny = true;
                break;
            }
        }
        if (!depositedAny) {
            Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes joyas para depositar.", Platform::TruncateText);
            s_StatusColor = (DWORD)Color4b(255, 90, 90, 255);
        }
    }

    // Bottom Action Button 2: "Cerrar"
    if (IsWorkZone(kStartX + 20, sy + 58, 150, 24) && ConsumeClick()) {
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
