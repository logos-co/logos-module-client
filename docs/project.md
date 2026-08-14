# logos-module-client — Project Description

## Overview

`logos-module-client` is a shared C++17/Qt 6 library (`liblogos_module_client`) on the
Logos SDK / developer layer. It is the **caller side** of the Logos module system: it lets
a host application — or a foreign-language binding — invoke a named method on a named
plugin asynchronously, pass typed parameters as JSON, receive the result through a
callback, and register listeners for the events a plugin emits.

In the platform, Logos modules are process-isolated Qt plugins that expose
`Q_INVOKABLE` methods and emit events, communicating over Qt Remote Objects /
`logos-protocol` transports. This library does not implement the transport or the
module-host runtime — it sits above them and forwards calls into the SDK stack:

```
logos-js-sdk / logos-rust-sdk / logos-logoscore-cli   (FFI / C++ consumers)
        |
        v
logos-module-client  (this repo — Proxy API + SDK client surface)
        |
        |  C++ path                       C ABI / FFI path
        v                                 v
logos-qt-host (LogosAPI)                  logos-protocol (lp_* C ABI;
  — from logos-plugin-qt                    LogosAPIClient / LogosObject)
        \                                /
         \                              /
          v                            v
        logos-cpp-sdk  ->  nixpkgs (Qt 6, OpenSSL, Boost, nlohmann_json)
```

It deliberately offers **two layers** so both C++ and non-C++ consumers can use it:

- a high-level C++ class, `LogosCoreClient`, built on the Qt host runtime
  (`LogosAPI` from `logos-plugin-qt`'s `logos-qt-host`; `LogosAPIClient` /
  `LogosObject` come from `logos-protocol` underneath) with cached, persistent
  per-plugin connections; and
- a flat `extern "C"` ABI (`logos_module_client.h`, `logos_sdk_c.h`) that
  FFI consumers such as `logos-js-sdk` and `logos-rust-sdk` link the same symbols
  through.

The C ABI forwards into the internal `ProxyAPI` namespace, which enforces host-policy
checks (`is_plugin_loaded` / `is_plugin_known` through host-supplied callbacks) and then
delegates the actual transport call to the `logos_sdk_c` surface. The `logos_sdk_*`
functions are themselves a back-compatibility facade over the language-neutral `lp_*`
C ABI from `logos-protocol` (`logos_protocol.h`): each target plugin gets a lazily-created,
process-lifetime `lp_client` (origin identity `"core"`), method calls go out via
`lp_invoke_async` and events via `lp_subscribe`.

This is a **library**, not an executable: it produces a shared lib, an installed-headers
output, and a GoogleTest binary, and is consumed by higher-level SDK bindings.

### Place in the dependency chain

| Direction | Repos |
|-----------|-------|
| Depends on | `logos-cpp-sdk`, `logos-protocol`, `logos-plugin-qt`, `logos-nix` |
| Consumed by | `logos-logoscore-cli`, `logos-js-sdk`, `logos-rust-sdk` |

## Project Structure

