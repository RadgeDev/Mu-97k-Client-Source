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
static DWORD s_JewelBankSoul = 0;
static DWORD s_JewelBankChaos = 0;
static DWORD s_JewelBankLife = 0;
static char s_JewelBankStatus[64] = { 0 };
static DWORD s_StatusColor = 0;

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
    int winW = *(int*)0x005E26E4;
    return w * (winW ? winW : 640) / 640;
}

static bool IsWorkZone(int x, int y, int w, int h)
{
    return JB_MouseX >= x && JB_MouseX <= x + w && JB_MouseY >= y && JB_MouseY <= y + h;
}

static int CenterTextPosY(const char* text, int y)
{
    SIZE sz = { 0, 0 };
    gFont.MeasureText(gFont.GetTextDC(), text, Platform::TextLength(text), &sz);
    int winH = *(int*)0x005E26E8;
    return y - ((480 * sz.cy / (winH ? winH : 480)) >> 1);
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

static void RenderButton(float x, float y, float w, float h, const char* text, bool enabled = true)
{
    GL_ResetState();
    if (!enabled) {
        glColor3f(0.35f, 0.35f, 0.35f);
        GL_DrawTexture(240, x, y, w, h, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
    } else {
        glColor3f(1.0f, 1.0f, 1.0f);
        GL_DrawTexture(240, x, y, w, h, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
        if (IsWorkZone((int)x, (int)y, (int)w, (int)h)) {
            glColor3f(0.85f, 0.65f, 0.3f);
            GL_SetBlendAdditive();
            GL_DrawTexture(240, x, y, w, h, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
            glColor3f(1.0f, 1.0f, 1.0f);
            GL_ResetState();
        }
    }
    EnableAlphaTest(true);
    m_dwBackColor = Color4b(255, 255, 255, 0);
    m_dwTextColor = enabled ? Color4b(240, 240, 240, 255) : Color4b(130, 130, 130, 255);
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
    RenderText((int)x, CenterTextPosY(text, (int)(y + h / 2)), (char*)text, RealWidth((int)w), 1, nullptr);
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

void JewelBank_Close(void)
{
    g_bJewelBankWindowOpen = false;
    InventoryOpened = 0;
    CloseInventoryRelatedWindows();

    // Send close packet 0xF8, 0x23 to server
    BYTE pkt[4] = { 0xC1, 0x04, 0xF8, 0x23 };
    gNetwork.SendC1(pkt, sizeof(pkt));
}

void JewelBank_ResetCharacter(void)
{
    g_bJewelBankWindowOpen = false;
    s_JewelBankBless = 0;
    s_JewelBankSoul = 0;
    s_JewelBankChaos = 0;
    s_JewelBankLife = 0;
    s_JewelBankStatus[0] = '\0';
}

void JewelBank_ResetSession(void)
{
    JewelBank_ResetCharacter();
}

void JewelBank_Recv(BYTE* Msg, int Size)
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
            InventoryOpened = 1;
            Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Bienvenido al Banco de Joyas", Platform::TruncateText);
            s_StatusColor = Color4b(200, 230, 255, 255);
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
                    Platform::TextFormatSecure(s_JewelBankStatus, "Depositadas %u joya(s).", count);
                } else {
                    Platform::TextFormatSecure(s_JewelBankStatus, "Retiradas %u joya(s).", count);
                }
                s_StatusColor = Color4b(120, 255, 120, 255);
            } else if (result == 1) {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Saldo insuficiente en el banco.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 100, 100, 255);
            } else if (result == 2) {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Sin espacio libre en inventario.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 160, 50, 255);
            } else if (result == 4) {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Operacion en curso... espera.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 255, 100, 255);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 100, 100, 255);
            }
        }
    }
}

