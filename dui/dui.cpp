#include <algorithm>
#include <vector>

#include "dui.h"

// global variables
U8          DUIMessageOSMap[MESSAGEMAP_TABLE_SIZE] = { 0 };
U64         dui_status;

HCURSOR	dui_hCursorArrow = nullptr;
HCURSOR	dui_hCursorWE = nullptr;
HCURSOR	dui_hCursorNS = nullptr;
HCURSOR	dui_hCursorHand = nullptr;
HCURSOR	dui_hCursorIBeam = nullptr;

int DUI_Init()
{
    int i;
    U8* p = DUIMessageOSMap;

    //g_hCursorWE = ::LoadCursor(NULL, IDC_SIZEWE);
    //g_hCursorNS = ::LoadCursor(NULL, IDC_SIZENS);
    dui_hCursorArrow = ::LoadCursor(NULL, IDC_ARROW);
    dui_hCursorHand = ::LoadCursor(nullptr, IDC_HAND);
    dui_hCursorIBeam = ::LoadCursor(NULL, IDC_IBEAM);

    if (nullptr == dui_hCursorHand || nullptr == dui_hCursorIBeam || nullptr == dui_hCursorArrow)
        return DUI_FAILED;

    for (i = 0; i < MESSAGEMAP_TABLE_SIZE; i++)
        p[i] = DUI_NULL;

#ifdef _WIN32
    // We establish the mapping relationship between Windows message to DUI message
    p[WM_CREATE] = DUI_CREATE;
    p[WM_DESTROY] = DUI_DESTROY;
    p[WM_ERASEBKGND] = DUI_ERASEBKGND;
    p[WM_PAINT] = DUI_PAINT;
    p[WM_TIMER] = DUI_TIMER;
    p[WM_MOUSEMOVE] = DUI_MOUSEMOVE;
    p[WM_MOUSEWHEEL] = DUI_MOUSEWHEEL;
    p[WM_LBUTTONDOWN] = DUI_LBUTTONDOWN;
    p[WM_LBUTTONUP] = DUI_LBUTTONUP;
    p[WM_LBUTTONDBLCLK] = DUI_LBUTTONDBLCLK;
    p[WM_RBUTTONDOWN] = DUI_RBUTTONDOWN;
    p[WM_RBUTTONUP] = DUI_RBUTTONUP;
    p[WM_RBUTTONDBLCLK] = DUI_RBUTTONDBLCLK;
    p[WM_MBUTTONDOWN] = DUI_MBUTTONDOWN;
    p[WM_MBUTTONUP] = DUI_MBUTTONUP;
    p[WM_MBUTTONDBLCLK] = DUI_MBUTTONDBLCLK;
    p[WM_CAPTURECHANGED] = DUI_CAPTURECHANGED;
    p[WM_KEYFIRST] = DUI_KEYFIRST;
    p[WM_KEYDOWN] = DUI_KEYDOWN;
    p[WM_KEYUP] = DUI_KEYUP;
    p[WM_CHAR] = DUI_CHAR;
    p[WM_DEADCHAR] = DUI_DEADCHAR;
    p[WM_SYSKEYDOWN] = DUI_SYSKEYDOWN;
    p[WM_SYSKEYUP] = DUI_SYSKEYUP;
    p[WM_SYSCHAR] = DUI_SYSCHAR;
    p[WM_SYSDEADCHAR] = DUI_SYSDEADCHAR;
    p[WM_GETMINMAXINFO] = DUI_GETMINMAXINFO;
    p[WM_SIZE] = DUI_SIZE;
    p[WM_SETCURSOR] = DUI_SETCURSOR;
    p[WM_SETFOCUS] = DUI_SETFOCUS;
    p[WM_MOUSEACTIVATE] = DUI_MOUSEACTIVATE;
    p[WM_MOUSELEAVE] = DUI_MOUSELEAVE;
    p[WM_MOUSEHOVER] = DUI_MOUSEHOVER;
    p[WM_NCLBUTTONDOWN] = DUI_NCLBUTTONDOWN;
#endif

    return DUI_OK;
}