```
logos-module-client/
├── CMakeLists.txt                # Top-level: Qt6/Qt5 find_package, GoogleTest
│                                 # (system or FetchContent v1.14.0), install rules
│                                 # for the lib + 5 public headers
├── README.md                     # Short project overview
├── flake.nix                     # Flake: inputs + packages (lib/include/tests +
│                                 # joined default) + checks.tests + devShell
├── flake.lock
├── .gitignore
├── src/
│   ├── CMakeLists.txt            # Builds the SHARED logos_module_client target;
│   │                             # resolves logos-protocol/logos-qt-host via
│   │                             # LOGOS_PROTOCOL_ROOT / LOGOS_QT_HOST_ROOT; AUTOMOC on
│   ├── logos_module_client.h     # Public C ABI: callback/host typedefs +
│   │                             # logos_module_client_* declarations (installed)
│   ├── logos_module_client.cpp   # Thin C-API wrappers forwarding to ProxyAPI / logos_sdk_*
│   ├── proxy_api.h               # ProxyAPI namespace + EventListener struct
│   ├── proxy_api.cpp             # Proxy layer: host-callback state, plugin
│   │                             # loaded/known gating, demo/load async via QTimer,
│   │                             # event-listener registry, delegates to logos_sdk_*
│   ├── logos_sdk_c.h             # Low-level C FFI bridge: LogosSdkCallback,
│   │                             # logos_sdk_call_method_async / register_event / shutdown
│   ├── logos_sdk_c.cpp           # Back-compat facade over logos-protocol's lp_* C ABI:
│   │                             # per-plugin lp_client cache (mutex-guarded),
│   │                             # {name,value,type} -> JSON coercion (nlohmann_json),
│   │                             # result-to-message conversion, lp_invoke_async / lp_subscribe
│   ├── logos_core_client.h       # LogosCoreClient QObject (high-level C++ async interface)
│   ├── logos_core_client.cpp     # LogosCoreClient impl over the Qt host runtime
│   │                             # (LogosAPI / LogosAPIClient / LogosObject)
│   ├── logos_json_utils.h        # LogosJsonUtils namespace (Qt-side JSON marshaling)
│   └── logos_json_utils.cpp      # Type coercion, QVariant<->JSON string, event JSON
├── tests/
│   ├── CMakeLists.txt            # Builds module_client_tests; wires the *_ROOT mock
│   │                             # include dirs; gtest_discover_tests
│   ├── test_main.cpp             # gtest entry point; constructs a QCoreApplication
│   ├── test_logos_json_utils.cpp # LogosJsonUtils unit tests
│   ├── test_proxy_api.cpp        # ProxyAPI tests with mock host callbacks
│   ├── test_logos_core_client.cpp# LogosCoreClient tests (LogosMockSetup)
│   └── test_logos_sdk_c.cpp      # logos_sdk_c facade tests (LogosMockSetup)
├── nix/
│   ├── default.nix               # Common config: pname/version 0.1.0, nativeBuildInputs,
│   │                             # buildInputs, cmakeFlags with the three *_ROOT vars
│   ├── build.nix                 # Shared build derivation (common + src)
│   ├── lib.nix                   # Extracts lib/ from the build derivation
│   ├── include.nix               # Installs headers + .cpp sources into include/
│   └── tests.nix                 # Builds module_client_tests; Linux patchelf rpath
├── docs/
│   ├── index.md                  # Docs index (links to spec + project)
│   ├── spec.md                   # Stack-agnostic specification
│   └── project.md                # This document
└── .github/
    └── workflows/
        └── ci.yml                # CI: on push/PR to master, nix build of checks.tests
```

## Technology Stack

| Component | Type | Purpose |
|-----------|------|---------|
| **C++17** | Language | Implementation language |
| **Qt 6** (Core, RemoteObjects) | Framework | Event loop, `QVariant`/`QJson*` marshaling, Remote Objects transport (linked PUBLIC) |
| **CMake 3.14+** + **Ninja** | Build system | Configure/build (`-GNinja`) |
| **GoogleTest** | Test framework | `module_client_tests` (system `find_package`, else FetchContent v1.14.0) |
| **nlohmann_json** | Library | JSON parse/coerce in the `logos_sdk_c` `lp_*` facade |
| **pkg-config** | Build tool | Dependency discovery |
| **Nix flakes** | Build/package | Reproducible builds; the only supported build path in-workspace |

