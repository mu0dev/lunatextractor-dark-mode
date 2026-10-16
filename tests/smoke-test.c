#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#ifdef _MSC_VER
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
static void pump(void) {
 DWORD end=GetTickCount()+1200; MSG m;
 while((LONG)(end-GetTickCount())>0) {
  while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
  Sleep(10);
 }
}
static int failures;
#define CHECK(c) do {printf("%s: %s\n",(c)?"PASS":"FAIL",#c);if(!(c))failures++;} while(0)
int main(void) {
 INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&cc);
 HMODULE dll=LoadLibraryW(L"DarkMode.dll"); CHECK(dll!=NULL);if(!dll){printf("LoadLibrary error: %lu\n",GetLastError());return 2;}
 wchar_t text[]=L"Japanese \x65e5\x672c\x8a9e\r\n";
 wchar_t *(*pass)(wchar_t*,const void*)=(wchar_t *(*)(wchar_t*,const void*))GetProcAddress(dll,"OnNewSentence");
 CHECK(pass && pass(text,NULL)==text);CHECK(wcscmp(text,L"Japanese \x65e5\x672c\x8a9e\r\n")==0);
 WNDCLASSW wc={0};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"LunaHostWindow";wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
 HWND w=CreateWindowW(wc.lpszClassName,L"Dark mode smoke test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,700,500,NULL,NULL,wc.hInstance,NULL);
 HWND edit=CreateWindowW(L"EDIT",text,WS_CHILD|WS_VISIBLE|WS_BORDER,20,20,600,100,w,NULL,wc.hInstance,NULL);
 HWND button=CreateWindowW(L"BUTTON",L"Checkbox",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,20,140,200,30,w,NULL,wc.hInstance,NULL);
 HWND list=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT,20,200,600,200,w,NULL,wc.hInstance,NULL);
 pump();
 HDC dc=GetDC(w);CHECK(GetPixel(dc,5,5)==RGB(30,32,36));
 HBRUSH b=(HBRUSH)SendMessageW(w,WM_CTLCOLOREDIT,(WPARAM)dc,(LPARAM)edit);
 LOGBRUSH brush={0};GetObjectW(b,sizeof(brush),&brush);CHECK(brush.lbColor==RGB(23,25,29));ReleaseDC(w,dc);
 CHECK(ListView_GetBkColor(list)==RGB(23,25,29));
 /* Regression: host resets the palette after plugin initialization. */
 ListView_SetBkColor(list,RGB(255,255,255));
 ListView_SetTextBkColor(list,RGB(255,255,255));
 ListView_SetTextColor(list,RGB(0,0,0));
 CHECK(ListView_GetBkColor(list)==RGB(23,25,29));
 CHECK(ListView_GetTextBkColor(list)==RGB(23,25,29));
 CHECK(ListView_GetTextColor(list)==RGB(232,234,238));
 SendMessageW(list,WM_THEMECHANGED,0,0);InvalidateRect(list,NULL,TRUE);UpdateWindow(list);
 CHECK(ListView_GetBkColor(list)==RGB(23,25,29));
 CHECK(ListView_GetTextBkColor(list)==RGB(23,25,29));
 CHECK(ListView_GetTextColor(list)==RGB(232,234,238));
 SendMessageW(button,BM_CLICK,0,0);CHECK(SendMessageW(button,BM_GETCHECK,0,0)==BST_CHECKED);
 SendMessageW(button,BM_CLICK,0,0);CHECK(SendMessageW(button,BM_GETCHECK,0,0)==BST_UNCHECKED);
 HWND second=CreateWindowW(wc.lpszClassName,L"Late settings window",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,400,300,w,NULL,wc.hInstance,NULL);
 printf("Second HWND=%p error=%lu\n",(void*)second,GetLastError());CHECK(second!=NULL);pump();
 printf("window procedures: main=%p second=%p default=%p\n",(void*)GetWindowLongPtrW(w,GWLP_WNDPROC),(void*)GetWindowLongPtrW(second,GWLP_WNDPROC),(void*)DefWindowProcW);
 /* Render into a bitmap: hosted runners may clip windows at screen edges. */
 dc=GetDC(second);HDC memory=CreateCompatibleDC(dc);
 HBITMAP bitmap=CreateCompatibleBitmap(dc,16,16);HGDIOBJ previous=SelectObject(memory,bitmap);
 SendMessageW(second,WM_ERASEBKGND,(WPARAM)memory,0);
 printf("Late paint pixel=%lu dc=%p bitmap=%p\n",GetPixel(memory,5,5),(void*)memory,(void*)bitmap);
 CHECK(GetPixel(memory,5,5)==RGB(30,32,36));
 SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(second,dc);
 FreeLibrary(dll);pump();CHECK(GetModuleHandleW(L"DarkMode.dll")!=NULL);
 DestroyWindow(second);DestroyWindow(w);printf("Failures: %d\n",failures);return failures?1:0;
}
