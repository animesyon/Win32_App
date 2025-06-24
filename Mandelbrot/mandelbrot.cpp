// Mandelbrot
// This program use multiple processors (OpenMP).

#include "framework.h"
#include "mandelbrot.h"
#include "stack.h"

const int MAX_LOADSTRING = 100;

const int FRACTAL_WIDTH = 1280;
const int FRACTAL_HEIGHT = 720;

// 5 bits per color, 2^5 = 32, [binary] 1 00000 00000 00000 = 32768
//                                        R     G     B
const int MAX_ITERATION = 32768;
const int MAX_VALUE_1 = 32;
const double MAX_VALUE_2 = (double)MAX_VALUE_1 - 1.0;

typedef struct {
    HWND hWnd;
    DWORD id;
    int width, height, max_iteration;
    double cx, cy, ox, oy;
    unsigned char* buffer;
}THREAD_DATA;

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

HANDLE thread;
THREAD_DATA data;
BITMAPINFOHEADER bmih;
CStack stack;
RECT rect;
LONG x, y;
bool busy, dragging, redo;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD WINAPI Function(LPVOID lpParam);

void GetUniqueName(wchar_t* filename, DWORD size);
void OutputDebugDuration(LONGLONG duration);
void SetValue(THREAD_DATA* data, RECT* rect);
void Fractals(int i1, int i2, THREAD_DATA* data);

void OnLButtonDown(HWND hWnd, WORD Key, int x, int y);
void OnLButtonUp(HWND hWnd, WORD Key, int x, int y);
void OnMouseMove(HWND hWnd, WORD Key, int x, int y);

void OnSize(HWND hWnd, int width, int height);
void OnPaint(HWND hWnd);
void OnCreate(HWND hWnd);
void OnDestroy(HWND hWnd);

void OnFileOpen(HWND hWnd);
void OnFileSaveAs(HWND hWnd);
void OnFileExit(HWND hWnd);

void OnViewFractal(HWND hWnd);
void OnViewGoBack(HWND hWnd);

// main program
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MANDELBROT, szWindowClass, MAX_LOADSTRING);

    // registers the window class
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MANDELBROT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MANDELBROT);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&wcex);

    hInst = hInstance;

    // creates window
    int X, Y, nWidth, nHeight, Cx, Cy;

    Cx = FRACTAL_WIDTH;
    Cy = FRACTAL_HEIGHT;

    nWidth = Cx + 16;
    nHeight = Cy + 59;

    X = (GetSystemMetrics(SM_CXSCREEN) - nWidth) / 2;
    Y = (GetSystemMetrics(SM_CYSCREEN) - nHeight) / 3;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        X, Y,
        nWidth, nHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MANDELBROT));

    // message loop
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