### Logos / external dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| **[logos-cpp-sdk](https://github.com/logos-co/logos-cpp-sdk)** | Flake input | Core SDK; pins nixpkgs/Qt; transitively supplies Boost/OpenSSL/nlohmann_json via `propagatedBuildInputs`. CMake root `LOGOS_CPP_SDK_ROOT` |
| **[logos-plugin-qt](https://github.com/logos-co/logos-plugin-qt)** | Flake input | Qt **host runtime** (`packages.<sys>.logos-qt-host`): `LogosAPI`, used by `LogosCoreClient`. CMake target `logos-qt-host::logos_qt_host`; resolved via `LOGOS_QT_HOST_ROOT`. This runtime used to ship from `logos-qt-sdk` as `logos-qt-sdk::logos_qt_sdk`; it was the only thing this repo took from there, so that input is gone. `LogosAPIClient` / `LogosObject` are `logos-protocol` types, not host-runtime ones |
| **[logos-protocol](https://github.com/logos-co/logos-protocol)** | Flake input | Language-neutral `lp_*` C ABI + transports (`logos_protocol.h`) that `logos_sdk_c.cpp` facades over; also provides the mock-transport headers (`logos_mock.h`, `LogosMockSetup`) for tests. Resolved via `LOGOS_PROTOCOL_ROOT` |
| **[logos-nix](https://github.com/logos-co/logos-nix)** | Flake input | Nix helper flake; provides the nixpkgs pin (`nixpkgs.follows = "logos-nix/nixpkgs"`) |
| **Qt6 qtbase + qtremoteobjects** | nixpkgs | Linked PUBLIC. Listed explicitly because the SDK does **not** propagate Qt (qtbase's `qtPreHook` setup-hook ordering can't be guaranteed through propagation) |
| **OpenSSL + Boost** | nixpkgs (transitive) | Pulled in by the SDK's plain-C++ TCP+SSL transport; listed in the devShell and on the Linux test-binary rpath |

The flake wires `follows` so a single nixpkgs/SDK pin flows through every input
(`logos-cpp-sdk.inputs.logos-protocol.follows`, `logos-plugin-qt.inputs.{logos-nix,logos-protocol}.follows`).

## Components

### Public C ABI — `logos_module_client.h`

The stable flat `extern "C"` surface that FFI consumers link. `result` is `1` = success,
`0` = failure. The implementation in `logos_module_client.cpp` is a set of thin wrappers
that forward into `ProxyAPI` and `logos_sdk_*`.

**Types:**

```c
typedef void (*LogosClientCallback)(int result, const char* message, void* user_data);

typedef struct {
    int (*is_plugin_loaded)(const char* plugin_name);
    int (*is_plugin_known)(const char* plugin_name);
    int (*load_plugin)(const char* plugin_name);
} LogosModuleClientHost;
```

**Functions:**

| Function | Behavior |
|----------|----------|
| `void logos_module_client_init(LogosModuleClientHost host)` | Initialize with a struct of host state callbacks. Forwards to `ProxyAPI::init`. |
| `void logos_module_client_init_with_callbacks(is_plugin_loaded, is_plugin_known, load_plugin)` | FFI-friendly init taking the three function pointers individually instead of the struct. |
| `void logos_module_client_async_operation(const char* data, LogosClientCallback callback, void* user_data)` | Demo async op: after a 2s `QTimer`, invokes `callback(1, "Async operation completed successfully for: <data>", user_data)`. `qFatal` if callback is null. |
| `void logos_module_client_load_plugin_async(const char* plugin_name, LogosClientCallback callback, void* user_data)` | Async plugin load. Null `plugin_name` → `callback(0, "Plugin name is null")`. Unknown plugin (per `is_plugin_known`) → `callback(0, "Plugin not found among known plugins: <name>")`. Otherwise after a 1s timer calls `host.load_plugin` and reports `1`/`0`. |
| `void logos_module_client_call_method_async(const char* plugin_name, const char* method_name, const char* params_json, LogosClientCallback callback, void* user_data)` | Call a plugin method async. `params_json` = JSON array of `{name,value,type}`. Null plugin/method → `callback(0, "Plugin name or method name is null")`. Plugin-not-loaded (per `is_plugin_loaded`) → `callback(0, "Plugin not loaded: <name>")`. Else delegates to `logos_sdk_call_method_async`. |
| `void logos_module_client_register_event_listener(const char* plugin_name, const char* event_name, LogosClientCallback callback, void* user_data)` | Record the listener in the registry and subscribe via `logos_sdk_register_event`. No-op (warns) if plugin not loaded, or names null. |
| `int  logos_module_client_get_event_listener_count(void)` | Returns the number of registered event listeners (registry size). |
| `void logos_module_client_clear_event_listeners(void)` | Clear all registered event listeners. |
| `void logos_module_client_shutdown(void)` | Calls `logos_sdk_shutdown()` (releases `lp_client`s) and clears event listeners. |

### Proxy layer — `ProxyAPI` (`proxy_api.h` / `proxy_api.cpp`)

The internal namespace the public C API forwards to. Holds the host callbacks
(`static LogosModuleClientHost s_host`) and the event-listener registry
(`static QList<EventListener> s_event_listeners`).

```cpp
struct EventListener {
    QString pluginName;
    QString eventName;
    LogosClientCallback callback;
    void* userData;
};

namespace ProxyAPI {
    void init(LogosModuleClientHost host);
    void asyncOperation(const char* data, LogosClientCallback callback, void* user_data);
    void loadPluginAsync(const char* plugin_name, LogosClientCallback callback, void* user_data);
    void callPluginMethodAsync(const char* plugin_name, const char* method_name,
                               const char* params_json, LogosClientCallback callback, void* user_data);
    void registerEventListener(const char* plugin_name, const char* event_name,
                               LogosClientCallback callback, void* user_data);
    const QList<EventListener>& eventListeners();
    void clearEventListeners();
}
```

- `asyncOperation`, `loadPluginAsync`, `callPluginMethodAsync`, and
  `registerEventListener` all **`qFatal`** (abort) when given a null callback.
- Gating happens here: `callPluginMethodAsync` checks `s_host.is_plugin_loaded`
  before delegating; `loadPluginAsync` checks `s_host.is_plugin_known` before the
  1s timer; `registerEventListener` skips (warns) if `is_plugin_loaded` is false.

### Low-level C facade — `logos_sdk_c` (`logos_sdk_c.h` / `logos_sdk_c.cpp`)

A back-compat facade over `logos-protocol`'s `lp_*` C ABI. Maintains a process-lifetime
cache of one `lp_client` per target plugin (`std::map<std::string, lp_client*>`, guarded
by a `std::mutex`), each created with origin identity `"core"` via
`lp_client_create(plugin_name, "core", nullptr, nullptr)`.

```c
typedef void (*LogosSdkCallback)(int result, const char* message, void* user_data);

void logos_sdk_call_method_async(const char* plugin_name, const char* method_name,
                                 const char* params_json, LogosSdkCallback callback, void* user_data);
void logos_sdk_register_event(const char* plugin_name, const char* event_name,
                              LogosSdkCallback callback, void* user_data);
void logos_sdk_shutdown(void);
```

| Function | Behavior |
|----------|----------|
| `logos_sdk_call_method_async` | Null `callback` → no-op. Null names → `callback(0, "Plugin name or method name is null")`. `params_json` may be `NULL` (defaults to `[]`). Bad JSON → `callback(0, "JSON parse error: ...")`; uncoercible typed value → `callback(0, "Invalid parameter: <name>")`; `lp_invoke_async` dispatch failure (`rc != LP_OK`) → `callback(0, "Failed to dispatch method call")`. On success the message is the result as a **plain (unquoted) string**. |
| `logos_sdk_register_event` | No-op if any of plugin/event/callback is null. Subscribes via `lp_subscribe`; the callback fires with `(1, data_json or "[]", user_data)` on each emission. The subscription lives for the process lifetime (no unsubscribe). |
| `logos_sdk_shutdown` | Destroys all cached `lp_client`s (`lp_client_destroy`) and clears the map. Safe to call multiple times; clients are lazily recreated on the next call. |

**Parameter coercion (`ffiParamsToJsonArray`).** The `{name,value,type}` FFI shape is
converted to a plain JSON value array. Coercion rules:

- string `value` + type `int`/`uint` → integer (`std::stoll`, full-string consume required)
- string `value` + type `double`/`float` → double (`std::stod`, full-string consume required)
- string `value` + type `bool` → `value == "true"`
- otherwise the value is passed through unchanged

A value that fails integer/double parsing yields `"Invalid parameter: <name>"`.

### High-level C++ client — `LogosCoreClient` (`logos_core_client.h` / `.cpp`)

A `QObject` subclass that holds a single persistent `LogosAPI("core")` and reuses cached
client connections, rather than creating ephemeral `LogosAPI` instances per call.

```cpp
class LogosCoreClient : public QObject {
    Q_OBJECT
public:
    using AsyncCallback = std::function<void(bool success, const QString& message)>;

    explicit LogosCoreClient(QObject* parent = nullptr);
    ~LogosCoreClient();

    void callMethodAsync(const QString& pluginName, const QString& methodName,
                         const QString& paramsJson, AsyncCallback callback);
    void subscribeEvent(const QString& pluginName, const QString& eventName,
                        AsyncCallback callback);
    LogosAPIClient* clientFor(const QString& pluginName);
};
```

| Method | Behavior |
|--------|----------|
| `callMethodAsync` | Null callback is a no-op. Parses params via `LogosJsonUtils::parseMethodParams` (errors → `callback(false, err)`), gets/creates a `LogosAPIClient` via `m_api->getClient(pluginName)`, and `invokeRemoteMethodAsync`. On a valid result → `callback(true, "Method call successful. Result: <...>")`; an invalid `QVariant` → `callback(false, "Method call returned invalid result")`. |
| `subscribeEvent` | Null callback is a no-op. Gets a client and a `LogosObject` via `requestObject`, then `onEvent`; fires `callback(true, formatEventJson(name, data))` on each emission. |
| `clientFor` | Get (or lazily create) a cached `LogosAPIClient` for the named plugin via `LogosAPI::getClient`. The same plugin name returns the same client pointer. |

### JSON marshaling helpers — `LogosJsonUtils` (`logos_json_utils.h` / `.cpp`)

The Qt-path marshaling used by `LogosCoreClient`.

```cpp
namespace LogosJsonUtils {
    QVariant     jsonParamToVariant(const QJsonObject& param);
    QVariantList parseMethodParams(const QString& json, bool* ok = nullptr, QString* errorMessage = nullptr);
    QString      variantToJsonString(const QVariant& value);
    QString      formatEventJson(const QString& eventName, const QVariantList& data);
}
```

- **Coercion types** (`jsonParamToVariant`): `string`/`QString`, `int`/`integer`,
  `bool`/`boolean` (`"true"`/`"1"`, `"false"`/`"0"`), `double`/`float`. Unknown types
  fall back to string. An uncoercible value returns an invalid `QVariant`.
- **`parseMethodParams`** returns the converted `QVariantList`; on a JSON parse error
  sets `*errorMessage = "JSON parse error: <...>"`; on an invalid value sets
  `*errorMessage = "Invalid parameter: <name>"` and returns an empty list.
- **`formatEventJson`** produces `{"event":"<name>","data":["v1","v2",...]}` — every
  data element is stringified with `.toString()` inside quotes.

## Building and Testing

All builds and tests go through Nix flakes. Prefer the workspace `ws` CLI.

### Build

```bash
# Via the workspace CLI (recommended)
ws build logos-module-client
ws build logos-module-client --auto-local     # with local dependency overrides

# Standalone nix (builds default = lib + include joined)
nix build

# Individual flake outputs
nix build .#logos-module-client-lib           # shared library only
nix build .#logos-module-client-include        # headers (+ .cpp sources)
nix build .#logos-module-client-tests          # GoogleTest binary
```

Build artifacts:

| Output | Contents |
|--------|----------|
| `logos-module-client-lib` | `lib/liblogos_module_client.{so,dylib}` |
| `logos-module-client-include` | `include/` — the 5 public headers plus the `.cpp` sources for header-only consumers |
| `logos-module-client-tests` | `bin/module_client_tests` |
| `logos-module-client` / `default` | symlinkJoin of `lib` + `include` |

### Test

```bash
# Via the workspace CLI
ws test logos-module-client
ws test logos-module-client --auto-local

# Standalone — runs the flake check (gtest with QT_QPA_PLATFORM=offscreen, XML output)
nix build .#checks.x86_64-linux.tests --print-build-logs
nix flake check

# Run the built test binary directly
./result/bin/module_client_tests --gtest_output=xml:results.xml
```

The flake `checks.tests` runs `module_client_tests` with `QT_QPA_PLATFORM=offscreen`
(and `QT_PLUGIN_PATH` set to qtbase's plugin dir on Linux), writing
`test-results.xml`. Test coverage:

| Test file | Covers |
|-----------|--------|
| `test_logos_json_utils.cpp` | `LogosJsonUtils` — type coercion, `parseMethodParams`, `variantToJsonString`, `formatEventJson` |
| `test_proxy_api.cpp` | `ProxyAPI` with mock host callbacks — null handling, `EXPECT_DEATH` on null callback, loaded/known gating, event-listener registry, JSON pipeline, `user_data` passthrough |
| `test_logos_core_client.cpp` | `LogosCoreClient` with `LogosMockSetup` — successful/param calls, invalid JSON/param/result errors, client caching |
| `test_logos_sdk_c.cpp` | `logos_sdk_c` facade with `LogosMockSetup` — success/params/null/invalid-json paths, `user_data`, shutdown idempotency, recreate-after-shutdown |

The tests depend on the mock-transport headers (`logos_mock.h`, `LogosMockSetup`) provided
by `logos-protocol` / `logos-cpp-sdk` via the `*_ROOT` include dirs — they are not vendored
in this repo.

### Develop

```bash
nix develop      # cmake, ninja, pkg-config, Qt6 (Core/RemoteObjects),
                 # gtest, openssl, boost, nlohmann_json
```

> The CMake config hard-errors (`FATAL_ERROR`) if `LOGOS_PROTOCOL_ROOT` /
> `LOGOS_QT_HOST_ROOT` do not point at built packages, so a raw `cmake ..` outside the
> Nix build will fail at configure time. Build through `ws build` / `nix build`.

### Continuous Integration

`.github/workflows/ci.yml` runs on every push/PR to `master`: it installs Nix
(`cachix/install-nix-action@v27`) + Cachix (`logos-co`), then runs:

```bash
nix build .#checks.x86_64-linux.tests --print-build-logs
```

## Examples

### FFI parameter JSON

The `{name,value,type}` array consumers pass as `params_json`:

```json
[
  {"name": "arg0", "value": "hello", "type": "string"},
  {"name": "arg1", "value": "42",    "type": "int"},
  {"name": "arg2", "value": "true",  "type": "bool"},
  {"name": "arg3", "value": "3.14",  "type": "double"}
]
```

### Event JSON delivered to listeners

```json
{"event": "myEvent", "data": ["hello", "42"]}
```

### C ABI call flow

```c
#include "logos_module_client.h"

static int  is_loaded(const char* name) { /* host state */ return 1; }
static int  is_known(const char* name)  { return 1; }
static int  do_load(const char* name)   { return 1; }

static void on_done(int result, const char* message, void* user_data) {
    /* result: 1 = success, 0 = failure */
    printf("[%d] %s\n", result, message);
}

int main(void) {
    logos_module_client_init_with_callbacks(is_loaded, is_known, do_load);

    const char* params = "[{\"name\":\"x\",\"value\":\"42\",\"type\":\"int\"}]";
    logos_module_client_call_method_async("math", "square", params, on_done, NULL);

    /* ... drive a Qt event loop so the async result is delivered ... */

    logos_module_client_shutdown();
    return 0;
}
```

### High-level C++ call

```cpp
LogosCoreClient client;
client.callMethodAsync(
    "math", "add",
    R"([{"name":"a","value":"10","type":"int"},{"name":"b","value":"20","type":"int"}])",
    [&](bool success, const QString& message) {
        // success == true, message == "Method call successful. Result: 30"
    });
```

### Test-style usage (with the protocol mock)

```cpp
m_mock->when("mod", "fn").thenReturn(QVariant("ok"));
logos_sdk_call_method_async("mod", "fn", "[]", testCCallback, nullptr);
// drive the event loop so the async callback fires
for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
```

### Error messages surfaced through the callback

`"JSON parse error: ..."`, `"Invalid parameter: <name>"`, `"Plugin not loaded: <name>"`,
`"Plugin not found among known plugins: <name>"`, `"Failed to dispatch method call"`,
`"Plugin name or method name is null"`, `"Method call returned invalid result"`.

## Known Limitations

- **No unsubscribe.** Both the `lp_subscribe` `EventSubscription` and each per-plugin
  `lp_client` live for the process lifetime; clients are released only by
  `logos_sdk_shutdown` / `logos_module_client_shutdown`.
- **`logos_module_client_async_operation` is a demo.** A fixed 2s `QTimer` with a canned
  success message — not a real operation.
- **Null callbacks abort.** `asyncOperation`, `loadPluginAsync`, `callPluginMethodAsync`,
  and `registerEventListener` `qFatal` (abort) on a null callback rather than returning an
  error. (The lower-level `logos_sdk_call_method_async` and `LogosCoreClient::callMethodAsync`
  instead treat a null callback as a no-op.)
- **Artificial load delay.** `loadPluginAsync` waits on a 1s timer before invoking
  `host.load_plugin`.
- **Result marshaling is stringly-typed/lossy.** Results return to callbacks as plain
  strings (string results unquoted), and `formatEventJson` stringifies every data element
  with `.toString()` inside quotes — there is no real typing of returned values.
- **Configures only inside Nix.** `AUTOMOC` is on and `src/CMakeLists.txt` hard-errors if
  `LOGOS_PROTOCOL_ROOT` / `LOGOS_QT_HOST_ROOT` are not built packages, so it effectively only
  configures within the Nix build.
- **Tests need external mock headers.** `logos_mock.h` / `LogosMockSetup` come from
  `logos-protocol` / `logos-cpp-sdk` via the `*_ROOT` include dirs; they are not vendored.
- **Not a packaged module / not a Python lib.** No `metadata.json`, no standalone
  executable, no `pyproject.toml` — it is a library consumed by SDK bindings.
- **Version pinned at `0.1.0`** in `nix/default.nix`.
