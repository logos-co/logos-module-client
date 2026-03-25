#ifndef LOGOS_MODULE_CLIENT_H
#define LOGOS_MODULE_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for async operations
typedef void (*LogosClientCallback)(int result, const char* message, void* user_data);

// Host callbacks for plugin state queries (provided by the host application)
typedef struct {
    int (*is_plugin_loaded)(const char* plugin_name);
    int (*is_plugin_known)(const char* plugin_name);
    int (*load_plugin)(const char* plugin_name);
} LogosModuleClientHost;

// Initialize the module client with host callbacks for state queries
void logos_module_client_init(LogosModuleClientHost host);

// Simple async operation example that uses a callback
void logos_module_client_async_operation(const char* data, LogosClientCallback callback, void* user_data);

// Async plugin loading with callback
void logos_module_client_load_plugin_async(const char* plugin_name, LogosClientCallback callback, void* user_data);

// Call a plugin method asynchronously
// params_json: JSON string containing array of {name, value, type} objects
void logos_module_client_call_method_async(
    const char* plugin_name,
    const char* method_name,
    const char* params_json,
    LogosClientCallback callback,
    void* user_data
);

// Register an event listener for a specific event from a specific plugin
void logos_module_client_register_event_listener(
    const char* plugin_name,
    const char* event_name,
    LogosClientCallback callback,
    void* user_data
);

// Get the number of registered event listeners
int logos_module_client_get_event_listener_count(void);

// Clear all registered event listeners
void logos_module_client_clear_event_listeners(void);

// Shut down the module client, releasing connections
void logos_module_client_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // LOGOS_MODULE_CLIENT_H
