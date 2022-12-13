/*
 * Dependencies:
 *  gdi32
 *  (kernel32)
 *  user32
 *  (comctl32)
 *  msimg32.lib
 */

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <time.h>
#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>

#include <iostream>
#include <string>

constexpr auto KEY_SHIFTED = 0x8000;
constexpr auto KEY_TOGGLED = 0x0001;
constexpr UINT BYTES_TO_READ = 18u;
constexpr UINT8 COLOR_CHANGE_OFFSET = 15u;
constexpr UINT MAX_MATRIX_SIZE = 15u;
constexpr UINT DEFAULT_MATRIX_SIZE = 3u;
constexpr LPDWORD RENDER_THREAD_ID = 0;

const TCHAR szWinClass[] = _T("Win32SampleApp");
const TCHAR szCfgName[] = _T("TicTacToe.cfg");
const TCHAR szSharedMemoryName[] = _T("Local\\TicTacToeFileMapping");
const TCHAR szNPWinnerMessage[] = _T("Noughts won!");
const TCHAR szCPWinnerMessage[] = _T("Crosses won!");
const TCHAR szNoughtsTurnTitle[] = _T("TicTacToe: Noughts turn");
const TCHAR szCrossesTurnTitle[] = _T("TicTacToe: Crosses turn");
const TCHAR szWinName[] = _T("TicTacToe: Noughts turn");
const TCHAR szSemaphoreName[] = _T("RenderThreadSemaphore");

void RunNotepad(void) {
  STARTUPINFO sInfo;
  PROCESS_INFORMATION pInfo;

  ZeroMemory(&sInfo, sizeof(STARTUPINFO));

  _tprintf(_T("Starting Notepad..."));
  CreateProcess(_T("C:\\Windows\\Notepad.exe"), NULL, NULL, NULL, FALSE, 0,
                NULL, NULL, &sInfo, &pInfo);
}

void UIntToUChar(const UINT& uint, UINT8* buffer) {
  buffer[0] = (uint >> 24);
  buffer[1] = (uint >> 16);
  buffer[2] = (uint >> 8);
  buffer[3] = uint;
}

void UCharToUInt(UINT8* buffer, UINT& uint) {
  uint = buffer[0];
  uint = (uint << 8) + buffer[1];
  uint = (uint << 8) + buffer[2];
  uint = (uint << 8) + buffer[3];
}

int DefineThreadPriority(WPARAM d) {
  switch (d) {
    case 0x31:
      return THREAD_PRIORITY_IDLE;
    case 0x32:
      return THREAD_PRIORITY_LOWEST;
    case 0x33:
      return THREAD_PRIORITY_BELOW_NORMAL;
    case 0x35:
      return THREAD_PRIORITY_ABOVE_NORMAL;
    case 0x36:
      return THREAD_PRIORITY_HIGHEST;
    case 0x37:
      return THREAD_PRIORITY_TIME_CRITICAL;
    default:
      return THREAD_PRIORITY_NORMAL;
  }
}

enum COLOR_CHANGE_STATE {
  GREEN_UP = 0,
  RED_DOWN = 1,
  BLUE_UP = 2,
  GREEN_DOWN = 3,
  RED_UP = 4,
  BLUE_DOWN = 5
};

enum GAME_TURN { NOUGHTS = 1u, CROSSES = 2u, DRAW = 3u };

class COLOR {
 public:
  COLOR() : red(0), green(0), blue(0) {}
  COLOR(UINT8 r, UINT8 g, UINT8 b) : red(r), green(g), blue(b) {}
  COLOR(COLORREF ref)
      : red((ref >> 16) & 0xFF), green((ref >> 8) & 0xFF), blue(ref & 0xFF) {}

  COLOR& operator=(const COLOR& lhs) {
    red = lhs.red;
    green = lhs.green;
    blue = lhs.blue;
    return *this;
  }

  void ToUChar(UINT8* buffer);
  void FromUChar(UINT8* buffer);

  COLORREF GetColorref();
  COLORREF GetContrast();

  COLOR operator*(const float& mult);

  void SetRGB(UINT8 r, UINT8 g, UINT8 b);

  UINT8 red;
  UINT8 green;
  UINT8 blue;
};

