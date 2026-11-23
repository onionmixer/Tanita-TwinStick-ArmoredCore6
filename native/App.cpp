#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <shellapi.h>
#include <ViGEm/Client.h>
#include "Mapper.h"

#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int kBackend = 100;
constexpr int kToggle = 101;
constexpr int kDriverLink = 102;
constexpr int kClientWidth = 520;
constexpr int kClientHeight = 364;
constexpr COLORREF kBackground = RGB(245, 247, 250);
constexpr COLORREF kText = RGB(23, 35, 52);
constexpr COLORREF kMuted = RGB(74, 89, 108);
constexpr wchar_t kViGEmDownload[] = L"https://github.com/nefarius/ViGEmBus/releases/latest";
enum class DriverStatus { Unknown, Missing, Stopped, Running };

class App {
public:
    HWND window{}, backend{}, toggle{}, driverLink{};
    void Initialize() {
        titleFont_ = MakeFont(24, FW_SEMIBOLD, L"Segoe UI");
        bodyFont_ = MakeFont(15, FW_NORMAL, L"Segoe UI");
        dataFont_ = MakeFont(14, FW_NORMAL, L"Consolas");
        backend = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                               24, 85, 320, 150, window, Menu(kBackend), nullptr, nullptr);
        SendMessageW(backend, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), FALSE);
        SendMessageW(backend, CB_SETITEMHEIGHT, WPARAM(-1), 26);
        SendMessageW(backend, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"ViGEmBus (Xbox 360)"));
        SendMessageW(backend, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Custom HID driver"));
        SendMessageW(backend, CB_SETCURSEL, 0, 0);
        RECT combo{};
        GetWindowRect(backend, &combo);
        toggle = CreateWindowW(L"BUTTON", L"Start output", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                              356, 85, 140, combo.bottom - combo.top, window, Menu(kToggle), nullptr, nullptr);
        SendMessageW(toggle, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), FALSE);
        driverLink = CreateWindowW(WC_LINK, L"<a>Open official ViGEmBus downloads</a>",
                                   WS_CHILD | WS_TABSTOP | LWS_TRANSPARENT,
                                   258, 331, 238, 22, window, Menu(kDriverLink), nullptr, nullptr);
        SendMessageW(driverLink, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), FALSE);
        RefreshDriverStatus();
        RAWINPUTDEVICE devices[]{{1, 4, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window},
                                 {1, 5, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window}};
        if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
            message_ = L"Raw Input registration failed: " + std::to_wstring(GetLastError());
        } else {
            Scan();
        }
        SetTimer(window, 1, 8, nullptr);
        Render();
    }

    void Paint() {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        SetBkMode(dc, TRANSPARENT);
        Fill(dc, paint.rcPaint, kBackground);
        Text(dc, 24, 20, L"TANITA Twin Stick", titleFont_, kText);
        Text(dc, 24, 62, L"Output device", bodyFont_, kMuted);

        Fill(dc, {24, 134, 496, 208}, RGB(255, 255, 255));
        Text(dc, 38, 144, device_ ? L"USB GAMESTICK  /  1F4F:9001" : L"Waiting for TANITA VCD-18-c",
             bodyFont_, device_ ? kText : kMuted);
        Text(dc, 38, 171, renderedInput_.c_str(), dataFont_, kMuted);

        Fill(dc, {24, 218, 496, 282}, RGB(255, 255, 255));
        Text(dc, 38, 226, L"Gamepad output", bodyFont_, kText);
        Text(dc, 378, 226, mapper_.camera ? L"Camera mode" : L"Combat mode", bodyFont_, RGB(25, 92, 155));
        Text(dc, 38, 252, renderedOutput_.c_str(), dataFont_, kMuted);

        const bool error = !Active() && message_ != L"Output stopped";
        const COLORREF accent = Active() ? RGB(30, 119, 80) : error ? RGB(160, 58, 37) : kMuted;
        const COLORREF surface = Active() ? RGB(231, 246, 237) : error ? RGB(253, 237, 229) : RGB(233, 238, 245);
        Fill(dc, {24, 294, 496, 324}, surface);
        Fill(dc, {24, 294, 27, 324}, accent);
        Text(dc, 38, 299, message_.c_str(), bodyFont_, accent);
        const wchar_t* driverText = L"ViGEmBus status unavailable";
        if (driverStatus_ == DriverStatus::Missing) driverText = L"ViGEmBus not installed";
        if (driverStatus_ == DriverStatus::Stopped) driverText = L"ViGEmBus installed, not running";
        if (driverStatus_ == DriverStatus::Running) driverText = L"ViGEmBus installed and running";
        Text(dc, 24, 333, driverText, bodyFont_, kMuted);
        EndPaint(window, &paint);
    }

    void RefreshDriverStatus() {
        DriverStatus detected = DriverStatus::Unknown;
        if (SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT)) {
            if (SC_HANDLE driver = OpenServiceW(manager, L"ViGEmBus", SERVICE_QUERY_STATUS)) {
                SERVICE_STATUS status{};
                if (QueryServiceStatus(driver, &status)) {
                    detected = status.dwCurrentState == SERVICE_RUNNING ? DriverStatus::Running : DriverStatus::Stopped;
                }
                CloseServiceHandle(driver);
            } else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
                detected = DriverStatus::Missing;
            }
            CloseServiceHandle(manager);
        }
        if (driverStatus_ != detected) {
            driverStatus_ = detected;
            ShowWindow(driverLink, detected == DriverStatus::Missing ? SW_SHOW : SW_HIDE);
            const RECT driverArea{24, 329, 496, 355};
            InvalidateRect(window, &driverArea, TRUE);
        }
    }

    void OpenDriverDownload() {
        if (driverStatus_ != DriverStatus::Missing) return;
        const auto result = ShellExecuteW(window, L"open", kViGEmDownload, nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) Stop(L"Could not open the ViGEmBus download page");
    }

    void ReleaseFonts() {
        DeleteObject(titleFont_);
        DeleteObject(bodyFont_);
        DeleteObject(dataFont_);
    }

    void Scan() {
        UINT count = 0;
        if (device_ || GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1) || !count)
            return;
        std::vector<RAWINPUTDEVICELIST> devices(count);
        count = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
        if (count == UINT(-1)) return;
        for (UINT i = 0; i < count && !device_; ++i) Attach(devices[i].hDevice);
    }

    void Attach(HANDLE device) {
        if (device_) return;
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &info, &size) == UINT(-1) ||
            info.dwType != RIM_TYPEHID || info.hid.dwVendorId != 0x1f4f || info.hid.dwProductId != 0x9001 ||
            info.hid.usUsagePage != 1 || (info.hid.usUsage != 4 && info.hid.usUsage != 5)) return;

        size = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &size) == UINT(-1) || !size) return;
        preparsed_.resize(size);
        HIDP_CAPS caps{};
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, preparsed_.data(), &size) == UINT(-1) ||
            HidP_GetCaps(Descriptor(), &caps) != HIDP_STATUS_SUCCESS || caps.InputReportByteLength != 64) {
            preparsed_.clear();
            return;
        }
        device_ = device;
        Render();
    }

    void Remove(HANDLE device) {
        if (device != device_) return;
        device_ = nullptr;
        preparsed_.clear();
        ResetInput();
        Scan();
        Render();
    }

    void Read(HRAWINPUT handle) {
        UINT size = 0;
        if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == UINT(-1) ||
            size < sizeof(RAWINPUTHEADER)) return;
        raw_.resize(size);
        if (GetRawInputData(handle, RID_INPUT, raw_.data(), &size, sizeof(RAWINPUTHEADER)) != size) return;
        const auto* input = reinterpret_cast<const RAWINPUT*>(raw_.data());
        if (input->header.dwType != RIM_TYPEHID || !device_ || input->header.hDevice != device_ ||
            size < offsetof(RAWINPUT, data.hid.bRawData)) return;
        const auto& hid = input->data.hid;
        if (hid.dwSizeHid != 64 || hid.dwCount > (size - offsetof(RAWINPUT, data.hid.bRawData)) / 64) return;
        for (DWORD i = 0; i < hid.dwCount; ++i) {
            auto* report = reinterpret_cast<PCHAR>(const_cast<BYTE*>(hid.bRawData + i * 64));
            if (report[0] == 1 && Decode(report)) Tick();
        }
    }

    void Tick() {
        const auto next = mapper_.Step(physical_, Mapper::Clock::now());
        if (std::memcmp(&next, &last_, sizeof(next)) != 0) {
            last_ = next;
            if (Active() && !Write(next)) Stop(L"Output disconnected or write failed");
        }
        Render();
    }

    void Toggle() {
        if (Active()) {
            Stop(L"Output stopped");
            return;
        }
        if (SendMessageW(backend, CB_GETCURSEL, 0, 0) == 1) {
            custom_ = CreateFileW(TANITA_DEVICE_PATH, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
            if (!Active()) {
                Stop(L"Custom driver unavailable: " + std::to_wstring(GetLastError()));
                return;
            }
        } else {
            RefreshDriverStatus();
            client_ = vigem_alloc();
            if (!client_ || !VIGEM_SUCCESS(vigem_connect(client_))) {
                Stop(driverStatus_ == DriverStatus::Missing ? L"ViGEmBus is not installed" :
                     L"Could not connect to ViGEmBus");
                return;
            }
            target_ = vigem_target_x360_alloc();
            if (!target_ || !VIGEM_SUCCESS(vigem_target_add(client_, target_))) {
                Stop(L"Could not create the Xbox 360 device");
                return;
            }
            added_ = true;
        }
        if (!Write(TanitaReport{})) {
            Stop(L"Could not send a neutral output report");
            return;
        }
        mapper_ = Mapper{};
        last_ = {};
        message_ = L"Output connected";
        EnableWindow(backend, FALSE);
        SetWindowTextW(toggle, L"Stop output");
        Render();
    }

    void Stop(std::wstring message) {
        if (Active()) Write(TanitaReport{});
        if (custom_ != INVALID_HANDLE_VALUE) CloseHandle(custom_);
        if (added_) vigem_target_remove(client_, target_);
        if (target_) vigem_target_free(target_);
        if (client_) {
            vigem_disconnect(client_);
            vigem_free(client_);
        }
        custom_ = INVALID_HANDLE_VALUE;
        added_ = false;
        target_ = nullptr;
        client_ = nullptr;
        mapper_ = Mapper{};
        last_ = {};
        message_ = std::move(message);
        EnableWindow(backend, TRUE);
        SetWindowTextW(toggle, L"Start output");
        Render();
    }

private:
    static HFONT MakeFont(int pixels, int weight, const wchar_t* face) {
        return CreateFontW(-pixels, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, face);
    }
    static void Fill(HDC dc, const RECT& rect, COLORREF color) {
        SetDCBrushColor(dc, color);
        FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    static void Text(HDC dc, int x, int y, const wchar_t* text, HFONT font, COLORREF color) {
        const auto old = SelectObject(dc, font);
        SetTextColor(dc, color);
        TextOutW(dc, x, y, text, lstrlenW(text));
        SelectObject(dc, old);
    }
    static HMENU Menu(int id) { return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)); }
    PHIDP_PREPARSED_DATA Descriptor() { return reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsed_.data()); }
    bool Active() const { return added_ || custom_ != INVALID_HANDLE_VALUE; }

    bool Decode(PCHAR report) {
        const auto value = [&](USAGE usage, ULONG& result) {
            return HidP_GetUsageValue(HidP_Input, 1, 0, usage, &result, Descriptor(), report, 64) ==
                   HIDP_STATUS_SUCCESS;
        };
        const auto axis = [](ULONG raw) {
            const double normalized = (std::min(raw, 255ul) - 127.5) / 127.5;
            return std::copysign(std::max(0., std::abs(normalized) - .15) / .85, normalized);
        };
        ULONG x{}, y{}, z{}, rz{}, hat{};
        if (!value(0x30, x) || !value(0x31, y) || !value(0x32, z) ||
            !value(0x35, rz) || !value(0x39, hat)) return false;
        USAGE usages[14]{};
        ULONG count = 14;
        if (HidP_GetUsages(HidP_Input, 9, 0, usages, &count, Descriptor(), report, 64) != HIDP_STATUS_SUCCESS)
            return false;
        physical_ = {axis(x), -axis(y), axis(z), -axis(rz), 0, hat < 8 ? static_cast<int>(hat) : 8};
        for (ULONG i = 0; i < count; ++i) {
            if (usages[i] >= 1 && usages[i] <= 14) physical_.buttons |= 1u << (usages[i] - 1);
        }
        return true;
    }

    bool Write(const TanitaReport& report) {
        if (added_) {
            const XUSB_REPORT xbox{report.buttons, report.leftTrigger, report.rightTrigger,
                                  report.leftX, report.leftY, report.rightX, report.rightY};
            return VIGEM_SUCCESS(vigem_target_x360_update(client_, target_, xbox));
        }
        if (custom_ == INVALID_HANDLE_VALUE) return false;
        DWORD returned = 0;
        return DeviceIoControl(custom_, TANITA_SUBMIT_V1, const_cast<TanitaReport*>(&report), sizeof(report),
                               nullptr, 0, &returned, nullptr) != FALSE;
    }

    void ResetInput() {
        physical_ = {};
        mapper_ = Mapper{};
        last_ = {};
        if (Active() && !Write(last_)) Stop(L"Could not reset output");
    }

    void Render() {
        wchar_t input[96]{}, output[96]{};
        swprintf_s(input, L"L %+.2f %+.2f   R %+.2f %+.2f   Buttons %04X   Hat %d",
                   physical_.lx, physical_.ly, physical_.rx, physical_.ry, physical_.buttons, physical_.hat);
        swprintf_s(output, L"L %d,%d   R %d,%d   Buttons %04X   LT/RT %u/%u",
                   last_.leftX, last_.leftY, last_.rightX, last_.rightY,
                   unsigned(last_.buttons), unsigned(last_.leftTrigger), unsigned(last_.rightTrigger));
        if (renderedInput_ != input || renderedOutput_ != output || renderedMessage_ != message_ ||
            renderedDevice_ != device_ || renderedCamera_ != mapper_.camera) {
            renderedInput_ = input;
            renderedOutput_ = output;
            renderedMessage_ = message_;
            renderedDevice_ = device_;
            renderedCamera_ = mapper_.camera;
            const RECT statusArea{24, 134, 496, 324};
            InvalidateRect(window, &statusArea, FALSE);
        }
    }

    HANDLE device_{};
    HANDLE custom_{INVALID_HANDLE_VALUE};
    PVIGEM_CLIENT client_{};
    PVIGEM_TARGET target_{};
    bool added_{};
    std::vector<BYTE> preparsed_;
    std::vector<BYTE> raw_;
    PhysicalInput physical_{};
    Mapper mapper_;
    TanitaReport last_{};
    std::wstring message_{L"Output stopped"};
    std::wstring renderedInput_, renderedOutput_, renderedMessage_;
    HANDLE renderedDevice_{};
    bool renderedCamera_{};
    HFONT titleFont_{}, bodyFont_{}, dataFont_{};
    DriverStatus driverStatus_{DriverStatus::Unknown};
};