void RenderJewelBankWindow(void)
{
    if (!g_bJewelBankWindowOpen) return;

    EnableAlphaTest(true);
    glColor3f(1.0f, 1.0f, 1.0f);
    RenderInventoryInterface(kStartX, kStartY, 0);

    // Title Header
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
    m_dwBackColor = Color4b(20, 20, 20, 255);
    m_dwTextColor = Color4b(255, 215, 0, 255);
    int y = kStartY + 12;
    RenderText(kStartX + 20, CenterTextPosY("BANCO DE JOYAS", y), (char*)"BANCO DE JOYAS", RealWidth(150), 1, nullptr);

    y += 11;
    m_dwBackColor = Color4b(255, 255, 255, 0);
    m_dwTextColor = Color4b(170, 210, 255, 255);
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
    RenderText(kStartX + 15, CenterTextPosY("Saldo Compartido por Cuenta", y), (char*)"Saldo Compartido por Cuenta", RealWidth(160), 1, nullptr);

    // Jewel Definitions
    static const struct {
        const char* name;
        int itemIndex;
        DWORD* pBalance;
        DWORD textColor;
    } jewels[4] = {
        { "Jewel of Bless", 461, &s_JewelBankBless, (DWORD)Color4b(255, 240, 120, 255) },
        { "Jewel of Soul",  462, &s_JewelBankSoul,  (DWORD)Color4b(160, 200, 255, 255) },
        { "Jewel of Chaos", 399, &s_JewelBankChaos, (DWORD)Color4b(255, 160, 120, 255) },
        { "Jewel of Life",  464, &s_JewelBankLife,  (DWORD)Color4b(255, 120, 180, 255) },
    };

    y = kStartY + 45;
    for (int i = 0; i < 4; ++i)
    {
        int invCount = GetItemCount(jewels[i].itemIndex, -1);

        // Header for jewel
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_BOLD));
        m_dwBackColor = Color4b(255, 255, 255, 0);
        m_dwTextColor = jewels[i].textColor;
        RenderText(kStartX + 16, y, (char*)jewels[i].name, RealWidth(160), 0, nullptr);

        // Balances
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
        char buf[64];
        Platform::TextFormatSecure(buf, "Banco: %u  |  Inv: %d", *jewels[i].pBalance, invCount);
        m_dwTextColor = Color4b(220, 220, 220, 255);
        RenderText(kStartX + 16, y + 14, buf, RealWidth(160), 0, nullptr);

        // Action Buttons: [-1] [-10] [+1] [+Todo]
        float bx = (float)(kStartX + 14);
        float by = (float)(y + 29);
        RenderButton(bx,       by, 34, 17, "-1", *jewels[i].pBalance >= 1);
        RenderButton(bx + 38,  by, 40, 17, "-10", *jewels[i].pBalance >= 10);
        RenderButton(bx + 82,  by, 34, 17, "+1", invCount >= 1);
        RenderButton(bx + 120, by, 44, 17, "+Todo", invCount >= 1);

        y += 56;
    }

    // Status Message Box
    if (s_JewelBankStatus[0] != '\0') {
        int sy = kStartY + 278;
        GL_DrawTexture(240, (float)kStartX + 14, (float)sy, 162, 38, 0.0f, 0.0f, 213.0f / 256.0f, 1.0f, 1, 1);
        EnableAlphaTest(true);
        m_dwBackColor = Color4b(255, 255, 255, 0);
        m_dwTextColor = s_StatusColor;
        gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
        RenderText(kStartX + 18, sy + 6, s_JewelBankStatus, RealWidth(154), 1, nullptr);
    }

    // Instructions note
    int iy = kStartY + 325;
    m_dwBackColor = Color4b(255, 255, 255, 0);
    m_dwTextColor = Color4b(160, 160, 160, 255);
    gFont.SelectFont(gFont.GetTextDC(), gFont.GetFont(FONT_NORMAL));
    RenderText(kStartX + 16, iy, (char*)"[-] Retirar joyas al inventario", RealWidth(160), 0, nullptr);
    RenderText(kStartX + 16, iy + 13, (char*)"[+] Depositar joyas al banco", RealWidth(160), 0, nullptr);
    RenderText(kStartX + 16, iy + 26, (char*)"Disponible para todos tus PJs", RealWidth(160), 0, nullptr);

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

void CheckJewelBankWindow(void)
{
    if (!g_bJewelBankWindowOpen) return;

    if (JB_MouseX >= kStartX && JB_MouseX < (kStartX + 190) && JB_MouseY >= 0 && JB_MouseY < 433) {
        UIState::CaptureMouseForUI();
    }

    static const int jewelItems[4] = { 461, 462, 399, 464 };
    static const DWORD* jewelBalances[4] = { &s_JewelBankBless, &s_JewelBankSoul, &s_JewelBankChaos, &s_JewelBankLife };

    int y = kStartY + 45;
    for (BYTE i = 0; i < 4; ++i)
    {
        int invCount = GetItemCount(jewelItems[i], -1);
        DWORD bal = *jewelBalances[i];

        float bx = (float)(kStartX + 14);
        float by = (float)(y + 29);

        // [-1]
        if (IsWorkZone((int)bx, (int)by, 34, 17) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (bal >= 1) {
                SendJewelBankAction(2, i, 1);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Saldo insuficiente.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 100, 100, 255);
            }
        }

        // [-10]
        if (IsWorkZone((int)(bx + 38), (int)by, 40, 17) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (bal >= 10) {
                SendJewelBankAction(2, i, 10);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "Se requieren al menos 10 joyas.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 100, 100, 255);
            }
        }

        // [+1]
        if (IsWorkZone((int)(bx + 82), (int)by, 34, 17) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (invCount >= 1) {
                SendJewelBankAction(1, i, 1);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 100, 100, 255);
            }
        }

        // [+Todo]
        if (IsWorkZone((int)(bx + 120), (int)by, 44, 17) && ConsumeClick()) {
            PlayBuffer(25, 0, 0);
            if (invCount >= 1) {
                SendJewelBankAction(1, i, 0xFFFFFFFF);
            } else {
                Platform::TextCopySecure(s_JewelBankStatus, sizeof(s_JewelBankStatus), "No tienes esa joya en inventario.", Platform::TruncateText);
                s_StatusColor = Color4b(255, 100, 100, 255);
            }
        }

        y += 56;
    }

    // Close button [X]
    const int cx = kStartX + 25, cy = kStartY + 395;
    if (IsWorkZone(cx, cy, 24, 24) && ConsumeClick()) {
        PlayBuffer(25, 0, 0);
        JewelBank_Close();
    }
}
