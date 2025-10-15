#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QPushButton>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <qstandardpaths.h>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onFetchStatsClicked);
    avatarLabel = ui->label_avatar;
    applyRoundedCorners();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onFetchStatsClicked()
{
    QString nickname = ui->lineEditIRL->text().trimmed();

    if (nickname.isEmpty())
    {
        return;
    }
    ui->label_NAME->setText(nickname);
    //ui->text_KR->setText
    QString apiUrl = QString("https://open.faceit.com/data/v4/players?nickname=%1&game=cs2").arg(nickname);
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    disconnect(networkManager, &QNetworkAccessManager::finished, nullptr, nullptr);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onRequestFinished);
    networkManager->get(request);
}

void MainWindow::onRequestFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        ui->text_elo->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        ui->text_elo->setText("Ошибка: некорректный JSON-ответ.");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QString playerId = jsonObj["player_id"].toString();
    if (playerId.isEmpty()) {
        ui->text_elo->setText("Не удалось найти игрока.");
        return;
    }

    currentPlayerId = playerId;




    // Выводим ELO
    if (jsonObj.contains("games") && jsonObj["games"].isObject())
    {
        QJsonObject games = jsonObj["games"].toObject();
        if (games.contains("cs2") && games["cs2"].isObject())
        {
            QJsonObject cs2 = games["cs2"].toObject();
            if (cs2.contains("faceit_elo"))
            {
                int elo = cs2["faceit_elo"].toInt();
                ui->text_elo->setText(QString("ELO: %1").arg(elo));
            }
        }
    }

    // Запрашиваем статистику игрока
    QString statsUrl = QString("https://open.faceit.com/data/v4/players/%1/stats/cs2").arg(playerId);
    QNetworkRequest statsRequest;
    statsRequest.setUrl(QUrl(statsUrl));
    statsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    disconnect(networkManager, &QNetworkAccessManager::finished, nullptr, nullptr);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onMatchHistoryFinished);
    networkManager->get(statsRequest);
}

void MainWindow::onMatchHistoryFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        ui->text_KD->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        ui->text_KD->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject root = jsonDoc.object();

    // Извлекаем K/R Ratio и Average K/D Ratio из lifetime
    if (root.contains("lifetime") && root["lifetime"].isObject())
    {
        QJsonObject lifetime = root["lifetime"].toObject();

        // Извлекаем ADR Ratio
        if (lifetime.contains("ADR"))
        {
            QString krRatioStr = lifetime["ADR"].toString();
            double krRatio = krRatioStr.toDouble();
            ui->text_KR->setText(QString("ADR: %1").arg(krRatio, 0, 'f', 2));
        }

        // Извлекаем Average K/D Ratio
        if (lifetime.contains("Average K/D Ratio"))
        {
            QString avgKDRatioStr = lifetime["Average K/D Ratio"].toString();
            double avgKDRatio = avgKDRatioStr.toDouble();
            ui->text_KD->setText(QString("KD: %1").arg(avgKDRatio, 0, 'f', 2));
        }

        // Извлекаем Matches
        if (lifetime.contains("Matches"))
        {
            QString avgKDRatioStr = lifetime["Matches"].toString();
            int Matches = avgKDRatioStr.toDouble();
            ui->text_Matches->setText(QString("Matches: %1").arg(Matches));
        }

    }


}




void MainWindow::applyRoundedCorners()
{
    QBitmap mask(size());
    QPainter painter(&mask);
    painter.fillRect(mask.rect(), Qt::white);
    painter.setBrush(Qt::black);
    painter.drawRoundedRect(mask.rect(), 10, 10); // 30 — радиус закругления
    setMask(mask);
}


