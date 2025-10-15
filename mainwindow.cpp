#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QPushButton>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QMovie>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Инициализация сетевых менеджеров
    networkManager = new QNetworkAccessManager(this);
    statsNetworkManager = new QNetworkAccessManager(this);
    avatarNetworkManager = new QNetworkAccessManager(this);

    // Подключение сигналов
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onFetchStatsClicked);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onRequestFinished);
    connect(statsNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onMatchHistoryFinished);
    connect(avatarNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onAvatarDownloaded);

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
    if (nickname.isEmpty()) {
        return;
    }

    // Блокировка UI на время запроса
    ui->pushButton->setEnabled(false);
    ui->label_NAME->setText("Загрузка...");
    ui->text_elo->clear();
    ui->text_KD->clear();
    ui->text_KR->clear();
    ui->text_Matches->clear();

    QString apiUrl = QString("https://open.faceit.com/data/v4/players?nickname=%1&game=cs2").arg(nickname);
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    networkManager->get(request);
}

void MainWindow::onRequestFinished(QNetworkReply *reply)
{
    ui->pushButton->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        ui->label_NAME->setText("Ошибка");
        ui->text_elo->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        ui->text_elo->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QString playerId = jsonObj["player_id"].toString();
    if (playerId.isEmpty()) {
        ui->text_elo->setText("Игрок не найден.");
        return;
    }

    currentPlayerId = playerId;
    ui->label_NAME->setText(jsonObj["nickname"].toString());

    // ELO
    if (jsonObj.contains("games") && jsonObj["games"].isObject()) {
        QJsonObject games = jsonObj["games"].toObject();
        if (games.contains("cs2") && games["cs2"].isObject()) {
            QJsonObject cs2 = games["cs2"].toObject();
            if (cs2.contains("faceit_elo")) {
                int elo = cs2["faceit_elo"].toInt();
                ui->text_elo->setText(QString("ELO: %1").arg(elo));
            }
        }
    }

    // Аватар
    QString avatarUrl = jsonObj["avatar"].toString();
    if (!avatarUrl.isEmpty()) {
        QNetworkRequest avatarRequest;
        avatarRequest.setUrl(QUrl(avatarUrl));
        avatarNetworkManager->get(avatarRequest);
    }

    // Статистика
    QString statsUrl = QString("https://open.faceit.com/data/v4/players/%1/stats/cs2").arg(playerId);
    QNetworkRequest statsRequest;
    statsRequest.setUrl(QUrl(statsUrl));
    statsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    statsNetworkManager->get(statsRequest);
}

void MainWindow::onMatchHistoryFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        ui->text_KD->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        ui->text_KD->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject root = jsonDoc.object();
    if (root.contains("lifetime") && root["lifetime"].isObject()) {
        QJsonObject lifetime = root["lifetime"].toObject();

        // ADR
        if (lifetime.contains("ADR")) {
            double adr = lifetime["ADR"].toString().toDouble();
            ui->text_KR->setText(QString("ADR: %1").arg(adr, 0, 'f', 2));
        }

        // K/D Ratio
        if (lifetime.contains("Average K/D Ratio")) {
            double kdRatio = lifetime["Average K/D Ratio"].toString().toDouble();
            ui->text_KD->setText(QString("KD: %1").arg(kdRatio, 0, 'f', 2));
        }

        // Matches
        if (lifetime.contains("Matches")) {
            int matches = lifetime["Matches"].toString().toInt();
            ui->text_Matches->setText(QString("Matches: %1").arg(matches));
        }
    }
}

void MainWindow::onAvatarDownloaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Ошибка загрузки аватара:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray avatarData = reply->readAll();
    if (avatarData.isEmpty()) {
        qDebug() << "Avatar data is empty!";
        reply->deleteLater();
        return;
    }

    QPixmap avatarPixmap;
    if (!avatarPixmap.loadFromData(avatarData)) {
        qDebug() << "Не удалось загрузить аватар!";
        reply->deleteLater();
        return;
    }

    avatarPixmap = avatarPixmap.scaled(81, 81, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    avatarLabel->setPixmap(avatarPixmap);
    reply->deleteLater();
}

void MainWindow::applyRoundedCorners()
{
    QBitmap mask(size());
    mask.fill(Qt::color0);
    QPainter painter(&mask);
    painter.setBrush(Qt::color1);
    painter.drawRoundedRect(mask.rect(), 10, 10);
    setMask(mask);
}
