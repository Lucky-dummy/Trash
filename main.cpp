
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <time.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

constexpr auto KEY_SHIFTED = 0x8000;
constexpr auto KEY_TOGGLED = 0x0001;
constexpr UINT BYTES_TO_READ = 18u;
constexpr UINT COLOR_CHANGE_OFFSET = 15u;
constexpr UINT MAX_MATRIX_SIZE = 32u;
constexpr UINT DEFAULT_SIZE = 3u;

const TCHAR szWinClass[] = _T("Win32SampleApp");
const TCHAR szWinName[] = _T("Win32SampleWindow");
const TCHAR szCfgName[] = _T("TicTacToe.cfg");
const TCHAR szSharedMemoryName[] = _T("Local\\TicTacToeFileMapping");
const TCHAR szNPWinnerMessage[] = _T("Noughts won!");
const TCHAR szCPWinnerMessage[] = _T("Crosses won!");

void RunNotepad(void) {
  STARTUPINFO sInfo;
  PROCESS_INFORMATION pInfo;

  ZeroMemory(&sInfo, sizeof(STARTUPINFO));

  puts("Starting Notepad...");
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

enum COLOR_CHANGE_STATE {
  GREEN_UP,
  RED_DOWN,
  BLUE_UP,
  GREEN_DOWN,
  RED_UP,
  BLUE_DOWN
};

enum GAME_TURN { NOUGHTS = 1u, CROSSES = 2u };

class COLOR {
 public:
  COLOR() : red(0), green(0), blue(0) {}
  COLOR(UINT8 r, UINT8 g, UINT8 b) : red(r), green(g), blue(b) {}

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

  void SetRGB(UINT8 r, UINT8 g, UINT8 b);

  UINT8 red;
  UINT8 green;
  UINT8 blue;
};

class FIELD {
 public:
  FIELD()
      : size(DEFAULT_SIZE), cellsFiled(0), hMapFile(nullptr), pBuf(nullptr) {}
  ~FIELD() {
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
  }

  bool TryOpenFileMapping();
  bool Create(UINT size);

  void SetCellValue(UINT x, UINT y, UINT8 value);
  UINT8 GetCellValue(UINT x, UINT y);

  UINT8 CheckGameField(UINT x, UINT y);

  GAME_TURN GetTurn();
  UINT GetSize();

 private:
  UINT size, cellsFiled;
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
        wmSynch(0) {}
  ~GAME();

  bool Create(int argc, char** argv, HINSTANCE hThisInstance);
  bool Close();

  void Show();

  void Resize();
  void ChangeBgColor();
  bool TryMakeTurn(UINT x, UINT y, GAME_TURN turn);
  void ProccessSynchMessage(WPARAM wParam, LPARAM lParam);
  void Render();
  void ChangeLinesColorUp();
  void ChangeLinesColorDown();

  RESOLUTION GetRes();
  UINT GetSize();
  UINT GetSynchMessage();

 private:
  UINT ReadFromFile();
  bool WriteToFile();

  void DefineColorChangeState();

  void SendSynchMessage(UINT x, UINT y);

  RESOLUTION res;
  FIELD field;
  COLOR bgColor, linesColor;
  COLOR_CHANGE_STATE linesColorChange;
  HWND hwnd;
  UINT wmSynch;
};

GAME game;

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
    case WM_DESTROY: {
      PostQuitMessage(0);
      return 0;
    }
    case WM_SIZE: {
      game.Resize();
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
              DestroyWindow(hwnd);
            }
            return 0;
          }
          case VK_RETURN: {
            game.ChangeBgColor();
            return 0;
          }
          case VK_ESCAPE: {
            DestroyWindow(hwnd);
            return 0;
          } 
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      RESOLUTION coords = game.GetRes();
      coords.width = GET_X_LPARAM(lParam) / (coords.width / game.GetSize());
      coords.height = GET_Y_LPARAM(lParam) / (coords.height / game.GetSize());
      game.TryMakeTurn(coords.width, coords.height, NOUGHTS);
      return 0;
    }
    case WM_RBUTTONUP: {
      RESOLUTION coords = game.GetRes();
      coords.width = GET_X_LPARAM(lParam) / (coords.width / game.GetSize());
      coords.height = GET_Y_LPARAM(lParam) / (coords.height / game.GetSize());
      game.TryMakeTurn(coords.width, coords.height, CROSSES);
      return 0;
    }
    case WM_PAINT: {
      game.Render();
      return 0;
    }
    case WM_MOUSEWHEEL: {
      if ((int16_t)HIWORD(wParam) > 0) {
        game.ChangeLinesColorUp();
      } else {
        game.ChangeLinesColorDown();
      }
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

// ---------------------------------------------------------- main start -------------------------------------------------------

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

  while ((bMessageOk = GetMessage(&message, NULL, 0, 0)) != 0) {
    if (bMessageOk == -1) {
      puts(
          "Suddenly, GetMessage failed! You can call GetLastError() to see "
          "what happend");
      break;
    }
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  if (!game.Close()) {
    return 1;
  }
  UnregisterClass(szWinClass, hThisInstance);

  return 0;
}

// ---------------------------------------------------------- main end --------------------------------------------------------

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
  if (((red * 299 + green * 587 + blue * 114) / 1000) >
      128) {
    return COLORREF(RGB(0, 0, 0));
  }
  return COLORREF(RGB(255, 255, 255));
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
  size = s;
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

UINT8 FIELD::GetCellValue(UINT x, UINT y) { return pBuf[x * size + y]; }

UINT8 FIELD::CheckGameField(UINT x, UINT y) {
  UINT8 value = pBuf[x * size + y];
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
  for (UINT i = 0; i < size; i++) {
    for (UINT j = 0; j < size; j++) {
      if (pBuf[i * size + j] == 0) {
        return 0;
      }
    }
  }
  return 3;
}

GAME_TURN FIELD::GetTurn() {
  return (pBuf[size * size] & 1) ? CROSSES : NOUGHTS;
}

UINT FIELD::GetSize() { return size; }

GAME::~GAME() {
}

bool GAME::Create(int argc, char** argv, HINSTANCE hThisInstance) {
  if (!(wmSynch = RegisterWindowMessage((LPCWSTR) "WM_TTTSYNCH"))) return 0;

  UINT size = ReadFromFile();
  bool isFirst = (!field.TryOpenFileMapping() && (argc > 1));
  if (isFirst) {
    size = atoi(argv[1]);
  }
  if (!field.Create(size)) {
    return 0;
  }
  if (isFirst) {
    if (!WriteToFile()) {
      return 0;
    }
  }

  hwnd =
      CreateWindow(szWinClass,          /* Classname */
                   szWinName,           /* Title Text */
                   WS_OVERLAPPEDWINDOW, /* default window */
                   CW_USEDEFAULT,       /* Windows decides the position */
                   CW_USEDEFAULT, /* where the window ends up on the screen */
                   res.width,  /* The programs width */
                   res.height,     /* and height in pixels */
                   HWND_DESKTOP,  /* The window is a child-window to desktop */
                   NULL,          /* No menu */
                   hThisInstance, /* Program Instance handler */
                   NULL           /* No Window Creation data */
  );

  HBRUSH hBrush = CreateSolidBrush(bgColor.GetColorref());
  hBrush = (HBRUSH)(DWORD_PTR)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                                              (LONG)hBrush);
  DeleteObject(hBrush);
  DefineColorChangeState();
  InvalidateRect(hwnd, NULL, TRUE);

  return 1;
}

