/*
 * 
 *  This is the entry of WoChat application
 * 
 */

#include "pch.h"  // the precompiled header file for Visual C++

#ifndef __cplusplus
#error WoChat Application requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLBASE_H__
#error WoChat Application requires atlbase.h to be included first
#endif

#ifndef __ATLWIN_H__
#error WoChat Application requires atlwin.h to be included first
#endif

#if !defined(_WIN64)
#error WoChat Application can only compiled in X64 mode
#endif

#include "resource.h"
#include "xbitmapdata.h"
#include "wochatdef.h"
#include "wochat.h"
#include "winlog.h"
#include "mosquitto.h"
#include "win0.h"
#include "win1.h"
#include "win2.h"
#include "win3.h"
#include "win4.h"
#include "win5.h"


////////////////////////////////////////////////////////////////
// global variables
////////////////////////////////////////////////////////////////
LONG 				g_threadCount = 0;
LONG				g_Quit = 0;
HINSTANCE			g_hInstance = nullptr;
HANDLE				g_MQTTPubEvent;

CRITICAL_SECTION    g_csMQTTSub;
CRITICAL_SECTION    g_csMQTTPub;

MQTTPubData			g_PubData = { 0 };
MQTTSubData			g_SubData = { 0 };

ID2D1Factory*		g_pD2DFactory = nullptr;
IDWriteFactory*		g_pDWriteFactory = nullptr;
IDWriteTextFormat*  g_pTextFormat[8] = { 0 };

D2D1_DRAW_TEXT_OPTIONS d2dDrawTextOptions = D2D1_DRAW_TEXT_OPTIONS_NONE;

HCURSOR				g_hCursorWE = nullptr;
HCURSOR				g_hCursorNS = nullptr;
HCURSOR				g_hCursorHand = nullptr;
HCURSOR				g_hCursorIBeam = nullptr;
wchar_t				g_AppPath[MAX_PATH + 1] = {0};

// private key and public key
U8					g_SK[32] = { 0 };
U8					g_PK[33] = { 0 };
U8					g_PKText[67] = { 0 };
wchar_t				g_PKTextW[67] = { 0 };

CAtlWinModule _Module;

static HMODULE hDLLD2D{};
static HMODULE hDLLDWrite{};

IDWriteTextFormat* GetTextFormat(U8 idx)
{
	if (idx < 8)
		return g_pTextFormat[idx];

	return nullptr;
}

/************************************************************************************************
*  The layout of the Main Window
*
* +------+------------------------+-----------------------------------------------------+
* |      |         Win1           |                       Win3                          |
* |      +------------------------+-----------------------------------------------------+
* |      |                        |                                                     |
* |      |                        |                                                     |
* |      |                        |                       Win4                          |
* | Win0 |                        |                                                     |
* |      |         Win2           |                                                     |
* |      |                        +-----------------------------------------------------|
* |      |                        |                                                     |
* |      |                        |                       Win5                          |
* |      |                        |                                                     |
* +------+------------------------+-----------------------------------------------------+
*
*
* We have one vertical bar and three horizonal bars.
*
*************************************************************************************************
*/

// the main window class for the whole application
class XWindow : public ATL::CWindowImpl<XWindow>
{
private:
	enum
	{
		STEPXY = 1,
		SPLITLINE_WIDTH = 1
	};

	enum class DrapMode { dragModeNone, dragModeV, dragModeH };
	DrapMode m_dragMode = DrapMode::dragModeNone;

	U32* m_screenBuff = nullptr;
	U32  m_screenSize = 0;

	RECT m_rectClient = { 0 };

	int m_splitterVPos = -1;               // splitter bar position
	int m_splitterVPosNew = -1;            // internal - new position while moving
	int m_splitterVPosOld = -1;            // keep align value

	int m_splitterHPos = -1;
	int m_splitterHPosNew = -1;
	int m_splitterHPosOld = -1;

	int m_cxDragOffset = 0;		// internal
	int m_cyDragOffset = 0;		// internal

	int m_splitterVPosToLeft = 300;       // the minum pixel to the left of the client area.
	int m_splitterVPosToRight = 400;       // the minum pixel to the right of the client area.
	int m_splitterHPosToTop = (XWIN3_HEIGHT + XWIN4_HEIGHT);        // the minum pixel to the top of the client area.
	int m_splitterHPosToBottom = XWIN5_HEIGHT;       // the minum pixel to the right of the client area.

	int m_xySplitterDefPos = -1;         // default position

	int m_marginLeft = 64;
	int m_marginRight = 0;

	int m_splitterHPosfix0 = XWIN1_HEIGHT;
	int m_splitterHPosfix1 = XWIN3_HEIGHT;

	UINT m_nDPI = 96;
	float m_deviceScaleFactor = 1.f;

	XWindow0 m_win0;
	XWindow1 m_win1;
	XWindow2 m_win2;
	XWindow3 m_win3;
	XWindow4 m_win4;
	XWindow5 m_win5;

	int m0 = 0;
	int m1 = 0;
	int m2 = 0;
	int m3 = 0;
	int m4 = 0;
	int m5 = 0;

	ID2D1HwndRenderTarget* m_pD2DRenderTarget = nullptr;
	ID2D1Bitmap* m_pixelBitmap0               = nullptr;
	ID2D1Bitmap* m_pixelBitmap1               = nullptr;
	ID2D1Bitmap* m_pixelBitmap2               = nullptr;
	ID2D1SolidColorBrush* m_pTextBrushSel     = nullptr;
	ID2D1SolidColorBrush* m_pTextBrush0       = nullptr;
	ID2D1SolidColorBrush* m_pTextBrush1       = nullptr;
	ID2D1SolidColorBrush* m_pCaretBrush       = nullptr;
	ID2D1SolidColorBrush* m_pBkgBrush0        = nullptr;
	ID2D1SolidColorBrush* m_pBkgBrush1        = nullptr;

public:
	DECLARE_XWND_CLASS(NULL, IDR_MAINFRAME, 0)

