#ifndef PROXY_API_H
#define PROXY_API_H

#include "logos_module_client.h"
#include <QString>
#include <QList>

// Structure to store event listener information
struct EventListener {
    QString pluginName;
    QString eventName;
    LogosClientCallback callback;
    void* userData;
};

namespace ProxyAPI {
    // Initialize with host callbacks for state queries
    void init(LogosModuleClientHost host);

    void asyncOperation(const char* data, LogosClientCallback callback, void* user_data);

    // Async plugin loading with callback
    void loadPluginAsync(const char* plugin_name, LogosClientCallback callback, void* user_data);

    // Proxy method to call plugin methods remotely with async callback
    void callPluginMethodAsync(
        const char* plugin_name,
        const char* method_name,
        const char* params_json,
        LogosClientCallback callback,
        void* user_data
    );

    // Register an event listener for a specific event from a specific plugin
    void registerEventListener(
        const char* plugin_name,
        const char* event_name,
        LogosClientCallback callback,
        void* user_data
    );

    // Access the registered event listeners (for testing/inspection)
    const QList<EventListener>& eventListeners();

    // Clear all registered event listeners
    void clearEventListeners();
}

#endif // PROXY_API_H