void DUI_Term()
{

}

int XControl::DoMouseMove(int x, int y, int idxActive)
{
    int r = 0;
    
    if (m_property & XCONTROL_PROP_LBDOWN) // the mouse is moving when the mouse left button is pressed
        return 0;

    if (x < 0) // fast path, for the most controls. The mouse is not hitting me
    {
        if (m_status != XCONTROL_STATE_NORMAL && m_status != XCONTROL_STATE_ACTIVE)
            r++;
        m_status = (idxActive != m_Id) ? XCONTROL_STATE_NORMAL : XCONTROL_STATE_ACTIVE;
        return r;
    }

    // the mousr point is in the inner area. In this case, we should set the status to hovered.
    if ((x >= left1) && (y >= top1) && (x < right1) && (y < bottom1))
    {
        r = (m_status == XCONTROL_STATE_HOVERED) ? 0 : 1;
        m_status = XCONTROL_STATE_HOVERED;
        ShowCursor(true);
        return r;
    }
    // the mousr point is in the outer area. In this case, we should set the status to hovered.
    if ((x >= left2) && (y >= top2) && (x < right2) && (y < bottom2))
    {
        r = (m_status == XCONTROL_STATE_HOVERED) ? 0 : 1;
        m_status = XCONTROL_STATE_HOVERED;
        ShowCursor(false);
        return r;
    }
    // the mousr is not hovered me
    if (m_status != XCONTROL_STATE_NORMAL && m_status != XCONTROL_STATE_ACTIVE)
        r++;

    m_status = (idxActive != m_Id) ? XCONTROL_STATE_NORMAL : XCONTROL_STATE_ACTIVE;

    return r;
}

int XControl::DoMouseLBClickDown(int x, int y, int* idxActive)
{
    int r = 0;
    U32 status = m_status;
    U32 prop = (XCONTROL_PROP_EDITBOX | XCONTROL_PROP_FOCUS) & m_property;

    m_property |= XCONTROL_PROP_LBDOWN; // record that the mouse left button is pressed
    if (x < 0) // fast path, for the most controls. The mouse is not hitting me
    {
        // if the old status is normal or active, we do not need to redraw
        r = (m_status == XCONTROL_STATE_NORMAL || m_status == XCONTROL_STATE_ACTIVE) ? 0 : 1; 
        m_status = (status != XCONTROL_STATE_ACTIVE) ? XCONTROL_STATE_NORMAL : XCONTROL_STATE_ACTIVE;

        if (prop == (XCONTROL_PROP_EDITBOX | XCONTROL_PROP_FOCUS)) // the edit box has focus, now we lose the focus
            r++;
            
        m_property &= ~XCONTROL_PROP_FOCUS;
        return r;
    }

    // the mousr point is in the inner area. In this case, we should set the status to hovered.
    if ((x >= left1) && (y >= top1) && (x < right1) && (y < bottom1))
    {
        r = (m_status == XCONTROL_STATE_PRESSED) ? 0 : 1;
        m_status = XCONTROL_STATE_PRESSED;
        m_property |= XCONTROL_PROP_FOCUS;
        if (idxActive)
            *idxActive = m_Id;
        ShowCursor(true);
        return r;
    }
#if 0
    else if ((x >= left2) && (y >= top2) && (x < right2) && (y < bottom2)) // outer area check
    {
        r = (m_status == XCONTROL_STATE_PRESSED) ? 0 : 1;
        m_status = XCONTROL_STATE_PRESSED;
        m_property |= XCONTROL_PROP_FOCUS;
        if (idxActive)
            *idxActive = m_Id;
        ShowCursor(false);
        return r;
    }
#endif
    // the mousr is not hovered me
     
    // if the old status is normal or active, we do not need to redraw
    r = (m_status == XCONTROL_STATE_NORMAL || m_status == XCONTROL_STATE_ACTIVE) ? 0 : 1;
    m_status = (status != XCONTROL_STATE_ACTIVE) ? XCONTROL_STATE_NORMAL : XCONTROL_STATE_ACTIVE;

    if (prop == (XCONTROL_PROP_EDITBOX | XCONTROL_PROP_FOCUS))
        r++;
    m_property &= ~XCONTROL_PROP_FOCUS;

    return r;
}