// window callback function
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:

        switch (LOWORD(wParam))
        {
        case IDM_OPEN:              OnFileOpen(hWnd);           break;
        case IDM_SAVE_AS:           OnFileSaveAs(hWnd);         break;
        case IDM_EXIT:              OnFileExit(hWnd);           break;
        case IDM_FRACTAL:           OnViewFractal(hWnd);        break;
        case IDM_GO_BACK:           OnViewGoBack(hWnd);         break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_LBUTTONDOWN:    OnLButtonDown(hWnd, WORD(wParam), LOWORD(lParam), HIWORD(lParam));   break;
    case WM_LBUTTONUP:      OnLButtonUp(hWnd, WORD(wParam), LOWORD(lParam), HIWORD(lParam));     break;
    case WM_MOUSEMOVE:      OnMouseMove(hWnd, WORD(wParam), LOWORD(lParam), HIWORD(lParam));     break;
    case WM_SIZE:           OnSize(hWnd, LOWORD(lParam), HIWORD(lParam));                    break;
    case WM_PAINT:          OnPaint(hWnd);									                            break;
    case WM_CREATE:         OnCreate(hWnd);								                                break;
    case WM_DESTROY:        OnDestroy(hWnd);								                            break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

DWORD WINAPI Function(LPVOID lpParam)
{
    THREAD_DATA* data = (THREAD_DATA*)lpParam;
    LARGE_INTEGER freq, t1, t2;
    LONGLONG duration;
    int *d, i, *i1, *i2, k, n, quo, rem;
    wchar_t str[100];

    swprintf_s(str, 100, L"THE THREAD 0x%X HAS STARTED....................\n", data->id);
    OutputDebugString(str);

    HMENU hMenu = GetMenu(data->hWnd);

    EnableMenuItem(hMenu, IDM_FRACTAL, MF_BYCOMMAND | MF_DISABLED);
    EnableMenuItem(hMenu, IDM_GO_BACK, MF_BYCOMMAND | MF_DISABLED);

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);

    n = omp_get_num_procs();
    quo = data->height / n;
    rem = data->height % n;

    d = new int[n];
    i1 = new int[n];
    i2 = new int[n];

    for (i = 0; i < n; i++)
        d[i] = quo;

    if (rem > 0)
        for (i = 0; i < rem; i++)
            ++d[i];

    k = 0;

    for (i = 0; i < n; i++) {

        i1[i] = k;
        i2[i] = k + d[i];

        k += d[i];
    }

    delete[] d;

    #pragma omp parallel for default(none) shared(i1, i2, data)

    for (i = 0; i < n; i++) {
        Fractals(i1[i], i2[i], data);
    }

    delete[] i1;
    delete[] i2;

    QueryPerformanceCounter(&t2);
    duration = ((t2.QuadPart - t1.QuadPart) * 1000LL) / freq.QuadPart;

    OutputDebugDuration(duration);

    InvalidateRect(data->hWnd, NULL, TRUE);

    if (stack.IsEmpty())
        EnableMenuItem(hMenu, IDM_GO_BACK, MF_BYCOMMAND | MF_DISABLED);
    else
        EnableMenuItem(hMenu, IDM_GO_BACK, MF_BYCOMMAND | MF_ENABLED);

    swprintf_s(str, 100, L"THE THREAD 0x%X HAS EXITED.....................\n", data->id);
    OutputDebugString(str);

    return 0;
}

void GetUniqueName(wchar_t* filename, DWORD size)
{
    time_t ltime;
    struct tm a;

    time(&ltime);
    _localtime64_s(&a, &ltime);

    swprintf_s(filename, size, L"%d%02d%02d%02d%02d%02d.mdl", a.tm_year + 1900, a.tm_mon + 1, a.tm_mday, a.tm_hour, a.tm_min, a.tm_sec);
}


void OutputDebugDuration(LONGLONG duration)
{
    LONGLONG tm, quo, ms, ss, mm, hh;

    tm = duration;

    quo = tm / 1000;
    ms = tm % 1000;

    tm = quo;

    quo = tm / 60;
    ss = tm % 60;

    tm = quo;

    hh = tm / 60;
    mm = tm % 60;

    wchar_t str[100];
    swprintf_s(str, 100, L"%lld:%02lld:%02lld:%03lld .....................................\n", hh, mm, ss, ms);
    OutputDebugString(str);
}

void SetValue(THREAD_DATA* data, RECT* rect)
{
    double sx, sy, cx, cy, w, h;

    sx = (double)(rect->right - rect->left);
    sy = (double)(rect->bottom - rect->top);

    w = (sx / (double)data->width) * data->cx;
    h = (sy / (double)data->height) * data->cy;

    cx = w;
    cy = ((double)data->height / (double)data->width) * cx;

    if (cy < h) {

        cy = h;
        cx = ((double)data->width / (double)data->height) * cy;
    }

    data->ox = ((double)rect->left / (double)(data->width - 1)) * data->cx + data->ox;
    data->oy = ((data->height - (double)rect->bottom) / (double)(data->height - 1)) * data->cy + data->oy;

    data->cx = cx;
    data->cy = cy;
}