	XWindow()
	{
		m_rectClient.left = m_rectClient.right = m_rectClient.top = m_rectClient.bottom = 0;
		m_win0.SetWindowId((const U8*)"DUIWin0", 7);
		m_win1.SetWindowId((const U8*)"DUIWin1", 7);
		m_win2.SetWindowId((const U8*)"DUIWin2", 7);
		m_win3.SetWindowId((const U8*)"DUIWin3", 7);
		m_win4.SetWindowId((const U8*)"DUIWin4", 7);
		m_win5.SetWindowId((const U8*)"DUIWin5", 7);
	}

	~XWindow()
	{
		if (nullptr != m_screenBuff)
			VirtualFree(m_screenBuff, 0, MEM_RELEASE);

		m_screenBuff = nullptr;
		m_screenSize = 0;
	}

	BEGIN_MSG_MAP(XWindow)
		MESSAGE_HANDLER_DUIWINDOW(DUI_ALLMESSAGE, OnDUIWindowMessage)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
		MESSAGE_HANDLER(WM_MOUSEHOVER, OnMouseHover)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDoubleClick)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_XWINDOWS00, OnWin0Message)
		MESSAGE_HANDLER(WM_XWINDOWS01, OnWin1Message)
		MESSAGE_HANDLER(WM_XWINDOWS02, OnWin2Message)
		MESSAGE_HANDLER(WM_XWINDOWS03, OnWin3Message)
		MESSAGE_HANDLER(WM_XWINDOWS04, OnWin4Message)
		MESSAGE_HANDLER(WM_XWINDOWS05, OnWin5Message)
		MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	END_MSG_MAP()

	// this is the routine to process all window messages for win0/1/2/3/4/5
	LRESULT OnDUIWindowMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		WPARAM wp = wParam;
		LPARAM lp = lParam;

		ClearDUIWindowReDraw(); // reset the bit to indicate the redraw before we handle the messages

		switch (uMsg)
		{
		case WM_MOUSEWHEEL:
			{
				POINT pt;
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);
				ScreenToClient(&pt);
				lp = MAKELONG(pt.x, pt.y);
			}
			break;
		case WM_CREATE:
			wp = (WPARAM)m_hWnd;
			break;
		default:
			break;
		}

		m_win0.HandleOSMessage((U32)uMsg, (U64)wp, (U64)lp, this);
		m_win1.HandleOSMessage((U32)uMsg, (U64)wp, (U64)lp, this);
		m_win2.HandleOSMessage((U32)uMsg, (U64)wp, (U64)lp, this);
		m_win3.HandleOSMessage((U32)uMsg, (U64)wp, (U64)lp, this);
		m_win4.HandleOSMessage((U32)uMsg, (U64)wp, (U64)lp, this);
		m_win5.HandleOSMessage((U32)uMsg, (U64)wp, (U64)lp, this);

		if (DUIWindowNeedReDraw()) // after all 5 virtual windows procced the window message, the whole window may need to fresh
		{
			// U64 s = DUIWindowNeedReDraw();
			Invalidate(); // send out the WM_PAINT messsage to the window to redraw
		}

		// to allow the host window to continue to handle the windows message
		bHandled = FALSE;
		return 0;
	}

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL bHandled)
	{
		DWORD dwThreadID0;
		DWORD dwThreadID1;
		HANDLE hThread0;
		HANDLE hThread1;

		// we create two threads. One thread for the MQTT subscription, and one for publication.
		hThread0 = ::CreateThread(NULL, 0, MQTTSubThread, m_hWnd, 0, &dwThreadID0);
		hThread1 = ::CreateThread(NULL, 0, MQTTPubThread, m_hWnd, 0, &dwThreadID1);

		if (nullptr == hThread0 || nullptr == hThread1)
		{
			MessageBox(TEXT("MQTT thread creation is failed!"), TEXT("WoChat Error"), MB_OK);
		}

		{
			XChatGroup* cg = m_win2.GetSelectedChatGroup();
			if (cg)
			{
				//m_win3.UpdateTitle(cg->name);
				m_win4.SetChatGroup(cg);
			}
		}

		m_nDPI = GetDpiForWindow(m_hWnd);

#if 0
		{
			TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT) };
			tme.dwFlags = TME_LEAVE | TME_HOVER;
			tme.hwndTrack = m_hWnd;
			tme.dwHoverTime = 3000; // in ms
			TrackMouseEvent(&tme);
		}
#endif
		UINT_PTR id = SetTimer(XWIN_CARET_TIMER, 666);

		return 0;
	}

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		return 0; // don't want flicker
	}

	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL bHandled)
	{
		KillTimer(XWIN_CARET_TIMER);
		
		// tell all threads to quit
		InterlockedIncrement(&g_Quit);

		SetEvent(g_MQTTPubEvent); // tell MQTT pub thread to quit gracefully
		
		ReleaseUnknown(m_pBkgBrush0);
		ReleaseUnknown(m_pBkgBrush1);
		ReleaseUnknown(m_pCaretBrush);
		ReleaseUnknown(m_pTextBrushSel);
		ReleaseUnknown(m_pTextBrush0);
		ReleaseUnknown(m_pTextBrush1);
		ReleaseUnknown(m_pixelBitmap0);
		ReleaseUnknown(m_pixelBitmap1);
		ReleaseUnknown(m_pixelBitmap2);
		ReleaseUnknown(m_pD2DRenderTarget);

		PostQuitMessage(0);

		return 0;
	}

	LRESULT OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;

		lpMMI->ptMinTrackSize.x = 800;
		lpMMI->ptMinTrackSize.y = 600;

		return 0;
	}

	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		bHandled = DUIWindowCursorIsSet() ? TRUE : FALSE;

		ClearDUIWindowCursor();

		if (((HWND)wParam == m_hWnd) && (LOWORD(lParam) == HTCLIENT))
		{
			DWORD dwPos = ::GetMessagePos();

			POINT ptPos = { GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos) };

			ScreenToClient(&ptPos);

			DrapMode mode = IsOverSplitterBar(ptPos.x, ptPos.y);
			if (DrapMode::dragModeNone != mode)
				bHandled = TRUE;
		}

		return 0;
	}

	bool IsOverSplitterRect(int x, int y) const
	{
		// -1 == don't check
		return (((x == -1) || ((x >= m_rectClient.left) && (x < m_rectClient.right))) &&
			((y == -1) || ((y >= m_rectClient.top) && (y < m_rectClient.bottom))));
	}

	DrapMode IsOverSplitterBar(int x, int y) const
	{
		int width = SPLITLINE_WIDTH << 1;
		if (!IsOverSplitterRect(x, y))
			return DrapMode::dragModeNone;

		ATLASSERT(m_splitterVPos > 0);
		if ((x >= m_splitterVPos) && (x < (m_splitterVPos + width)))
			return DrapMode::dragModeV;

		if (m_splitterHPos > 0)
		{
			if ((x >= m_splitterVPos + width) && (y >= m_splitterHPos) && (y < (m_splitterHPos + width)))
				return DrapMode::dragModeH;
		}
		return DrapMode::dragModeNone;
	}

	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
