#include "proxy_api.h"
#include "logos_sdk_c.h"
#include <QDebug>
#include <QTimer>

static LogosModuleClientHost s_host = {};
static QList<EventListener> s_event_listeners;

namespace ProxyAPI {

    void init(LogosModuleClientHost host) {
        s_host = host;
    }

    void asyncOperation(const char* data, LogosClientCallback callback, void* user_data) {
        if (!callback) qFatal("asyncOperation: callback must not be null");

        QString inputData = data ? QString::fromUtf8(data) : QString("no data");
        qDebug() << "Starting async operation with data:" << inputData;

        QTimer* timer = new QTimer();
        timer->setSingleShot(true);
        timer->setInterval(2000);

        QObject::connect(timer, &QTimer::timeout, [=]() {
            qDebug() << "Async operation completed for data:" << inputData;
            QString resultMessage = QString("Async operation completed successfully for: %1").arg(inputData);
            QByteArray messageBytes = resultMessage.toUtf8();
            callback(1, messageBytes.constData(), user_data);
            timer->deleteLater();
        });

        timer->start();
        qDebug() << "Async operation timer started, will complete in 2 seconds";
    }

    void loadPluginAsync(const char* plugin_name, LogosClientCallback callback, void* user_data) {
        if (!callback) qFatal("loadPluginAsync: callback must not be null");

        if (!plugin_name) {
            qWarning() << "loadPluginAsync: plugin_name is null";
            callback(0, "Plugin name is null", user_data);
            return;
        }

        QString name = QString::fromUtf8(plugin_name);
        qDebug() << "Starting async plugin load for:" << name;

        if (s_host.is_plugin_known && !s_host.is_plugin_known(plugin_name)) {
            QString errorMsg = QString("Plugin not found among known plugins: %1").arg(name);
            QByteArray errorBytes = errorMsg.toUtf8();
            callback(0, errorBytes.constData(), user_data);
            return;
        }

        QTimer* timer = new QTimer();
        timer->setSingleShot(true);
        timer->setInterval(1000);

        QObject::connect(timer, &QTimer::timeout, [=]() {
            qDebug() << "Executing async plugin load for:" << name;
            bool success = false;
            if (s_host.load_plugin) {
                success = s_host.load_plugin(plugin_name);
            }

            QString resultMessage;
            if (success) {
                resultMessage = QString("Plugin '%1' loaded successfully").arg(name);
            } else {
                resultMessage = QString("Failed to load plugin '%1'").arg(name);
            }

            QByteArray messageBytes = resultMessage.toUtf8();
            callback(success ? 1 : 0, messageBytes.constData(), user_data);
            timer->deleteLater();
        });

        timer->start();
        qDebug() << "Async plugin load timer started for:" << name;
    }

    void callPluginMethodAsync(const char* plugin_name, const char* method_name, const char* params_json, LogosClientCallback callback, void* user_data) {
        if (!callback) qFatal("callPluginMethodAsync: callback must not be null");

        if (!plugin_name || !method_name) {
            qWarning() << "callPluginMethodAsync: plugin_name or method_name is null";
            callback(0, "Plugin name or method name is null", user_data);
            return;
        }

        if (s_host.is_plugin_loaded && !s_host.is_plugin_loaded(plugin_name)) {
            QString pluginNameStr = QString::fromUtf8(plugin_name);
            QString errorMsg = QString("Plugin not loaded: %1").arg(pluginNameStr);
            QByteArray errorBytes = errorMsg.toUtf8();
            callback(0, errorBytes.constData(), user_data);
            return;
        }

        logos_sdk_call_method_async(plugin_name, method_name, params_json, callback, user_data);
    }

    void registerEventListener(const char* plugin_name, const char* event_name, LogosClientCallback callback, void* user_data) {
        if (!callback) qFatal("registerEventListener: callback must not be null");

        if (!plugin_name || !event_name) {
            qWarning() << "registerEventListener: plugin_name or event_name is null";
            return;
        }

        QString pluginNameStr = QString::fromUtf8(plugin_name);
        QString eventNameStr = QString::fromUtf8(event_name);

        qDebug() << "Registering event listener for plugin:" << pluginNameStr << "event:" << eventNameStr;

        if (s_host.is_plugin_loaded && !s_host.is_plugin_loaded(plugin_name)) {
            qWarning() << "Cannot register event listener: Plugin not loaded:" << pluginNameStr;
            return;
        }

        EventListener listener;
        listener.pluginName = pluginNameStr;
        listener.eventName = eventNameStr;
        listener.callback = callback;
        listener.userData = user_data;
        s_event_listeners.append(listener);

        logos_sdk_register_event(plugin_name, event_name, callback, user_data);
    }

    const QList<EventListener>& eventListeners() {
        return s_event_listeners;
    }

    void clearEventListeners() {
        s_event_listeners.clear();
    }

}