// i1 - the horizontal pixel position
// i2 - the vertical pixel position
void Fractals(int i1, int i2, THREAD_DATA* data)
{
    int h, i, j, k, l, quo;
    double ax, ay, bx, by, rx, ry;
    unsigned char r, g, b;

    for (i = i1; i < i2; i++) {

        for (j = 0; j < data->width; j++) {

            bx = ((double)j / (double)(data->width - 1)) * data->cx + data->ox;
            by = ((double)i / (double)(data->height - 1)) * data->cy + data->oy;

            ax = 0.0;
            ay = 0.0;

            h = 0;

            for (k = 0; k < data->max_iteration; k++) {

                // f(a) = a * a + b
                // a and b are complex numbers

                rx = ax * ax - ay * ay + bx;
                ry = 2.0 * ax * ay + by;

                if ((rx * rx + ry * ry) > 16.0) {
                    h = k;
                    break;
                }

                ax = rx;
                ay = ry;
            }

            // h - iteration count
            // r, g, b - color eqivqlent of iteratiion count
            quo = h / 32;
            b = (unsigned char)(((double)(h % MAX_VALUE_1) / MAX_VALUE_2) * 255.0);
            g = (unsigned char)(((double)(quo % MAX_VALUE_1) / MAX_VALUE_2) * 255.0);
            r = (unsigned char)(((double)(quo / MAX_VALUE_1) / MAX_VALUE_2) * 255.0);

            l = i * data->width + j;
            l *= 3;

            data->buffer[l] = b;
            data->buffer[l + 1] = g;
            data->buffer[l + 2] = r;
        }
    }
}

void OnLButtonDown(HWND hWnd, WORD Key, int x, int y)
{
    if (busy) return;

    rect.left = rect.top = 0;
    rect.right = rect.bottom = 0;

    InvalidateRect(hWnd, NULL, TRUE);

    dragging = true;

    ::x = x;
    ::y = y;

    HMENU hMenu = GetMenu(hWnd);
    EnableMenuItem(hMenu, IDM_FRACTAL, MF_BYCOMMAND | MF_DISABLED);
}

void OnLButtonUp(HWND hWnd, WORD Key, int x, int y)
{
    if (busy) return;

    dragging = false;
}

void OnMouseMove(HWND hWnd, WORD Key, int x, int y)
{
    if (busy) return;

    RECT rct;

    if (dragging) {

        HMENU hMenu = GetMenu(hWnd);
        EnableMenuItem(hMenu, IDM_FRACTAL, MF_BYCOMMAND | MF_ENABLED);

        if (::x > x) {
            rect.left = x;
            rect.right = ::x;
        }
        else {
            rect.left = ::x;
            rect.right = x;
        }

        if (::y > y) {
            rect.top = y;
            rect.bottom = ::y;
        }
        else {
            rect.top = ::y;
            rect.bottom = y;
        }

        SetRect(&rct, rect.left - 10, rect.top - 10, rect.right + 10, rect.bottom + 10);
        InvalidateRect(hWnd, &rct, TRUE);
    }
}

void OnSize(HWND hWnd, int width, int height)
{
    
}

void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hDC, hDC1;
    HBITMAP hBitmap;
    HPEN hPen;

    hDC = BeginPaint(hWnd, &ps);

    hBitmap = CreateCompatibleBitmap(hDC, data.width, data.height);
    SetDIBits(hDC, hBitmap, 0, (UINT)data.height, data.buffer, (BITMAPINFO*)&bmih, DIB_RGB_COLORS);

    hDC1 = CreateCompatibleDC(NULL);
    SelectObject(hDC1, hBitmap);

    BitBlt(hDC, 0, 0, data.width, data.height, hDC1, 0, 0, SRCCOPY);

    if (dragging) {

        hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 0));
        SelectObject(hDC, hPen);

        MoveToEx(hDC, rect.left, rect.top, NULL);
        LineTo(hDC, rect.right, rect.top);
        LineTo(hDC, rect.right, rect.bottom);
        LineTo(hDC, rect.left, rect.bottom);
        LineTo(hDC, rect.left, rect.top);

        DeleteObject(hPen);
    }

    DeleteObject(hBitmap);

    DeleteObject(hDC);
    DeleteObject(hDC1);

    EndPaint(hWnd, &ps);
}