class FIELD {
 public:
  FIELD()
      : size(DEFAULT_MATRIX_SIZE),
        cellsFiled(0u),
        hMapFile(nullptr),
        pBuf(nullptr) {}
  ~FIELD() {
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
  }

  bool TryOpenFileMapping();
  bool Create(UINT size);

  void SetCellValue(UINT x, UINT y, UINT8 value);
  UINT8 GetCellValue(UINT x, UINT y);

  UINT8 CheckGameField(WPARAM x, LPARAM y);

  GAME_TURN GetTurn();
  UINT GetSize();

 private:
  UINT size, cellsFiled, turnsCount;
  HANDLE hMapFile;
  LPTSTR pBuf;
};

struct RESOLUTION {
  RESOLUTION(UINT w, UINT h) : width(w), height(h) {}

  UINT width;
  UINT height;
};

class GAME {
 public:
  GAME()
      : res(320, 240),
        field(FIELD()),
        bgColor(0, 0, 255),
        linesColor(255, 0, 0),
        linesColorChange(GREEN_UP),
        wmSynch(0),
        backHDC(0),
        backBMP() {}
  ~GAME();

  bool Create(int argc, char** argv, HINSTANCE hThisInstance);
  bool Close();

  void Show();

  void Display();
  void Resize();
  void ChangeBgColor();
  bool TryMakeTurn(UINT x, UINT y, GAME_TURN turn);
  bool ProccessSynchMessage(WPARAM wParam, LPARAM lParam);
  void Render();
  void ChangeLinesColorUp();
  void ChangeLinesColorDown();

  RESOLUTION GetRes();
  UINT GetSize();
  UINT GetSynchMessage();

 private:
  UINT ReadFromFile();
  bool WriteToFile();

  void DefineColorChangeState(COLOR& color, COLOR_CHANGE_STATE& colorState);
  void ChangeColorUp(COLOR& color, COLOR_CHANGE_STATE& colorState,
                     UINT8 colorOffset);
  void ChangeColorDown(COLOR& color, COLOR_CHANGE_STATE& colorState,
                       UINT8 colorOffset);

  void SendSynchMessage(UINT x, UINT y);
  bool SetWinTitle(GAME_TURN);

  RESOLUTION res;
  UINT wmSynch;
  FIELD field;
  COLOR bgColor, linesColor;
  COLOR_CHANGE_STATE linesColorChange;
  HWND hwnd;
  HDC backHDC;
  HBITMAP backBMP;
  RECT clientRect;
};

GAME game;
HANDLE hRenderSemaphore;
HANDLE hRenderThread;
bool isRenderThreadActive;
bool isRenderThreadPaused;

inline void LockRenderThread() {
  if (!isRenderThreadPaused) {
    WaitForSingleObject(hRenderSemaphore, INFINITE);
  }
}

