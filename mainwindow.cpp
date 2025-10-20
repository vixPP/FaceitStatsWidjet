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
    , totalKills(0)
    , totalDeaths(0)
    , totalADR(0.0) // Инициализация
    , matchesCount(0)
    , processedMatches(0)
{
    ui->setupUi(this);
    bestMapImageLabel = ui->label_BestMapImage;
    // Инициализация сетевых менеджеров
    networkManager = new QNetworkAccessManager(this);
    statsNetworkManager = new QNetworkAccessManager(this);
    avatarNetworkManager = new QNetworkAccessManager(this);
    bestMapImageNetworkManager = new QNetworkAccessManager(this);

    // Подключение сигналов
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onFetchStatsClicked);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onRequestFinished);
    connect(statsNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onMatchHistoryFinished);
    connect(avatarNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onAvatarDownloaded);
    connect(bestMapImageNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onBestMapImageDownloaded);
    historyNetworkManager = new QNetworkAccessManager(this);
    connect(historyNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onMatchHistoryFetched);
    matchStatsNetworkManager = new QNetworkAccessManager(this);
    connect(matchStatsNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onMatchStatsFetched);

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

    if (reply->error() != QNetworkReply::NoError)
    {
        ui->label_NAME->setText("Ошибка");
        ui->text_elo->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    //qDebug().noquote() << "Player Info JSON:" << QString(responseData); //JSON otvet
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        ui->text_elo->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QString playerId = jsonObj["player_id"].toString();
    if (playerId.isEmpty())
    {
        ui->text_elo->setText("Игрок не найден.");
        return;
    }

    currentPlayerId = playerId;
    ui->label_NAME->setText(jsonObj["nickname"].toString());

    // ELO
    if (jsonObj.contains("games") && jsonObj["games"].isObject())
    {
        QJsonObject games = jsonObj["games"].toObject();
        if (games.contains("cs2") && games["cs2"].isObject())
        {
            QJsonObject cs2 = games["cs2"].toObject();
            if (cs2.contains("faceit_elo"))
            {
                int elo = cs2["faceit_elo"].toInt();
                ui->text_elo->setText(QString("ELO:      %1").arg(elo));
            }
        }
    }

    // Аватар
    QString avatarUrl = jsonObj["avatar"].toString();
    if (!avatarUrl.isEmpty())
    {
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

    QString matchHistoryUrl = QString("https://open.faceit.com/data/v4/players/%1/history?game=cs2&offset=0&limit=10").arg(playerId);
    QNetworkRequest matchHistoryRequest;
    matchHistoryRequest.setUrl(QUrl(matchHistoryUrl));
    matchHistoryRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    matchHistoryRequest.setRawHeader("Accept", "application/json");
    matchHistoryRequest.setRawHeader("User-Agent", "MyApp/1.0");
    historyNetworkManager->get(matchHistoryRequest);
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
    qDebug().noquote() <<  "Player Stats JSON:" << QString(responseData);
    reply->deleteLater();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        ui->text_KD->setText("Ошибка: некорректный JSON.");
        return;
    }
    QJsonObject root = jsonDoc.object();

    // Обработка статистики по картам
    if (root.contains("segments") && root["segments"].isArray())
    {
        QJsonArray segments = root["segments"].toArray();
        QString bestMap;
        double bestKDRatio = 0.0;
        QString bestMapImageUrl;
        double AVGK = 0.0;
        int TotalMatches = 0;
        double winRate = 0.0;

        for (const QJsonValue &segmentValue : segments)
        {
            if (!segmentValue.isObject()) continue;
            QJsonObject segment = segmentValue.toObject();
            if (segment["type"].toString() == "Map" && segment.contains("stats"))
            {
                QJsonObject stats = segment["stats"].toObject();


                if (stats.contains("Average K/D Ratio"))
                {
                    double kdRatio = stats["Average K/D Ratio"].toString().toDouble();


                    if (kdRatio > bestKDRatio)
                    {
                        bestKDRatio = kdRatio;
                        bestMap = segment["label"].toString();
                        bestMapImageUrl = segment["img_regular"].toString();
                        AVGK = stats["Average Kills"].toString().toDouble();
                        TotalMatches = stats["Total Matches"].toString().toInt();
                        winRate = stats["Win Rate %"].toString().toDouble();
                    }

                }

            }
        }

        if (!bestMap.isEmpty())
        {

            ui->text_BestMapName->setText(QString("%1").arg(bestMap));
            ui->Text_KD_BestMap->setText(QString("KD:      %2").arg(bestKDRatio, 0, 'f', 2));
            ui->Text_AVGK_BestMap->setText(QString("AVG.K:   %1").arg(AVGK, 0, 'f', 1));
            ui->TotalOnBestMapMatches->setText(QString("Matches: %1").arg(TotalMatches));
            ui->Text_WInRate_BestMap->setText(QString("W.Rate:  %1").arg(winRate));

            if (!bestMapImageUrl.isEmpty())
            {
                QNetworkRequest imageRequest;
                imageRequest.setUrl(QUrl(bestMapImageUrl));
                bestMapImageNetworkManager->get(imageRequest);
            }
        }
    }

    // Остальная логика обработки статистики
    if (root.contains("lifetime") && root["lifetime"].isObject())
    {
        QJsonObject lifetime = root["lifetime"].toObject();
        if (lifetime.contains("ADR"))
        {
            double adr = lifetime["ADR"].toString().toDouble();
            ui->text_KR->setText(QString("ADR:      %1").arg(adr, 0, 'f', 2));
        }
        if (lifetime.contains("Average K/D Ratio"))
        {
            double kdRatio = lifetime["Average K/D Ratio"].toString().toDouble();
            ui->text_KD->setText(QString("KD:       %1").arg(kdRatio, 0, 'f', 2));
        }
        if (lifetime.contains("Matches"))
        {
            int matches = lifetime["Matches"].toString().toInt();
            ui->text_Matches->setText(QString("Matches:  %1").arg(matches));
        }
    }
}



