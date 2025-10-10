#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QPushButton>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onFetchStatsClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onFetchStatsClicked()
{
    QString nickname = ui->lineEditIRL->text().trimmed();
    if (nickname.isEmpty()) {
        return;
    }

    QString apiUrl = QString("https://open.faceit.com/data/v4/players?nickname=%1&game=cs2").arg(nickname);
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    disconnect(networkManager, &QNetworkAccessManager::finished, nullptr, nullptr);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onRequestFinished);
    networkManager->get(request);
}

void MainWindow::onRequestFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        ui->label_elo->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        ui->label_elo->setText("Ошибка: некорректный JSON-ответ.");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QString playerId = jsonObj["player_id"].toString();
    if (playerId.isEmpty()) {
        ui->label_elo->setText("Не удалось найти игрока.");
        return;
    }

    currentPlayerId = playerId;

    // Выводим ELO
    if (jsonObj.contains("games") && jsonObj["games"].isObject()) {
        QJsonObject games = jsonObj["games"].toObject();
        if (games.contains("cs2") && games["cs2"].isObject()) {
            QJsonObject cs2 = games["cs2"].toObject();
            if (cs2.contains("faceit_elo")) {
                int elo = cs2["faceit_elo"].toInt();
                ui->label_elo->setText(QString("ELO: %1").arg(elo));
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

void MainWindow::onMatchHistoryFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        ui->label_KD->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        ui->label_KD->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject root = jsonDoc.object();

    // Извлекаем общие убийства из lifetime
    double totalKills = 0;
    if (root.contains("lifetime") && root["lifetime"].isObject()) {
        QJsonObject lifetime = root["lifetime"].toObject();
        if (lifetime.contains("Total Kills with extended stats")) {
            totalKills = lifetime["Total Kills with extended stats"].toDouble();
        }
    }

    // Суммируем смерти из всех сегментов (карт)
    double totalDeaths = 0;
    if (root.contains("segments") && root["segments"].isArray()) {
        QJsonArray segments = root["segments"].toArray();
        for (const QJsonValue &segmentValue : segments) {
            QJsonObject segment = segmentValue.toObject();
            if (segment.contains("stats") && segment["stats"].isObject()) {
                QJsonObject stats = segment["stats"].toObject();
                if (stats.contains("Deaths")) {
                    totalDeaths += stats["Deaths"].toDouble();
                }
            }
        }
    }

    qDebug() << "Total Kills:" << totalKills;
    qDebug() << "Total Deaths:" << totalDeaths;

    // Проверяем, что данные корректны
    if (totalKills <= 0 || totalDeaths <= 0) {
        ui->label_KD->setText("Ошибка: нет данных для расчёта KD.");
        return;
    }

    // Рассчитываем KD
    double kdRatio = totalKills / totalDeaths;

    // Выводим результат
    ui->label_KD->setText(
        QString("Общий KD: %1 (Убийств: %2, Смертей: %3)")
            .arg(kdRatio, 0, 'f', 2)
            .arg(totalKills, 0, 'f', 0)
            .arg(totalDeaths, 0, 'f', 0)
    );
}


