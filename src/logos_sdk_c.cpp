#include "logos_sdk_c.h"
#include "logos_core_client.h"
#include <QString>
#include <QByteArray>
#include <cstdlib>
#include <cstring>

static LogosCoreClient* s_coreClient = nullptr;

static LogosCoreClient* ensureClient()
{
    if (!s_coreClient)
        s_coreClient = new LogosCoreClient();
    return s_coreClient;
}

char* logos_sdk_call_method_sync(
    const char* plugin_name,
    const char* method_name,
    const char* params_json)
{
    if (!plugin_name || !method_name) return nullptr;

    QString pluginStr = QString::fromUtf8(plugin_name);
    QString methodStr = QString::fromUtf8(method_name);
    QString paramsStr = params_json ? QString::fromUtf8(params_json) : QStringLiteral("[]");

    QString result = ensureClient()->callMethodSync(pluginStr, methodStr, paramsStr);
    if (result.isNull()) return nullptr;

    QByteArray bytes = result.toUtf8();
    char* out = static_cast<char*>(std::malloc(bytes.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, bytes.constData(), bytes.size() + 1);
    return out;
}

void logos_sdk_free_string(char* str)
{
    std::free(str);
}

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

    QString pluginStr = QString::fromUtf8(plugin_name);
    QString methodStr = QString::fromUtf8(method_name);
    QString paramsStr = params_json ? QString::fromUtf8(params_json) : QStringLiteral("[]");

    ensureClient()->callMethodAsync(pluginStr, methodStr, paramsStr,
        [callback, user_data](bool success, const QString& message) {
            QByteArray msgBytes = message.toUtf8();
            callback(success ? 1 : 0, msgBytes.constData(), user_data);
        });
}

void logos_sdk_register_event(
    const char* plugin_name,
    const char* event_name,
    LogosSdkCallback callback,
    void* user_data)
{
    if (!plugin_name || !event_name || !callback) return;

    QString pluginStr = QString::fromUtf8(plugin_name);
    QString eventStr = QString::fromUtf8(event_name);

    ensureClient()->subscribeEvent(pluginStr, eventStr,
        [callback, user_data](bool success, const QString& message) {
            QByteArray msgBytes = message.toUtf8();
            callback(success ? 1 : 0, msgBytes.constData(), user_data);
        });
}

void logos_sdk_shutdown()
{
    delete s_coreClient;
    s_coreClient = nullptr;
}