void MainWindow::onMatchHistoryFetched(QNetworkReply *reply)
{
    totalKills = 0;  // Сброс счётчиков
    matchesCount = 0;
    totalDeaths = 0;
    processedMatches = 0;
    totalADR = 0;

    if (reply->error() != QNetworkReply::NoError)
    {
        ui->text_AVG_LastMatches->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        qDebug().noquote() << jsonDoc.toJson(QJsonDocument::Indented); 
        ui->text_AVG_LastMatches->setText("Ошибка: некорректный JSON.");
        return;
    }


    QJsonObject root = jsonDoc.object();
    if (root.contains("items") && root["items"].isArray())
    {
        QJsonArray matches = root["items"].toArray();
        for (const QJsonValue &matchValue : matches)
        {
            if (!matchValue.isObject()) continue;
            QJsonObject match = matchValue.toObject();
            QString matchId = match["match_id"].toString();
            if (!matchId.isEmpty())
            {
                // Запрашиваем статистику матча
                QString matchStatsUrl = QString("https://open.faceit.com/data/v4/matches/%1/stats").arg(matchId);
                QNetworkRequest matchStatsRequest;
                matchStatsRequest.setUrl(QUrl(matchStatsUrl));
                matchStatsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
                matchStatsNetworkManager->get(matchStatsRequest);
            }
        }
    }
}


void MainWindow::onMatchStatsFetched(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Ошибка запроса статистики матча:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        qDebug() << "Ошибка разбора JSON статистики матча.";
        return;
    }

    QJsonObject root = jsonDoc.object();
    if (root.contains("rounds") && root["rounds"].isArray())
    {
        QJsonArray rounds = root["rounds"].toArray();
        for (const QJsonValue &roundValue : rounds)
        {
            if (!roundValue.isObject()) continue;
            QJsonObject round = roundValue.toObject();
            if (round.contains("teams") && round["teams"].isArray())
            {
                QJsonArray teams = round["teams"].toArray();
                for (const QJsonValue &teamValue : teams)
                {
                    if (!teamValue.isObject()) continue;
                    QJsonObject team = teamValue.toObject();
                    if (team.contains("players") && team["players"].isArray())
                    {
                        QJsonArray players = team["players"].toArray();
                        for (const QJsonValue &playerValue : players)
                        {
                            if (!playerValue.isObject()) continue;
                            QJsonObject player = playerValue.toObject();
                            if (player["player_id"].toString() == currentPlayerId)
                            {
                                if (player.contains("player_stats") && player["player_stats"].isObject())
                                {
                                    QJsonObject stats = player["player_stats"].toObject();
                                    if (stats.contains("Kills"))
                                    {
                                        // Преобразуем строку в число
                                        int kills = stats["Kills"].toString().toInt();
                                        totalKills += kills;
                                        matchesCount++;
                                        qDebug() << "Match Kills:" << kills
                                                 << "Match deathes" << totalDeaths
                                                 << "Total Kills:" << totalKills
                                                 << "Matches Count:" << matchesCount;
                                    }
                                    if (stats.contains("Deaths"))
                                    {
                                        int deaths = stats["Deaths"].toString().toInt();
                                        totalDeaths += deaths;
                                    }
                                    if (stats.contains("ADR"))
                                    {
                                                   double adr = stats["ADR"].toString().toDouble();
                                                   totalADR += adr;
                                                   qDebug() << "Match ADR:" << adr << "Total ADR:" << totalADR;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    processedMatches++;
    if (processedMatches == 10) {
        if (matchesCount > 0) {
            double averageKills = static_cast<double>(totalKills) / matchesCount;
            ui->text_AVG_LastMatches->setText(QString("AVG.K     %1").arg(averageKills, 0, 'f', 2));

            double kdRatio = (totalDeaths == 0) ? totalKills : static_cast<double>(totalKills) / totalDeaths;
            ui->text_KD_LastMatches->setText(QString("KD        %1").arg(kdRatio, 0, 'f', 2));

            double averageADR = totalADR / matchesCount;
            ui->text_ADR_LastMatches->setText(QString("ADR       %1").arg(averageADR, 0, 'f', 2));
        } else
        {
            ui->text_AVG_LastMatches->setText("Нет данных о матчах.");
        }
    }
}

void MainWindow::onAvatarDownloaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Ошибка загрузки аватара:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray avatarData = reply->readAll();
    if (avatarData.isEmpty())
    {
        qDebug() << "Avatar data is empty!";
        reply->deleteLater();
        return;
    }

    QPixmap avatarPixmap;
    if (!avatarPixmap.loadFromData(avatarData))
    {
        qDebug() << "Не удалось загрузить аватар!";
        reply->deleteLater();
        return;
    }

    avatarPixmap = avatarPixmap.scaled(81, 81, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    avatarLabel->setPixmap(avatarPixmap);
    reply->deleteLater();
}


void MainWindow::onBestMapImageDownloaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Ошибка загрузки изображения карты:" << reply->errorString();
        reply->deleteLater();
        return;
    }
    QByteArray imageData = reply->readAll();
    if (imageData.isEmpty())
    {
        qDebug() << "Image data is empty!";
        reply->deleteLater();
        return;
    }
    QPixmap mapPixmap;
    if (!mapPixmap.loadFromData(imageData))
    {
        qDebug() << "Не удалось загрузить изображение карты!";
        reply->deleteLater();
        return;
    }
    mapPixmap = mapPixmap.scaled(91, 61, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    bestMapImageLabel->setPixmap(mapPixmap);
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
