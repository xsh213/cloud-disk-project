#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <functional>
#include <QString>

class HttpClient;

class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);
    ~ApiClient();

    // 登录：username + password -> 成功返回 token，失败返回错误信息
    void login(const QString &username, const QString &password,
               std::function<void(bool success, const QString &token, const QString &msg)> callback);

    // 注册
    void registerUser(const QString &username, const QString &password,
                      std::function<void(bool success, const QString &msg)> callback);

private:
    HttpClient *httpClient;
};

#endif // APICLIENT_H