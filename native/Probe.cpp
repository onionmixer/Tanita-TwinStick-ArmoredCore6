#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <iostream>
#include <vector>

namespace {
constexpr DWORD kTanitaVendorId = 0x1f4f;
constexpr DWORD kTanitaProductId = 0x9001;

bool IsTwinStick(const RID_DEVICE_INFO& device) {
    return device.dwType == RIM_TYPEHID &&
           device.hid.dwVendorId == kTanitaVendorId &&
           device.hid.dwProductId == kTanitaProductId &&
           device.hid.usUsagePage == 1 &&
           (device.hid.usUsage == 4 || device.hid.usUsage == 5);
}

bool PrintInputCapabilities(HANDLE device) {
    UINT size = 0;
    if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &size) == UINT(-1) || !size) {
        return false;
    }
    std::vector<BYTE> data(size);
    if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, data.data(), &size) == UINT(-1)) {
        return false;
    }
    auto preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(data.data());
    HIDP_CAPS caps{};
    if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS) {
        return false;
    }

    std::cout << "USB GAMESTICK (VID 1F4F, PID 9001), input report bytes: "
              << caps.InputReportByteLength << '\n';
    USHORT count = caps.NumberInputValueCaps;
    std::vector<HIDP_VALUE_CAPS> values(count);
    if (count && HidP_GetValueCaps(HidP_Input, values.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) {
        return false;
    }
    for (USHORT i = 0; i < count; ++i) {
        const auto& cap = values[i];
        std::cout << "value report=" << unsigned(cap.ReportID) << " link=" << cap.LinkCollection
                  << " page=0x" << std::hex << cap.UsagePage << " usage=0x"
                  << (cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage);
        if (cap.IsRange) {
            std::cout << "-0x" << cap.Range.UsageMax;
        }
        std::cout << std::dec << " logical=" << cap.LogicalMin << ".." << cap.LogicalMax
                  << " bits=" << cap.BitSize << " null=" << unsigned(cap.HasNull) << '\n';
    }
    count = caps.NumberInputButtonCaps;
    std::vector<HIDP_BUTTON_CAPS> buttons(count);
    if (count && HidP_GetButtonCaps(HidP_Input, buttons.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) {
        return false;
    }
    for (USHORT i = 0; i < count; ++i) {
        const auto& cap = buttons[i];
        std::cout << "button report=" << unsigned(cap.ReportID) << " link=" << cap.LinkCollection
                  << " page=0x" << std::hex << cap.UsagePage << " usage=0x"
                  << (cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage);
        if (cap.IsRange) {
            std::cout << "-0x" << cap.Range.UsageMax;
        }
        std::cout << std::dec << '\n';
    }
    return true;
}
} // namespace

int main() {
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1)) {
        std::cerr << "Raw Input enumeration failed: " << GetLastError() << '\n';
        return 1;
    }
    std::vector<RAWINPUTDEVICELIST> devices(count);
    if (count) {
        count = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
        if (count == UINT(-1)) {
            std::cerr << "Raw Input device list changed or could not be read: " << GetLastError() << '\n';
            return 1;
        }
    }

    unsigned matches = 0;
    for (UINT i = 0; i < count; ++i) {
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        const auto handle = devices[i].hDevice;
        if (GetRawInputDeviceInfoW(handle, RIDI_DEVICEINFO, &info, &size) == UINT(-1) || !IsTwinStick(info)) {
            continue;
        }
        ++matches;
        if (!PrintInputCapabilities(handle)) {
            std::cerr << "Could not read the Twin Stick HID capabilities.\n";
            return 1;
        }
    }
    std::cout << "Matching joystick/gamepad collections: " << matches << '\n';
    return matches ? 0 : 2;
}
