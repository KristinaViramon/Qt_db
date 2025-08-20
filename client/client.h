#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include<QPushButton>
#include "loginform.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class client;
}
QT_END_NAMESPACE
class client : public QMainWindow
{
    Q_OBJECT
public:
    client(QWidget *parent = nullptr);
    ~client();
    void connectToServer();

signals:
    void databasesReceived(const QString &databases);

public slots:
    void sendRequest();


private:
    QTcpSocket* clientSocket;
     Ui::client *ui;
    QTreeWidget *treeWidget;
    QTableWidget *tableWidget;
    loginform *loginForm;
    QString username;  // Переменная для хранения логина
    QString password;  // Переменная для хранения пароля
    bool isFullAccessRequest = false; // Флаг для отслеживания типа запроса
    bool isTableModified = false;
    QPushButton *addRowButton = nullptr;
    QPushButton *saveButton = nullptr;
    QPushButton *deleteButton = nullptr;
    QTreeWidgetItem *previousItem = nullptr;




private slots:
    void onSocketConnected();  // Добавлен слот для обработки события подключения к серверу
    void onSocketDisconnected();  // Добавлен слот для обработки события отключения от сервера
    void readServerResponse();
    void onDatabaseOrTableClicked(QTreeWidgetItem *item, int column);
    void onLoginDataEntered(const QString &username, const QString &password);
    void onGetButtonClicked();
    void saveDataToLocalDatabase(const QJsonObject &data);
    void onPushToServerButtonClicked();
     QString getToLocalServer();
    void openButtonClicked();
     void onAddRowClicked();
    void onSaveButtonClicked(bool isTransition);
     void onSelectionChanged();
    void onDeleteRowClicked();
};

#endif // CLIENT_H
