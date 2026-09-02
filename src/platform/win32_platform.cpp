#include "renderer/vk_renderer.cpp"
#include "defines.h"

/*
 * Global Variables
 */
static HWND window = 0;
static bool running = true;

/*
 * Windows Specific Functions
 */

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg)
    {
    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

bool platform_create_window() {
    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASS windowClass = {};
    windowClass.lpfnWndProc = platform_window_callback;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "vulkan_engine_class";
    windowClass.hCursor = LoadCursor(NULL, IDC_CROSS);

    if(!RegisterClassA(&windowClass)) {
        MessageBoxA(0, "Failed registering window class", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    window = CreateWindowExA(
        WS_EX_APPWINDOW, 
        "vulkan_engine_class", 
        "This is a window ;)",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED, 
        100, 100, 1500, 720, 
        0, 0, instance, 0
    );

    std::cout << "Created HWND = " << window << std::endl;
    std::cout << "IsWindow = " << IsWindow(window) << std::endl;

    if (window == 0) {
        MessageBoxA(0, "Failed creating window", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(window, SW_SHOW);

    return true;
}

void platform_update_window(HWND window) {
    MSG msg;

    while(PeekMessageA(&msg, window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

/*
 * Implemented Functions
 */
void platform_get_window_size(uint32_t* width, uint32_t* height) {
    std::cout << "Getting window size..." << '\n';
    RECT rect;
    GetClientRect(window, &rect);

    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

// TODO: add asserts to ERROR
char* platform_read_file(char* path, uint64_t* length) {
    std::cout << "Reading File at " << path << '\n';
    char* result = 0;
    
    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size)) {
            *length = (uint64_t)size.QuadPart;
            result = new char[size.QuadPart];

            DWORD bytesRead;
            if(ReadFile(file, result, *length, &bytesRead, 0)) {
                std::cout << "Success" << '\n';
                // Success
            } else {
                std::cout << "ERROR: Failed to read file at path: " << path << std::endl;
            }
        } else {
            std::cout << "ERROR: Failed to get size of file at path: " << path << std::endl;
        }

        CloseHandle(file);
    } else {
        std::cout << "ERROR: Failed to read file at path: " << path << std::endl;
    }

    return result;
}

int main() {
    VkContext vkcontext = {};

    std::cout << "Init HWND = " << window << std::endl;
    std::cout << "IsWindow = " << IsWindow(window) << std::endl;
    if (!platform_create_window()) {
        return -1;
    }

    if (!Vk_Init(&vkcontext, window)) {
        return -1;
    }

    while (running) 
    {
        platform_update_window(window);
        if (!vk_render(&vkcontext)) {
            return -1;
        }
    }
    
    return 0;
}