bool GAME::Close() { 
  if (!WriteToFile()) {
    return 0;
  }
  DestroyWindow(hwnd);
  return 1;
}

void GAME::Show() { ShowWindow(hwnd, SW_SHOW); }

void GAME::Resize() {
  RECT rect;
  GetClientRect(hwnd, &rect);
  res.width = rect.right;
  res.height = rect.bottom;
  InvalidateRect(hwnd, NULL, TRUE);
}

void GAME::ChangeBgColor() {
  bgColor.SetRGB((uint8_t)(rand() % 256), (uint8_t)(rand() % 256),
                 (uint8_t)(rand() % 256));
  HBRUSH hBrush = CreateSolidBrush(bgColor.GetColorref());
  hBrush = (HBRUSH)(DWORD_PTR)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                                              (LONG)hBrush);
  DeleteObject(hBrush);
  InvalidateRect(hwnd, NULL, TRUE);
}

bool GAME::TryMakeTurn(UINT x, UINT y, GAME_TURN turn) { 
  if (turn == field.GetTurn()) {
    field.SetCellValue(x, y, turn);
    InvalidateRect(hwnd, NULL, TRUE);
    SendSynchMessage(x, y);
    switch (field.CheckGameField(x, y)) {
      case NOUGHTS: {
        MessageBox(hwnd, szNPWinnerMessage, L"Game over", MB_OK);
        Close();
        break;
      }
      case CROSSES: {
        MessageBox(hwnd, szCPWinnerMessage, L"Game over", MB_OK);
        Close();
        break;
      }
      case 3: {
        MessageBox(hwnd, L"Draw!", L"Game over", MB_OK);
        Close();
        break;
      }
    }
    return true;
  }
  return false;
}

