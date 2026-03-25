# logos-module-client

Proxy API and SDK client library for calling Logos module methods asynchronously.

Provides:
- **C API** (`logos_module_client.h`) — async method calls, event listeners, plugin loading
- **LogosCoreClient** — C++ high-level async interface with cached connections
- **LogosJsonUtils** — JSON parameter parsing/serialization for FFI consumers
- **logos_sdk_c** — low-level C FFI bridge to the SDK

## Dependencies

- `logos-cpp-sdk` — core SDK (LogosAPI, LogosAPIClient, transports)
- Qt 6 (Core, RemoteObjects)

## Building

```bash
# Via workspace
ws build logos-module-client

# Standalone
nix build

# Tests
ws test logos-module-client
```
