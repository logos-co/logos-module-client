# Logos Module Client Specification

## Overview

The Logos Module Client is the **caller side** of inter-module communication on the Logos platform. Logos applications are assembled from independently developed **modules** — process-isolated plugins that expose named, invokable methods and emit named events. The runtime publishes those modules; this library lets a host application, or a foreign-language binding, reach *into* them: invoke a named method on a named module, hand it typed parameters, receive the result through a completion callback, and subscribe to the events the module emits.

It exists so that the work of *calling a module* — marshaling parameters, gating the call on whether the target is loaded, routing it over the transport, delivering the result, and managing event subscriptions — is implemented once and offered through two parallel surfaces:

- a **high-level interface** for native (C++/Qt) hosts that want cached, connection-aware calls, and
- a **flat, stable foreign-function-interface (FFI) surface** so consumers written in other languages (e.g. JavaScript and Rust SDKs) can drive the exact same behavior over a simple C calling convention.

The library is purely a client. It owns no modules, hosts no runtime, and exposes no command-line program. It is linked into something that already has a running Logos runtime nearby and needs to talk to the modules in it.

### Where it sits

```
   foreign-language SDK bindings            native host application
   (JS SDK, Rust SDK, …)                    (C++/Qt)
            │                                        │
            │ flat C ABI                             │ high-level client
            ▼                                        ▼
   ┌────────────────────────────────────────────────────────────┐
   │                    Logos Module Client                       │
   │                                                              │
   │   ┌──────────────┐   ┌──────────────┐   ┌────────────────┐  │
   │   │  Proxy API   │   │  SDK client  │   │  high-level     │  │
   │   │  (gating +   │──▶│  facade      │   │  native client  │  │
   │   │  registry)   │   │  (marshal +  │   │  (cached        │  │
   │   │              │   │  dispatch)   │   │  connections)   │  │
   │   └──────────────┘   └──────┬───────┘   └───────┬─────────┘  │
   └─────────────────────────────┼───────────────────┼───────────┘
                                 │                   │
                                 ▼                   ▼
                        language-neutral       native developer
                        protocol transport     layer (per-module
                        (per-module client)    cached clients)
                                 │                   │
                                 ▼                   ▼
                    ┌──────────────────────────────────────────┐
                    │  process-isolated Logos modules           │
                    │  (methods + events, identity "core")      │
                    └──────────────────────────────────────────┘
```

Within the platform's layering (SDK / developer layer over the core runtime over the modules), this library is a **developer-layer convenience for the calling party**. It always acts under the fixed origin identity **`core`** — it speaks to modules from the host/core perspective rather than as a peer module.

## Domain Model

### Concepts

| Term | Definition |
|------|------------|
| **Module / plugin** | A process-isolated Logos plugin that exposes named, invokable methods and emits named events. The client addresses a module by its string name. |
| **Method call** | A request to run one named method on one named module, carrying an ordered list of parameters. Always **asynchronous**: the call returns immediately and the outcome arrives later via a callback. |
| **Event subscription** | A standing registration of interest in one named event from one named module. The listener's callback fires once per emission, for the lifetime of the process. |
| **Origin identity `core`** | The fixed source identity the client presents to every module it talks to. It marks the caller as the host/core, not a peer module. |
| **Completion callback** | The single way results and outcomes are delivered. It conveys an outcome flag (success / failure), a message string, and an opaque caller-supplied context value that is passed straight back through. |
| **Host-policy callbacks** | A small set of queries the embedding host supplies so the client can ask, before dispatching, "is this module known?" / "is this module loaded?" and request "load this module". |
| **Typed parameter** | One argument to a method call, described by a name, a value, and a type hint that says how to coerce the value before sending it. |
| **Parameter list** | The full set of typed parameters for one call, expressed (on the FFI surface) as a JSON array of typed-parameter objects. |
| **Event payload** | The data delivered with an event emission, surfaced to listeners as a JSON document naming the event and listing its data values. |

### The completion-callback contract

Every operation that produces an outcome reports it the same way, through a caller-provided callback. Two equivalent shapes exist for the two surfaces:

- **FFI surface** — the callback receives an integer outcome (`1` = success, `0` = failure), a human-readable message string, and the opaque context value the caller passed in. The context value is never inspected; it is round-tripped verbatim so the caller can correlate the completion with the call site.
- **Native surface** — the callback receives a boolean success flag and a message string.

The message string carries either the operation's result (on success) or a diagnostic (on failure). There is no separate error channel; the outcome flag plus the message string is the entire contract.

### Typed parameters and coercion

Foreign-language callers cannot rely on a shared static type system, so parameters are passed self-describingly: a list of objects, each carrying a **name**, a **value**, and a **type** hint. Before a call is dispatched, each value is coerced according to its hint:

