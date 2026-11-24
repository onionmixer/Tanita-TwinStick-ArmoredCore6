#pragma once

#include <gdiplus.h>
#include <shlwapi.h>
#include <bit>
#include <cstring>
#include <iterator>
#include <string>
#include "Mapper.h"
#include "Resources.h"

class MappingView {
public:
    const Bindings& CurrentBindings() const { return bindings_; }

    void LoadBindings() {
        Bindings stored;
        Bindings::StoredActions data{};
        DWORD size = static_cast<DWORD>(sizeof(data));
        const auto result = RegGetValueW(HKEY_CURRENT_USER, kSettings, kBindingsValue, RRF_RT_REG_BINARY,
                                         nullptr, data.data(), &size);
        if (result == ERROR_FILE_NOT_FOUND) {
            // V1 stored only the button slots. The missing lever pairs keep their defaults.
            size = static_cast<DWORD>(sizeof(stored.actions));
            if (RegGetValueW(HKEY_CURRENT_USER, kSettings, L"ButtonBindingsV1", RRF_RT_REG_BINARY,
                             nullptr, stored.actions.data(), &size) != ERROR_SUCCESS ||
                size != sizeof(stored.actions)) return;
        } else {
            if (result != ERROR_SUCCESS || size != sizeof(data)) return;
            std::copy_n(data.begin(), stored.actions.size(), stored.actions.begin());
            std::copy_n(data.begin() + stored.actions.size(), stored.leverActions.size(), stored.leverActions.begin());
        }
        for (auto action : stored.actions)
            if (action >= OutputAction::Count || action == OutputAction::DefaultLever) return;
        for (auto action : stored.leverActions) if (action >= OutputAction::Count) return;
        bindings_ = stored;
    }

    void Open(HWND owner) {
        if (window_) {
            SetForegroundWindow(window_);
            return;
        }
        const auto instance = GetModuleHandleW(nullptr);
        WNDCLASSW type{};
        type.lpfnWndProc = WindowProc;
        type.hInstance = instance;
        type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        type.lpszClassName = L"TanitaMappingWindow";
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        constexpr DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
        RECT bounds{0, 0, 1080, 625};
        AdjustWindowRect(&bounds, style, FALSE);
        window_ = CreateWindowExW(WS_EX_TOOLWINDOW, type.lpszClassName, L"Twin Stick mapping and input test",
                                  style, CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                                  bounds.bottom - bounds.top, owner, nullptr, instance, this);
        if (window_) ShowWindow(window_, SW_SHOW);
    }

    void Update(const PhysicalInput& input, const TanitaReport& output, bool connected, bool outputActive) {
        if (!window_) return;
        const unsigned oldActive = ActiveInputs();
        const bool connectionChanged = connected_ != connected || outputActive_ != outputActive;
        const bool outputChanged = std::memcmp(&output_, &output, sizeof(output)) != 0;
        input_ = input;
        output_ = output;
        connected_ = connected;
        outputActive_ = outputActive;
        if (captureWindow_) UpdateCapture();
        const unsigned active = ActiveInputs();
        bool stepChanged = false;
        if (!connected_) armed_ = detected_ = false;
        if (testing_ && connected_) {
            const bool neutral = active == 0;
            if (index_ == 0 && neutral) {
                Advance(false);
                stepChanged = true;
            } else if (index_ > 0) {
                const auto& step = Steps[index_ - 1];
                if (armed_ && Matches(step)) {
                    armed_ = false;
                    detected_ = true;
                    stepChanged = true;
                } else if (neutral && detected_) {
                    Advance(false);
                    stepChanged = true;
                } else if (neutral) {
                    armed_ = true;
                }
            }
        }
        if (connectionChanged || outputChanged || oldActive != active || stepChanged)
            InvalidateRect(window_, nullptr, FALSE);
        if (connectionChanged) RefreshEditor();
    }

