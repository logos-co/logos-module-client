#include "logos_sdk_c.h"

// The logos_sdk_* C surface is now a thin back-compat facade over the
// language-neutral lp_* C ABI from logos-protocol. Existing FFI consumers
// (logos-rust-sdk and friends) keep linking the same symbols with the same
// semantics; new consumers should use lp_* directly (logos_protocol.h).
#include "logos_protocol.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace {

std::mutex& clientsMutex()
{
    static std::mutex m;
    return m;
}

// One lp_client per target plugin, created lazily and kept for the process
// lifetime (mirrors LogosCoreClient's cached connections). The historical
// origin identity of this facade is "core".
std::map<std::string, lp_client*>& clients()
{
    static std::map<std::string, lp_client*> c;
    return c;
}

lp_client* clientFor(const char* plugin_name)
{
    std::lock_guard<std::mutex> lock(clientsMutex());
    auto& map = clients();
    auto it = map.find(plugin_name);
    if (it != map.end()) return it->second;
    lp_client* client = lp_client_create(plugin_name, "core", nullptr, nullptr);
    if (client) map.emplace(plugin_name, client);
    return client;
}

// logos_sdk_* params arrive in the historical FFI shape
// [{"name":...,"value":...,"type":...}, ...]; lp_invoke takes a plain JSON
// array of values. Values may arrive natively typed or as strings with a
// coercion hint in "type".
//
// Validation mirrors the historical LogosJsonUtils::parseMethodParams
// semantics: malformed JSON and uncoercible typed values fail FAST through
// the callback ("JSON parse error: ..." / "Invalid parameter: <name>")
// instead of going out over the transport.
bool ffiParamsToJsonArray(const char* params_json, std::string* outArray,
                          std::string* error)
{
    nlohmann::json out = nlohmann::json::array();
    if (!params_json || !*params_json) { *outArray = out.dump(); return true; }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(params_json);
    } catch (const nlohmann::json::parse_error& e) {
        *error = std::string("JSON parse error: ") + e.what();
        return false;
    }
    if (!parsed.is_array()) { *outArray = out.dump(); return true; }

    for (const auto& entry : parsed) {
        if (!entry.is_object() || !entry.contains("value")) {
            out.push_back(entry);
            continue;
        }
        const nlohmann::json& value = entry["value"];
        const std::string type =
            entry.contains("type") && entry["type"].is_string()
                ? entry["type"].get<std::string>() : "";
        const std::string name =
            entry.contains("name") && entry["name"].is_string()
                ? entry["name"].get<std::string>() : "";
        if (value.is_string() && (type == "int" || type == "uint")) {
            try {
                size_t used = 0;
                const std::string& s = value.get_ref<const std::string&>();
                const long long n = std::stoll(s, &used);
                if (used != s.size()) throw std::invalid_argument(s);
                out.push_back(n);
                continue;
            } catch (...) {
                *error = "Invalid parameter: " + name;
                return false;
            }
        } else if (value.is_string() && (type == "double" || type == "float")) {
            try {
                size_t used = 0;
                const std::string& s = value.get_ref<const std::string&>();
                const double d = std::stod(s, &used);
                if (used != s.size()) throw std::invalid_argument(s);
                out.push_back(d);
                continue;
            } catch (...) {
                *error = "Invalid parameter: " + name;
                return false;
            }
        } else if (value.is_string() && type == "bool") {
            out.push_back(value.get<std::string>() == "true");
            continue;
        }
        out.push_back(value);
    }
    *outArray = out.dump();
    return true;
}

// Historical message semantics: the callback message is the result as a
// plain string (no JSON quoting for strings), matching the QVariant
// .toString() behavior consumers parse against.
std::string jsonToMessage(const char* json)
{
    if (!json) return std::string();
    nlohmann::json parsed = nlohmann::json::parse(json, nullptr,
                                                  /*allow_exceptions=*/false);
    if (parsed.is_discarded()) return json;
    if (parsed.is_string()) return parsed.get<std::string>();
    if (parsed.is_null()) return std::string();
    return parsed.dump();
}

struct AsyncCall {
    LogosSdkCallback callback;
    void* userData;
};

struct EventSubscription {
    LogosSdkCallback callback;
    void* userData;
};

} // namespace

void logos_sdk_call_method_async(
    const char* plugin_name,
    const char* method_name,
    const char* params_json,
    LogosSdkCallback callback,
    void* user_data)
{
    if (!callback) return;

    if (!plugin_name || !method_name) {
        callback(0, "Plugin name or method name is null", user_data);
        return;
    }

    lp_client* client = clientFor(plugin_name);
    if (!client) {
        callback(0, "Failed to create protocol client", user_data);
        return;
    }

    std::string args;
    std::string paramError;
    if (!ffiParamsToJsonArray(params_json, &args, &paramError)) {
        callback(0, paramError.c_str(), user_data);
        return;
    }

    auto* call = new AsyncCall{callback, user_data};
    const int rc = lp_invoke_async(
        client, method_name, args.c_str(), 0,
        [](int ok, const char* json, void* opaque) {
            auto* c = static_cast<AsyncCall*>(opaque);
            const std::string message = jsonToMessage(json);
            c->callback(ok ? 1 : 0, message.c_str(), c->userData);
            delete c;
        },
        call);
    if (rc != LP_OK) {
        delete call;
        callback(0, "Failed to dispatch method call", user_data);
    }
}

void logos_sdk_register_event(
    const char* plugin_name,
    const char* event_name,
    LogosSdkCallback callback,
    void* user_data)
{
    if (!plugin_name || !event_name || !callback) return;

    lp_client* client = clientFor(plugin_name);
    if (!client) return;

    // Subscription objects live for the process lifetime (the historical
    // facade had no unsubscribe either); the EventSubscription is owned by
    // the lp_subscribe callback context.
    auto* sub = new EventSubscription{callback, user_data};
    lp_subscribe(
        client, event_name,
        [](const char* /*event_name*/, const char* data_json, void* opaque) {
            auto* s = static_cast<EventSubscription*>(opaque);
            s->callback(1, data_json ? data_json : "[]", s->userData);
        },
        sub);
}

void logos_sdk_shutdown()
{
    std::lock_guard<std::mutex> lock(clientsMutex());
    for (auto& [name, client] : clients())
        lp_client_destroy(client);
    clients().clear();
}
