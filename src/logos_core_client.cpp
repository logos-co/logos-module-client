#include "logos_core_client.h"
#include "logos_api.h"
#include "logos_api_client.h"
#include "logos_object.h"
#include "logos_json_utils.h"
#include <QDebug>

LogosCoreClient::LogosCoreClient(QObject* parent)
    : QObject(parent)
    , m_api(new LogosAPI("core", this))
{
}

LogosCoreClient::~LogosCoreClient()
{
}

LogosAPIClient* LogosCoreClient::clientFor(const QString& pluginName)
{
    return m_api->getClient(pluginName);
}

void LogosCoreClient::callMethodAsync(const QString& pluginName,
                                       const QString& methodName,
                                       const QString& paramsJson,
                                       AsyncCallback callback)
{
    if (!callback) return;

    bool ok = false;
    QString errorMessage;
    QVariantList args = LogosJsonUtils::parseMethodParams(paramsJson, &ok, &errorMessage);

    if (!ok) {
        callback(false, errorMessage);
        return;
    }

    LogosAPIClient* client = m_api->getClient(pluginName);
    if (!client) {
        callback(false, QString("Failed to get client for plugin: %1").arg(pluginName));
        return;
    }

    client->invokeRemoteMethodAsync(
        pluginName, methodName, args,
        [callback, pluginName, methodName](QVariant result) {
            if (result.isValid()) {
                QString resultStr;
                if (result.canConvert<QString>()) {
                    resultStr = result.toString();
                } else {
                    resultStr = QString("Result of type: %1").arg(result.typeName());
                }
                callback(true, QString("Method call successful. Result: %1").arg(resultStr));
            } else {
                callback(false, QStringLiteral("Method call returned invalid result"));
            }
        });
}

QString LogosCoreClient::callMethodSync(const QString& pluginName,
                                         const QString& methodName,
                                         const QString& paramsJson)
{
    bool ok = false;
    QString errorMessage;
    QVariantList args = LogosJsonUtils::parseMethodParams(paramsJson, &ok, &errorMessage);

    if (!ok) {
        qWarning() << "LogosCoreClient::callMethodSync: failed to parse params:" << errorMessage;
        return QString();
    }

    LogosAPIClient* client = m_api->getClient(pluginName);
    if (!client) {
        qWarning() << "LogosCoreClient::callMethodSync: failed to get client for:" << pluginName;
        return QString();
    }

    QVariant result = client->invokeRemoteMethod(pluginName, methodName, args);

    if (!result.isValid()) {
        return QString();
    }

    if (result.canConvert<QString>()) {
        return result.toString();
    }

    return QString("Result of type: %1").arg(result.typeName());
}

void LogosCoreClient::subscribeEvent(const QString& pluginName,
                                      const QString& eventName,
                                      AsyncCallback callback)
{
    if (!callback) return;

    LogosAPIClient* client = m_api->getClient(pluginName);
    if (!client) {
        qWarning() << "LogosCoreClient: Failed to get client for event subscription:" << pluginName;
        return;
    }

    LogosObject* obj = client->requestObject(pluginName);
    if (!obj) {
        qWarning() << "LogosCoreClient: Failed to get object for event subscription:" << pluginName;
        return;
    }

    client->onEvent(obj, eventName,
        [callback](const QString& evName, const QVariantList& evData) {
            QString json = LogosJsonUtils::formatEventJson(evName, evData);
            callback(true, json);
        });
}