void GAME::ProccessSynchMessage(WPARAM wParam, LPARAM lParam) {
  InvalidateRect(hwnd, NULL, TRUE);
  switch (field.CheckGameField(wParam, lParam)) {
    case NOUGHTS: {
      MessageBox(hwnd, szNPWinnerMessage, L"Game over", MB_OK);
      Close();
      break;
    }
    case CROSSES: {
      MessageBox(hwnd, szCPWinnerMessage, L"Game over", MB_OK);
      Close();
      break;
    }
    case 3: {
      MessageBox(hwnd, L"Draw!", L"Game over", MB_OK);
      Close();
      break;
    }
  }
}

void GAME::Render() {
  PAINTSTRUCT strPaint;
  HDC hdc;
  hdc = BeginPaint(hwnd, &strPaint);
  if (!hdc) {
    puts("BeginPaint error!");
    return;
  }

  HPEN hPen = CreatePen(PS_SOLID, NULL, linesColor.GetColorref());
  HPEN hDefaultPen = (HPEN)SelectObject(hdc, hPen);

  UINT szOffsetX = res.width / field.GetSize();
  UINT szOffsetY = res.height / field.GetSize();
  for (int i = 1; i < field.GetSize(); i++) {
    MoveToEx(hdc, szOffsetX * i, 0, NULL);
    LineTo(hdc, szOffsetX * i, res.height);
    MoveToEx(hdc, 0, szOffsetY * i, NULL);
    LineTo(hdc, res.width, szOffsetY * i);
  }

  hPen = (HPEN)SelectObject(hdc, hDefaultPen);
  DeleteObject(hPen);

  hPen = CreatePen(PS_SOLID, NULL, bgColor.GetContrast());
  hDefaultPen = (HPEN)SelectObject(hdc, hPen);
  HBRUSH hDefaultBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor.GetColorref()));

  UINT szEllipseOffsetX = szOffsetX / 10;
  UINT szEllipseOffsetY = szOffsetY / 10;
  for (UINT i = 0; i < field.GetSize(); i++) {
    for (UINT j = 0; j < field.GetSize(); j++) {
      switch (field.GetCellValue(i, j)) {
        case NOUGHTS: {
          Ellipse(hdc, szEllipseOffsetX + szOffsetX * i,
                  szEllipseOffsetY + szOffsetY * j,
                  szOffsetX * (i + 1) - szEllipseOffsetX,
                  szOffsetY * (j + 1) - szEllipseOffsetY);
          break;
        }
        case CROSSES: {
          MoveToEx(hdc, szEllipseOffsetX + szOffsetX * i,
                   szEllipseOffsetY + szOffsetY * j, NULL);
          LineTo(hdc, szOffsetX * (i + 1) - szEllipseOffsetX,
                 szOffsetY * (j + 1) - szEllipseOffsetY);
          MoveToEx(hdc, szOffsetX * (i + 1) - szEllipseOffsetX,
                   szEllipseOffsetY + szOffsetY * j, NULL);
          LineTo(hdc, szEllipseOffsetX + szOffsetX * i,
                 szOffsetY * (j + 1) - szEllipseOffsetY);
          break;
        }
      }
    }
  }

  hDefaultBrush = (HBRUSH)SelectObject(hdc, hDefaultBrush);
  DeleteObject(hDefaultBrush);
  hPen = (HPEN)SelectObject(hdc, hDefaultPen);
  DeleteObject(hPen);
  EndPaint(hwnd, &strPaint);
}

void GAME::ChangeLinesColorUp() {
  switch (linesColorChange) {
    case GREEN_UP:
      linesColor.green += COLOR_CHANGE_OFFSET;
      if ((linesColor.green) == 255) {
        linesColorChange = RED_DOWN;
      }
      break;
    case RED_DOWN:
      linesColor.red -= COLOR_CHANGE_OFFSET;
      if ((linesColor.red) == 0) {
        linesColorChange = BLUE_UP;
      }
      break;
    case BLUE_UP:
      linesColor.blue += COLOR_CHANGE_OFFSET;
      if ((linesColor.blue) == 255) {
        linesColorChange = GREEN_DOWN;
      }
      break;
    case GREEN_DOWN:
      linesColor.green -= COLOR_CHANGE_OFFSET;
      if ((linesColor.green) == 0) {
        linesColorChange = RED_UP;
      }
      break;
    case RED_UP:
      linesColor.red += COLOR_CHANGE_OFFSET;
      if ((linesColor.red) == 255) {
        linesColorChange = BLUE_DOWN;
      }
      break;
    case BLUE_DOWN:
      linesColor.blue -= COLOR_CHANGE_OFFSET;
      if ((linesColor.blue) == 0) {
        linesColorChange = GREEN_UP;
      }
      break;
  }
  InvalidateRect(hwnd, NULL, TRUE);
}

