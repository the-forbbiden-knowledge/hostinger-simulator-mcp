// Local-only WebView2 viewer. The only outbound network exception is the explicit GitHub Releases updater.
#include "simulator/service_policy.hpp"
#include <windows.h>
#include <shellapi.h>
#include <WebView2.h>
#include <wrl.h>
#include <string>
#include <string_view>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kTitle[] = L"Hostinger Simulator — Offline Preview";
constexpr wchar_t kOrigin[] = L"https://simulator.invalid/";
constexpr wchar_t kHome[] = L"https://simulator.invalid/index.html";
HWND window_handle = nullptr;
ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> browser;
ComPtr<ICoreWebView2Environment> environment;

std::wstring utf8_to_wide(const std::string& text) {
  if (text.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   text.data(), static_cast<int>(text.size()),
                                   nullptr, 0);
  if (n <= 0) return L"{\"ok\":false,\"error\":\"invalid_fixture_encoding\"}";
  std::wstring result(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), result.data(), n);
  return result;
}

std::wstring application_directory() {
  wchar_t path[MAX_PATH]{};
  if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return {};
  std::wstring directory(path);
  const auto position = directory.find_last_of(L"\\/");
  if (position == std::wstring::npos) return {};
  return directory.substr(0, position);
}

std::wstring local_assets() {
  const auto directory = application_directory();
  return directory.empty() ? std::wstring{} : directory + L"\\ui";
}

