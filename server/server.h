#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QVector>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    QString getUserLoginsString() const;
    void startServer();


public slots:
    void newClientConnection();
     void socketDisconnected();
     void socketReadyRead();

private:
     QMap<QTcpSocket*, QByteArray> clientBuffers; // Буферы для клиентов
    QTcpServer*             bdServer;
    QString getDatabases(const QString& username, const QString& password);
    QString getFullAccessDatabases(const QString &username, const QString &password);
    QString saveDataToServerDatabase(const QString& username, const QString& password, const QString& jsonString);
};

#endif // SERVER_H