    bool HandleNavigation(MSG& message) const {
        return window_ && (message.hwnd == window_ || IsChild(window_, message.hwnd)) &&
               IsDialogMessageW(window_, &message);
    }

private:
    enum class Kind { Button, Hat, Lever };
    struct Control {
        Kind kind;
        int value;
        int x, y;
        const wchar_t* name;
        const wchar_t* fixedAction{};
    };
    struct Step { int control; int direction; const wchar_t* label; };
    // Locations are measured on the 784 x 502 reference image.
    static constexpr Control Controls[]{
        {Kind::Hat,0,181,96,L"Direction up"},
        {Kind::Hat,2,200,115,L"Direction right"},
        {Kind::Hat,4,181,134,L"Direction down"},
        {Kind::Hat,6,162,115,L"Direction left"},
        {Kind::Lever,0,282,138,L"Left lever",L"Match directions: move. Opposed fore/aft: camera."},
        {Kind::Button,3,353,98,L"Circle"},
        {Kind::Button,4,392,98,L"Triangle"},
        {Kind::Button,2,430,98,L"Cross"},
        {Kind::Button,1,392,140,L"Square"},
        {Kind::Lever,1,501,138,L"Right lever",L"Spread levers: jump. Both TURBO held: camera."},
        {Kind::Button,9,587,101,L"SHARE"},
        {Kind::Button,13,629,101,L"PS"},
        {Kind::Button,10,608,141,L"OPTIONS"},
        {Kind::Button,5,240,333,L"Trigger (L1)"},
        {Kind::Button,7,510,275,L"TURBO (L2)",L"Xbox L3  /  Assault boost. Both TURBO: camera."},
        {Kind::Button,11,510,257,L"Gray button 1 (L3)"},
        {Kind::Button,11,529,274,L"Gray button 2 (L3)"},
        {Kind::Button,6,240,333,L"Trigger (R1)"},
        {Kind::Button,8,510,275,L"TURBO (R2)",L"Xbox L3  /  Assault boost. Both TURBO: camera."},
        {Kind::Button,12,510,257,L"Gray button 1 (R3)"},
        {Kind::Button,12,529,274,L"Gray button 2 (R3)"},
    };
    static constexpr Step Steps[]{
        {4,0,L"Left lever forward"}, {4,2,L"Left lever right"}, {4,4,L"Left lever back"}, {4,6,L"Left lever left"},
        {9,0,L"Right lever forward"}, {9,2,L"Right lever right"}, {9,4,L"Right lever back"}, {9,6,L"Right lever left"},
        {0,-1,L"Direction up"}, {1,-1,L"Direction right"}, {2,-1,L"Direction down"}, {3,-1,L"Direction left"},
        {5,-1,L"Circle"}, {6,-1,L"Triangle"}, {7,-1,L"Cross"}, {8,-1,L"Square"},
        {10,-1,L"SHARE"}, {11,-1,L"PS"}, {12,-1,L"OPTIONS"},
        {13,-1,L"Left trigger"}, {14,-1,L"Left TURBO"}, {15,-1,L"Left gray button, first"}, {16,-1,L"Left gray button, second"},
        {17,-1,L"Right trigger"}, {18,-1,L"Right TURBO"}, {19,-1,L"Right gray button, first"}, {20,-1,L"Right gray button, second"},
    };
    static constexpr int kImageX = 14, kImageY = 40, kImageWidth = 523, kPanelHeight = 151;
    static constexpr int kGripY = 219, kGripWidth = 250, kGripHeight = 169;
    static constexpr int kGripSourceX = 195, kGripSourceY = 229;
    static constexpr int kGripSourceWidth = 390, kGripSourceHeight = 264;
    static constexpr int kCalloutX = 557, kRowHeight = 20;
    static constexpr int kMarkerRadius = 7;
    static constexpr int kLeftGrip = 301, kRightGrip = 302, kTest = 303, kSkip = 304;
    static constexpr int kAction = 305, kCapture = 306, kDefaults = 307;
    static constexpr wchar_t kSettings[] = L"Software\\TanitaTwinStickBridge";
    static constexpr wchar_t kBindingsValue[] = L"InputBindingsV2";

    static int BindingSlot(int index) {
        const auto& control = Controls[index];
        if (control.kind == Kind::Hat) return Bindings::HatSlot(control.value);
        if (control.kind == Kind::Button && !Bindings::IsTurbo(control.value))
            return control.value - 1;
        return -1;
    }

    const wchar_t* ActionLabel(int index) const {
        const int slot = BindingSlot(index);
        return slot < 0 ? Controls[index].fixedAction : OutputLabels[static_cast<unsigned>(bindings_.actions[slot])];
    }

    void RefreshEditor() {
        const bool editable = !testing_ && !outputActive_;
        EnableWindow(actionPicker_, editable);
        EnableWindow(captureButton_, editable && connected_ && SendMessageW(actionPicker_, CB_GETCURSEL, 0, 0) != CB_ERR);
        EnableWindow(defaultsButton_, editable);
    }

