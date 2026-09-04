#include "Communication/HttpClient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include "Utils/Config.h"

HttpClient::HttpClient(QObject* parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished,
        this, &HttpClient::onReplyFinished);
}

void HttpClient::setToken(const QString& token) {
    this->token = token;
}

void HttpClient::get(const QString& path,
    std::function<void(bool, const QJsonObject&)> callback) {
    QUrl url(Config::BASE_URL + path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }
    manager->get(request);
}

void HttpClient::post(const QString& path, const QByteArray& data,
    std::function<void(bool, const QJsonObject&)> callback) {
    QUrl url(Config::BASE_URL + path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }
    manager->post(request, data);
}

void HttpClient::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError) {
            qDebug() << "Response:" << doc.object();
        }
        else {
            qDebug() << "JSON Parse Error:" << err.errorString();
        }
    }
    else {
        qDebug() << "Network Error:" << reply->errorString();
    }
    reply->deleteLater();
}