void GAME::ChangeLinesColorDown() {
  switch (linesColorChange) {
    case GREEN_UP:
      if (linesColor.green > 0) {
        linesColor.green -= COLOR_CHANGE_OFFSET;
      } else {
        linesColor.blue += COLOR_CHANGE_OFFSET;
        linesColorChange = BLUE_DOWN;
      }
      break;
    case RED_DOWN:
      if (linesColor.red < 255) {
        linesColor.red += COLOR_CHANGE_OFFSET;
      } else {
        linesColor.green -= COLOR_CHANGE_OFFSET;
        linesColorChange = GREEN_UP;
      }
      break;
    case BLUE_UP:
      if (linesColor.blue > 0) {
        linesColor.blue -= COLOR_CHANGE_OFFSET;
      } else {
        linesColor.red += COLOR_CHANGE_OFFSET;
        linesColorChange = RED_DOWN;
      }
      break;
    case GREEN_DOWN:
      if (linesColor.green < 255) {
        linesColor.green += COLOR_CHANGE_OFFSET;
      } else {
        linesColor.blue -= COLOR_CHANGE_OFFSET;
        linesColorChange = BLUE_UP;
      }
      break;
    case RED_UP:
      if (linesColor.red > 0) {
        linesColor.red -= COLOR_CHANGE_OFFSET;
      } else {
        linesColor.green += COLOR_CHANGE_OFFSET;
        linesColorChange = GREEN_DOWN;
      }
      break;
    case BLUE_DOWN:
      if (linesColor.blue < 255) {
        linesColor.blue += COLOR_CHANGE_OFFSET;
      } else {
        linesColor.red -= COLOR_CHANGE_OFFSET;
        linesColorChange = RED_UP;
      }
      break;
  }
  InvalidateRect(hwnd, NULL, TRUE);
}

RESOLUTION GAME::GetRes() { return res; }

UINT GAME::GetSize() { return field.GetSize(); }

UINT GAME::GetSynchMessage() { return wmSynch; }

UINT GAME::ReadFromFile() {
  UINT size = DEFAULT_SIZE;
  HANDLE hFile = CreateFile(szCfgName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    return size;
  }

  UINT8* buffer = new UINT8[BYTES_TO_READ];
  DWORD dwBytesRead;
  if (!ReadFile(hFile, buffer, BYTES_TO_READ, &dwBytesRead, NULL)) {
    puts("ReadFile error!");
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
    return 1;
  }

  SetFilePointer(hFile, NULL, NULL, FILE_BEGIN);
  SetEndOfFile(hFile);
  UINT8* buffer = new UINT8[BYTES_TO_READ];
  DWORD dwBytesToWrite = BYTES_TO_READ;

  UIntToUChar(field.GetSize(), buffer);
  UIntToUChar(res.width, buffer + 4);
  UIntToUChar(res.height, buffer + 8);
  bgColor.ToUChar(buffer + 12);
  linesColor.ToUChar(buffer + 15);

  DWORD dwBytesWritten;
  WriteFile(hFile, buffer, dwBytesToWrite, &dwBytesWritten, NULL);
  if (dwBytesToWrite != dwBytesWritten) {
    puts("WriteFile error!");
    return false;
  }
  delete[] buffer;
  CloseHandle(hFile);

  return true; 
}

void GAME::DefineColorChangeState() {
  if (linesColor.red == 255 && linesColor.green < 255) {
    linesColorChange = GREEN_UP;
  } else if (linesColor.red == 255 && linesColor.blue > 0) {
    linesColorChange = BLUE_DOWN;
  } else if (linesColor.green == 255 && linesColor.blue < 255) {
    linesColorChange = BLUE_UP;
  } else if (linesColor.green == 255 && linesColor.red > 0) {
    linesColorChange = RED_DOWN;
  } else if (linesColor.blue == 255 && linesColor.red < 255) {
    linesColorChange = RED_UP;
  } else {
    linesColorChange = GREEN_DOWN;
  }
}

void GAME::SendSynchMessage(UINT x, UINT y) {
  SendMessage(HWND_BROADCAST, wmSynch, x, y);
}