    bool SaveBindings(const Bindings& next) {
        HKEY key{};
        LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, kSettings, 0, nullptr, 0,
                                         KEY_SET_VALUE, nullptr, &key, nullptr);
        if (result == ERROR_SUCCESS) {
            Bindings::StoredActions data{};
            auto tail = std::copy(next.actions.begin(), next.actions.end(), data.begin());
            std::copy(next.leverActions.begin(), next.leverActions.end(), tail);
            result = RegSetValueExW(key, kBindingsValue, 0, REG_BINARY,
                                    reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(sizeof(data)));
            RegCloseKey(key);
        }
        if (result == ERROR_SUCCESS) {
            bindings_ = next;
            editStatus_ = L"Mapping saved.";
        } else {
            editStatus_ = L"Could not save mapping (Windows error " + std::to_wstring(result) + L").";
        }
        RefreshEditor();
        return result == ERROR_SUCCESS;
    }

    std::wstring InputName(int index) const {
        const auto& control = Controls[index];
        return std::wstring(index >= 17 ? L"Right grip / " : index >= 13 ? L"Left grip / " : L"") + control.name;
    }

    bool HasCapture() const { return capturedControl_ >= 0 || capturedPair_ >= 0; }

    static std::wstring PairName(int pair) {
        constexpr const wchar_t* directions[]{L"\u2191", L"\u2197", L"\u2192", L"\u2198",
                                              L"\u2193", L"\u2199", L"\u2190", L"\u2196"};
        return std::wstring(L"Left lever ") + directions[pair / Bindings::DirectionCount] +
               L" + Right lever " + directions[pair % Bindings::DirectionCount];
    }

    void StartCapture() {
        const auto action = SendMessageW(actionPicker_, CB_GETCURSEL, 0, 0);
        if (!connected_ || outputActive_ || testing_ || action < 0 ||
            action >= static_cast<int>(OutputAction::Count)) return;
        capturedAction_ = static_cast<OutputAction>(action);
        const auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CAPTURE_INPUT), window_, CaptureProc,
                                            reinterpret_cast<LPARAM>(this));
        if (result == IDCANCEL) editStatus_ = L"Input assign canceled";
    }

    void RetryCapture() {
        capturedControl_ = -1;
        capturedPair_ = -1;
        candidatePair_ = -1;
        captureArmed_ = false;
        captureCanceled_ = false;
        captureDeadline_ = Mapper::Clock::now() + std::chrono::seconds(4);
        captureWarning_.clear();
        UpdateCapture();
    }

    void UpdateCapture() {
        const auto now = Mapper::Clock::now();
        if (captureCanceled_) {
            if (now >= captureCloseAt_) EndDialog(captureWindow_, IDCANCEL);
            return;
        }
        if (!connected_ || outputActive_) {
            capturedControl_ = -1;
            capturedPair_ = candidatePair_ = -1;
            captureDeadline_ = {};
        }
        if (!HasCapture() && captureDeadline_ != Mapper::Clock::time_point{} && now >= captureDeadline_) {
            CancelCapture(now);
            return;
        }
        const bool waiting = !HasCapture() && now < captureDeadline_;
        const bool lever = IsActive(Controls[4]) || IsActive(Controls[9]);
        const bool hat = input_.hat >= 0 && input_.hat < 8;
        const unsigned buttons = std::popcount(input_.buttons) + unsigned(hat);
        if (waiting) {
            // Only capture a fresh input after all controls have returned to neutral.
            if (!buttons && !lever) {
                captureArmed_ = true;
                candidatePair_ = -1;
                captureWarning_.clear();
            } else if (captureArmed_) {
                if (!buttons && lever) {
                    const int pair = LeverPairIndex(input_);
                    if (pair != candidatePair_) {
                        candidatePair_ = pair;
                        candidateSince_ = now;
                    } else if (pair >= 0 && now - candidateSince_ >= std::chrono::milliseconds(100)) {
                        // Allow the second lever to settle before capturing a cardinal or diagonal pair.
                        capturedPair_ = pair;
                    }
                } else if (buttons == 1 && !lever && capturedAction_ != OutputAction::DefaultLever) {
                    for (int i = 0; i < int(std::size(Controls)); ++i) {
                        const auto& control = Controls[i];
                        const bool matchingHat = control.kind != Kind::Hat || input_.hat == control.value;
                        if (BindingSlot(i) >= 0 && matchingHat && IsActive(control)) {
                            capturedControl_ = i;
                            break;
                        }
                    }
                }
                if (buttons && !HasCapture()) {
                    captureArmed_ = false;
                    candidatePair_ = -1;
                    captureWarning_ = L"Release the controls. Use one button or both levers.";
                }
            }
        }
        std::wstring status;
        if (!connected_) status = L"Twin Stick disconnected. Reconnect it and select Retry.";
        else if (outputActive_) status = L"Stop virtual output, then select Retry.";
        else if (HasCapture()) status = captureWarning_.empty() ?
            L"Input detected. Review the result and save the mapping." : captureWarning_;
        else if (!waiting) status = L"Choose Retry to start another four-second capture.";
        else {
            const auto seconds = std::chrono::ceil<std::chrono::seconds>(captureDeadline_ - now).count();
            status = std::to_wstring(seconds) + L" s remaining. ";
            status += captureWarning_.empty() ? (captureArmed_ ? L"Press one button or move both levers." :
                                                                L"Release the Twin Stick controls.") : captureWarning_;
        }
        SetCaptureText(IDC_CAPTURE_STATUS, status);
        wchar_t live[160]{};
        swprintf_s(live, L"Live: L %+.2f %+.2f  R %+.2f %+.2f  |  Buttons %04X  Hat %d",
                   input_.lx, input_.ly, input_.rx, input_.ry, input_.buttons, input_.hat);
        SetCaptureText(IDC_CAPTURE_LIVE, live);
        SetCaptureText(IDC_CAPTURE_RESULT, capturedPair_ >= 0 ? L"Detected: " + PairName(capturedPair_) :
                       capturedControl_ >= 0 ? L"Detected: " + InputName(capturedControl_) : L"Detected input: none");
        EnableWindow(GetDlgItem(captureWindow_, IDOK), HasCapture());
        EnableWindow(GetDlgItem(captureWindow_, IDC_CAPTURE_RETRY), connected_ && !outputActive_);
    }

    void CancelCapture(Mapper::Clock::time_point now) {
        if (captureCanceled_) return;
        captureCanceled_ = true;
        captureCloseAt_ = now + std::chrono::seconds(1);
        const HWND status = GetDlgItem(captureWindow_, IDC_CAPTURE_STATUS);
        const auto normal = reinterpret_cast<HFONT>(SendMessageW(status, WM_GETFONT, 0, 0));
        LOGFONTW face{};
        if (normal && GetObjectW(normal, sizeof(face), &face)) {
            face.lfHeight *= 3;
            face.lfWeight = FW_BOLD;
            captureCanceledFont_ = CreateFontIndirectW(&face);
            if (captureCanceledFont_)
                SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(captureCanceledFont_), TRUE);
        }
        SetWindowLongPtrW(status, GWL_STYLE, (GetWindowLongPtrW(status, GWL_STYLE) & ~SS_TYPEMASK) | SS_CENTER);
        RECT messageArea{12, 34, 304, 85};
        MapDialogRect(captureWindow_, &messageArea);
        SetWindowPos(status, nullptr, messageArea.left, messageArea.top,
                     messageArea.right - messageArea.left, messageArea.bottom - messageArea.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(GetDlgItem(captureWindow_, IDC_CAPTURE_LIVE), SW_HIDE);
        ShowWindow(GetDlgItem(captureWindow_, IDC_CAPTURE_RESULT), SW_HIDE);
        SetCaptureText(IDC_CAPTURE_STATUS, L"Input assign canceled");
        InvalidateRect(captureWindow_, nullptr, TRUE);
        for (int id : {IDOK, IDC_CAPTURE_RETRY, IDCANCEL})
            EnableWindow(GetDlgItem(captureWindow_, id), FALSE);
    }

    void SetCaptureText(int id, const std::wstring& value) const {
        wchar_t existing[256]{};
        GetDlgItemTextW(captureWindow_, id, existing, int(std::size(existing)));
        if (value != existing) SetDlgItemTextW(captureWindow_, id, value.c_str());
    }

    static INT_PTR CALLBACK CaptureProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_INITDIALOG) {
            auto* view = reinterpret_cast<MappingView*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, lParam);
            view->captureWindow_ = dialog;
            SetDlgItemTextW(dialog, IDC_CAPTURE_ACTION, OutputLabels[static_cast<unsigned>(view->capturedAction_)]);
            view->RetryCapture();
            return TRUE;
        }
        auto* view = reinterpret_cast<MappingView*>(GetWindowLongPtrW(dialog, DWLP_USER));
        if (!view) return FALSE;
        if (message == WM_COMMAND) {
            switch (LOWORD(wParam)) {
            case IDOK:
                if (!view->captureCanceled_ && view->HasCapture() &&
                    view->connected_ && !view->outputActive_) {
                    Bindings next = view->bindings_;
                    if (view->capturedPair_ >= 0)
                        next.leverActions[view->capturedPair_] = view->capturedAction_;
                    else next.actions[BindingSlot(view->capturedControl_)] = view->capturedAction_;
                    if (view->SaveBindings(next)) {
                        if (view->capturedPair_ >= 0) {
                            view->editStatus_ = PairName(view->capturedPair_) + L": " +
                                (view->capturedAction_ == OutputAction::DefaultLever ? L"Default behavior restored." :
                                 OutputLabels[static_cast<unsigned>(view->capturedAction_)]);
                        } else view->selected_ = view->capturedControl_;
                        EndDialog(dialog, IDOK);
                    } else {
                        view->captureWarning_ = view->editStatus_;
                        view->SetCaptureText(IDC_CAPTURE_STATUS, view->captureWarning_);
                    }
                }
                return TRUE;
            case IDC_CAPTURE_RETRY:
                if (!view->captureCanceled_ && view->connected_ && !view->outputActive_) view->RetryCapture();
                return TRUE;
            case IDCANCEL:
                view->CancelCapture(Mapper::Clock::now());
                return TRUE;
            }
        }
        if (message == WM_CLOSE) {
            view->CancelCapture(Mapper::Clock::now());
            return TRUE;
        }
        if (message == WM_CTLCOLORSTATIC && view->captureCanceled_ &&
            GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) == IDC_CAPTURE_STATUS) {
            const auto dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(188, 24, 37));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_3DFACE));
        }
        if (message == WM_NCDESTROY) {
            if (view->captureCanceledFont_) DeleteObject(view->captureCanceledFont_);
            view->captureCanceledFont_ = nullptr;
            view->captureWindow_ = nullptr;
        }
        return FALSE;
    }

    bool IsActive(const Control& control) const {
        if (control.kind == Kind::Button) return Mapper::Pressed(input_, control.value);
        if (control.kind == Kind::Hat) return HatIncludesDirection(input_.hat, control.value);
        const auto [x,y] = Lever(control.value);
        return LeverDirection(x, y) != std::pair{0, 0};
    }

    unsigned ActiveInputs() const {
        if (!connected_) return 0;
        unsigned mask = 0;
        for (unsigned i = 0; i < std::size(Controls); ++i) {
            if (IsActive(Controls[i])) mask |= 1u << i;
        }
        return mask;
    }

    std::pair<double, double> Lever(int side) const {
        return side ? std::pair{input_.rx, input_.ry} : std::pair{input_.lx, input_.ly};
    }

    bool Matches(const Step& step) const {
        const auto& control = Controls[step.control];
        if (control.kind == Kind::Hat) return input_.hat == control.value;
        if (control.kind == Kind::Button) return IsActive(control);
        const auto [x,y] = Lever(control.value);
        constexpr std::pair<int, int> cardinal[]{{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
        return LeverDirection(x, y) == cardinal[step.direction / 2];
    }

    void Advance(bool skipped) {
        if (index_ > 0) {
            if (skipped) ++skipped_;
            else ++passed_;
        }
        detected_ = false;
        armed_ = ActiveInputs() == 0;
        if (++index_ > std::size(Steps)) {
            testing_ = false;
            complete_ = true;
            UpdateButtons();
            return;
        }
        selected_ = Steps[index_ - 1].control;
        UpdateButtons();
        RefreshEditor();
    }

    void UpdateButtons() {
        SetWindowTextW(testButton_, testing_ ? L"Cancel test" : L"Start input test");
        EnableWindow(skipButton_, testing_ && index_ > 0);
        RefreshEditor();
    }

    void Command(int id) {
        if (id == kTest) {
            testing_ = !testing_;
            complete_ = false;
            index_ = passed_ = skipped_ = 0;
            armed_ = detected_ = false;
        } else if (id == kSkip && testing_ && index_ > 0) {
            Advance(true);
        } else if ((id == kLeftGrip || id == kRightGrip) && !testing_) {
            selected_ = id == kRightGrip ? 17 : 13;
            editStatus_.clear();
            RefreshEditor();
        } else if (id == kCapture) {
            StartCapture();
        } else if (id == kDefaults && !testing_ && !outputActive_) {
            if (MessageBoxW(window_, L"Replace all custom assignments with the default mappings?",
                            L"Restore default mappings", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES &&
                SaveBindings(Bindings{})) editStatus_ = L"Default mappings restored.";
        }
        UpdateButtons();
        InvalidateRect(window_, nullptr, FALSE);
    }

    void Click(int x, int y) {
        if (testing_) return;
        for (int i = 0; i < int(std::size(Controls)); ++i) {
            const auto point = MarkerPoint(i);
            const bool row = x >= kCalloutX && x < 1064 && y >= RowY(i) && y < RowY(i) + kRowHeight;
            const bool marker = std::hypot(x - point.X, y - point.Y) <= kMarkerRadius + 1;
            if (!row && !marker) continue;
            selected_ = i;
            editStatus_.clear();
            RefreshEditor();
            InvalidateRect(window_, nullptr, FALSE);
            break;
        }
    }

    static int RowY(int index) {
        const int groupSpacing = index >= 17 ? 44 : index >= 13 ? 22 : 0;
        return 62 + index * kRowHeight + groupSpacing;
    }

    static int GripX(bool right) { return right ? 278 : 19; }

    static Gdiplus::Point MarkerPoint(int index) {
        const auto& control = Controls[index];
        if (index < 13) {
            return {kImageX + MulDiv(control.x, kImageWidth, 784),
                    kImageY + MulDiv(control.y, kPanelHeight, 225)};
        }
        return {GripX(index >= 17) + MulDiv(control.x - kGripSourceX, kGripWidth, kGripSourceWidth),
                kGripY + MulDiv(control.y - kGripSourceY, kGripHeight, kGripSourceHeight)};
    }

    static void DrawLeader(Gdiplus::Graphics& graphics, int index, int destinationY,
                           Gdiplus::Color color) {
        const auto anchor = MarkerPoint(index);
        const Gdiplus::Point points[]{anchor, {kCalloutX - 24, destinationY}, {kCalloutX, destinationY}};
        const Gdiplus::Pen halo(Gdiplus::Color(245, 255, 255, 255), 4.0f);
        const Gdiplus::Pen leader(color, 1.5f);
        graphics.DrawLines(&halo, points, 3);
        graphics.DrawLines(&leader, points, 3);
        const Gdiplus::SolidBrush dot(color);
        graphics.FillEllipse(&dot, anchor.X - 2, anchor.Y - 2, 4, 4);
    }

    void Create(HWND window) {
        window_ = window;
        markerFont_ = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH, L"Segoe UI");
        selected_ = 11;
        testing_ = complete_ = detected_ = armed_ = false;
        const auto instance = GetModuleHandleW(nullptr);
        HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_TWIN_STICK_MAP), RT_RCDATA);
        if (resource) {
            const auto* data = static_cast<const BYTE*>(LockResource(LoadResource(instance, resource)));
            if (data) stream_ = SHCreateMemStream(data, SizeofResource(instance, resource));
            if (stream_) image_ = Gdiplus::Image::FromStream(stream_);
        }
        Button(L"Left grip", kLeftGrip, 16, 92);
        Button(L"Right grip", kRightGrip, 114, 92);
        testButton_ = Button(L"Start input test", kTest, 224, 128);
        skipButton_ = Button(L"Skip input", kSkip, 364, 118);
        actionPicker_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                        16, 562, 340, 300, window_,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAction)), nullptr, nullptr);
        SendMessageW(actionPicker_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
        for (const auto* action : OutputLabels)
            SendMessageW(actionPicker_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(action));
        RECT picker{};
        GetWindowRect(actionPicker_, &picker);
        captureButton_ = Button(L"Capture input", kCapture, 368, 125, 562, picker.bottom - picker.top);
        defaultsButton_ = Button(L"Restore defaults", kDefaults, 505, 132, 562, picker.bottom - picker.top);
        editStatus_.clear();
        UpdateButtons();
        RefreshEditor();
    }

    HWND Button(const wchar_t* label, int id, int x, int width, int y = 473, int height = 30) {
        HWND button = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    x, y, width, height, window_,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
        return button;
    }

    void Paint() {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window_, &paint);
        FillRect(dc, &paint.rcPaint, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        const unsigned active = ActiveInputs();
        int live = -1;
        // Highlight every active row but draw only one extra leader to keep the diagram readable.
        for (int i = 0; i < int(std::size(Controls)); ++i) {
            if (i != selected_ && (active & (1u << i))) {
                live = i;
                break;
            }
        }
        const bool selectedActive = (active & (1u << selected_)) != 0;
        const Gdiplus::Color blue(255, 25, 93, 170), green(255, 14, 132, 78);
        {
            Gdiplus::Graphics graphics(dc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            if (image_ && image_->GetLastStatus() == Gdiplus::Ok) {
                graphics.DrawImage(image_, Gdiplus::Rect(kImageX, kImageY, kImageWidth, kPanelHeight),
                                   0, 0, 1568, 450, Gdiplus::UnitPixel);
                for (bool right : {false, true}) {
                    graphics.DrawImage(image_, Gdiplus::Rect(GripX(right), kGripY, kGripWidth, kGripHeight),
                                       2 * kGripSourceX, 2 * kGripSourceY, 2 * kGripSourceWidth,
                                       2 * kGripSourceHeight, Gdiplus::UnitPixel);
                }
            }
            DrawGripFrame(graphics, false);
            DrawGripFrame(graphics, true);
            const Gdiplus::SolidBrush panel(Gdiplus::Color(255, 243, 248, 253));
            graphics.FillRectangle(&panel, kCalloutX, 35, 507, 500);
            DrawLeader(graphics, selected_, RowY(selected_) + 10, selectedActive ? green : blue);
            if (live >= 0 && !selectedActive) DrawLeader(graphics, live, RowY(live) + 10, green);
            for (int i = 0; i < int(std::size(Controls)); ++i) {
                const bool pressed = (active & (1u << i)) != 0;
                const Gdiplus::Color color = pressed ? green : selected_ == i ? blue :
                    Gdiplus::Color(235, 98, 125, 152);
                const auto point = MarkerPoint(i);
                Gdiplus::Pen ring(color, pressed || selected_ == i ? 2.5f : 1.25f);
                const Gdiplus::SolidBrush marker(Gdiplus::Color(249, 255, 255, 255));
                graphics.FillEllipse(&marker, point.X - kMarkerRadius, point.Y - kMarkerRadius,
                                     2 * kMarkerRadius, 2 * kMarkerRadius);
                graphics.DrawEllipse(&ring, point.X - kMarkerRadius, point.Y - kMarkerRadius,
                                     2 * kMarkerRadius, 2 * kMarkerRadius);
            }
        }
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(dc, RGB(30, 52, 78));
        Line(dc, 16, 12, connected_ ? L"VCD-18-c connected  |  Blue: selected  |  Green: input detected" :
                                     L"Connect a VCD-18-c to see live input");
        SetTextColor(dc, RGB(25, 93, 170));
        Line(dc, kCalloutX + 12, 41, L"Control panel");
        Line(dc, kCalloutX + 12, RowY(13) - 20, L"Left grip");
        Line(dc, kCalloutX + 12, RowY(17) - 20, L"Right grip");
        for (int i = 0; i < int(std::size(Controls)); ++i) {
            const bool pressed = (active & (1u << i)) != 0;
            DrawRow(dc, i, i == selected_, pressed);
            const auto point = MarkerPoint(i);
            RECT marker{point.X - 8, point.Y - 8, point.X + 8, point.Y + 8};
            const auto number = std::to_wstring(i + 1);
            SetTextColor(dc, RGB(188, 24, 37));
            const auto previousFont = markerFont_ ? SelectObject(dc, markerFont_) : nullptr;
            DrawTextW(dc, number.c_str(), -1, &marker, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (previousFont) SelectObject(dc, previousFont);
        }
        SetTextColor(dc, RGB(30, 52, 78));
        DrawGripHeading(dc, false, L"Left grip");
        DrawGripHeading(dc, true, L"Right grip");
        wchar_t output[150]{};
        swprintf_s(output, L"%ls  |  L %d,%d   R %d,%d   Buttons %04X   LT/RT %u/%u",
                   outputActive_ ? L"Active output" : L"Output preview", output_.leftX, output_.leftY,
                   output_.rightX, output_.rightY, unsigned(output_.buttons),
                   unsigned(output_.leftTrigger), unsigned(output_.rightTrigger));
        Line(dc, 16, 412, output);
        std::wstring prompt = L"Click a marked control or start the guided test.";
        if (testing_) {
            if (!connected_) prompt = L"Waiting for the Twin Stick to reconnect.";
            else if (!index_) prompt = L"Release all controls to begin.";
            else prompt = std::to_wstring(index_) + L"/" + std::to_wstring(std::size(Steps)) + L"  " +
                          Steps[index_ - 1].label + (detected_ ? L"  \u2014  Detected. Release to continue." :
                                                               L"  \u2014  Press or move now.");
        } else if (complete_) {
            prompt = L"Test complete  \u2014  " + std::to_wstring(passed_) + L" detected, " +
                     std::to_wstring(skipped_) + L" skipped.";
        }
        Line(dc, 16, 440, prompt);
        Line(dc, 16, 537, L"Button Assignment (AC6)");
        std::wstring editHint = editStatus_;
        if (outputActive_) editHint = L"Stop virtual output to edit mappings.";
        else if (testing_) editHint = L"Finish or cancel the input test to edit mappings.";
        else if (!connected_) editHint = L"Connect a Twin Stick to capture a button.";
        else if (editHint.empty()) editHint = L"Select an AC6 action, then capture an input within 4 seconds.";
        Line(dc, 16, 597, editHint);
        EndPaint(window_, &paint);
    }

    void DrawRow(HDC dc, int index, bool selected, bool pressed) const {
        const auto& control = Controls[index];
        const int y = RowY(index);
        if (selected || pressed) {
            const RECT fill{kCalloutX + 1, y, 1063, y + kRowHeight};
            SetDCBrushColor(dc, pressed ? RGB(225, 243, 231) : RGB(222, 236, 252));
            FillRect(dc, &fill, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        }
        SetTextColor(dc, pressed ? RGB(14, 108, 65) : RGB(30, 52, 78));
        Line(dc, kCalloutX + 12, y + 3, std::to_wstring(index + 1));
        Line(dc, kCalloutX + 39, y + 3, control.name);
        Line(dc, kCalloutX + 178, y + 3, ActionLabel(index));
    }

    static void DrawGripFrame(Gdiplus::Graphics& graphics, bool right) {
        const int x = GripX(right);
        const Gdiplus::SolidBrush tint(Gdiplus::Color(255, 225, 237, 250));
        constexpr int thickness = 2;
        graphics.FillRectangle(&tint, x, 194, kGripWidth, kGripY - 194);
        graphics.FillRectangle(&tint, x, kGripY, thickness, kGripHeight);
        graphics.FillRectangle(&tint, x + kGripWidth - thickness, kGripY, thickness, kGripHeight);
        graphics.FillRectangle(&tint, x, kGripY + kGripHeight, kGripWidth, thickness);
    }

    static void DrawGripHeading(HDC dc, bool right, const wchar_t* label) {
        const int x = GripX(right);
        RECT heading{x, 194, x + kGripWidth, kGripY};
        SetTextColor(dc, RGB(30, 52, 78));
        DrawTextW(dc, label, -1, &heading, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    static void Line(HDC dc, int x, int y, const std::wstring& text) {
        TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* view = reinterpret_cast<MappingView*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!view) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_CREATE: view->Create(window); return 0;
        case WM_PAINT: view->Paint(); return 0;
        case WM_LBUTTONUP: view->Click(LOWORD(lParam), HIWORD(lParam)); return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) view->Command(LOWORD(wParam));
            if (LOWORD(wParam) == kAction && HIWORD(wParam) == CBN_SELCHANGE) view->RefreshEditor();
            return 0;
        case WM_DESTROY:
            if (view->markerFont_) DeleteObject(view->markerFont_);
            view->markerFont_ = nullptr;
            delete view->image_;
            view->image_ = nullptr;
            if (view->stream_) view->stream_->Release();
            view->stream_ = nullptr;
            view->window_ = nullptr;
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    HWND window_{}, testButton_{}, skipButton_{}, actionPicker_{}, captureButton_{}, defaultsButton_{};
    HWND captureWindow_{};
    HFONT captureCanceledFont_{};
    Mapper::Clock::time_point captureDeadline_{};
    Mapper::Clock::time_point captureCloseAt_{};
    OutputAction capturedAction_{OutputAction::None};
    int capturedControl_{-1};
    int capturedPair_{-1}, candidatePair_{-1};
    Mapper::Clock::time_point candidateSince_{};
    bool captureArmed_{}, captureCanceled_{};
    std::wstring captureWarning_;
    HFONT markerFont_{};
    IStream* stream_{};
    Gdiplus::Image* image_{};
    PhysicalInput input_{};
    TanitaReport output_{};
    Bindings bindings_;
    std::wstring editStatus_;
    bool connected_{}, outputActive_{}, testing_{}, complete_{}, detected_{}, armed_{};
    int selected_{11};
    size_t index_{}, passed_{}, skipped_{};
};
