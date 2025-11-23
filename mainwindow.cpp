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
#include <QTimer>
#include <QLineEdit>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , totalKills(0)
    , totalDeaths(0)
    , totalADR(0.0)
    , matchesCount(0)
    , processedMatches(0)
    , matchesToFetch(10)
    , matchWindow(nullptr)
{
    ui->setupUi(this);

    // Поле для ввода ссылки на комнату
    roomUrlEdit = new QLineEdit(this);
    roomUrlEdit->setGeometry(10, 440, 260, 25);
    roomUrlEdit->setPlaceholderText("Вставьте ссылку на комнату матча FaceIt...");
    roomUrlEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: #2e2e2d;"
        "    color: white;"
        "    border: 1px solid #4CAF50;"
        "    border-radius: 5px;"
        "    padding: 5px;"
        "}"
    );

    // Кнопка для загрузки комнаты
    roomMatchButton = new QPushButton("Загрузить", this);
    roomMatchButton->setGeometry(275, 440, 90, 25);
    roomMatchButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );
    connect(roomMatchButton, &QPushButton::clicked, this, &MainWindow::onRoomMatchButtonClicked);

    messageTimer = new QTimer(this);
    messageTimer->setSingleShot(true);
    connect(messageTimer, &QTimer::timeout, this, [this]()
    {
        ui->label_Errors->clear();
    });

    buttonSearcheTimer = new QTimer(this);
    buttonSearcheTimer->setSingleShot(true);
    connect(buttonSearcheTimer, &QTimer::timeout, this, [this]()
    {
        ui->pushButton->setEnabled(true);
    });

    button10Timer = new QTimer(this);
    button10Timer->setSingleShot(true);
    connect(button10Timer, &QTimer::timeout, this, [this]()
    {
        ui->pushButton_10->setEnabled(true);
    });

    button20Timer = new QTimer(this);
    button20Timer->setSingleShot(true);
    connect(button20Timer, &QTimer::timeout, this, [this]()
    {
        ui->pushButton_20->setEnabled(true);
    });

    button30Timer = new QTimer(this);
    button30Timer->setSingleShot(true);
    connect(button30Timer, &QTimer::timeout, this, [this]()
    {
        ui->pushButton_30->setEnabled(true);
    });

    QSettings settings("config.ini", QSettings::IniFormat);
    QString savedNickname = settings.value("Player/Nickname", "").toString();
    if (!savedNickname.isEmpty())
    {
        ui->lineEditIRL->setText(savedNickname);
        QTimer::singleShot(100, this, &MainWindow::autoFetchStats);
    }

    ui->label_NAME->setAlignment(Qt::AlignCenter);
    ui->label_NAME->setWordWrap(true);
    bestMapImageLabel = ui->label_BestMapImage;

    // Инициализация сетевых менеджеров
    networkManager = new QNetworkAccessManager(this);
    statsNetworkManager = new QNetworkAccessManager(this);
    avatarNetworkManager = new QNetworkAccessManager(this);
    bestMapImageNetworkManager = new QNetworkAccessManager(this);

    matchesToFetch = 10;

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
    connect(ui->pushButton_10, &QPushButton::clicked, this, &MainWindow::onFetch10MatchesClicked);
    connect(ui->pushButton_20, &QPushButton::clicked, this, &MainWindow::onFetch20MatchesClicked);
    connect(ui->pushButton_30, &QPushButton::clicked, this, &MainWindow::onFetch30MatchesClicked);
    internalStatsNetworkManager = new QNetworkAccessManager(this);
    connect(internalStatsNetworkManager, &QNetworkAccessManager::finished, this, &MainWindow::onInternalMatchStatsFetched);
    connect(ui->Button_Save, &QPushButton::clicked, this, &MainWindow::on_Button_Save_clicked);

    avatarLabel = ui->label_avatar;
    applyRoundedCorners();
}

void MainWindow::onRoomMatchButtonClicked()
{
    QString roomUrl = roomUrlEdit->text().trimmed();

    if (roomUrl.isEmpty())
    {
        ui->label_Errors->setText("Введите ссылку на комнату матча");
        return;
    }

    QString matchId = extractMatchIdFromUrl(roomUrl);

    if (matchId.isEmpty())
    {
        ui->label_Errors->setText("Неверный формат ссылки");
        return;
    }

    qDebug() << "Loading room with match ID:" << matchId;

    if (!matchWindow) {
        matchWindow = new MatchWindow(apiKey, this);
        connect(matchWindow, &MatchWindow::destroyed, this, [this]() {
            matchWindow = nullptr;
        });
    }

    matchWindow->loadMatchById(matchId, currentPlayerId);
    matchWindow->show();
    matchWindow->activateWindow();

    //ui->label_Errors->setText("Загрузка комнаты...");
}