#if 0
		static int ml = 0;

		DebugPrintf("================MOUSE LEAVE is %d\n", ++ml);
#endif
		return 0;
	}

	LRESULT OnMouseHover(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
#if 0
		static int mh = 0;

		DebugPrintf("================MOUSE HOVER is %d\n", ++mh);
#endif
		return 0;
	}

	LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnLButtonDoubleClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}
#if 0
	bool SetSplitterPos(int xyPos = -1, bool bUpdate = true)
	{
		if (xyPos == -1)   // -1 == default position
		{
			if (m_xySplitterDefPos != -1)
			{
				xyPos = m_xySplitterDefPos;
			}
			else   // not set, use middle position
			{
				if (m_bVertical)
					xyPos = (m_rcSplitter.right - m_rcSplitter.left - m_cxySplitBar - m_cxyBarEdge) / 2;
				else
					xyPos = (m_rcSplitter.bottom - m_rcSplitter.top - m_cxySplitBar - m_cxyBarEdge) / 2;
			}
		}

		// Adjust if out of valid range
		int cxyMax = 0;
		if (m_bVertical)
			cxyMax = m_rcSplitter.right - m_rcSplitter.left;
		else
			cxyMax = m_rcSplitter.bottom - m_rcSplitter.top;

		if (xyPos < m_cxyMin + m_cxyBarEdge)
			xyPos = m_cxyMin;
		else if (xyPos > (cxyMax - m_cxySplitBar - m_cxyBarEdge - m_cxyMin))
			xyPos = cxyMax - m_cxySplitBar - m_cxyBarEdge - m_cxyMin;

		// Set new position and update if requested
		bool bRet = (m_xySplitterPos != xyPos);
		m_xySplitterPos = xyPos;

		if (m_bUpdateProportionalPos)
		{
			if (IsProportional())
				StoreProportionalPos();
			else if (IsRightAligned())
				StoreRightAlignPos();
		}
		else
		{
			m_bUpdateProportionalPos = true;
		}

		if (bUpdate && bRet)
			UpdateSplitterLayout();

		return bRet;
	}