| Type hint | Coerced to | Notes |
|-----------|-----------|-------|
| `string` / `QString` | text | The default behavior. |
| `int` / `integer` (and `uint` on the FFI surface) | integer | The whole value must be a valid integer. |
| `bool` / `boolean` | boolean | `true` (and, on the native surface, `1`) is true; `false` / `0` is false. |
| `double` / `float` | floating-point number | The whole value must be a valid number. |
| *anything else* | text | An unknown hint falls back to treating the value as a string. |

Coercion is **fail-fast** and happens before anything goes over the wire. If the parameter list is not valid JSON, the call fails immediately with a *JSON parse error* message. If a value cannot be coerced to its declared numeric type, the call fails immediately with an *Invalid parameter: &lt;name&gt;* message. A malformed call is never dispatched to a module — the failure is reported through the same completion callback as any other outcome.

### Result and event representation

Results and events are deliberately **stringly-typed** at the boundary, matching the historical semantics that existing consumers parse against:

- A **method result** is delivered as a plain string. A string result is returned *unquoted* (not as a JSON-quoted string); structured results are rendered as their JSON form; a null/empty result yields an empty message.
- An **event payload** is delivered as a JSON document of the shape `{"event":"<name>","data":["v1","v2",...]}`, where each data element is rendered as a string.

This is lossy by design: the client favors a stable, language-neutral string contract over preserving rich native types across the boundary.

## Features & Functional Requirements

The library provides the following capabilities. Each is offered through the FFI surface, the native surface, or both, as noted.

### 1. Asynchronous method invocation

The core capability: call a named method on a named module with an ordered parameter list, and receive the outcome via callback.

- **FFI:** *call method async* — takes the module name, method name, the parameter list as JSON, a completion callback, and an opaque context. After host-policy gating, it marshals the parameters and dispatches the call; the callback fires with success/failure and the result-or-diagnostic message.
- **Native:** *call method async* — same intent, expressed with native string types and a boolean-outcome callback. Parameters are still supplied in the same typed-parameter JSON form.

Functional requirements:
- A null module name or method name must fail through the callback rather than dispatch.
- A call to a module the host reports as **not loaded** must fail with a *Plugin not loaded: &lt;name&gt;* message and must not be dispatched.
- Parameter validation must fail fast (see *Typed parameters and coercion*).
- A transport-level dispatch failure must surface as a *Failed to dispatch method call* outcome.
- On success the callback's message is the result as a plain string.

### 2. Event subscription

Register a standing listener for a named event from a named module.

- **FFI:** *register event listener* — records the listener in an inspectable registry and subscribes for delivery. The callback fires once per emission with the event payload JSON.
- **Native:** *subscribe event* — subscribes and delivers each emission as event-payload JSON via the boolean-outcome callback.

Functional requirements:
- A listener may only be registered for a module the host reports as **loaded**; otherwise the registration is a logged no-op.
- Null module or event names are a no-op.
- Each emission delivers the event payload to the listener; subscriptions persist for the lifetime of the process (there is no unsubscribe — see *Limitations*).

### 3. Event-listener registry

The FFI surface maintains a registry of the listeners it has registered, for introspection and test support.

- *count event listeners* — returns how many listeners are currently registered.
- *clear event listeners* — drops all registered listeners from the registry.

### 4. Host-policy gating and initialization

The client does not decide on its own whether a module is loadable or loaded; it asks the embedding host. The host supplies three queries at initialization:

| Query | Question it answers |
|-------|---------------------|
| *is plugin loaded* | Is this module currently loaded and callable? |
| *is plugin known* | Has this module been discovered (and so could be loaded)? |
| *load plugin* | Load this module now; did it succeed? |

- **FFI:** *init* (with a struct of the three callbacks) and *init with callbacks* (the same, passed as three separate function pointers — friendlier for FFI binding generators).

Gating uses these to refuse calls to unloaded modules and to refuse loads of unknown modules before any work is dispatched.

### 5. Asynchronous module loading

*load plugin async* requests that a module be loaded.

- A null name fails through the callback.
- A module the host reports as **unknown** fails with a *Plugin not found among known plugins: &lt;name&gt;* message, without attempting a load.
- Otherwise the host's *load plugin* query is invoked and its success/failure is reported via the callback. (The current implementation interposes a short artificial delay before the load — see *Limitations*.)

### 6. Demonstration operation

*async operation* is an illustrative, self-contained async call: after a brief delay it reports a canned success message echoing the input. It exists to demonstrate the callback contract and is not a real platform operation (see *Limitations*).

### 7. Cached native connections

The native high-level client holds a single persistent connection context under identity `core` and hands out a **cached per-module client** on demand: asking for the client for a given module name twice returns the same client. Repeated calls and subscriptions to the same module reuse the connection rather than creating an ephemeral one per call.

### 8. Shutdown

*shutdown* tears the client down cleanly: it releases the cached per-module transport connections and clears the event-listener registry. It is safe to call repeatedly; connections are lazily re-created on the next call after a shutdown.

