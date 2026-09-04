#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <functional>

class HttpClient : public QObject {
    Q_OBJECT

public:
    explicit HttpClient(QObject* parent = nullptr);

    // 设置 Token（登录成功后调用）
    void setToken(const QString& token);

    // GET 请求
    void get(const QString& path,
        std::function<void(bool success, const QJsonObject& data)> callback);

    // POST 请求
    void post(const QString& path,
        const QByteArray& data,
        std::function<void(bool success, const QJsonObject& data)> callback);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* manager;
    QString token;
};

#endif
