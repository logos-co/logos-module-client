#include <gtest/gtest.h>
#include "proxy_api.h"
#include "logos_module_client.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <cstring>
#include "logos_json_utils.h"

// ── Mock host state ─────────────────────────────────────────────────────────

static QStringList s_mock_loaded_plugins;
static QHash<QString, QString> s_mock_known_plugins;
static bool s_mock_load_result = false;

static int mock_is_plugin_loaded(const char* name) {
    return s_mock_loaded_plugins.contains(QString::fromUtf8(name)) ? 1 : 0;
}

static int mock_is_plugin_known(const char* name) {
    return s_mock_known_plugins.contains(QString::fromUtf8(name)) ? 1 : 0;
}

static int mock_load_plugin(const char* name) {
    Q_UNUSED(name);
    return s_mock_load_result ? 1 : 0;
}

// ── Callback helpers ────────────────────────────────────────────────────────

static bool s_callback_called = false;
static int s_callback_success = -1;
static QString s_callback_message;

void testCallback(int success, const char* message, void* user_data) {
    Q_UNUSED(user_data);
    s_callback_called = true;
    s_callback_success = success;
    s_callback_message = message ? QString::fromUtf8(message) : QString();
}

// ── Test fixture ────────────────────────────────────────────────────────────

class ProxyAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        s_mock_loaded_plugins.clear();
        s_mock_known_plugins.clear();
        s_mock_load_result = false;

        LogosModuleClientHost host;
        host.is_plugin_loaded = mock_is_plugin_loaded;
        host.is_plugin_known = mock_is_plugin_known;
        host.load_plugin = mock_load_plugin;
        logos_module_client_init(host);

        ProxyAPI::clearEventListeners();

        s_callback_called = false;
        s_callback_success = -1;
        s_callback_message.clear();
    }

    void TearDown() override {
        for (int i = 0; i < 10; ++i)
            QCoreApplication::processEvents();

        ProxyAPI::clearEventListeners();
        s_mock_loaded_plugins.clear();
        s_mock_known_plugins.clear();

        s_callback_called = false;
        s_callback_success = -1;
        s_callback_message.clear();
    }
};

// =============================================================================
// asyncOperation Tests
// =============================================================================

TEST_F(ProxyAPITest, AsyncOperation_HandlesNullData) {
    auto noop = [](int, const char*, void*) {};
    EXPECT_NO_THROW(ProxyAPI::asyncOperation(nullptr, noop, nullptr));
}

TEST_F(ProxyAPITest, AsyncOperation_AcceptsValidData) {
    auto noop = [](int, const char*, void*) {};
    EXPECT_NO_THROW(ProxyAPI::asyncOperation("test data", noop, nullptr));
}

// =============================================================================
// loadPluginAsync Tests
// =============================================================================

TEST_F(ProxyAPITest, LoadPluginAsync_AssertsWithNullCallback) {
    EXPECT_DEATH(ProxyAPI::loadPluginAsync("test_plugin", nullptr, nullptr), "");
}

TEST_F(ProxyAPITest, LoadPluginAsync_FailsWithNullPluginName) {
    ProxyAPI::loadPluginAsync(nullptr, testCallback, nullptr);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_callback_success, 0);
    EXPECT_FALSE(s_callback_message.isEmpty());
}

TEST_F(ProxyAPITest, LoadPluginAsync_FailsForUnknownPlugin) {
    ProxyAPI::loadPluginAsync("nonexistent_plugin", testCallback, nullptr);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_callback_success, 0);
    EXPECT_TRUE(s_callback_message.contains("not found"));
}

TEST_F(ProxyAPITest, LoadPluginAsync_AcceptsKnownPlugin) {
    s_mock_known_plugins.insert("test_plugin", "/path/to/plugin");

    auto noop = [](int, const char*, void*) {};
    EXPECT_NO_THROW(ProxyAPI::loadPluginAsync("test_plugin", noop, nullptr));
    EXPECT_FALSE(s_callback_called);
}

// =============================================================================
// callPluginMethodAsync Tests
// =============================================================================

TEST_F(ProxyAPITest, CallPluginMethodAsync_AssertsWithNullCallback) {
    EXPECT_DEATH(ProxyAPI::callPluginMethodAsync("plugin", "method", "[]", nullptr, nullptr), "");
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_FailsWithNullPluginName) {
    ProxyAPI::callPluginMethodAsync(nullptr, "method", "[]", testCallback, nullptr);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_callback_success, 0);
    EXPECT_FALSE(s_callback_message.isEmpty());
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_FailsWithNullMethodName) {
    ProxyAPI::callPluginMethodAsync("plugin", nullptr, "[]", testCallback, nullptr);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_callback_success, 0);
    EXPECT_FALSE(s_callback_message.isEmpty());
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_FailsForUnloadedPlugin) {
    ProxyAPI::callPluginMethodAsync("unloaded_plugin", "method", "[]", testCallback, nullptr);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_callback_success, 0);
    EXPECT_TRUE(s_callback_message.contains("not loaded"));
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_AcceptsLoadedPlugin) {
    s_mock_loaded_plugins.append("test_plugin");

    EXPECT_NO_THROW(ProxyAPI::callPluginMethodAsync("test_plugin", "testMethod", "[]", testCallback, nullptr));

    if (s_callback_called) {
        EXPECT_FALSE(s_callback_message.contains("not loaded"));
        EXPECT_FALSE(s_callback_message.contains("null"));
    }
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_HandlesNullParamsJson) {
    s_mock_loaded_plugins.append("test_plugin");

    EXPECT_NO_THROW(ProxyAPI::callPluginMethodAsync("test_plugin", "testMethod", nullptr, testCallback, nullptr));
}

