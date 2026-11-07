#include "runtime/NativeWindow.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(SPORE_NATIVE_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdlib>
#endif

namespace Spore::Terrain::View {
namespace {
void ValidateCallbacks(uint32_t w,uint32_t h,const DesktopCallbacks& cb) {
  if(!cb.onFrame||w<64||h<64||w>1920||h>1080)
    throw std::invalid_argument("invalid window size or frame callback");
}
void CheckFrame(const Frame& f,uint32_t w,uint32_t h) {
  if(f.width!=w||f.height!=h||f.rgb.size()!=size_t(w)*h)
    throw std::runtime_error("desktop callback returned an invalid frame");
}
#if defined(_WIN32)
struct WindowState {
  const DesktopCallbacks* cb=nullptr;
  bool alive=true;
};
int KeyForWindows(WPARAM v) {
  switch(v) {
  case VK_LEFT:return LeftArrow;case VK_RIGHT:return RightArrow;
  case VK_UP:return UpArrow;case VK_DOWN:return DownArrow;
  case VK_F5:return F5;case VK_F9:return F9;
  case VK_ESCAPE:return 27;case VK_SPACE:return 32;
  case VK_ADD:case VK_OEM_PLUS:return '+';
  case VK_SUBTRACT:case VK_OEM_MINUS:return '-';
  default:return v>=32 && v<=126?static_cast<int>(v):0;
  }
}
LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_NCCREATE){
    auto* cs=reinterpret_cast<CREATESTRUCTW*>(lp);
    SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto* state=reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
  if(!state)return DefWindowProcW(hwnd,msg,wp,lp);
  switch(msg){
  case WM_CLOSE:
    if(state->cb->onInput)state->cb->onInput({InputKind::Close});
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:state->alive=false;PostQuitMessage(0);return 0;
  case WM_KEYDOWN:{
    const int key=KeyForWindows(wp);
    if(state->cb->onInput && key)
      state->cb->onInput({InputKind::KeyDown,key});
    if(key==27)DestroyWindow(hwnd);
    return 0;
  }
  case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:case WM_MBUTTONDOWN:{
    const int button=msg==WM_LBUTTONDOWN?1:msg==WM_MBUTTONDOWN?2:3;
    if(state->cb->onInput){
      const int x=static_cast<short>(LOWORD(lp));
      const int y=static_cast<short>(HIWORD(lp));
      state->cb->onInput({InputKind::MouseDown,0,x,y,button});
    }
    return 0;
  }
  case WM_ERASEBKGND:return 1;
  case WM_PAINT:{
    PAINTSTRUCT paint;BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);
    return 0;
  }
  default:return DefWindowProcW(hwnd,msg,wp,lp);
  }
}
#elif defined(SPORE_NATIVE_X11)
int KeyForX11(KeySym value) {
  switch(value){
  case XK_Left:return LeftArrow;case XK_Right:return RightArrow;
  case XK_Up:return UpArrow;case XK_Down:return DownArrow;
  case XK_F5:return F5;case XK_F9:return F9;
  case XK_Escape:return 27;case XK_space:return 32;
  case XK_equal:case XK_plus:case XK_KP_Add:return '+';
  case XK_minus:case XK_KP_Subtract:return '-';
  default:
    if(value>=XK_a&&value<=XK_z)return int('A'+value-XK_a);
    if(value>=XK_A&&value<=XK_Z)return int(value);
    if(value>=XK_0&&value<=XK_9)return int(value);
    return 0;
  }
}
#endif
} // namespace