inline void UnlockRenderThread() {
  if (!isRenderThreadPaused) {
    ReleaseSemaphore(hRenderSemaphore, 1, NULL);
  }
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
    case WM_CLOSE: {
      if (!game.Close()) {
        return 1;
      }
    }
    case WM_DESTROY: {
      PostQuitMessage(0);
      return 0;
    }
    case WM_SIZE: {
      LockRenderThread();
      game.Resize();
      UnlockRenderThread();
      return 0;
    }
    case WM_KEYDOWN: {
      switch (wParam) {
        case 67: {
          if (GetKeyState(VK_SHIFT) & KEY_SHIFTED) {
            RunNotepad();
          }
          return 0;
        }
        case 81: {
          if (GetKeyState(VK_CONTROL) & KEY_SHIFTED) {
            if (!game.Close()) {
              return 1;
            }
          }
          return 0;
        }
        case VK_RETURN: {
          LockRenderThread();
          game.ChangeBgColor();
          UnlockRenderThread();
          return 0;
        }
        case VK_ESCAPE: {
          if (!game.Close()) {
            return 1;
          }
          return 0;
        }
        case VK_SPACE: {
          if (isRenderThreadPaused) {
            ReleaseSemaphore(hRenderSemaphore, 1, NULL);
          } else {
            WaitForSingleObject(hRenderSemaphore, INFINITE);
          }
          isRenderThreadPaused = !isRenderThreadPaused;
          return 0;
        }
        default: {
          if (wParam > 48 && wParam < 56) {
            if (!SetThreadPriority(hRenderThread,
                                   DefineThreadPriority(wParam))) {
              _tprintf(_T("Thread priority changing error!\n"));
            }
          }
        }
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      LockRenderThread();
      RESOLUTION coords = game.GetRes();
      coords.width = GET_X_LPARAM(lParam) / (coords.width / game.GetSize());
      coords.height = GET_Y_LPARAM(lParam) / (coords.height / game.GetSize());
      game.TryMakeTurn(coords.width, coords.height, NOUGHTS);
      return 0;
    }
    case WM_RBUTTONUP: {
      LockRenderThread();
      RESOLUTION coords = game.GetRes();
      coords.width = GET_X_LPARAM(lParam) / (coords.width / game.GetSize());
      coords.height = GET_Y_LPARAM(lParam) / (coords.height / game.GetSize());
      game.TryMakeTurn(coords.width, coords.height, CROSSES);
      return 0;
    }
    case WM_PAINT: {
      game.Display();
      return 0;
    }
    case WM_MOUSEWHEEL: {
      LockRenderThread();
      if ((int16_t)HIWORD(wParam) > 0) {
        game.ChangeLinesColorUp();
      } else {
        game.ChangeLinesColorDown();
      }
      UnlockRenderThread();
      return 0;
    }
    default: {
      if (message == game.GetSynchMessage()) {
        game.ProccessSynchMessage(wParam, lParam);
      }
    }
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

DWORD WINAPI RenderThreadFunction(LPVOID) {
  game.Render();
  return TRUE;
}

int main(int argc, char** argv) {
  srand(time(NULL));

  BOOL bMessageOk;
  MSG message;
  WNDCLASS winCl = {0};

  HINSTANCE hThisInstance = GetModuleHandle(NULL);

  winCl.hInstance = hThisInstance;
  winCl.lpszClassName = szWinClass;
  winCl.lpfnWndProc = WindowProcedure;
  winCl.hbrBackground = CreateSolidBrush(COLORREF(RGB(0, 0, 0)));
  if (!RegisterClass(&winCl)) return 1;

  game.Create(argc, argv, hThisInstance);
  game.Show();

  isRenderThreadActive = true;
  isRenderThreadPaused = false;
  hRenderSemaphore = CreateSemaphore(NULL, 1, 1, NULL);
  hRenderThread =
      CreateThread(NULL, 0, RenderThreadFunction, NULL, 0, RENDER_THREAD_ID);

  while ((bMessageOk = GetMessage(&message, NULL, 0, 0)) != 0) {
    if (bMessageOk == -1) {
      _tprintf(_T("Suddenly, GetMessage failed! Last error code is %d"),
               GetLastError());
      break;
    }
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  CloseHandle(hRenderThread);
  CloseHandle(hRenderSemaphore);
  UnregisterClass(szWinClass, hThisInstance);

  return 0;
}

void COLOR::ToUChar(UINT8* buffer) {
  buffer[0] = red;
  buffer[1] = green;
  buffer[2] = blue;
}

void COLOR::FromUChar(UINT8* buffer) {
  red = buffer[0];
  green = buffer[1];
  blue = buffer[2];
}

COLORREF COLOR::GetColorref() { return COLORREF(RGB(red, green, blue)); }

COLORREF COLOR::GetContrast() {
  if (((red * 299 + green * 587 + blue * 114) / 1000) > 128) {
    return COLORREF(RGB(0, 0, 0));
  }
  return COLORREF(RGB(255, 255, 255));
}

COLOR COLOR::operator*(const float& mult) {
  COLOR temp(*this);
  temp.red = (UINT)((FLOAT)(temp.red * mult));
  temp.green = (UINT)((FLOAT)(temp.green * mult));
  temp.blue = (UINT)((FLOAT)(temp.blue * mult));
  return temp;
}

void COLOR::SetRGB(UINT8 r, UINT8 g, UINT8 b) {
  red = r;
  green = g;
  blue = b;
}

bool FIELD::TryOpenFileMapping() {
  hMapFile = OpenFileMapping(PAGE_READWRITE, FALSE, szSharedMemoryName);
  if (hMapFile == NULL) {
    return false;
  }
  CloseHandle(hMapFile);
  return true;
}

bool FIELD::Create(UINT s) {
  if (s <= MAX_MATRIX_SIZE) {
    size = s;
  }
  hMapFile =
      CreateFileMapping(INVALID_HANDLE_VALUE,  // use paging file
                        NULL,                  // default security
                        PAGE_READWRITE,        // read/write access
                        0,         // maximum object size (high-order DWORD)
                        size + 1,  // maximum object size (low-order DWORD)
                        szSharedMemoryName);
  if (hMapFile == NULL) {
    _tprintf(TEXT("Could not create file mapping object (%d).\n"),
             GetLastError());
    return 0;
  }
  pBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size + 1);
  return ((pBuf == nullptr) ? false : true);
}

void FIELD::SetCellValue(UINT x, UINT y, UINT8 value) {
  if (pBuf[x * size + y] == 0) {
    pBuf[x * size + y] = value;
    cellsFiled++;
    pBuf[size * size] = !pBuf[size * size];
  }
}

UINT8 FIELD::GetCellValue(UINT x, UINT y) {
  return static_cast<UINT8>(pBuf[x * size + y]);
}

UINT8 FIELD::CheckGameField(WPARAM x, LPARAM y) {
  UINT8 value = static_cast<UINT8>(pBuf[x * size + y]);
  bool flag = true;
  for (UINT i = 0; i < size; i++) {
    if (pBuf[x * size + i] != value) {
      flag = false;
      break;
    }
  }
  if (flag) {
    return value;
  }
  flag = true;
  for (UINT i = 0; i < size; i++) {
    if (pBuf[i * size + y] != value) {
      flag = false;
      break;
    }
  }
  if (flag) {
    return value;
  }
  if (x == y || x == (size - 1 - y)) {
    flag = true;
    for (UINT i = 0; i < size; i++) {
      if (pBuf[i * size + i] != value) {
        flag = false;
        break;
      }
    }
    if (flag) {
      return value;
    }
    flag = true;
    for (UINT i = 0; i < size; i++) {
      if (pBuf[(size - 1 - i) * size + i] != value) {
        flag = false;
        break;
      }
    }
    if (flag) {
      return value;
    }
  }
  if (cellsFiled == size * size) {
    return DRAW;
  }
  return 0;
}

GAME_TURN FIELD::GetTurn() {
  return (pBuf[size * size] & 1) ? CROSSES : NOUGHTS;
}

UINT FIELD::GetSize() { return size; }

GAME::~GAME() {}

bool GAME::Create(int argc, char** argv, HINSTANCE hThisInstance) {
  if (!(wmSynch = RegisterWindowMessage((LPCTSTR) _T("WM_TTTSYNCH")))) return 0;

  UINT size = ReadFromFile();
  if (!field.TryOpenFileMapping() && (argc > 1)) {
    size = atoi(argv[1]);
    if (!WriteToFile()) {
      return 0;
    }
  }
  if (!field.Create(size)) {
    return 0;
  }

  hwnd =
      CreateWindow(szWinClass,          /* Classname */
                   szWinName,           /* Title Text */
                   WS_OVERLAPPEDWINDOW, /* default window */
                   CW_USEDEFAULT,       /* Windows decides the position */
                   CW_USEDEFAULT, /* where the window ends up on the screen */
                   res.width,     /* The programs width */
                   res.height,    /* and height in pixels */
                   HWND_DESKTOP,  /* The window is a child-window to desktop */
                   NULL,          /* No menu */
                   hThisInstance, /* Program Instance handler */
                   NULL           /* No Window Creation data */
      );

  HBRUSH hBrush = CreateSolidBrush(bgColor.GetColorref());
  hBrush = (HBRUSH)(DWORD_PTR)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                                              (LONG)hBrush);

  DeleteObject(hBrush);
  DefineColorChangeState(linesColor, linesColorChange);

  return 1;
}

bool GAME::Close() {
  isRenderThreadActive = false;
  LockRenderThread();
  DeleteDC(backHDC);
  if (hwnd != NULL) {
    if (!WriteToFile()) {
      return 0;
    }
    DestroyWindow(hwnd);
  }
  UnlockRenderThread();
  return 1;
}

void GAME::Show() {
  ShowWindow(hwnd, SW_SHOW);
#ifdef NDEBUG  // RELEASE
  ShowWindow(GetConsoleWindow(), SW_HIDE);
#else  // DEBUG
  ShowWindow(GetConsoleWindow(), SW_SHOW);
#endif
}

void GAME::Display() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  BitBlt(hdc, 0, 0, res.width, res.height, backHDC, 0, 0, SRCCOPY);
  EndPaint(hwnd, &ps);
}