// =============================================================================
// registerEventListener Tests
// =============================================================================

TEST_F(ProxyAPITest, RegisterEventListener_DoesNotCrashWithNullNames) {
    EXPECT_NO_THROW(ProxyAPI::registerEventListener(nullptr, "event", testCallback, nullptr));
    EXPECT_NO_THROW(ProxyAPI::registerEventListener("plugin", nullptr, testCallback, nullptr));
}

TEST_F(ProxyAPITest, RegisterEventListener_DoesNotRegisterForUnloadedPlugin) {
    ProxyAPI::registerEventListener("unloaded_plugin", "test_event", testCallback, nullptr);

    EXPECT_EQ(ProxyAPI::eventListeners().size(), 0);
}

TEST_F(ProxyAPITest, RegisterEventListener_AddsToEventListenersListForLoadedPlugin) {
    s_mock_loaded_plugins.append("test_plugin");

    ProxyAPI::registerEventListener("test_plugin", "test_event", testCallback, nullptr);

    const auto& listeners = ProxyAPI::eventListeners();
    ASSERT_EQ(listeners.size(), 1);
    EXPECT_EQ(listeners[0].pluginName.toStdString(), "test_plugin");
    EXPECT_EQ(listeners[0].eventName.toStdString(), "test_event");
    EXPECT_EQ(listeners[0].callback, testCallback);
}

TEST_F(ProxyAPITest, RegisterEventListener_CanRegisterMultipleListeners) {
    s_mock_loaded_plugins.append("plugin1");
    s_mock_loaded_plugins.append("plugin2");

    ProxyAPI::registerEventListener("plugin1", "event1", testCallback, nullptr);
    ProxyAPI::registerEventListener("plugin2", "event2", testCallback, nullptr);
    ProxyAPI::registerEventListener("plugin1", "event2", testCallback, nullptr);

    EXPECT_EQ(ProxyAPI::eventListeners().size(), 3);
}

TEST_F(ProxyAPITest, RegisterEventListener_StoresUserData) {
    s_mock_loaded_plugins.append("test_plugin");

    int userData = 42;

    ProxyAPI::registerEventListener("test_plugin", "test_event", testCallback, &userData);

    const auto& listeners = ProxyAPI::eventListeners();
    ASSERT_EQ(listeners.size(), 1);
    EXPECT_EQ(listeners[0].userData, &userData);
}

// =============================================================================
// Full JSON Array Pipeline Tests
// =============================================================================

TEST_F(ProxyAPITest, JsonArrayPipeline_MixedTypes) {
    QString paramsJson = R"([
        {"name":"arg0","value":"hello","type":"string"},
        {"name":"arg1","value":"42","type":"int"},
        {"name":"arg2","value":"true","type":"bool"},
        {"name":"arg3","value":"3.14","type":"double"}
    ])";

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(paramsJson.toUtf8(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);

    QJsonArray paramsArray = jsonDoc.array();
    QVariantList args;
    for (const QJsonValue& paramValue : paramsArray) {
        ASSERT_TRUE(paramValue.isObject());
        QJsonObject paramObj = paramValue.toObject();
        QVariant variant = LogosJsonUtils::jsonParamToVariant(paramObj);
        ASSERT_TRUE(variant.isValid()) << "Failed for param: " << paramObj.value("name").toString().toStdString();
        args.append(variant);
    }

    ASSERT_EQ(args.size(), 4);
    EXPECT_EQ(args[0].toString(), "hello");
    EXPECT_EQ(args[1].toInt(), 42);
    EXPECT_EQ(args[2].toBool(), true);
    EXPECT_NEAR(args[3].toDouble(), 3.14, 0.001);
}