int RunDesktop(const std::string& title,uint32_t initialWidth,uint32_t initialHeight,
               const DesktopCallbacks& callbacks) {
  ValidateCallbacks(initialWidth,initialHeight,callbacks);
#if defined(_WIN32)
  constexpr wchar_t kClass[]=L"SporeNativeSoftwareView";
  WNDCLASSW wc{};
  wc.lpfnWndProc=WindowProc;
  wc.hInstance=GetModuleHandleW(nullptr);
  wc.lpszClassName=kClass;
  wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
  wc.style=CS_HREDRAW|CS_VREDRAW;
  if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
    throw std::runtime_error("Win32 viewer window class registration failed");
  WindowState state{&callbacks,true};
  const std::wstring heading(title.begin(),title.end());
  HWND hwnd=CreateWindowExW(0,kClass,heading.c_str(),WS_OVERLAPPEDWINDOW,
         CW_USEDEFAULT,CW_USEDEFAULT,int(initialWidth)+16,int(initialHeight)+39,
         nullptr,nullptr,wc.hInstance,&state);
  if(!hwnd)throw std::runtime_error("Win32 viewer window creation failed");
  ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
  MSG message{};
  while(state.alive){
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
      if(message.message==WM_QUIT){state.alive=false;break;}
      TranslateMessage(&message);DispatchMessageW(&message);
    }
    if(!state.alive)break;
    if(callbacks.shouldClose && callbacks.shouldClose())break;
    if(callbacks.onTick)callbacks.onTick();
    RECT rect{};
    GetClientRect(hwnd,&rect);
    const int width=std::clamp<int>(rect.right-rect.left,64,1920);
    const int height=std::clamp<int>(rect.bottom-rect.top,64,1080);
    const auto frame=callbacks.onFrame(uint32_t(width),uint32_t(height));
    CheckFrame(frame,uint32_t(width),uint32_t(height));
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth=width;
    bmi.bmiHeader.biHeight=-height; // top-down BGRA
    bmi.bmiHeader.biPlanes=1;
    bmi.bmiHeader.biBitCount=32;
    bmi.bmiHeader.biCompression=BI_RGB;
    HDC dc=GetDC(hwnd);
    if(!dc)throw std::runtime_error("Win32 GDI window context failed");
    StretchDIBits(dc,0,0,width,height,0,0,width,height,
                  frame.rgb.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
    ReleaseDC(hwnd,dc);
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
  if(IsWindow(hwnd))DestroyWindow(hwnd);
  return 0;
#elif defined(SPORE_NATIVE_X11)
  Display* display=XOpenDisplay(nullptr);
  if(!display)throw std::runtime_error("no X11 display; use headless CLI mode");
  struct DisplayClose{
    Display* display;
    ~DisplayClose(){if(display)XCloseDisplay(display);}
  } close{display};
  const int screen=DefaultScreen(display);
  const auto root=RootWindow(display,screen);
  const auto win=XCreateSimpleWindow(display,root,0,0,
                                    initialWidth,initialHeight,0,
                                    BlackPixel(display,screen),
                                    WhitePixel(display,screen));
  XStoreName(display,win,title.c_str());
  XSelectInput(display,win,ExposureMask|KeyPressMask|ButtonPressMask|
                                StructureNotifyMask);
  Atom wmDelete=XInternAtom(display,"WM_DELETE_WINDOW",False);
  XSetWMProtocols(display,win,&wmDelete,1);
  XMapWindow(display,win);
  GC gc=XCreateGC(display,win,0,nullptr);
  struct WindowClose{
    Display* display;Window window;GC gc;
    ~WindowClose(){XFreeGC(display,gc);XDestroyWindow(display,window);}
  } windowClose{display,win,gc};
  bool alive=true;
  uint32_t width=initialWidth,height=initialHeight;
  while(alive){
    while(XPending(display)){
      XEvent event;XNextEvent(display,&event);
      switch(event.type){
      case ClientMessage:
        if(Atom(event.xclient.data.l[0])==wmDelete)alive=false;
        break;
      case DestroyNotify:alive=false;break;
      case ConfigureNotify:
        width=uint32_t(std::clamp(event.xconfigure.width,64,1920));
        height=uint32_t(std::clamp(event.xconfigure.height,64,1080));
        break;
      case KeyPress:{
        const int key=KeyForX11(XLookupKeysym(&event.xkey,0));
        if(callbacks.onInput&&key)callbacks.onInput({InputKind::KeyDown,key});
        if(key==27)alive=false;
        break;
      }
      case ButtonPress:{
        const int key=event.xbutton.button==Button1?1:
                      event.xbutton.button==Button3?3:
                      event.xbutton.button==Button2?2:0;
        if(callbacks.onInput&&key)
          callbacks.onInput({InputKind::MouseDown,0,
                             event.xbutton.x,event.xbutton.y,key});
        break;
      }
      default:break;
      }
    }
    if(!alive)break;
    if(callbacks.shouldClose && callbacks.shouldClose())break;
    if(callbacks.onTick)callbacks.onTick();
    const auto frame=callbacks.onFrame(width,height);
    CheckFrame(frame,width,height);
    auto* pixels=static_cast<char*>(std::malloc(frame.rgb.size()*4));
    if(!pixels)throw std::bad_alloc();
    std::memcpy(pixels,frame.rgb.data(),frame.rgb.size()*4);
    XImage* image=XCreateImage(display,DefaultVisual(display,screen),
                              uint32_t(DefaultDepth(display,screen)),ZPixmap,
                              0,pixels,width,height,32,0);
    if(!image){std::free(pixels);throw std::runtime_error("X11 framebuffer creation failed");}
    XPutImage(display,win,gc,image,0,0,0,0,width,height);
    XDestroyImage(image);
    XFlush(display);
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
  return 0;
#else
  (void)title;
  throw std::runtime_error("native desktop viewer unavailable on this platform");
#endif
}

} // namespace Spore::Terrain::View
