#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <wchar.h>

/* LunaHost UI-only plugin. No game hooks and no text transformations. */
static HMODULE module;
static HHOOK hook, queuedHook;
static DWORD uiThread;
static BOOL busy, dark = TRUE;
static HBRUSH background, field;
static const COLORREF BG = RGB(30,32,36), FIELD = RGB(23,25,29);
static const COLORREF FG = RGB(232,234,238), MUTED = RGB(155,160,170);
static const UINT_PTR SUBCLASS_ID = 0x4c554e41;
static LRESULT CALLBACK themed(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);

static BOOL isClass(HWND w, const wchar_t *name) {
    wchar_t cls[128];
    return GetClassNameW(w,cls,128) && lstrcmpiW(cls,name)==0;
}
static BOOL belongs(HWND w) {
    return isClass(GetAncestor(w,GA_ROOT),L"LunaHostWindow");
}
static BOOL highContrast(void) {
    HIGHCONTRASTW hc = {sizeof(hc),0,NULL};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0) && (hc.dwFlags&HCF_HIGHCONTRASTON);
}
static void fill(HDC dc, RECT *r, COLORREF c) {
    SetDCBrushColor(dc,c); FillRect(dc,r,(HBRUSH)GetStockObject(DC_BRUSH));
}
static void frame(HDC dc, RECT *r, COLORREF c) {
    SetDCBrushColor(dc,c); FrameRect(dc,r,(HBRUSH)GetStockObject(DC_BRUSH));
}
static void colors(HWND w) {
    if(isClass(w,WC_LISTVIEWW)) {
        ListView_SetBkColor(w,dark ? FIELD : GetSysColor(COLOR_WINDOW));
        ListView_SetTextBkColor(w,dark ? FIELD : GetSysColor(COLOR_WINDOW));
        ListView_SetTextColor(w,dark ? FG : GetSysColor(COLOR_WINDOWTEXT));
    }
    if(isClass(w,L"LunaHostWindow")) {
        BOOL enabled = dark;
        /* Supported on Windows 11; older Windows safely retains its title bar. */
        DwmSetWindowAttribute(w,20,&enabled,sizeof(enabled));
    }
}
static void install(HWND w) {
    DWORD_PTR ignored;
    if(GetWindowThreadProcessId(w,NULL)!=uiThread || !belongs(w)) return;
    if(GetWindowSubclass(w,themed,SUBCLASS_ID,&ignored)) return;
    if(SetWindowSubclass(w,themed,SUBCLASS_ID,0)) {
        colors(w);
        InvalidateRect(w,NULL,TRUE);
    }
}
static BOOL CALLBACK child(HWND w, LPARAM unused) { (void)unused; install(w); return TRUE; }
static BOOL CALLBACK refresh(HWND w, LPARAM unused) {
    (void)unused; colors(w); InvalidateRect(w,NULL,TRUE); return TRUE;
}
static BOOL supportedButton(HWND w) {
    LONG type = GetWindowLongW(w,GWL_STYLE)&BS_TYPEMASK;
    return type==BS_PUSHBUTTON || type==BS_DEFPUSHBUTTON || type==BS_AUTOCHECKBOX || type==BS_CHECKBOX;
}
static void paintButton(HWND w, HDC dc) {
    int saved=SaveDC(dc);
    RECT r; GetClientRect(w,&r);
    LONG style=GetWindowLongW(w,GWL_STYLE), type=style&BS_TYPEMASK;
    LRESULT state=SendMessageW(w,BM_GETSTATE,0,0);
    BOOL enabled=IsWindowEnabled(w), check=type==BS_AUTOCHECKBOX || type==BS_CHECKBOX;
    BOOL focus=GetFocus()==w;
    HFONT font=(HFONT)SendMessageW(w,WM_GETFONT,0,0);
    if(font)SelectObject(dc,font);
    TEXTMETRICW tm; GetTextMetricsW(dc,&tm);
    int pad=tm.tmHeight/3; if(pad<4)pad=4;
    fill(dc,&r,check?BG:(state&BST_PUSHED)?RGB(62,72,86):RGB(45,49,56));
    RECT text=r;
    if(check) {
        int size=tm.tmHeight; if(size>r.bottom-4)size=r.bottom-4;
        RECT box={pad,(r.bottom-size)/2,pad+size,(r.bottom+size)/2};
        if(style&BS_RIGHTBUTTON) {box.left=r.right-pad-size;box.right=r.right-pad; text.right=box.left-pad;}
        else text.left=box.right+pad;
        fill(dc,&box,FIELD); frame(dc,&box,enabled?MUTED:RGB(80,84,90));
        if(SendMessageW(w,BM_GETCHECK,0,0)!=BST_UNCHECKED) {
            HPEN pen=CreatePen(PS_SOLID,2,enabled?RGB(137,190,255):MUTED);
            HGDIOBJ old=SelectObject(dc,pen);
            MoveToEx(dc,box.left+size/4,box.top+size/2,NULL);
            LineTo(dc,box.left+size*2/5,box.bottom-size/4);
            LineTo(dc,box.right-size/5,box.top+size/4);
            SelectObject(dc,old);DeleteObject(pen);
        }
    } else frame(dc,&r,focus||type==BS_DEFPUSHBUTTON?RGB(137,190,255):RGB(78,84,94));
    wchar_t caption[1024];GetWindowTextW(w,caption,1024);
    SetTextColor(dc,enabled?FG:MUTED);SetBkMode(dc,TRANSPARENT);
    text.left+=pad;text.right-=pad;
    UINT flags=DT_VCENTER|DT_SINGLELINE|(check?DT_LEFT:DT_CENTER);
    LRESULT cues=SendMessageW(w,WM_QUERYUISTATE,0,0);
    if(cues&UISF_HIDEACCEL)flags|=DT_HIDEPREFIX;
    DrawTextW(dc,caption,-1,&text,flags);
    if(focus && !(cues&UISF_HIDEFOCUS)) {InflateRect(&r,-3,-3);DrawFocusRect(dc,&r);}
    RestoreDC(dc,saved);
}
static void paintHeader(HWND w, HDC dc) {
    int saved=SaveDC(dc); RECT client;GetClientRect(w,&client);fill(dc,&client,RGB(45,49,56));
    HFONT font=(HFONT)SendMessageW(w,WM_GETFONT,0,0);if(font)SelectObject(dc,font);
    SetTextColor(dc,FG);SetBkMode(dc,TRANSPARENT);
    int count=Header_GetItemCount(w);
    for(int i=0;i<count;i++) {
        RECT r;if(!Header_GetItemRect(w,i,&r))continue;
        wchar_t text[1024]={0};HDITEMW item={0};item.mask=HDI_TEXT|HDI_FORMAT;item.pszText=text;item.cchTextMax=1024;
        if(!Header_GetItem(w,i,&item))continue;
        frame(dc,&r,RGB(78,84,94));r.left+=6;r.right-=6;
        UINT align=(item.fmt&HDF_JUSTIFYMASK)==HDF_RIGHT?DT_RIGHT:(item.fmt&HDF_JUSTIFYMASK)==HDF_CENTER?DT_CENTER:DT_LEFT;
        if(item.fmt&(HDF_SORTUP|HDF_SORTDOWN)) {
            RECT arrow=r;arrow.left=arrow.right-16;r.right-=18;
            DrawTextW(dc,item.fmt&HDF_SORTUP?L"\x25b4":L"\x25be",1,&arrow,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        DrawTextW(dc,text,-1,&r,align|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    }
    RestoreDC(dc,saved);
}
static LRESULT CALLBACK themed(HWND w, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
    (void)data;
    if(msg==WM_NCDESTROY) {RemoveWindowSubclass(w,themed,id);return DefSubclassProc(w,msg,wp,lp);}
    if(msg==WM_SETTINGCHANGE) {
        BOOL wanted=!highContrast();
        if(wanted!=dark) {
            dark=wanted;
            HWND root=GetAncestor(w,GA_ROOT);colors(root);EnumChildWindows(root,refresh,0);
            InvalidateRect(root,NULL,TRUE);
        }
    }
    /* The host and native theme can reset these after initial subclassing. */
    if(dark && isClass(w,WC_LISTVIEWW)) {
        switch(msg) {
        case LVM_SETBKCOLOR: case LVM_SETTEXTBKCOLOR: lp=(LPARAM)FIELD;break;
        case LVM_SETTEXTCOLOR: lp=(LPARAM)FG;break;
        case WM_PAINT: case WM_PRINTCLIENT:
            if(ListView_GetBkColor(w)!=FIELD)ListView_SetBkColor(w,FIELD);
            if(ListView_GetTextBkColor(w)!=FIELD)ListView_SetTextBkColor(w,FIELD);
            if(ListView_GetTextColor(w)!=FG)ListView_SetTextColor(w,FG);
            break;
        }
    }
    if(dark) {
        switch(msg) {
        case WM_ERASEBKGND:
            if(isClass(w,L"LunaHostWindow")) {RECT r;GetClientRect(w,&r);FillRect((HDC)wp,&r,background);return 1;}
            break;
        case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: case WM_CTLCOLORDLG: {
            BOOL input=msg==WM_CTLCOLOREDIT || msg==WM_CTLCOLORLISTBOX || isClass((HWND)lp,L"Edit");
            SetTextColor((HDC)wp,IsWindowEnabled((HWND)lp)?FG:MUTED);
            SetBkColor((HDC)wp,input?FIELD:BG);
            return (LRESULT)(input?field:background);
        }
        case WM_PAINT:
            if(isClass(w,WC_HEADERW)) {PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);paintHeader(w,dc);EndPaint(w,&ps);return 0;}
            if(isClass(w,L"Button") && supportedButton(w)) {
                PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);paintButton(w,dc);EndPaint(w,&ps);return 0;
            }
            break;
        case WM_PRINTCLIENT:
            if(isClass(w,WC_HEADERW)) {paintHeader(w,(HDC)wp);return 0;}
            if(isClass(w,L"Button") && supportedButton(w)) {paintButton(w,(HDC)wp);return 0;}
            break;
        }
    }
    LRESULT result=DefSubclassProc(w,msg,wp,lp);
    if(dark && isClass(w,L"Button")) {
        switch(msg) {
        case BM_SETCHECK: case BM_SETSTATE: case WM_ENABLE: case WM_SETFOCUS: case WM_KILLFOCUS:
        case WM_SETTEXT: case WM_UPDATEUISTATE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_KEYDOWN: case WM_KEYUP: InvalidateRect(w,NULL,FALSE);break;
        }
    }
    return result;
}
static LRESULT CALLBACK onMessage(int code, WPARAM wp, LPARAM lp) {
    if(code>=0 && !busy) {
        const CWPSTRUCT *m=(const CWPSTRUCT*)lp;
        if((m->message==WM_SHOWWINDOW || m->message==WM_NULL || m->message==WM_PAINT || m->message==WM_ERASEBKGND || m->message==WM_WINDOWPOSCHANGED || m->message==WM_PARENTNOTIFY) && belongs(m->hwnd)) {
            busy=TRUE; install(m->hwnd);
            if(m->message!=WM_PAINT && isClass(m->hwnd,L"LunaHostWindow"))EnumChildWindows(m->hwnd,child,0);
            busy=FALSE;
        }
    }
    return CallNextHookEx(hook,code,wp,lp);
}
/* Windows does not deliver posted WM_PAINT through WH_CALLWNDPROC.
   Attach on the owning UI thread before queued messages are dispatched too. */
static LRESULT CALLBACK onQueued(int code, WPARAM wp, LPARAM lp) {
    if(code>=0 && wp==PM_REMOVE && !busy) {
        const MSG *m=(const MSG*)lp;
        if(m->hwnd && belongs(m->hwnd)) {
            busy=TRUE;install(m->hwnd);
            if(isClass(m->hwnd,L"LunaHostWindow"))EnumChildWindows(m->hwnd,child,0);
            busy=FALSE;
        }
    }
    return CallNextHookEx(queuedHook,code,wp,lp);
}
static BOOL CALLBACK findHost(HWND w, LPARAM out) {
    DWORD pid; DWORD tid=GetWindowThreadProcessId(w,&pid);
    if(pid==GetCurrentProcessId() && isClass(w,L"LunaHostWindow")) {
        *(HWND*)out=w;uiThread=tid;return FALSE;
    }
    return TRUE;
}
static DWORD WINAPI initialize(void *unused) {
    (void)unused;
    /* Pin before installing callbacks: disabling/removing a plugin must not leave dangling code. */
    HMODULE pinned;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                          (LPCWSTR)&initialize,&pinned))return 0;
    HWND host=NULL;
    for(int i=0;i<200 && !host;i++) {EnumWindows(findHost,(LPARAM)&host);if(!host)Sleep(50);}
    if(!host)return 0;
    dark=!highContrast();background=CreateSolidBrush(BG);field=CreateSolidBrush(FIELD);
    if(!background || !field)return 0;
    HANDLE lifetime=OpenThread(SYNCHRONIZE,FALSE,uiThread);
    if(!lifetime)return 0;
    hook=SetWindowsHookExW(WH_CALLWNDPROC,onMessage,module,uiThread);
    queuedHook=SetWindowsHookExW(WH_GETMESSAGE,onQueued,module,uiThread);
    if(hook) {DWORD_PTR result;SendMessageTimeoutW(host,WM_NULL,0,0,SMTO_ABORTIFHUNG,2000,&result);}
    /* Windows removes hooks when the installing thread exits. Keep their
       owner alive until the UI thread finishes (Wine did not expose this). */
    if(hook || queuedHook)WaitForSingleObject(lifetime,INFINITE);
    if(hook)UnhookWindowsHookEx(hook);
    if(queuedHook)UnhookWindowsHookEx(queuedHook);
    CloseHandle(lifetime);
    return 0;
}
__declspec(dllexport) wchar_t *OnNewSentence(wchar_t *sentence, const void *info) {
    (void)info;return sentence;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH) {
        module=instance;DisableThreadLibraryCalls(instance);
        HANDLE worker=CreateThread(NULL,0,initialize,NULL,0,NULL);
        if(worker)CloseHandle(worker);
    }
    return TRUE;
}