#endif

	LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		bool bChanged = false;
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);

		if (!DUIWindowInDragMode())
		{
			if (::GetCapture() == m_hWnd)
			{
#if 0
				int	newSplitPos;
				int	newSplitPos;
				switch (m_dragMode)
				{
				case DrapMode::dragModeV:
				{
					newSplitPos = xPos - m_cxDragOffset;
					if (newSplitPos == -1)   // avoid -1, that means default position
						newSplitPos = -2;
					if (m_splitterVPos != newSplitPos)
					{
						if (newSplitPos >= m_splitterVPosToLeft && newSplitPos < (m_rectClient.right - m_rectClient.left - m_splitterVPosToRight))
						{
							if (SetSplitterPos(newSplitPos, true))
								bChanged = true;
						}
					}
				}
				break;
				case DrapMode::dragModeH:
				{
					newSplitPos = yPos - m_cyDragOffset;
					if (newSplitPos == -1)   // avoid -1, that means default position
						newSplitPos = -2;
					if (m_splitterHPos != newSplitPos)
					{
						if (newSplitPos >= m_splitterHPosToTop && newSplitPos < (m_rectClient.bottom - m_rectClient.top - m_splitterHPosToBottom))
						{
							if (SetSplitterPos(newSplitPos, false))
								bChanged = true;
						}
					}
				}
				break;
				default:
					break;
				}
				if (bChanged)
					AdjustDUIWindowPosition();
#endif
			}
			else
			{
				DrapMode mode = IsOverSplitterBar(xPos, yPos);
				switch (mode)
				{
				case DrapMode::dragModeV:
					ATLASSERT(nullptr != g_hCursorWE);
					::SetCursor(g_hCursorWE);
					break;
				case DrapMode::dragModeH:
					ATLASSERT(nullptr != g_hCursorNS);
					::SetCursor(g_hCursorNS);
					break;
				default:
					break;
				}
			}
			if (bChanged)
				Invalidate();
		}

		return 0;
	}

	LRESULT OnWin0Message(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (wParam == 0)
		{
			U8 ctlId = (U8)lParam;
		}

		return 0;
	}

	LRESULT OnWin1Message(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnWin2Message(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnWin3Message(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	int GetCurrentPublicKey(U8* pk)
	{
		return m_win4.GetPublicKey(pk);
	}

	LRESULT OnWin4Message(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		switch (wParam)
		{
#if 0
		case WIN4_GET_PUBLICKEY:
			{
				U8* pk = (U8*)lParam;
				if (pk)
				{
					m_win4.GetPublicKey(pk);
				}
			}
			break;
#endif
		case WIN4_UPDATE_MESSAGE:
			{
				MessageTask* mt = (MessageTask*)lParam;
				if (mt)
				{
					int ret = m_win4.UpdateMyMessage(mt);
					if (0 == ret)
						Invalidate();
				}
			}
			break;
		default:
			break;
		}
		return 0;
	}

	LRESULT OnWin5Message(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		ReleaseUnknown(m_pD2DRenderTarget);

		if (SIZE_MINIMIZED != wParam)
		{
			XRECT area;
			XRECT* r = &area;

			GetClientRect(&m_rectClient);
			ATLASSERT(0 == m_rectClient.left);
			ATLASSERT(0 == m_rectClient.top);
			ATLASSERT(m_rectClient.right > 0);
			ATLASSERT(m_rectClient.bottom > 0);

			U32 w = (U32)(m_rectClient.right - m_rectClient.left);
			U32 h = (U32)(m_rectClient.bottom - m_rectClient.top);

			if (nullptr != m_screenBuff)
			{
				VirtualFree(m_screenBuff, 0, MEM_RELEASE);
				m_screenBuff = nullptr;
				m_screenSize = 0;
			}

			m_screenSize = DUI_ALIGN_PAGE(w * h * sizeof(U32));
			ATLASSERT(m_screenSize >= (w * h * sizeof(U32)));

			m_screenBuff = (U32*)VirtualAlloc(NULL, m_screenSize, MEM_COMMIT, PAGE_READWRITE);
			if (nullptr == m_screenBuff)
			{
				m_win0.UpdateSize(nullptr, nullptr);
				m_win1.UpdateSize(nullptr, nullptr);
				m_win2.UpdateSize(nullptr, nullptr);
				m_win3.UpdateSize(nullptr, nullptr);
				m_win4.UpdateSize(nullptr, nullptr);
				m_win5.UpdateSize(nullptr, nullptr);
				Invalidate();
				return 0;
			}

			if (m_splitterVPos < 0)
			{
				m_splitterVPos = (XWIN0_WIDTH + XWIN1_WIDTH);
				if (m_splitterVPos < m_splitterVPosToLeft)
					m_splitterVPos = m_splitterVPosToLeft;

				if (m_splitterVPos > (m_rectClient.right - m_rectClient.left - m_splitterVPosToRight))
				{
					m_splitterVPos = (m_rectClient.right - m_rectClient.left - m_splitterVPosToRight);
					ATLASSERT(m_splitterVPos > m_splitterVPosToLeft);
				}
				m_splitterVPosOld = m_splitterVPos;
			}

			m_splitterVPos = m_splitterVPosOld;

			if (m_splitterHPos < 0)
			{
				m_splitterHPos = m_rectClient.bottom - m_rectClient.top - m_splitterHPosToBottom;
				if (m_splitterHPos < m_splitterHPosToTop)
					m_splitterHPos = m_splitterHPosToTop;

				if (m_splitterHPos > (m_rectClient.bottom - m_rectClient.top - m_splitterHPosToBottom))
				{
					m_splitterHPos = m_rectClient.bottom - m_rectClient.top - m_splitterHPosToBottom;
					ATLASSERT(m_splitterHPos > m_splitterHPosToTop);
				}
				m_splitterHPosOld = (m_rectClient.bottom - m_rectClient.top) - m_splitterHPos;
			}

			if (m_splitterHPos > 0) // if(m_splitterHPos <= 0) then windows 5 is hidden
			{
				m_splitterHPos = (m_rectClient.bottom - m_rectClient.top) - m_splitterHPosOld;
			}


			if (nullptr != m_screenBuff)
			{
				U32* dst = m_screenBuff;
				U32 size;

				r->left = m_rectClient.left;
				r->right = XWIN0_WIDTH;
				r->top = m_rectClient.top;
				r->bottom = m_rectClient.bottom;
				m_win0.UpdateSize(r, dst);
				size = (U32)((r->right - r->left) * (r->bottom - r->top));
				dst += size;

				r->left = XWIN0_WIDTH;
				r->right = m_splitterVPos;
				r->top = m_rectClient.top;
				r->bottom = m_splitterHPosfix0;
				m_win1.UpdateSize(r, dst);
				size = (U32)((r->right - r->left) * (r->bottom - r->top));
				dst += size;

				r->left = XWIN0_WIDTH;
				r->right = m_splitterVPos;
				r->top = m_splitterHPosfix0 + SPLITLINE_WIDTH;
				r->bottom = m_rectClient.bottom;
				m_win2.UpdateSize(r, dst);
				size = (U32)((r->right - r->left) * (r->bottom - r->top));
				dst += size;

				r->left = m_splitterVPos + SPLITLINE_WIDTH;
				r->right = m_rectClient.right;
				r->top = m_rectClient.top;
				r->bottom = m_splitterHPosfix1;
				m_win3.UpdateSize(r, dst);
				size = (U32)((r->right - r->left) * (r->bottom - r->top));
				dst += size;

				r->left = m_splitterVPos + SPLITLINE_WIDTH;
				r->right = m_rectClient.right;
				r->top = m_splitterHPosfix1 + SPLITLINE_WIDTH;
				r->bottom = m_splitterHPos;
				m_win4.UpdateSize(r, dst);
				size = (U32)((r->right - r->left) * (r->bottom - r->top));
				dst += size;

				r->left = m_splitterVPos + SPLITLINE_WIDTH;
				r->right = m_rectClient.right;
				r->top = m_splitterHPos + SPLITLINE_WIDTH;
				r->bottom = m_rectClient.bottom;
				m_win5.UpdateSize(r, dst);
			}

			Invalidate();
		}
		return 0;
	}

	// When the virtial or horizonal bar is changed, 
	// We need to change the position of windows 0/1/2/3/4/5
	void AdjustDUIWindowPosition()
	{
		if (nullptr != m_screenBuff)
		{
			U32* dst;
			U32  size;

			// window 0 does not need to change
			XRECT area, * r;
			XRECT* xr = m_win0.GetWindowArea();

			area.left = xr->left;  area.top = xr->top; area.right = xr->right; area.bottom = xr->bottom;
			r = &area;

			dst = m_screenBuff;
			size = (U32)((r->right - r->left) * (r->bottom - r->top));
			dst += size;

			// windows 1
			r->left = r->right; r->right = m_splitterVPos; r->top = m_rectClient.top; r->bottom = m_splitterHPosfix0;
			size = (U32)((r->right - r->left) * (r->bottom - r->top));
			m_win1.UpdateSize(r, dst);
			dst += size;

			// windows 2
			r->top = m_splitterHPosfix0 + SPLITLINE_WIDTH; r->bottom = m_rectClient.bottom;
			size = (U32)((r->right - r->left) * (r->bottom - r->top));
			m_win2.UpdateSize(r, dst);

			// windows 3
			dst += size;
			r->left = m_splitterVPos + SPLITLINE_WIDTH; r->right = m_rectClient.right; r->top = m_rectClient.top; r->bottom = m_splitterHPosfix1;
			size = (U32)((r->right - r->left) * (r->bottom - r->top));
			m_win3.UpdateSize(r, dst);

			// windows 4
			dst += size;
			r->top = m_splitterHPosfix1 + SPLITLINE_WIDTH; r->bottom = m_splitterHPos;
			size = (U32)((r->right - r->left) * (r->bottom - r->top));
			m_win4.UpdateSize(r, dst);

			// windows 5
			dst += size;
			r->top = m_splitterHPos + SPLITLINE_WIDTH; r->bottom = m_rectClient.bottom;
			size = (U32)((r->right - r->left) * (r->bottom - r->top));
			m_win5.UpdateSize(r, dst);
		}
	}

	void DrawSeperatedLines()
	{
		if (nullptr != m_pixelBitmap0)
		{
			if (m_splitterVPos > 0)
			{
				D2D1_RECT_F rect = D2D1::RectF(
					static_cast<FLOAT>(m_splitterVPos),
					static_cast<FLOAT>(m_rectClient.top),
					static_cast<FLOAT>(m_splitterVPos + SPLITLINE_WIDTH), // m_cxySplitBar),
					static_cast<FLOAT>(m_rectClient.bottom)
				);
				m_pD2DRenderTarget->DrawBitmap(m_pixelBitmap0, &rect);
			}
			if (m_splitterHPosfix0 > 0)
			{
				D2D1_RECT_F rect = D2D1::RectF(
					static_cast<FLOAT>(XWIN0_WIDTH),
					static_cast<FLOAT>(m_splitterHPosfix0),
					static_cast<FLOAT>(m_splitterVPos),
					static_cast<FLOAT>(m_splitterHPosfix0 + SPLITLINE_WIDTH)
				);
				m_pD2DRenderTarget->DrawBitmap(m_pixelBitmap0, &rect);
			}
			if (m_splitterHPosfix1 > 0)
			{
				D2D1_RECT_F rect = D2D1::RectF(
					static_cast<FLOAT>(m_splitterVPos + 2),
					static_cast<FLOAT>(m_splitterHPosfix1),
					static_cast<FLOAT>(m_rectClient.right),
					static_cast<FLOAT>(m_splitterHPosfix1 + SPLITLINE_WIDTH)
				);
				m_pD2DRenderTarget->DrawBitmap(m_pixelBitmap0, &rect);
			}
			if (m_splitterHPos > 0)
			{
				D2D1_RECT_F rect = D2D1::RectF(
					static_cast<FLOAT>(m_splitterVPos + SPLITLINE_WIDTH),
					static_cast<FLOAT>(m_splitterHPos),
					static_cast<FLOAT>(m_rectClient.right),
					static_cast<FLOAT>(m_splitterHPos + SPLITLINE_WIDTH)
				);
				m_pD2DRenderTarget->DrawBitmap(m_pixelBitmap0, &rect);
			}
		}
	}

	int GetFirstIntegralMultipleDeviceScaleFactor() const noexcept 
	{
		return static_cast<int>(std::ceil(m_deviceScaleFactor));
	}

	D2D1_SIZE_U GetSizeUFromRect(const RECT& rc, const int scaleFactor) noexcept 
	{
		const long width = rc.right - rc.left;
		const long height = rc.bottom - rc.top;
		const UINT32 scaledWidth = width * scaleFactor;
		const UINT32 scaledHeight = height * scaleFactor;
		return D2D1::SizeU(scaledWidth, scaledHeight);
	}

	HRESULT CreateDeviceDependantResource()
	{
		HRESULT hr = S_OK;
		if (nullptr == m_pD2DRenderTarget)  // Create a Direct2D render target.
		{
			RECT rc;
			GetClientRect(&rc);
#if 0
			D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
			D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties();
			renderTargetProperties.dpiX = m_nDPI;
			renderTargetProperties.dpiY = m_nDPI;
			renderTargetProperties.pixelFormat = pixelFormat;

			D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTragetproperties
				= D2D1::HwndRenderTargetProperties(m_hWnd, D2D1::SizeU(m_rectClient.right - m_rectClient.left, m_rectClient.bottom - m_rectClient.top));
#endif
			const int integralDeviceScaleFactor = GetFirstIntegralMultipleDeviceScaleFactor();
			D2D1_RENDER_TARGET_PROPERTIES drtp{};
			drtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
			drtp.usage = D2D1_RENDER_TARGET_USAGE_NONE;
			drtp.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
			drtp.dpiX = 96.f * integralDeviceScaleFactor;
			drtp.dpiY = 96.f * integralDeviceScaleFactor;
			// drtp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN);
			drtp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);

			D2D1_HWND_RENDER_TARGET_PROPERTIES dhrtp{};
			dhrtp.hwnd = m_hWnd;
			dhrtp.pixelSize = GetSizeUFromRect(rc, integralDeviceScaleFactor);
			dhrtp.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

			ATLASSERT(nullptr != g_pD2DFactory);

			//hr = g_pD2DFactory->CreateHwndRenderTarget(renderTargetProperties, hwndRenderTragetproperties, &m_pD2DRenderTarget);
			hr = g_pD2DFactory->CreateHwndRenderTarget(drtp, dhrtp, &m_pD2DRenderTarget);

			if (S_OK == hr && nullptr != m_pD2DRenderTarget)
			{
				ReleaseUnknown(m_pBkgBrush0);
				ReleaseUnknown(m_pBkgBrush1);
				ReleaseUnknown(m_pCaretBrush);
				ReleaseUnknown(m_pTextBrushSel);
				ReleaseUnknown(m_pTextBrush0);
				ReleaseUnknown(m_pTextBrush1);
				ReleaseUnknown(m_pixelBitmap0);
				//ReleaseUnknown(m_pixelBitmap1);
				//ReleaseUnknown(m_pixelBitmap2);

				U32 pixel[1] = { 0xFFEEEEEE };
				hr = m_pD2DRenderTarget->CreateBitmap(
					D2D1::SizeU(1, 1), pixel, 4, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
					&m_pixelBitmap0);
				if (S_OK == hr && nullptr != m_pixelBitmap0)
				{
#if 0
					pixel[0] = 0xFFFFFFFF;
					hr = m_pD2DRenderTarget->CreateBitmap(
						D2D1::SizeU(1, 1), pixel, 4, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
						&m_pixelBitmap1);
					ATLASSERT(S_OK == hr);

					pixel[0] = 0xFF6AEA9E;
					hr = m_pD2DRenderTarget->CreateBitmap(
						D2D1::SizeU(1, 1), pixel, 4, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
						&m_pixelBitmap2);
					ATLASSERT(S_OK == hr);
#endif
					hr = m_pD2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x000000), &m_pTextBrush0);
					if (S_OK == hr && nullptr != m_pTextBrush0)
					{
						hr = m_pD2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x000000), &m_pTextBrush1);
						if (S_OK == hr && nullptr != m_pTextBrush1)
						{
							hr = m_pD2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x87CEFA), &m_pTextBrushSel);
							if (S_OK == hr && nullptr != m_pTextBrushSel)
							{
								hr = m_pD2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x000000), &m_pCaretBrush);
								if (S_OK == hr && nullptr != m_pCaretBrush)
								{
									hr = m_pD2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x95EC6A), &m_pBkgBrush0);
									if (S_OK == hr && nullptr != m_pBkgBrush0)
										hr = m_pD2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), &m_pBkgBrush1);
								}
							}
						}
					}
				}
			}
		}
		return hr;
	}

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
	{
		static U32 track = 0;
		static wchar_t xtitle[256+1] = { 0 };
		PAINTSTRUCT ps;
		BeginPaint(&ps);

		HRESULT hr = CreateDeviceDependantResource();

		if (S_OK == hr && nullptr != m_pD2DRenderTarget 
			&& nullptr != m_pixelBitmap0 
			&& nullptr != m_pTextBrush0 
			&& nullptr != m_pTextBrush1 
			&& nullptr != m_pTextBrushSel
			&& nullptr != m_pCaretBrush
			&& nullptr != m_pBkgBrush0
			&& nullptr != m_pBkgBrush1)
		{
			m_pD2DRenderTarget->BeginDraw();
			////////////////////////////////////////////////////////////////////////////////////////////////////
			//m_pD2DRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
			//m_pD2DRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
		
			DrawSeperatedLines(); // draw seperator lines

			// draw five windows
			m0 += m_win0.Draw(m_pD2DRenderTarget, m_pTextBrush0, m_pTextBrushSel, m_pCaretBrush, m_pBkgBrush0, m_pBkgBrush1);
			m1 += m_win1.Draw(m_pD2DRenderTarget, m_pTextBrush0, m_pTextBrushSel, m_pCaretBrush, m_pBkgBrush0, m_pBkgBrush1);
			m2 += m_win2.Draw(m_pD2DRenderTarget, m_pTextBrush0, m_pTextBrushSel, m_pCaretBrush, m_pBkgBrush0, m_pBkgBrush1);
			m3 += m_win3.Draw(m_pD2DRenderTarget, m_pTextBrush0, m_pTextBrushSel, m_pCaretBrush, m_pBkgBrush0, m_pBkgBrush1);
			m4 += m_win4.Draw(m_pD2DRenderTarget, m_pTextBrush0, m_pTextBrushSel, m_pCaretBrush, m_pBkgBrush0, m_pBkgBrush1);
			m5 += m_win5.Draw(m_pD2DRenderTarget, m_pTextBrush0, m_pTextBrushSel, m_pCaretBrush, m_pBkgBrush0, m_pBkgBrush1);

			hr = m_pD2DRenderTarget->EndDraw();
			////////////////////////////////////////////////////////////////////////////////////////////////////
			if (FAILED(hr) || D2DERR_RECREATE_TARGET == hr)
			{
				ReleaseUnknown(m_pD2DRenderTarget);
			}
		}

		swprintf((wchar_t*)xtitle, 256, L"WoChat - [%06d] threads: %d W0:%d - W1:%d - W2:%d - W3:%d - W4:%d - W5:%d -", ++track, (int)g_threadCount,
			m0,m1,m2,m3,m4,m5);
		::SetWindowTextW(m_hWnd, (LPCWSTR)xtitle);

		EndPaint(&ps);
		return 0;
	}
};

