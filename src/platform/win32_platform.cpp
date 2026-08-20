#include "renderer/vk_renderer.cpp"

static bool running = true;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg)
    {
    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

bool platform_create_window(HWND* window) {
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

    *window = CreateWindowExA(
        WS_EX_APPWINDOW, 
        "vulkan_engine_class", 
        "This is a window ;)",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED, 
        100, 100, 1600, 720, 
        0, 0, instance, 0
    );

    std::cout << "Created HWND = " << *window << std::endl;
    std::cout << "IsWindow = " << IsWindow(*window) << std::endl;

    if (*window == 0) {
        MessageBoxA(0, "Failed creating window", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(*window, SW_SHOW);

    return true;
}

void platform_update_window(HWND window) {
    MSG msg;

    while(PeekMessageA(&msg, window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int main() {
    VkContext vkcontext = {};

    HWND window = 0;
    std::cout << "Init HWND = " << window << std::endl;
    std::cout << "IsWindow = " << IsWindow(window) << std::endl;
    if (!platform_create_window(&window)) {
        return -1;
    }

    if (!Vk_Init(&vkcontext, window)) {
        return -1;
    }

    while (running) 
    {
        platform_update_window(window);
    }
    
    return 0;
}