TEST_F(ProxyAPITest, JsonArrayPipeline_EmptyArray) {
    QString paramsJson = "[]";

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(paramsJson.toUtf8(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);

    QJsonArray paramsArray = jsonDoc.array();
    QVariantList args;
    for (const QJsonValue& paramValue : paramsArray) {
        if (paramValue.isObject()) {
            args.append(LogosJsonUtils::jsonParamToVariant(paramValue.toObject()));
        }
    }

    EXPECT_EQ(args.size(), 0);
}

TEST_F(ProxyAPITest, JsonArrayPipeline_SingleStringParam) {
    QString paramsJson = R"([{"name":"arg0","value":"test message","type":"string"}])";

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(paramsJson.toUtf8(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);

    QJsonArray paramsArray = jsonDoc.array();
    ASSERT_EQ(paramsArray.size(), 1);

    QVariant result = LogosJsonUtils::jsonParamToVariant(paramsArray[0].toObject());
    ASSERT_TRUE(result.isValid());
    EXPECT_EQ(result.toString(), "test message");
}

TEST_F(ProxyAPITest, JsonArrayPipeline_InvalidJson) {
    QString paramsJson = "not valid json at all";

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(paramsJson.toUtf8(), &parseError);
    EXPECT_NE(parseError.error, QJsonParseError::NoError);
}

TEST_F(ProxyAPITest, JsonArrayPipeline_InvalidParamInArray) {
    QString paramsJson = R"([
        {"name":"arg0","value":"hello","type":"string"},
        {"name":"arg1","value":"not_a_number","type":"int"}
    ])";

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(paramsJson.toUtf8(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);

    QJsonArray paramsArray = jsonDoc.array();
    bool hadInvalid = false;
    QVariantList args;
    for (const QJsonValue& paramValue : paramsArray) {
        if (paramValue.isObject()) {
            QVariant variant = LogosJsonUtils::jsonParamToVariant(paramValue.toObject());
            if (!variant.isValid()) {
                hadInvalid = true;
                break;
            }
            args.append(variant);
        }
    }

    EXPECT_TRUE(hadInvalid);
    EXPECT_EQ(args.size(), 1);
    EXPECT_EQ(args[0].toString(), "hello");
}

// =============================================================================
// callPluginMethodAsync JSON Error Handling Tests
// =============================================================================

TEST_F(ProxyAPITest, CallPluginMethodAsync_ReportsJsonParseError) {
    s_mock_loaded_plugins.append("test_plugin");

    static bool jsonErrorCalled = false;
    static int jsonErrorSuccess = -1;
    static QString jsonErrorMessage;
    jsonErrorCalled = false;
    jsonErrorSuccess = -1;
    jsonErrorMessage.clear();

    auto jsonErrorCallback = [](int success, const char* message, void* /*user_data*/) {
        jsonErrorCalled = true;
        jsonErrorSuccess = success;
        jsonErrorMessage = message ? QString::fromUtf8(message) : QString();
    };

    ProxyAPI::callPluginMethodAsync("test_plugin", "method", "invalid json!", jsonErrorCallback, nullptr);

    for (int i = 0; i < 50; ++i)
        QCoreApplication::processEvents();

    EXPECT_TRUE(jsonErrorCalled);
    EXPECT_EQ(jsonErrorSuccess, 0);
    EXPECT_TRUE(jsonErrorMessage.contains("JSON parse error"));
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_ReportsInvalidParamError) {
    s_mock_loaded_plugins.append("test_plugin");

    static bool paramErrorCalled = false;
    static int paramErrorSuccess = -1;
    static QString paramErrorMessage;
    paramErrorCalled = false;
    paramErrorSuccess = -1;
    paramErrorMessage.clear();

    auto paramErrorCallback = [](int success, const char* message, void* /*user_data*/) {
        paramErrorCalled = true;
        paramErrorSuccess = success;
        paramErrorMessage = message ? QString::fromUtf8(message) : QString();
    };

    QString paramsJson = R"([{"name":"arg0","value":"not_a_number","type":"int"}])";
    ProxyAPI::callPluginMethodAsync("test_plugin", "method", paramsJson.toUtf8().constData(), paramErrorCallback, nullptr);

    for (int i = 0; i < 50; ++i)
        QCoreApplication::processEvents();

    EXPECT_TRUE(paramErrorCalled);
    EXPECT_EQ(paramErrorSuccess, 0);
    EXPECT_TRUE(paramErrorMessage.contains("Invalid parameter"));
}

// =============================================================================
// User data passthrough test
// =============================================================================

static void* s_received_user_data = nullptr;
static void userDataCallback(int success, const char* message, void* user_data) {
    s_callback_called = true;
    s_callback_success = success;
    s_callback_message = message ? QString::fromUtf8(message) : QString();
    s_received_user_data = user_data;
}

TEST_F(ProxyAPITest, CallPluginMethodAsync_PassesThroughUserData) {
    s_received_user_data = nullptr;

    int myData = 123;
    ProxyAPI::callPluginMethodAsync(nullptr, "method", "[]", userDataCallback, &myData);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_received_user_data, &myData);
}

TEST_F(ProxyAPITest, LoadPluginAsync_PassesThroughUserData) {
    s_received_user_data = nullptr;

    int myData = 456;
    ProxyAPI::loadPluginAsync(nullptr, userDataCallback, &myData);

    EXPECT_TRUE(s_callback_called);
    EXPECT_EQ(s_received_user_data, &myData);
}
