#include "logos_module_client.h"
#include "proxy_api.h"
#include "logos_sdk_c.h"

// === C API Implementation (Thin Wrappers) ===

void logos_module_client_init(LogosModuleClientHost host) {
    ProxyAPI::init(host);
}

void logos_module_client_async_operation(const char* data, LogosClientCallback callback, void* user_data) {
    ProxyAPI::asyncOperation(data, callback, user_data);
}

void logos_module_client_load_plugin_async(const char* plugin_name, LogosClientCallback callback, void* user_data) {
    ProxyAPI::loadPluginAsync(plugin_name, callback, user_data);
}

void logos_module_client_call_method_async(const char* plugin_name, const char* method_name, const char* params_json, LogosClientCallback callback, void* user_data) {
    ProxyAPI::callPluginMethodAsync(plugin_name, method_name, params_json, callback, user_data);
}

void logos_module_client_register_event_listener(const char* plugin_name, const char* event_name, LogosClientCallback callback, void* user_data) {
    ProxyAPI::registerEventListener(plugin_name, event_name, callback, user_data);
}

int logos_module_client_get_event_listener_count(void) {
    return ProxyAPI::eventListeners().size();
}

void logos_module_client_clear_event_listeners(void) {
    ProxyAPI::clearEventListeners();
}

void logos_module_client_shutdown(void) {
    logos_sdk_shutdown();
    ProxyAPI::clearEventListeners();
}