void GAME::Resize() {
  GetClientRect(hwnd, &clientRect);
  res.width = clientRect.right;
  res.height = clientRect.bottom;

  HDC hdc;
  hdc = GetDC(hwnd);
  backHDC = CreateCompatibleDC(hdc);
  backBMP = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
  HBITMAP oldBMP = (HBITMAP)SelectObject(backHDC, backBMP);
  DeleteObject(oldBMP);
  ReleaseDC(hwnd, hdc);
}

void GAME::ChangeBgColor() {
  bgColor = COLOR(rand() % 255, rand() % 255, rand() % 255);
  HBRUSH hBrush = CreateSolidBrush(bgColor.GetColorref());
  hBrush = (HBRUSH)(DWORD_PTR)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                                              (LONG)hBrush);
  DeleteObject(hBrush);
}

bool GAME::TryMakeTurn(UINT x, UINT y, GAME_TURN turn) {
  if (turn == field.GetTurn()) {
    field.SetCellValue(x, y, turn);
    SendSynchMessage(field.CheckGameField(x, y), NULL);
    return true;
  } else {
    UnlockRenderThread();
  }
  return false;
}

bool GAME::ProccessSynchMessage(WPARAM wParam, LPARAM lParam) {
  if (!SetWinTitle(field.GetTurn())) {
    return false;
  }
  UnlockRenderThread();
  switch (wParam) {
    case NOUGHTS: {
      MessageBox(hwnd, szNPWinnerMessage, _T("Game over"), MB_OK);
      break;
    }
    case CROSSES: {
      MessageBox(hwnd, szCPWinnerMessage, _T("Game over"), MB_OK);
      break;
    }
    case DRAW: {
      MessageBox(hwnd, _T("Draw!"), _T("Game over"), MB_OK);
      break;
    }
    default: {
      return true;
    }
  }
  if (!game.Close()) {
    return 1;
  }
  return true;
}