## Behavior & Contracts

### Diagnostic messages

The failure messages a caller can observe through the completion callback are part of the contract:

| Message | Meaning |
|---------|---------|
| `Plugin name or method name is null` | A required name argument was null. |
| `Plugin not loaded: <name>` | The target module is not currently loaded (gating refused the call). |
| `Plugin not found among known plugins: <name>` | A load was requested for a module the host doesn't know about. |
| `Plugin name is null` | A load was requested with a null module name. |
| `JSON parse error: …` | The parameter list was not valid JSON. |
| `Invalid parameter: <name>` | A parameter value could not be coerced to its declared numeric type. |
| `Failed to dispatch method call` | The call passed validation but the transport refused to dispatch it. |

On success, method-call callbacks carry the result string (the native surface prefixes it with `Method call successful. Result: …`); load callbacks carry a success confirmation.

### Outcome semantics

- The FFI outcome integer is `1` for success and `0` for failure, uniformly.
- The opaque context value is always passed back to the callback unchanged.
- A native method call that returns an *invalid* (empty) result reports failure with a *returned invalid result* message rather than a false success.

### Lifetime guarantees

- A per-module connection, once created, lives for the process lifetime until *shutdown* is called; the same module always maps to the same connection.
- An event subscription lives for the process lifetime; there is no per-subscription teardown short of process exit.
- *shutdown* is idempotent and restores the client to a state where the next call re-establishes connections lazily.

## Use Cases & Workflows

### Foreign-language consumer: call a module method

```
1. The host binding calls init (or init-with-callbacks), registering the three
   host-policy queries.
2. The binding calls "call method async" with:
     module = "math", method = "add",
     params = [{"name":"a","value":"10","type":"int"},
               {"name":"b","value":"20","type":"int"}]
3. Gating asks the host "is math loaded?".
     - not loaded  → callback(0, "Plugin not loaded: math", ctx); done.
     - loaded      → continue.
4. Parameters are coerced (a → 10, b → 20). On bad JSON or a bad value the
   callback fires with the corresponding diagnostic and nothing is dispatched.
5. The call is dispatched to the "math" module under identity "core".
6. When the module replies, the callback fires:
     callback(1, "30", ctx)        // result as a plain string
   or, on a transport failure:
     callback(0, "Failed to dispatch method call", ctx)
```

### Foreign-language consumer: subscribe to a module event

```
1. After init, the binding calls "register event listener" with
   module = "chat", event = "message_received", a callback, and a context.
2. Gating asks the host "is chat loaded?".
     - not loaded → logged no-op (nothing is registered).
     - loaded     → the listener is recorded in the registry and subscribed.
3. Each time chat emits "message_received", the callback fires with:
     callback(1, {"event":"message_received","data":["alice","hello"]}, ctx)
4. The subscription persists until the process exits or shutdown is called.
```

### Native host: cached calls and subscriptions

```
1. Construct the high-level client (one persistent connection context, identity "core").
2. callMethodAsync("storage", "load_config",
                   "[{\"name\":\"path\",\"value\":\"/etc/app.json\",\"type\":\"string\"}]",
                   [](bool ok, const QString& msg){ ... });
     - params parsed; on error the lambda gets (false, errorMessage).
     - a cached client for "storage" is obtained (created once, reused after).
     - on reply the lambda gets (true, "Method call successful. Result: …").
3. subscribeEvent("storage", "config_changed",
                  [](bool, const QString& json){ ... });   // fires per emission
4. Subsequent calls/subscriptions to "storage" reuse the same cached client.
```

### Module load

```
1. init with host-policy queries.
2. "load plugin async" for module "waku".
     - null name        → callback(0, "Plugin name is null", ctx).
     - host says unknown → callback(0, "Plugin not found among known plugins: waku", ctx).
     - otherwise         → host "load plugin" runs; callback reports 1/0 with a confirmation.
```

### Shutdown

```
1. "shutdown" releases all cached per-module connections and clears the listener registry.
2. The client is safe to use again afterward; the next call lazily re-creates connections.
```

## Limitations

- **No unsubscribe.** Event subscriptions and per-module connections live for the process lifetime. Connections are released only by *shutdown*; individual subscriptions cannot be torn down before process exit.
- **Stringly-typed boundary.** Method results return as plain strings (string results unquoted) and event payload elements are stringified. Rich native typing is not preserved across the boundary.
- **Null-callback handling.** Several operations treat a null callback as a fatal programming error (they abort) rather than reporting it as a recoverable failure.
- **Demonstration paths.** The *async operation* is a canned demo, and *load plugin async* interposes an artificial delay before invoking the host's load query; neither is intended as a model of a real high-throughput operation.
- **Caller side only.** This is a client library. It does not host modules, does not run a module runtime, exposes no command-line tool, and is not itself a packaged Logos module — it is consumed by higher-level SDK bindings.