int XControl::DoMouseLBClickUp(int x, int y, int* idxActive)
{
    int r = 0;
    U32 status = m_status;

    if (x < 0) // fast path, for the most controls
    {
        // if the old status is normal or active, we do not need to redraw
        r = (m_status == XCONTROL_STATE_NORMAL || m_status == XCONTROL_STATE_ACTIVE) ? 0 : 1; 
        m_status = (status != XCONTROL_STATE_ACTIVE) ? XCONTROL_STATE_NORMAL : XCONTROL_STATE_ACTIVE;
        m_property &= ~(XCONTROL_PROP_FOCUS| XCONTROL_PROP_LBDOWN);
        return r;
    }

    // the mousr point is in the inner area. In this case, we should set the status to hovered.
    if ((x >= left1) && (y >= top1) && (x < right1) && (y < bottom1))
    {
        r = (m_status == XCONTROL_STATE_HOVERED) ? 0 : 1;
        m_status = XCONTROL_STATE_HOVERED;
        if (idxActive)
            *idxActive = m_Id;
        ShowCursor(true);
        m_property &= ~XCONTROL_PROP_LBDOWN;
        return r;
    }
#if 0
    else if ((x >= left2) && (y >= top2) && (x < right2) && (y < bottom2)) // outer area check
    {
        r = (m_status == XCONTROL_STATE_HOVERED) ? 0 : 1;
        m_status = XCONTROL_STATE_HOVERED;
        if (idxActive)
            *idxActive = m_Id;
        ShowCursor(false);
        return r;
    }
#endif
    // the mousr is not hovered me
    // if the old status is normal or active, we do not need to redraw
    r = (m_status == XCONTROL_STATE_NORMAL || m_status == XCONTROL_STATE_ACTIVE) ? 0 : 1;
    m_status = (status != XCONTROL_STATE_ACTIVE) ? XCONTROL_STATE_NORMAL : XCONTROL_STATE_ACTIVE;
    m_property &= ~XCONTROL_PROP_LBDOWN;
    return r;
}

int XButton::Draw(int dx, int dy)
{
    if (nullptr != m_parentBuf)
    {
        U32* dst = m_parentBuf;
        U32* src;
        int w = right1 - left1;
        int h = bottom1 - top1;
        XBitmap* bitmap;

        assert(w > 0 && h > 0);
        assert(m_parentW > w);
        assert(m_parentH > h);
        switch (m_status)
        {
        case XCONTROL_STATE_PRESSED:
            bitmap = imgPress;
            break;
        case XCONTROL_STATE_HOVERED:
            bitmap = imgHover;
            break;
        case XCONTROL_STATE_ACTIVE:
            bitmap = imgActive;
            break;
        default:
            bitmap = imgNormal;
            break;
        }
        assert(nullptr != bitmap);
        assert(w == bitmap->w);
        assert(h == bitmap->h);

        src = bitmap->data;
        assert(nullptr != src);

        if (XCONTROL_PROP_ROUND & m_property)
            DUI_ScreenDrawRectRound(dst, m_parentW, m_parentH, src, bitmap->w, bitmap->h, left1, top1, m_Color0, m_Color1);
        else
            DUI_ScreenDrawRect(dst, m_parentW, m_parentH, src, bitmap->w, bitmap->h, left1, top1);
    }

    return 0;
}