bool launch_update_helper() {
  const auto directory = application_directory();
  if (directory.empty()) return false;
  const auto script = directory + L"\\tools\\update.ps1";
  if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

  wchar_t system_directory[MAX_PATH]{};
  if (!GetSystemDirectoryW(system_directory, MAX_PATH)) return false;
  const std::wstring powershell =
      std::wstring(system_directory) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
  if (GetFileAttributesW(powershell.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
  const auto arguments =
      L"-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File \"" +
      script + L"\" -InstallDirectory \"" + directory +
      L"\" -ParentProcessId " + std::to_wstring(GetCurrentProcessId());
  const auto result = ShellExecuteW(nullptr, L"open", powershell.c_str(),
                                    arguments.c_str(), directory.c_str(), SW_HIDE);
  return reinterpret_cast<INT_PTR>(result) > 32;
}

bool is_local(const std::wstring& url) {
  return url.rfind(kOrigin, 0) == 0;
}

void resize_browser() {
  if (!controller || !window_handle) return;
  RECT rect{};
  GetClientRect(window_handle, &rect);
  controller->put_Bounds(rect);
}

void send_fixture(const std::wstring& command) {
  if (!browser) return;
  // Service fixtures are read-only. The sole state-changing command opens the fixed, local GitHub release updater.
  std::string json;
  if (command == L"service.list") {
    json = simulator::service_policy::list();
  } else if (command == L"service.report") {
    json = simulator::service_policy::report();
  } else if (command == L"service.simulate.hosted") {
    json = simulator::service_policy::simulate("hosted_end_user_helpdesk", "reviewer");
  } else if (command == L"service.evaluate.hosted") {
    json = simulator::service_policy::evaluate(
        "reviewer", "hosted", "same_tenant", "restore_preview", "approved_read_only");
  } else if (command == L"service.evaluate.managed") {
    json = simulator::service_policy::evaluate(
        "reviewer", "managed", "same_tenant", "end_user_support_preview", "approved_read_only");
  } else if (command == L"app.update") {
    json = launch_update_helper()
        ? R"({"ok":true,"action":"update_started","network_scope":"github_releases_only"})"
        : R"({"ok":false,"action":"update_not_started","error":"update_helper_missing_or_unavailable"})";
  } else {
    json = "{\"ok\":false,\"decision\":\"deny\",\"error\":\"unsupported_read_only_fixture\"}";
  }
  const auto message = utf8_to_wide(json);
  browser->PostWebMessageAsString(message.c_str());
}

void error_message(const wchar_t* message) {
  MessageBoxW(window_handle, message, kTitle, MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_SIZE: resize_browser(); return 0;
    case WM_GETMINMAXINFO: return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_DESTROY:
      browser.Reset();
      if (controller) controller->Close();
      controller.Reset();
      environment.Reset();
      PostQuitMessage(0);
      return 0;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

void create_view() {
  const std::wstring assets = local_assets();
  if (assets.empty() || GetFileAttributesW((assets + L"\\index.html").c_str()) == INVALID_FILE_ATTRIBUTES) {
    error_message(L"Os arquivos de interface locais (ui/index.html) nao foram encontrados.");
    DestroyWindow(window_handle);
    return;
  }

  const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [assets](HRESULT status, ICoreWebView2Environment* created) -> HRESULT {
            if (FAILED(status) || !created) {
              error_message(L"WebView2 Runtime nao encontrado ou inicializacao falhou.");
              DestroyWindow(window_handle);
              return S_OK;
            }
            environment = created;
            created->CreateCoreWebView2Controller(
                window_handle,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [assets](HRESULT result, ICoreWebView2Controller* created_controller) -> HRESULT {
                      if (FAILED(result) || !created_controller) {
                        error_message(L"Nao foi possivel criar a WebView2.");
                        DestroyWindow(window_handle);
                        return S_OK;
                      }
                      controller = created_controller;
                      controller->get_CoreWebView2(&browser);
                      if (!browser) {
                        DestroyWindow(window_handle);
                        return S_OK;
                      }
                      ComPtr<ICoreWebView2_3> virtual_host;
                      if (FAILED(browser.As(&virtual_host)) ||
                          FAILED(virtual_host->SetVirtualHostNameToFolderMapping(
                              L"simulator.invalid", assets.c_str(),
                              COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS))) {
                        error_message(L"Mapeamento de recursos locais indisponivel.");
                        DestroyWindow(window_handle);
                        return S_OK;
                      }
                      ComPtr<ICoreWebView2Settings> settings;
                      browser->get_Settings(&settings);
                      if (settings) {
                        settings->put_AreDevToolsEnabled(FALSE);
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->put_IsWebMessageEnabled(TRUE);
                      }

                      EventRegistrationToken token{};
                      browser->add_NavigationStarting(
                          Callback<ICoreWebView2NavigationStartingEventHandler>(
                              [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                LPWSTR address = nullptr;
                                if (SUCCEEDED(args->get_Uri(&address))) {
                                  const bool permitted = address && is_local(address);
                                  CoTaskMemFree(address);
                                  if (!permitted) args->put_Cancel(TRUE);
                                } else {
                                  args->put_Cancel(TRUE);
                                }
                                return S_OK;
                              }).Get(), &token);
                      browser->add_NewWindowRequested(
                          Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                              [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                args->put_Handled(TRUE);
                                return S_OK;
                              }).Get(), &token);

                      browser->AddWebResourceRequestedFilter(
                          L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
                      browser->add_WebResourceRequested(
                          Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                              [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                                ComPtr<ICoreWebView2WebResourceRequest> request;
                                if (FAILED(args->get_Request(&request))) return S_OK;
                                LPWSTR address = nullptr;
                                if (FAILED(request->get_Uri(&address))) return S_OK;
                                const bool permitted = address && is_local(address);
                                CoTaskMemFree(address);
                                if (!permitted && environment) {
                                  ComPtr<ICoreWebView2WebResourceResponse> forbidden;
                                  if (SUCCEEDED(environment->CreateWebResourceResponse(
                                          nullptr, 403, L"Forbidden", L"Content-Type: text/plain",
                                          &forbidden))) {
                                    args->put_Response(forbidden.Get());
                                  }
                                }
                                return S_OK;
                              }).Get(), &token);
                      browser->add_WebMessageReceived(
                          Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                              [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                LPWSTR source = nullptr;
                                if (FAILED(args->get_Source(&source))) return S_OK;
                                const bool permitted = source && is_local(source);
                                CoTaskMemFree(source);
                                if (!permitted) return S_OK;
                                LPWSTR message = nullptr;
                                if (SUCCEEDED(args->TryGetWebMessageAsString(&message))) {
                                  if (message) send_fixture(message);
                                  CoTaskMemFree(message);
                                }
                                return S_OK;
                              }).Get(), &token);
                      resize_browser();
                      browser->Navigate(kHome);
                      return S_OK;
                    }).Get());
            return S_OK;
          }).Get());
  if (FAILED(hr)) {
    error_message(L"Nao foi possivel iniciar o WebView2. Verifique o Runtime.");
    DestroyWindow(window_handle);
  }
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"HostingerSimulatorOfflineWindow";
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
  wc.hIconSm = static_cast<HICON>(LoadImageW(
      instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (!RegisterClassExW(&wc)) { CoUninitialize(); return 1; }
  window_handle = CreateWindowExW(
      0, wc.lpszClassName, kTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
      nullptr, nullptr, instance, nullptr);
  if (!window_handle) { CoUninitialize(); return 1; }
  // STARTUPINFO can override the first ShowWindow request. Enforce maximized
  // on startup without removing the standard restore/minimize window controls.
  ShowWindow(window_handle, SW_SHOWMAXIMIZED);
  ShowWindow(window_handle, SW_SHOWMAXIMIZED);
  UpdateWindow(window_handle);
  create_view();
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  CoUninitialize();
  return static_cast<int>(message.wParam);
}
