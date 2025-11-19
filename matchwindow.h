#ifndef MATCHWINDOW_H
#define MATCHWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

class MatchWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MatchWindow(QWidget *parent = nullptr);
    ~MatchWindow() = default;


private slots:
    void onBackButtonClicked();

private:
    void setupUI();

    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QPushButton *backButton;
    QLabel *infoLabel;
    QLabel *Player1Label;
    QLabel *Player2Label;
    QLabel *Player3Label;
    QLabel *Player4Label;
    QLabel *Player5Label;
    QLabel *Player6Label;
};

#endif // MATCHWINDOW_H