class CWoChatThreadManager
{
public:
	// the main UI thread proc
	static DWORD WINAPI RunThread(LPVOID lpData)
	{
		DWORD dwRet = 0;
		RECT rw = { 0 };
		MSG msg = { 0 };

		InterlockedIncrement(&g_threadCount);

		XWindow winMain;

		rw.left = 100; rw.top = 100; rw.right = rw.left + 1024; rw.bottom = rw.top + 768;
		winMain.Create(NULL, rw, _T("WoChat"), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE);
		if (FALSE == winMain.IsWindow())
			goto ExitMainUIThread;

		//winMain.CenterWindow();
		winMain.ShowWindow(SW_SHOW);

		// Main message loop:
		while (GetMessageW(&msg, nullptr, 0, 0))
		{
			if (!TranslateAcceleratorW(msg.hwnd, 0, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

	ExitMainUIThread:
		InterlockedDecrement(&g_threadCount);
		return dwRet;
	}

	DWORD m_dwCount = 0;
	HANDLE m_arrThreadHandles[MAXIMUM_WAIT_OBJECTS - 1];

	CWoChatThreadManager()
	{
		for (int i = 0; i < (MAXIMUM_WAIT_OBJECTS - 1); i++)
			m_arrThreadHandles[i] = nullptr;
	}

	// Operations
	DWORD AddThread(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		if (m_dwCount == (MAXIMUM_WAIT_OBJECTS - 1))
		{
			::MessageBox(NULL, _T("ERROR: Cannot create ANY MORE threads!!!"), _T("WoChat"), MB_OK);
			return 0;
		}

		DWORD dwThreadID;
		HANDLE hThread = ::CreateThread(NULL, 0, RunThread, nullptr, 0, &dwThreadID);
		if (hThread == NULL)
		{
			::MessageBox(NULL, _T("ERROR: Cannot create thread!!!"), _T("WoChat"), MB_OK);
			return 0;
		}

		m_arrThreadHandles[m_dwCount] = hThread;
		m_dwCount++;
		return dwThreadID;
	}

	void RemoveThread(DWORD dwIndex)
	{
		::CloseHandle(m_arrThreadHandles[dwIndex]);
		if (dwIndex != (m_dwCount - 1))
			m_arrThreadHandles[dwIndex] = m_arrThreadHandles[m_dwCount - 1];
		m_dwCount--;
	}

	int Run(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		MSG msg;
		// force message queue to be created
		::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

		AddThread(lpstrCmdLine, nCmdShow);

		int nRet = m_dwCount;
		DWORD dwRet;
		while (m_dwCount > 0)
		{
			dwRet = ::MsgWaitForMultipleObjects(m_dwCount, m_arrThreadHandles, FALSE, INFINITE, QS_ALLINPUT);

			if (dwRet == 0xFFFFFFFF)
			{
				::MessageBox(NULL, _T("ERROR: Wait for multiple objects failed!!!"), _T("WoChat"), MB_OK);
			}
			else if (dwRet >= WAIT_OBJECT_0 && dwRet <= (WAIT_OBJECT_0 + m_dwCount - 1))
			{
				RemoveThread(dwRet - WAIT_OBJECT_0);
			}
			else if (dwRet == (WAIT_OBJECT_0 + m_dwCount))
			{
				if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_USER)
						AddThread(nullptr, SW_SHOWNORMAL);
				}
			}
			else
			{
				::MessageBeep((UINT)-1);
			}
		}

		return nRet;
	}
};

static void LoadD2DOnce()
{
	DWORD loadLibraryFlags = 0;
	HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");

	if (kernel32) 
	{
		if (::GetProcAddress(kernel32, "SetDefaultDllDirectories")) 
		{
			// Availability of SetDefaultDllDirectories implies Windows 8+ or
			// that KB2533623 has been installed so LoadLibraryEx can be called
			// with LOAD_LIBRARY_SEARCH_SYSTEM32.
			loadLibraryFlags = LOAD_LIBRARY_SEARCH_SYSTEM32;
		}
	}

	typedef HRESULT(WINAPI* D2D1CFSig)(D2D1_FACTORY_TYPE factoryType, REFIID riid, CONST D2D1_FACTORY_OPTIONS* pFactoryOptions, IUnknown** factory);
	typedef HRESULT(WINAPI* DWriteCFSig)(DWRITE_FACTORY_TYPE factoryType, REFIID iid, IUnknown** factory);

	hDLLD2D = ::LoadLibraryEx(TEXT("D2D1.DLL"), 0, loadLibraryFlags);
	D2D1CFSig fnD2DCF = DLLFunction<D2D1CFSig>(hDLLD2D, "D2D1CreateFactory");

	if (fnD2DCF) 
	{
		// A single threaded factory as Scintilla always draw on the GUI thread
		fnD2DCF(D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory),
			nullptr,
			reinterpret_cast<IUnknown**>(&g_pD2DFactory));
	}