void OnCreate(HWND hWnd)
{
    HMENU hMenu = GetMenu(hWnd);
    EnableMenuItem(hMenu, IDM_FRACTAL, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(hMenu, IDM_GO_BACK, MF_BYCOMMAND | MF_DISABLED);

    thread = NULL;

    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = FRACTAL_WIDTH;
    bmih.biHeight = FRACTAL_HEIGHT;
    bmih.biPlanes = 1;
    bmih.biBitCount = 24;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    data.hWnd = hWnd;

    data.max_iteration = MAX_ITERATION;

    data.width = FRACTAL_WIDTH;
    data.height = FRACTAL_HEIGHT;

    data.cx = 4.5;
    data.cy = data.cx * ((double)data.height / (double)data.width);

    data.ox = -data.cx * 0.65;
    data.oy = -data.cy * 0.5;

    int i, n;

    n = 3 * data.width * data.height;

    data.buffer = NULL;
    data.buffer = new unsigned char[n];

    for (i = 0; i < n; i++) {
        data.buffer[i] = 0;
    }

    busy = dragging = redo = false;

    rect.left = rect.top = 0;
    rect.right = rect.bottom = 0;
}

void OnDestroy(HWND hWnd)
{
    if (thread != NULL) CloseHandle(thread);
    if (data.buffer != NULL) delete[] data.buffer;

    PostQuitMessage(0);
}

void OnFileOpen(HWND hWnd)
{
    HRESULT hr;
    IFileOpenDialog* pFileOpen;
    FILEOPENDIALOGOPTIONS options;
    COMDLG_FILTERSPEC fs[2];
    IShellItem* pItem;
    PWSTR pszFile;
    wchar_t filename[MAX_PATH];
    wchar_t str[MAX_PATH], name[2][20], spec[2][6];
    bool cancel;

    cancel = true;

    // Initialize COM.
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (SUCCEEDED(hr)) {

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr)) {

            // Filter.
            wcscpy_s(name[0], 20, L"Mandelbrot");
            wcscpy_s(spec[0], 6, L"*.mdl");

            fs[0].pszName = name[0];
            fs[0].pszSpec = spec[0];

            wcscpy_s(name[1], 20, L"All Files");
            wcscpy_s(spec[1], 6, L"*.*");

            fs[1].pszName = name[1];
            fs[1].pszSpec = spec[1];

            pFileOpen->SetFileTypes(2, fs);
            hr = pFileOpen->GetOptions(&options);

            if (SUCCEEDED(hr)) {
                options |= FOS_STRICTFILETYPES;
                pFileOpen->SetOptions(options);
            }

            // Show the Open dialog box.
            hr = pFileOpen->Show(hWnd);

            if (SUCCEEDED(hr)) {

                // Get the result object.
                hr = pFileOpen->GetResult(&pItem);

                if (SUCCEEDED(hr)) {

                    // Gets the filename that the user made in the dialog.
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFile);

                    // Copy the file name.
                    if (SUCCEEDED(hr)) {

                        cancel = false;

                        wcscpy_s(filename, MAX_PATH, pszFile);

                        // Release memory.
                        CoTaskMemFree(pszFile);
                    }

                    // Release result object.
                    pItem->Release();
                }
            }

            // Release FileOpenDialog object.
            pFileOpen->Release();
        }

        // Release COM.
        CoUninitialize();
    }

    if (cancel) return;

    if (!stack.Open(filename)) return;

    swprintf_s(str, MAX_PATH, L"%s - %s", szTitle, filename);
    SetWindowText(hWnd, str);

    OnViewGoBack(hWnd);

    redo = true;
}