QString MainWindow::extractMatchIdFromUrl(const QString &url)
{
    QRegularExpression regex("room/1-([a-f0-9-]+)");
    QRegularExpressionMatch match = regex.match(url);

    if (match.hasMatch()) {
        return "1-" + match.captured(1);
    }

    QRegularExpression regex2("room/([a-f0-9-]+)");
    QRegularExpressionMatch match2 = regex2.match(url);

    if (match2.hasMatch()) {
        return match2.captured(1);
    }

    return "";
}

void MainWindow::autoFetchStats()
{
    QString nickname = ui->lineEditIRL->text().trimmed();
    if (nickname.isEmpty())
    {
        return;
    }

    totalKills = 0;
    totalDeaths = 0;
    totalADR = 0.0;
    matchesCount = 0;
    processedMatches = 0;
    matchKDRatios.clear();

    ui->label_NAME->setText("Загрузка...");
    ui->text_ALLelo->clear();
    ui->text_ALLKD->clear();
    ui->text_ALLKR->clear();
    ui->text_ALLMatches->clear();
    ui->text_AVG_LastMatches->clear();
    ui->text_KD_LastMatches->clear();
    ui->text_ADR_LastMatches->clear();
    ui->text_ELO_Change->clear();

    QString apiUrl = QString("https://open.faceit.com/data/v4/players?nickname=%1&game=cs2").arg(nickname);
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    networkManager->get(request);
}

void MainWindow::on_Button_Save_clicked()
{
    QString nickname = ui->lineEditIRL->text().trimmed();
    if (!nickname.isEmpty())
    {
        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("Player/Nickname", nickname);
        ui->label_Errors->setText("Никнейм сохранён!");
        messageTimer->start(5000);
    } else
    {
        ui->label_Errors->setText("Ошибка: никнейм пуст!");
    }
}

void MainWindow::onFetchStatsClicked()
{
    totalKills = 0;
    totalDeaths = 0;
    totalADR = 0.0;
    matchesCount = 0;
    processedMatches = 0;

    QString nickname = ui->lineEditIRL->text().trimmed();
    if (nickname.isEmpty())
    {
        return;
    }

    ui->pushButton->setEnabled(false);
    buttonSearcheTimer->start(3000);

    ui->label_NAME->setText("Загрузка...");
    ui->text_ALLelo->clear();
    ui->text_ALLKD->clear();
    ui->text_ALLKR->clear();
    ui->text_ALLMatches->clear();

    QString apiUrl = QString("https://open.faceit.com/data/v4/players?nickname=%1&game=cs2").arg(nickname);
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    networkManager->get(request);
}