	hDLLDWrite = ::LoadLibraryEx(TEXT("DWRITE.DLL"), 0, loadLibraryFlags); 
	DWriteCFSig fnDWCF = DLLFunction<DWriteCFSig>(hDLLDWrite, "DWriteCreateFactory");
	if (fnDWCF) 
	{
		const GUID IID_IDWriteFactory2 = // 0439fc60-ca44-4994-8dee-3a9af7b732ec
		{ 0x0439fc60, 0xca44, 0x4994, { 0x8d, 0xee, 0x3a, 0x9a, 0xf7, 0xb7, 0x32, 0xec } };

		const HRESULT hr = fnDWCF(DWRITE_FACTORY_TYPE_SHARED,
			IID_IDWriteFactory2,
			reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
	
		if (SUCCEEDED(hr)) 
		{
			// D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
			d2dDrawTextOptions = static_cast<D2D1_DRAW_TEXT_OPTIONS>(0x00000004);
		}
		else 
		{
			fnDWCF(DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
		}
	}
}

static bool LoadD2D() 
{
	static std::once_flag once;

	ReleaseUnknown(g_pD2DFactory);
	ReleaseUnknown(g_pDWriteFactory);

	std::call_once(once, LoadD2DOnce);

	return g_pDWriteFactory && g_pD2DFactory;
}


static int InitDirectWriteTextFormat(HINSTANCE hInstance)
{
	U8 idx;
	HRESULT hr = S_OK;

#if 0
	g_pDWriteFactory = nullptr;
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
	if (S_OK != hr || nullptr == g_pDWriteFactory)
	{
		return 11;
	}
#endif 
	
	ATLASSERT(g_pDWriteFactory);

	for (U8 i = 0; i < 8; i++)
	{
		ReleaseUnknown(g_pTextFormat[i]);
	}

	idx = WT_TEXTFORMAT_MAINTEXT;
	hr = g_pDWriteFactory->CreateTextFormat(
		L"Microsoft Yahei",
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		13.5f,
		L"en-US",
		&(g_pTextFormat[idx])
	);

	if (S_OK != hr || nullptr == g_pTextFormat[idx])
	{
		return (20 + idx);
	}

	idx = WT_TEXTFORMAT_TITLE;
	hr = g_pDWriteFactory->CreateTextFormat(
		L"Arial",
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		11.5f,
		L"en-US",
		&(g_pTextFormat[idx])
	);

	if (S_OK != hr || nullptr == g_pTextFormat[idx])
	{
		return (20 + idx);
	}

	return 0;
}

static int GetApplicationPath(HINSTANCE hInstance)
{
	int iRet = 0;
	DWORD len, i = 0;

	len = GetModuleFileNameW(hInstance, g_AppPath, MAX_PATH);
	if (len > 0)
	{
		for (i = len - 1; i > 0; i--)
		{
			if (g_AppPath[i] == L'\\')
				break;
		}
		g_AppPath[i] = L'\0';
	}
	else
	{
		g_AppPath[0] = L'\0';
	}

	if (i == 0)
		iRet = 30;

	return iRet;
}

static int InitInstance(HINSTANCE hInstance)
{
	int iRet = 0;
	DWORD length = 0;
	HRESULT hr = S_OK;

	g_MQTTPubEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (NULL == g_MQTTPubEvent)
	{
		return 1;
	}

	InitializeCriticalSection(&g_csMQTTSub);
	InitializeCriticalSection(&g_csMQTTPub);

	g_hCursorWE = ::LoadCursor(NULL, IDC_SIZEWE);
	g_hCursorNS = ::LoadCursor(NULL, IDC_SIZENS);
	g_hCursorHand = ::LoadCursor(nullptr, IDC_HAND);
	g_hCursorIBeam = ::LoadCursor(NULL, IDC_IBEAM);
	if (NULL == g_hCursorWE || NULL == g_hCursorNS || NULL == g_hCursorHand || NULL == g_hCursorIBeam)
	{
		return 2;
	}

#if 0
	// Initialize Direct2D and DirectWrite
	g_pD2DFactory = nullptr;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
	if (S_OK != hr || nullptr == g_pD2DFactory)
	{
		MessageBox(NULL, _T("The calling of D2D1CreateFactory() is failed"), _T("WoChat Error"), MB_OK);
		return 3;
	}
#endif 

	if (!LoadD2D())
	{
		return 3;
	}

	iRet = InitDirectWriteTextFormat(hInstance);
	if (iRet)
		return iRet;

	iRet = GetApplicationPath(hInstance);
	if (iRet)
		return iRet;

	iRet = GetKeys(g_AppPath, g_SK, g_PK);

	if (iRet)
		return 5;

	Raw2HexString(g_PK, 33, g_PKText, nullptr);
	for (int i = 0; i < 66; i++)
		g_PKTextW[i] = g_PKText[i];

	g_PKTextW[66] = L'\0';

	iRet = MQTT::MQTT_Init();

	if (MOSQ_ERR_SUCCESS != iRet)
		return 6;

	iRet = DUI_Init();

	g_PubData.host = MQTT_DEFAULT_HOST;
	g_PubData.port = MQTT_DEFAULT_PORT;

	g_SubData.host = MQTT_DEFAULT_HOST;
	g_SubData.port = MQTT_DEFAULT_PORT;

#if 0
	AESEncrypt(NULL, 0, NULL);
	
	{
		int return_val;
		size_t len;
		U8 randomize[32];
		U8 compressed_pubkey[33];
		secp256k1_context* ctx;
		secp256k1_pubkey pubkey;

		ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
		for (int i = 0; i < 32; i++)
			randomize[i] = 137 - i;
		return_val = secp256k1_context_randomize(ctx, randomize);
		return_val = secp256k1_ec_pubkey_create(ctx, &pubkey, g_SK);
		len = sizeof(compressed_pubkey);
		return_val = secp256k1_ec_pubkey_serialize(ctx, compressed_pubkey, &len, &pubkey, SECP256K1_EC_COMPRESSED);
		secp256k1_context_destroy(ctx);
	}

	{
		U32 crc;
		char* p = "abc1";
		INIT_CRC32C(crc);

		COMP_CRC32C(crc, p, 4);
		FIN_CRC32C(crc);

		int i = 1;
	}

	InitWoChatDatabase(g_AppPath);

	{
		U32 hv;
		hv = hash_bytes((const unsigned char*)"03342EF59638622ABEE545AFABA85463932DA66967F2BA4EE6254F9988BEE5F0C2", 66);
		hv = hash_bytes((const unsigned char*)"B5C10E18A44C596BC0C213B9A3DD3E724C35B03BF2EBBADFB1B7C09AD2F9324C", 64);
		hv = 0;
	}

	{
		bool found;
		HTAB* ht;
		HASHCTL ctl;
		ctl.keysize = 66;
		ctl.entrysize = ctl.keysize + sizeof(void*);
		ctl.hcxt = nullptr;

		ht = hash_create("HASHT", 100, &ctl, HASH_ELEM | HASH_BLOBS);

		void* ptr = hash_search(ht, g_PKText, HASH_ENTER, &found);

		hash_destroy(ht);

	}
	{
		int i = 321;
		DebugPrintf("================The value is %d\n", i);
	}
#endif

	return iRet;
}

static void ExitInstance(HINSTANCE hInstance)
{
	UINT i, tries;

	// tell all threads to quit
	InterlockedIncrement(&g_Quit);

	// wait for all threads to quit gracefully
	tries = 20;
	while (g_threadCount && tries > 0)
	{
		Sleep(1000);
		tries--;
	}

	DUI_Term();

	ReleaseUnknown(g_pDWriteFactory);
	ReleaseUnknown(g_pD2DFactory);
	
	if (hDLLDWrite) 
	{
		FreeLibrary(hDLLDWrite);
		hDLLDWrite = {};
	}
	if (hDLLD2D) 
	{
		FreeLibrary(hDLLD2D);
		hDLLD2D = {};
	}
#if 0
	ReleaseUnknown(&g_pDWriteFactory);
	ReleaseUnknown(&g_pD2DFactory);
#endif

	CloseHandle(g_MQTTPubEvent);
	DeleteCriticalSection(&g_csMQTTSub);
	DeleteCriticalSection(&g_csMQTTPub);

	MQTT::MQTT_Term();
}

// The main entry of WoChat
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nShowCmd)
{
	int iRet = 0;
	HRESULT hr;

	UNREFERENCED_PARAMETER(hPrevInstance);

	g_Quit = 0;
	g_threadCount = 0;
	g_hInstance = hInstance;  // save the instance

	// The Microsoft Security Development Lifecycle recommends that all
	// applications include the following call to ensure that heap corruptions
	// do not go unnoticed and therefore do not introduce opportunities
	// for security exploits.
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (S_OK != hr)
	{
		MessageBoxW(NULL, L"The calling of CoInitializeEx() is failed", L"WoChat Error", MB_OK);
		return 0;
	}

	iRet = InitInstance(hInstance);

	if (0 != iRet)
	{
		wchar_t msg[MAX_PATH + 1] = { 0 };
		swprintf((wchar_t*)msg, MAX_PATH, L"InitInstance() is failed. Return code:[%d]", iRet);

		MessageBoxW(NULL, (LPCWSTR)msg, L"WoChat Error", MB_OK);

		goto ExitThisApplication;
	}
	else  // BLOCK: Run application
	{
		CWoChatThreadManager mgr;
		iRet = mgr.Run(lpCmdLine, nShowCmd);
	}

ExitThisApplication:
	ExitInstance(hInstance);
	CoUninitialize();

	return 0;
}


int GetCurrentPublicKey(void* parent, U8* pk)
{
	int r = 1;
	XWindow* pThis = static_cast<XWindow*>(parent);

	if (pThis && pk)
	{
		r = pThis->GetCurrentPublicKey(pk);
	}
	return r;
}