void OnFileSaveAs(HWND hWnd)
{
    HRESULT hr;
    IFileSaveDialog* pFileSave;
    FILEOPENDIALOGOPTIONS options;
    COMDLG_FILTERSPEC fs[2];
    IShellItem* pItem;
    PWSTR pszFile;
    UINT filetype;
    wchar_t str[MAX_PATH], filename[MAX_PATH];
    wchar_t name[2][20], spec[2][6];
    bool cancel;

    cancel = true;

    // Initialize COM.
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (SUCCEEDED(hr)) {

        // Create the FileSaveDialog object.
        hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));

        if (SUCCEEDED(hr)) {

            // Set default filename extension
            pFileSave->SetDefaultExtension(L"mdl");

            // Set default filename.
            GetUniqueName(filename, MAX_PATH);
            pFileSave->SetFileName(filename);

            // Filter
            wcscpy_s(name[0], 20, L"Mandelbrot");
            wcscpy_s(spec[0], 6, L"*.mdl");

            fs[0].pszName = name[0];
            fs[0].pszSpec = spec[0];

            wcscpy_s(name[1], 20, L"All Files");
            wcscpy_s(spec[1], 6, L"*.*");

            fs[1].pszName = name[1];
            fs[1].pszSpec = spec[1];

            pFileSave->SetFileTypes(2, fs);
            hr = pFileSave->GetOptions(&options);

            if (SUCCEEDED(hr)) {
                options |= FOS_STRICTFILETYPES;
                pFileSave->SetOptions(options);
            }

            // Show the Save dialog box.
            hr = pFileSave->Show(hWnd);

            if (SUCCEEDED(hr)) {

                // Get the result object.
                hr = pFileSave->GetResult(&pItem);

                if (SUCCEEDED(hr)) {

                    // Gets the filename that the user made in the dialog.
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFile);

                    // Copy the file name.
                    if (SUCCEEDED(hr)) {

                        cancel = false;

                        wcscpy_s(filename, MAX_PATH, pszFile);

                        pFileSave->GetFileTypeIndex(&filetype);

                        // Release memory.
                        CoTaskMemFree(pszFile);
                    }

                    // Release result object.
                    pItem->Release();
                }
            }

            // Release FileSaveDialog object.
            pFileSave->Release();
        }

        // Release COM.
        CoUninitialize();
    }

    if (cancel) return;

    OutputDebugString(filename);
    OutputDebugString(L"\n");

    if (!stack.Save(filename, data.cx, data.cy, data.ox, data.oy)) return;

    swprintf_s(str, MAX_PATH, L"%s - %s", szTitle, filename);
    SetWindowText(hWnd, str);
}

void OnFileExit(HWND hWnd)
{
    DestroyWindow(hWnd);
}

void OnViewFractal(HWND hWnd)
{
    if (redo) {
        stack.Push(data.cx, data.cy, data.ox, data.oy);
        SetValue(&data, &rect);
    }

    redo = true;

    HMENU hMenu = GetMenu(hWnd);

    if (stack.IsEmpty())
        EnableMenuItem(hMenu, IDM_GO_BACK, MF_BYCOMMAND | MF_DISABLED);
    else
        EnableMenuItem(hMenu, IDM_GO_BACK, MF_BYCOMMAND | MF_ENABLED);

    if (thread != NULL) CloseHandle(thread);
    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Function, &data, 0, &data.id);
}

void OnViewGoBack(HWND hWnd)
{
    stack.Pop(&data.cx, &data.cy, &data.ox, &data.oy);

    if (thread != NULL) CloseHandle(thread);
    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Function, &data, 0, &data.id);
}