App app;

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        app.window = window;
        app.Initialize();
        return 0;
    case WM_INPUT:
        app.Read(reinterpret_cast<HRAWINPUT>(lParam));
        break;
    case WM_PAINT:
        app.Paint();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) app.RefreshDriverStatus();
        break;
    case WM_NOTIFY: {
        const auto* notice = reinterpret_cast<const NMHDR*>(lParam);
        if (notice->idFrom == kDriverLink && (notice->code == NM_CLICK || notice->code == NM_RETURN)) {
            app.OpenDriverDownload();
            return 0;
        }
        break;
    }
    case WM_INPUT_DEVICE_CHANGE:
        if (wParam == GIDC_ARRIVAL) app.Attach(reinterpret_cast<HANDLE>(lParam));
        if (wParam == GIDC_REMOVAL) app.Remove(reinterpret_cast<HANDLE>(lParam));
        return 0;
    case WM_TIMER:
        app.Tick();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == kToggle && HIWORD(wParam) == BN_CLICKED) app.Toggle();
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        app.Stop(L"Output stopped");
        app.ReleaseFonts();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    const INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LINK_CLASS};
    InitCommonControlsEx(&controls);
    WNDCLASSW type{};
    type.lpfnWndProc = WindowProc;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.lpszClassName = L"TanitaNativeWindow";
    if (!RegisterClassW(&type)) return 1;
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT bounds{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRect(&bounds, style, FALSE);
    HWND window = CreateWindowW(type.lpszClassName, L"TANITA VCD-18-c", style,
                                CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                                bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message{};
    BOOL result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return result == -1 ? 1 : static_cast<int>(message.wParam);
}
