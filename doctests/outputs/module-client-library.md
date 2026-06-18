# Validating logos-module-client as a Library

[`logos-module-client`](https://github.com/logos-co/logos-module-client) is the
**proxy API and SDK client library** that callers use to invoke Logos module methods
asynchronously. It provides a C API (`logos_module_client.h`), the high-level
`LogosCoreClient` C++ interface with cached connections, `LogosJsonUtils` for FFI
parameter (de)serialization, and the low-level `logos_sdk_c` FFI bridge to the SDK.

Because it is a *library*, the way to prove **this** commit works is to build the
library + headers it publishes and to build and run its own GoogleTest suite — which
exercises every part of the public surface against the SDK's in-process mock:

1. Build the test binary (`#logos-module-client-tests`) from the commit under test.
2. Build the library + headers package and list what it ships.
3. List the GoogleTest suites baked into the binary.
4. Run individual suites (`LogosJsonUtilsTest`, `LogosCoreClientTest`) and then the
   full suite, all under the headless `offscreen` Qt platform.

Every artifact is built from the commit under test, so a green run is direct evidence
that this change keeps the client library building and its async API behaving.

**What you'll build:** This commit of `logos-module-client` built as a library + headers package, plus its GoogleTest binary run end-to-end under a headless Qt platform.

**What you'll learn:**

- How `logos-module-client` exposes its library, headers, and test binary as flake outputs
- What public headers the library ships (`logos_module_client.h`, `LogosCoreClient`, …)
- How to list and run individual GoogleTest suites with `--gtest_list_tests` / `--gtest_filter`
- Why a Qt-based test binary runs headless with `QT_QPA_PLATFORM=offscreen`

## Prerequisites

- **Nix** with flakes enabled. Install from [nixos.org](https://nixos.org/download.html), then enable flakes:

```bash
mkdir -p ~/.config/nix
echo 'experimental-features = nix-command flakes' >> ~/.config/nix/nix.conf
```

Verify: `nix flake --help >/dev/null 2>&1 && echo "Flakes enabled"`

- A Linux or macOS machine. The test binary builds on `QCoreApplication` and runs headless via `QT_QPA_PLATFORM=offscreen` — no display required.

---

## Step 1: Build the test binary

The flake exposes the GoogleTest suite as `#logos-module-client-tests`. Build it
from the commit under test and link it as `./mc-tests`; the executable lands at
`./mc-tests/bin/module_client_tests`.

> The `` in the URL pins the build to the commit under test: the doc-test
> runner expands it to a concrete ref (locally this checkout's `HEAD` — see
> `run.sh`; in CI the commit being tested). With no pin it falls back to the latest
> `master`. Developing against a local checkout? Replace the GitHub reference with
> `.`, e.g. `nix build '.#logos-module-client-tests' -o mc-tests`.

### 1.1 Build #logos-module-client-tests

```bash
nix build 'github:logos-co/logos-module-client#logos-module-client-tests' -o mc-tests
```

The result symlink `./mc-tests` holds the GoogleTest binary at `bin/module_client_tests`.

---

## Step 2: Build the library + headers

The default package is the consumable library: the compiled `LogosCoreClient` /
C-API library joined with its public headers. Build it and list the headers a
downstream consumer would `#include`.

### 2.1 Build the default (library + headers) package

```bash
nix build 'github:logos-co/logos-module-client' -o mc
```

### 2.2 List the public headers

```bash
ls mc/include
```

These are the headers an FFI or C++ consumer links against — the C API
(`logos_module_client.h`) plus the `LogosCoreClient` / `LogosJsonUtils` surface.

---

## Step 3: Explore the test suites

The binary is built on `QCoreApplication`, so it runs without a display once the
Qt platform is set to `offscreen`. List the GoogleTest suites it contains.

### 3.1 List all tests

```bash
QT_QPA_PLATFORM=offscreen ./mc-tests/bin/module_client_tests --gtest_list_tests
```

---

## Step 4: Run individual suites

`--gtest_filter` runs a subset. Start with `LogosJsonUtilsTest` (the FFI JSON
parameter conversions) and `LogosCoreClientTest` (the high-level async API over the
SDK's in-process mock).

### 4.1 Run the JSON utils suite

```bash
QT_QPA_PLATFORM=offscreen ./mc-tests/bin/module_client_tests --gtest_filter='LogosJsonUtilsTest.*'
```

### 4.2 Run the LogosCoreClient suite

```bash
QT_QPA_PLATFORM=offscreen ./mc-tests/bin/module_client_tests --gtest_filter='LogosCoreClientTest.*'
```

---

## Step 5: Run the full suite

Finally, run every suite. A clean `[  PASSED  ]` with zero failures is the
end-to-end proof that this commit's client library behaves across its whole
surface — JSON utils, the proxy API, the C FFI bridge, and the high-level client.

### 5.1 Run all tests

```bash
QT_QPA_PLATFORM=offscreen ./mc-tests/bin/module_client_tests
```

---

## Recap

You validated this commit of `logos-module-client` as a consumable library:

| Artifact | Built from | Proves |
|---|---|---|
| `#logos-module-client-tests` | `packages.logos-module-client-tests` | the GoogleTest suite compiles and links |
| default package | `packages.default` (lib + headers) | the library + public headers are shippable |
| full `gtest` run | the binary above | the async client API behaves across its whole surface |

Because all of it is built from the commit under test, a green run is evidence the
client library still builds and its async method-call API still works.