USHORT ToUS(UINT8 color) { return (USHORT)(UINT)(color * 65535 / 255); }

void GAME::Render() {
  HDC hdc;
  GetClientRect(hwnd, &clientRect);
  hdc = GetDC(hwnd);
  backHDC = CreateCompatibleDC(hdc);
  backBMP = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
  HGDIOBJ oldBMP = SelectObject(backHDC, backBMP);
  DeleteObject(oldBMP);
  ReleaseDC(hwnd, hdc);

  struct Carriege {
    Carriege() : pos(0), way(true) {}
    Carriege operator++() {
      switch (way) {
        case true: {
          if (++pos == 100) {
            way = false;
          }
          return *this;
        }
        case false: {
          if (--pos == 0) {
            way = true;
          }
          return *this;
        }
      }
    }

    operator UINT() const { return pos; }

    UINT pos;
    bool way;
  } t;

  while (isRenderThreadActive) {
    WaitForSingleObject(hRenderSemaphore, INFINITE);

    ++t;
    COLOR beetweenClr = bgColor * 0.67f;
    TRIVERTEX vertexesT[2] = {
        {0, 0, ToUS(bgColor.red), ToUS(bgColor.green), ToUS(bgColor.blue),
         0x0000},
        {res.width, res.height * t / 100, ToUS(beetweenClr.red),
         ToUS(beetweenClr.green), ToUS(beetweenClr.blue), 0x0000}};
    TRIVERTEX vertexesB[2] = {
        {0, res.height * t / 100, ToUS(beetweenClr.red),
         ToUS(beetweenClr.green), ToUS(beetweenClr.blue), 0x0000},
        {res.width, res.height, ToUS(bgColor.red), ToUS(bgColor.green),
         ToUS(bgColor.blue), 0x0000}};
    GRADIENT_RECT gRect;
    gRect.UpperLeft = 0;
    gRect.LowerRight = 1;

    GradientFill(backHDC, vertexesT, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
    GradientFill(backHDC, vertexesB, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

    HPEN hPen = CreatePen(PS_SOLID, NULL, linesColor.GetColorref());
    HPEN hDefaultPen = (HPEN)SelectObject(backHDC, hPen);

    UINT szOffsetX = res.width / field.GetSize();
    UINT szOffsetY = res.height / field.GetSize();
    for (UINT i = 1; i < field.GetSize(); i++) {
      MoveToEx(backHDC, szOffsetX * i, 0, NULL);
      LineTo(backHDC, szOffsetX * i, res.height);
      MoveToEx(backHDC, 0, szOffsetY * i, NULL);
      LineTo(backHDC, res.width, szOffsetY * i);
    }

    hPen = (HPEN)SelectObject(backHDC, hDefaultPen);
    DeleteObject(hPen);

    hPen = CreatePen(PS_SOLID, 3, bgColor.GetContrast());
    hDefaultPen = (HPEN)SelectObject(backHDC, hPen);
    HBRUSH hDefaultBrush =
        (HBRUSH)SelectObject(backHDC, GetStockObject(NULL_BRUSH));

    UINT szEllipseOffsetX = szOffsetX / 10;
    UINT szEllipseOffsetY = szOffsetY / 10;
    for (UINT i = 0; i < field.GetSize(); i++) {
      for (UINT j = 0; j < field.GetSize(); j++) {
        switch (field.GetCellValue(i, j)) {
          case NOUGHTS: {
            Ellipse(backHDC, szEllipseOffsetX + szOffsetX * i,
                    szEllipseOffsetY + szOffsetY * j,
                    szOffsetX * (i + 1) - szEllipseOffsetX,
                    szOffsetY * (j + 1) - szEllipseOffsetY);
            break;
          }
          case CROSSES: {
            MoveToEx(backHDC, szEllipseOffsetX + szOffsetX * i,
                     szEllipseOffsetY + szOffsetY * j, NULL);
            LineTo(backHDC, szOffsetX * (i + 1) - szEllipseOffsetX,
                   szOffsetY * (j + 1) - szEllipseOffsetY);
            MoveToEx(backHDC, szOffsetX * (i + 1) - szEllipseOffsetX,
                     szEllipseOffsetY + szOffsetY * j, NULL);
            LineTo(backHDC, szEllipseOffsetX + szOffsetX * i,
                   szOffsetY * (j + 1) - szEllipseOffsetY);
            break;
          }
        }
      }
    }

    hDefaultBrush = (HBRUSH)SelectObject(backHDC, hDefaultBrush);
    DeleteObject(hDefaultBrush);
    hPen = (HPEN)SelectObject(backHDC, hDefaultPen);
    DeleteObject(hPen);

    ReleaseSemaphore(hRenderSemaphore, 1, NULL);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);

    Sleep(1000 / 60);
  }

  SelectObject(backHDC, oldBMP);
  DeleteObject(backBMP);
  DeleteDC(backHDC);
}

void GAME::ChangeLinesColorUp() {
  ChangeColorUp(linesColor, linesColorChange, COLOR_CHANGE_OFFSET);
}

void GAME::ChangeLinesColorDown() {
  ChangeColorDown(linesColor, linesColorChange, COLOR_CHANGE_OFFSET);
}

bool GAME::SetWinTitle(GAME_TURN turn) {
  if (!SetWindowText(
          hwnd, (turn == NOUGHTS) ? szNoughtsTurnTitle : szCrossesTurnTitle)) {
    return false;
  }
  return true;
}

RESOLUTION GAME::GetRes() { return res; }

UINT GAME::GetSize() { return field.GetSize(); }

UINT GAME::GetSynchMessage() { return wmSynch; }

UINT GAME::ReadFromFile() {
  UINT size = DEFAULT_MATRIX_SIZE;
  HANDLE hFile =
      CreateFile(szCfgName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    return size;
  }

  UINT8* buffer = new UINT8[BYTES_TO_READ];
  DWORD dwBytesRead;
  if (!ReadFile(hFile, buffer, BYTES_TO_READ, &dwBytesRead, NULL)) {
    _tprintf(_T("ReadFile error!"));
    return size;
  }

  if (dwBytesRead != BYTES_TO_READ) {
    return size;
  }

  UCharToUInt(buffer, size);
  UCharToUInt(buffer + 4, res.width);
  UCharToUInt(buffer + 8, res.height);
  bgColor.FromUChar(buffer + 12);
  linesColor.FromUChar(buffer + 15);

  delete[] buffer;
  CloseHandle(hFile);

  return size;
}

bool GAME::WriteToFile() {
  HANDLE hFile =
      CreateFile(szCfgName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    _tprintf(TEXT("CreateFile error (%d)\n"), GetLastError());
    return 1;
  }

  SetFilePointer(hFile, NULL, NULL, FILE_BEGIN);
  SetEndOfFile(hFile);
  UINT8* buffer = new UINT8[BYTES_TO_READ];
  DWORD dwBytesToWrite = BYTES_TO_READ;

  RECT rect;
  GetWindowRect(hwnd, &rect);

  UIntToUChar(field.GetSize(), buffer);
  UIntToUChar(rect.right - rect.left, buffer + 4);
  UIntToUChar(rect.bottom - rect.top, buffer + 8);
  bgColor.ToUChar(buffer + 12);
  linesColor.ToUChar(buffer + 15);

  DWORD dwBytesWritten;
  WriteFile(hFile, buffer, dwBytesToWrite, &dwBytesWritten, NULL);
  if (dwBytesToWrite != dwBytesWritten) {
    _tprintf(_T("WriteFile error!"));
    return false;
  }
  delete[] buffer;
  CloseHandle(hFile);

  return true;
}

void GAME::DefineColorChangeState(COLOR& color,
                                  COLOR_CHANGE_STATE& colorState) {
  if (color.red == 255 && color.green < 255) {
    colorState = GREEN_UP;
  } else if (color.red == 255 && color.blue > 0) {
    colorState = BLUE_DOWN;
  } else if (color.green == 255 && color.blue < 255) {
    colorState = BLUE_UP;
  } else if (color.green == 255 && color.red > 0) {
    colorState = RED_DOWN;
  } else if (color.blue == 255 && color.red < 255) {
    colorState = RED_UP;
  } else {
    colorState = GREEN_DOWN;
  }
}

void GAME::ChangeColorUp(COLOR& color, COLOR_CHANGE_STATE& colorState,
                         UINT8 colorOffset) {
  switch (colorState) {
    case GREEN_UP:
      color.green += colorOffset;
      if ((color.green) == 255) {
        colorState = RED_DOWN;
      }
      break;
    case RED_DOWN:
      color.red -= colorOffset;
      if ((color.red) == 0) {
        colorState = BLUE_UP;
      }
      break;
    case BLUE_UP:
      color.blue += colorOffset;
      if ((color.blue) == 255) {
        colorState = GREEN_DOWN;
      }
      break;
    case GREEN_DOWN:
      color.green -= colorOffset;
      if ((color.green) == 0) {
        colorState = RED_UP;
      }
      break;
    case RED_UP:
      color.red += colorOffset;
      if ((color.red) == 255) {
        colorState = BLUE_DOWN;
      }
      break;
    case BLUE_DOWN:
      color.blue -= colorOffset;
      if ((color.blue) == 0) {
        colorState = GREEN_UP;
      }
      break;
  }
}

void GAME::ChangeColorDown(COLOR& color, COLOR_CHANGE_STATE& colorState,
                           UINT8 colorOffset) {
  switch (colorState) {
    case GREEN_UP:
      if (color.green > 0) {
        color.green -= colorOffset;
      } else {
        color.blue += colorOffset;
        colorState = BLUE_DOWN;
      }
      break;
    case RED_DOWN:
      if (color.red < 255) {
        color.red += colorOffset;
      } else {
        color.green -= colorOffset;
        colorState = GREEN_UP;
      }
      break;
    case BLUE_UP:
      if (color.blue > 0) {
        color.blue -= colorOffset;
      } else {
        color.red += colorOffset;
        colorState = RED_DOWN;
      }
      break;
    case GREEN_DOWN:
      if (color.green < 255) {
        color.green += colorOffset;
      } else {
        color.blue -= colorOffset;
        colorState = BLUE_UP;
      }
      break;
    case RED_UP:
      if (color.red > 0) {
        color.red -= colorOffset;
      } else {
        color.green += colorOffset;
        colorState = GREEN_DOWN;
      }
      break;
    case BLUE_DOWN:
      if (color.blue < 255) {
        color.blue += colorOffset;
      } else {
        color.red -= colorOffset;
        colorState = RED_UP;
      }
      break;
  }
}

void GAME::SendSynchMessage(UINT x, UINT y) {
  PostMessage(HWND_BROADCAST, wmSynch, x, y);
}