void MainWindow::onRequestFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        ui->label_NAME->setText("Ошибка");
        ui->text_ALLelo->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        ui->text_ALLelo->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QString playerId = jsonObj["player_id"].toString();
    if (playerId.isEmpty())
    {
        ui->text_ALLelo->setText("Игрок не найден.");
        return;
    }

    currentPlayerId = playerId;
    ui->label_NAME->setText(jsonObj["nickname"].toString());

    if (jsonObj.contains("games") && jsonObj["games"].isObject())
    {
        QJsonObject games = jsonObj["games"].toObject();
        if (games.contains("cs2") && games["cs2"].isObject())
        {
            QJsonObject cs2 = games["cs2"].toObject();
            if (cs2.contains("faceit_elo"))
            {
                currentPlayerElo = cs2["faceit_elo"].toInt();
                ui->text_ALLelo->setText(QString("ELO:      %1").arg(currentPlayerElo));
            }
        }
    }

    QString avatarUrl = jsonObj["avatar"].toString();
    if (!avatarUrl.isEmpty())
    {
        QNetworkRequest avatarRequest;
        avatarRequest.setUrl(QUrl(avatarUrl));
        avatarNetworkManager->get(avatarRequest);
    }

    QString statsUrl = QString("https://open.faceit.com/data/v4/players/%1/stats/cs2").arg(playerId);
    QNetworkRequest statsRequest;
    statsRequest.setUrl(QUrl(statsUrl));
    statsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    statsNetworkManager->get(statsRequest);

    int matchesToRequest = matchesToFetch + 1;
    QString matchHistoryUrl = QString("https://open.faceit.com/data/v4/players/%1/history?game=cs2&offset=0&limit=%2").arg(playerId).arg(matchesToRequest);

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
        ui->text_ALLKD->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray responseData = reply->readAll();
    qDebug() << "=== RAW JSON RESPONSE ===";
    qDebug() << QString::fromUtf8(responseData);
    qDebug() << "=== END RAW JSON ===";
    reply->deleteLater();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        ui->text_ALLKD->setText("Ошибка: некорректный JSON.");
        return;
    }
    QJsonObject root = jsonDoc.object();

    if (root.contains("segments") && root["segments"].isArray())
    {
        QJsonArray segments = root["segments"].toArray();
        QString bestMap;
        double bestKDRatio = 0.0;
        QString bestMapImageUrl;
        double AVGK = 0.0;
        int TotalMatches = 0;
        double winRate = 0.0; // ИЗМЕНИТЕ НА double

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
                        // ИСПОЛЬЗУЙТЕ toDouble() для винрейта
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
            ui->Text_WInRate_BestMap->setText(QString("W.Rate:  %1%").arg(winRate, 0, 'f', 1)); // ДОБАВЬТЕ % В ВЫВОД
        }
    }

    if (root.contains("lifetime") && root["lifetime"].isObject())
    {
        QJsonObject lifetime = root["lifetime"].toObject();
        if (lifetime.contains("ADR"))
        {
            double adr = lifetime["ADR"].toString().toDouble();
            ui->text_ALLKR->setText(QString("ADR:      %1").arg(adr, 0, 'f', 2));
        }
        if (lifetime.contains("Average K/D Ratio"))
        {
            double kdRatio = lifetime["Average K/D Ratio"].toString().toDouble();
            ui->text_ALLKD->setText(QString("KD:       %1").arg(kdRatio, 0, 'f', 2));
        }
        if (lifetime.contains("Matches"))
        {
            int matches = lifetime["Matches"].toString().toInt();
            ui->text_ALLMatches->setText(QString("Matches:  %1").arg(matches));
        }
        if (lifetime.contains("Win Rate %"))
        {
            double winRate = lifetime["Win Rate %"].toString().toDouble(); // ИСПОЛЬЗУЙТЕ toDouble()
            ui->text_ALLWINR->setText(QString("W.Rate:   %1%").arg(winRate, 0, 'f', 1));
        }
        // ДОБАВЬТЕ ОТЛАДОЧНЫЙ ВЫВОД ДЛЯ ПРОВЕРКИ КЛЮЧЕЙ
        qDebug() << "Lifetime keys:" << lifetime.keys();
    }
}

