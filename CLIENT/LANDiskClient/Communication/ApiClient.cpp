#include "Communication/ApiClient.h"
#include "Communication/HttpClient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include "Utils/Config.h"

ApiClient::ApiClient(QObject *parent) : QObject(parent) {
    httpClient = new HttpClient(this);
}

ApiClient::~ApiClient() {
    // httpClient 会被 Qt 父子关系自动释放
}

void ApiClient::login(const QString &username, const QString &password,
                      std::function<void(bool, const QString &, const QString &)> callback) {
    QJsonObject obj;
    obj["username"] = username;
    obj["password"] = password;

    QByteArray data = QJsonDocument(obj).toJson();

    httpClient->post(Config::LOGIN_PATH, data, [callback](bool success, const QJsonObject &resp) {
        if (!success) {
            callback(false, "", "网络请求失败");
            return;
        }

        int code = resp["code"].toInt();
        QString msg = resp["msg"].toString();

        if (code == 0) {
            QString token = resp["data"].toObject()["token"].toString();
            callback(true, token, msg);
        } else {
            callback(false, "", msg);
        }
    });
}

void ApiClient::registerUser(const QString &username, const QString &password,
                             std::function<void(bool, const QString &)> callback) {
    QJsonObject obj;
    obj["username"] = username;
    obj["password"] = password;

    QByteArray data = QJsonDocument(obj).toJson();

    httpClient->post(Config::REGISTER_PATH, data, [callback](bool success, const QJsonObject &resp) {
        if (!success) {
            callback(false, "网络请求失败");
            return;
        }

        int code = resp["code"].toInt();
        QString msg = resp["msg"].toString();

        if (code == 0) {
            callback(true, msg);
        } else {
            callback(false, msg);
        }
    });
}