void MainWindow::onMatchHistoryFetched(QNetworkReply *reply)
{
    totalKills = 0;
    matchesCount = 0;
    totalDeaths = 0;
    processedMatches = 0;
    totalADR = 0;
    matchKDRatios.clear();

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
        ui->text_AVG_LastMatches->setText("Ошибка: некорректный JSON.");
        return;
    }

    QJsonObject root = jsonDoc.object();
    QString eloMatchId;

    if (root.contains("items") && root["items"].isArray())
    {
        QJsonArray matches = root["items"].toArray();

        if (matches.size() > matchesToFetch)
        {
            QJsonObject eloMatch = matches.last().toObject();
            eloMatchId = eloMatch["match_id"].toString();
            qDebug() << "Матч для ELO (N+1):" << eloMatchId;

            for (int i = 0; i < matchesToFetch && i < matches.size(); i++)
            {
                QJsonObject match = matches[i].toObject();
                QString matchId = match["match_id"].toString();
                if (!matchId.isEmpty())
                {
                    QString matchStatsUrl = QString("https://open.faceit.com/data/v4/matches/%1/stats").arg(matchId);
                    QNetworkRequest matchStatsRequest;
                    matchStatsRequest.setUrl(QUrl(matchStatsUrl));
                    matchStatsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
                    matchStatsNetworkManager->get(matchStatsRequest);
                }
            }
        }
        else
        {
            if (!matches.isEmpty())
            {
                QJsonObject eloMatch = matches.last().toObject();
                eloMatchId = eloMatch["match_id"].toString();
                qDebug() << "Матч для ELO (последний доступный):" << eloMatchId;
            }

            for (const QJsonValue &matchValue : matches)
            {
                if (!matchValue.isObject()) continue;
                QJsonObject match = matchValue.toObject();
                QString matchId = match["match_id"].toString();
                if (!matchId.isEmpty())
                {
                    QString matchStatsUrl = QString("https://open.faceit.com/data/v4/matches/%1/stats").arg(matchId);
                    QNetworkRequest matchStatsRequest;
                    matchStatsRequest.setUrl(QUrl(matchStatsUrl));
                    matchStatsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
                    matchStatsNetworkManager->get(matchStatsRequest);
                }
            }
        }

        if (!eloMatchId.isEmpty())
        {
            fetchInternalMatchStats(eloMatchId);
        }
        else
        {
            qDebug() << "Не найден матч для расчета ELO";
            ui->text_ELO_Change->setText("ELO+:     N/A");
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
    bool playerFoundInMatch = false;
    int matchKills = 0;
    int matchDeaths = 0;
    double matchADR = 0.0;
    double matchKDRatio = 0.0;

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
                                playerFoundInMatch = true;
                                if (player.contains("player_stats") && player["player_stats"].isObject())
                                {
                                    QJsonObject stats = player["player_stats"].toObject();
                                    if (stats.contains("Kills"))
                                    {
                                        matchKills = stats["Kills"].toString().toInt();
                                        totalKills += matchKills;
                                    }
                                    if (stats.contains("Deaths"))
                                    {
                                        matchDeaths = stats["Deaths"].toString().toInt();
                                        totalDeaths += matchDeaths;
                                    }
                                    if (stats.contains("ADR"))
                                    {
                                        matchADR = stats["ADR"].toString().toDouble();
                                        totalADR += matchADR;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (playerFoundInMatch)
    {
        matchesCount++;
        matchKDRatio = (matchDeaths == 0) ? matchKills : static_cast<double>(matchKills) / matchDeaths;
        matchKDRatios.append(matchKDRatio);

        if (matchesCount == 10 || matchesCount == 20 || matchesCount == 30)
        {
            double totalKD = (totalDeaths == 0) ? totalKills : static_cast<double>(totalKills) / totalDeaths;
            //qDebug().noquote() << QString("Общий KD за %1 матчей: %2").arg(matchesCount).arg(totalKD, 0, 'f', 2);
        }
    }

    processedMatches++;
    //qDebug() << "Processed:" << processedMatches << "Matches Count:" << matchesCount;

    if (processedMatches == matchesToFetch)
    {
        if (matchesCount > 0)
        {
            double averageKills = static_cast<double>(totalKills) / matchesCount;
            ui->text_AVG_LastMatches->setText(QString("AVG.K:    %1").arg(averageKills, 0, 'f', 2));
            double averageKD = std::accumulate(matchKDRatios.begin(), matchKDRatios.end(), 0.0) / matchKDRatios.size();
            ui->text_KD_LastMatches->setText (QString("KD:       %1").arg(averageKD, 0, 'f', 2));
            double averageADR = totalADR / matchesCount;
            ui->text_ADR_LastMatches->setText(QString("ADR:      %1").arg(averageADR, 0, 'f', 2));
        }
        else
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

void MainWindow::onFetch10MatchesClicked()
{
    ui->pushButton_10->setEnabled(false);
    ui->pushButton_20->setEnabled(false);
    ui->pushButton_30->setEnabled(false);

    button10Timer->start(3000);
    button20Timer->start(3000);
    button30Timer->start(3000);

    totalKills = 0;
    totalDeaths = 0;
    totalADR = 0.0;
    matchesCount = 0;
    processedMatches = 0;
    matchKDRatios.clear();

    matchesToFetch = 10;
    int matchesToRequest = 11;

    ui->label_2->setText("   Last 10");

    QString playerId = currentPlayerId;
    if (playerId.isEmpty()) return;

    QString matchHistoryUrl = QString("https://open.faceit.com/data/v4/players/%1/history?game=cs2&offset=0&limit=%2").arg(playerId).arg(matchesToRequest);
    QNetworkRequest matchHistoryRequest;
    matchHistoryRequest.setUrl(QUrl(matchHistoryUrl));
    matchHistoryRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    matchHistoryRequest.setRawHeader("Accept", "application/json");
    matchHistoryRequest.setRawHeader("User-Agent", "MyApp/1.0");
    historyNetworkManager->get(matchHistoryRequest);
}

void MainWindow::onFetch20MatchesClicked()
{
    ui->pushButton_10->setEnabled(false);
    ui->pushButton_20->setEnabled(false);
    ui->pushButton_30->setEnabled(false);

    button10Timer->start(3000);
    button20Timer->start(3000);
    button30Timer->start(3000);

    totalKills = 0;
    totalDeaths = 0;
    totalADR = 0.0;
    matchesCount = 0;
    processedMatches = 0;
    matchKDRatios.clear();

    matchesToFetch = 20;
    int matchesToRequest = 21;

    ui->label_2->setText("   Last 20");

    QString playerId = currentPlayerId;
    if (playerId.isEmpty()) return;

    QString matchHistoryUrl = QString("https://open.faceit.com/data/v4/players/%1/history?game=cs2&offset=0&limit=%2").arg(playerId).arg(matchesToRequest);
    QNetworkRequest matchHistoryRequest;
    matchHistoryRequest.setUrl(QUrl(matchHistoryUrl));
    matchHistoryRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    matchHistoryRequest.setRawHeader("Accept", "application/json");
    matchHistoryRequest.setRawHeader("User-Agent", "MyApp/1.0");
    historyNetworkManager->get(matchHistoryRequest);
}

void MainWindow::onFetch30MatchesClicked()
{
    ui->pushButton_10->setEnabled(false);
    ui->pushButton_20->setEnabled(false);
    ui->pushButton_30->setEnabled(false);

    button10Timer->start(3000);
    button20Timer->start(3000);
    button30Timer->start(3000);

    totalKills = 0;
    totalDeaths = 0;
    totalADR = 0.0;
    matchesCount = 0;
    processedMatches = 0;
    matchKDRatios.clear();

    matchesToFetch = 30;
    int matchesToRequest = 31;

    ui->label_2->setText("   Last 30");

    QString playerId = currentPlayerId;
    if (playerId.isEmpty()) return;

    QString matchHistoryUrl = QString("https://open.faceit.com/data/v4/players/%1/history?game=cs2&offset=0&limit=%2").arg(playerId).arg(matchesToRequest);
    QNetworkRequest matchHistoryRequest;
    matchHistoryRequest.setUrl(QUrl(matchHistoryUrl));
    matchHistoryRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    matchHistoryRequest.setRawHeader("Accept", "application/json");
    matchHistoryRequest.setRawHeader("User-Agent", "MyApp/1.0");
    historyNetworkManager->get(matchHistoryRequest);
}

void MainWindow::fetchInternalMatchStats(const QString &matchId)
{
    QString internalStatsUrl = QString("https://www.faceit.com/api/stats/v3/matches/%1").arg(matchId);
    qDebug() << "Запрашиваем ELO для матча:" << matchId;
    QNetworkRequest internalStatsRequest;
    internalStatsRequest.setUrl(QUrl(internalStatsUrl));
    internalStatsNetworkManager->get(internalStatsRequest);
}

void MainWindow::onInternalMatchStatsFetched(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Ошибка запроса внутренней статистики матча:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

    if (jsonDoc.isNull())
    {
        qDebug() << "Ошибка разбора JSON: документ пуст.";
        reply->deleteLater();
        return;
    }

    if (!jsonDoc.isArray())
    {
        qDebug() << "JSON не является массивом.";
        reply->deleteLater();
        return;
    }

    QJsonArray matches = jsonDoc.array();
    if (matches.isEmpty())
    {
        qDebug() << "Массив матчей пуст.";
        reply->deleteLater();
        return;
    }

    QJsonObject match = matches.first().toObject();
    if (!match.contains("teams") || !match["teams"].isArray())
    {
        qDebug() << "В JSON нет ключа 'teams' или он не является массивом.";
        reply->deleteLater();
        return;
    }

    QJsonArray teams = match["teams"].toArray();
    QString targetPlayerId = currentPlayerId.trimmed();

    for (const QJsonValue &teamValue : teams)
    {
        if (!teamValue.isObject())
            continue;

        QJsonObject team = teamValue.toObject();

        if (!team.contains("players") || !team["players"].isArray())
        {
            continue;
        }

        QJsonArray players = team["players"].toArray();

        for (const QJsonValue &playerValue : players)
        {
            if (!playerValue.isObject())
                continue;

            QJsonObject player = playerValue.toObject();
            QString playerId = player.contains("playerId") ? player["playerId"].toString().trimmed() : "Unknown ID";

            if (playerId == targetPlayerId)
            {
                int DifElo;
                int EloPastGames = player.contains("elo") ? player["elo"].toInt() : -1;
                DifElo = currentPlayerElo - EloPastGames;

                qDebug() << "Текущее ELO:" << currentPlayerElo << "ELO в старом матче:" << EloPastGames << "Разница:" << DifElo;
                ui->text_ELO_Change->setText( QString("ELO+:     %1%2").arg(DifElo >= 0 ? "+" : "").arg(DifElo));

                reply->deleteLater();
                return;
            }
        }
    }

    qDebug() << "Игрок не найден в этом матче. PlayerId:" << targetPlayerId;
    ui->text_ELO_Change->setText("Игрок не найден в этом матче.");
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

MainWindow::~MainWindow()
{
    delete button10Timer;
    delete button20Timer;
    delete button30Timer;
    delete ui;
}
