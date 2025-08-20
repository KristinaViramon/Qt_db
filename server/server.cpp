#include "server.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

Server::Server(QObject *parent) : QObject(parent)
{
    bdServer = new QTcpServer(this);

    connect(bdServer, &QTcpServer::newConnection, this, &Server::newClientConnection);
}

void Server::startServer()
{
    if (bdServer->listen(QHostAddress("127.0.0.1"), 12345)) {
        qDebug() << "Server listening on port 12345...";
    } else {
        qDebug() << "Error: Unable to start the server.";
    }
}

void Server::newClientConnection()
{
    QTcpSocket *clientSocket = bdServer->nextPendingConnection();
      connect(clientSocket, &QTcpSocket::readyRead, this, &Server::socketReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
}
void Server::socketDisconnected()
{
    QTcpSocket *disconnectedSocket = qobject_cast<QTcpSocket*>(sender());
    disconnectedSocket->deleteLater();
}

void Server::socketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QString receivedMessage = QString::fromUtf8(data);
    QStringList parts = receivedMessage.split('|');

    if (parts.size() < 1) return;

    QString command = parts[0];
    if (command == "GET_DATABASES") {
        QString response = getDatabases(parts[1], parts[2]); // parts[1] - username, parts[2] - password
        socket->write(response.toUtf8());
    } else if (command == "GET_FULL_ACCESS_DATABASES") {
        QString response = getFullAccessDatabases(parts[1], parts[2]); // parts[1] - username, parts[2] - password
        socket->write(response.toUtf8());
    }else if(command == "PUSH_DATA"){
        QString response = saveDataToServerDatabase(parts[1], parts[2], parts[3]);
    }
}


QString Server::getDatabases(const QString& username, const QString& password)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setPort(5432);
    db.setDatabaseName("kurs");  // Укажите вашу базу данных, которая подключена по умолчанию
    db.setUserName("username");  // Ваше имя пользователя
    db.setPassword(password);      // Ваш пароль

    if (!db.open()) {
        qDebug() << "Ошибка подключения к базе данных:" << db.lastError().text();
        return R"({"error": "Не удалось подключиться к основной базе данных."})";
    }

    QSqlQuery query("SELECT datname FROM pg_database WHERE datistemplate = false;", db);
    QJsonObject resultObject;  // Объект, который будет содержать все базы данных
    bool anyDatabaseAccessible = false;

    while (query.next()) {
        QString dbName = query.value(0).toString();

        // Подключаемся к каждой базе данных
        QSqlDatabase dbInstance = QSqlDatabase::addDatabase("QPSQL", dbName);
        dbInstance.setHostName("localhost");
        dbInstance.setPort(5432);
        dbInstance.setUserName(username);
        dbInstance.setPassword(password);
        dbInstance.setDatabaseName(dbName);

        if (!dbInstance.open()) {
            qDebug() << "Ошибка подключения к базе данных" << dbName;
            continue; // Переходим к следующей базе, если не удается подключиться
        }

        anyDatabaseAccessible = true;

        // Сохраняем таблицы и их данные
        QSqlQuery tablesQuery("SELECT table_name FROM information_schema.tables WHERE table_schema = 'public';", dbInstance);
        QJsonObject dbObject;  // Объект для базы данных

        while (tablesQuery.next()) {
            QString tableName = tablesQuery.value(0).toString();

            // Запрашиваем данные из каждой таблицы
            QSqlQuery dataQuery(QString("SELECT * FROM \"%1\"").arg(tableName), dbInstance);
            QJsonArray tableDataArray;  // Массив данных для таблицы

            while (dataQuery.next()) {
                QJsonObject rowObject;
                for (int i = 0; i < dataQuery.record().count(); ++i) {
                    rowObject[dataQuery.record().fieldName(i)] = dataQuery.value(i).toString();
                }
                tableDataArray.append(rowObject);
            }

            dbObject[tableName] = tableDataArray;
        }

        resultObject[dbName] = dbObject;

        dbInstance.close();
    }

    db.close();

    if (!anyDatabaseAccessible) {
        // Если ни одна база данных не доступна
        return R"({"error": "Доступ к базам данных с указанными учетными данными невозможен."})";
    }

    return QJsonDocument(resultObject).toJson(QJsonDocument::Compact);
}


QString Server::getFullAccessDatabases(const QString& username, const QString& password)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setPort(5432);
    db.setDatabaseName("kurs");  // База данных для подключения по умолчанию
    db.setUserName("postgres");  // Имя пользователя для подключения
    db.setPassword("8698");      // Пароль для подключения

    if (!db.open()) {
        qDebug() << "Database connection error:" << db.lastError().text();
        return "{}";  // Возвращаем пустой объект JSON в случае ошибки
    }

    // Получаем список всех баз данных, исключая 'postgres'
    QSqlQuery query(
        "SELECT datname "
        "FROM pg_database "
        "WHERE datistemplate = false AND datname != 'postgres';", db);

    QJsonObject resultObject;

    while (query.next()) {
        QString dbName = query.value(0).toString();

        // Подключаемся к каждой базе данных
        QSqlDatabase dbInstance = QSqlDatabase::addDatabase("QPSQL", dbName);
        dbInstance.setHostName("localhost");
        dbInstance.setPort(5432);
        dbInstance.setUserName(username);
        dbInstance.setPassword(password);
        dbInstance.setDatabaseName(dbName);

        if (!dbInstance.open()) {
            qDebug() << "Error connecting to database" << dbName << ":" << dbInstance.lastError().text();
            continue;  // Если не удалось подключиться, переходим к следующей базе данных
        }

        // Проверяем доступные таблицы для пользователя с полными привилегиями
        QSqlQuery tablesQuery(QString(
                                  "SELECT table_name "
                                  "FROM information_schema.role_table_grants "
                                  "WHERE grantee = '%1' "
                                  "AND privilege_type IN ('INSERT', 'UPDATE', 'DELETE') "
                                  "AND table_schema = 'public';")
                                  .arg(username), dbInstance);
        QJsonObject dbObject;

        while (tablesQuery.next()) {
            QString tableName = tablesQuery.value(0).toString();

            // Запросим данные из каждой таблицы
            QSqlQuery dataQuery(QString("SELECT * FROM \"%1\"").arg(tableName), dbInstance);
            QJsonArray tableDataArray;

            while (dataQuery.next()) {
                QJsonObject rowObject;
                for (int i = 0; i < dataQuery.record().count(); ++i) {
                    rowObject[dataQuery.record().fieldName(i)] = dataQuery.value(i).toString();
                }
                tableDataArray.append(rowObject);
            }

            // Добавляем данные таблицы в объект базы данных
            dbObject[tableName] = tableDataArray;
        }

        // Добавляем базу данных с её таблицами в общий результат
        resultObject[dbName] = dbObject;

        dbInstance.close();
    }

    db.close();
    return QJsonDocument(resultObject).toJson(QJsonDocument::Compact);
}

QString Server::saveDataToServerDatabase(const QString& username, const QString& password, const QString& jsonString)
{
    // Преобразуем строку JSON в QJsonDocument
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qDebug() << "Invalid JSON format.";
        return "ERROR: Invalid JSON format.";
    }

    QJsonObject rootObject = jsonDoc.object();

    // Подключаемся к основной базе данных для проверки учетных данных
    QSqlDatabase localDb = QSqlDatabase::addDatabase("QPSQL", "serverConnection");
    localDb.setHostName("localhost");
    localDb.setPort(5432);
    localDb.setDatabaseName("kurs");
    localDb.setUserName(username);
    localDb.setPassword(password);

    if (!localDb.open()) {
        qDebug() << "Database connection error:" << localDb.lastError().text();
        return "ERROR: Failed to connect to the database.";
    }

    QSqlQuery query(localDb);

    for (const QString &dbName : rootObject.keys()) {
        QJsonObject dbObject = rootObject.value(dbName).toObject();

        // Подключаемся к базе данных
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", dbName);
        db.setHostName("localhost");
        db.setPort(5432);
        db.setUserName(username);
        db.setPassword(password);
        db.setDatabaseName(dbName);

        if (!db.open()) {
            qDebug() << "Не удалось подключиться к базе данных:" << dbName << ":" << db.lastError().text();
            continue;
        }

        QSqlQuery tableQuery(db);
        db.transaction(); // Начинаем транзакцию

        for (const QString &tableName : dbObject.keys()) {
            QJsonArray tableData = dbObject.value(tableName).toArray();

            if (tableData.isEmpty()) {
                continue; // Пропускаем пустые таблицы
            }

            // Удаляем старые данные из таблицы
            QString deleteQuery = QString("DELETE FROM \"%1\"").arg(tableName);
            if (!tableQuery.exec(deleteQuery)) {
                qDebug() << "Ошибка при очистке таблицы:" << tableQuery.lastError().text();
                db.rollback();
                db.close();
                continue;
            }

            // Вставляем новые данные
            for (const QJsonValue &rowValue : tableData) {
                QJsonObject row = rowValue.toObject();
                QStringList columnNames = row.keys();
                QStringList columnValues;

                for (const QString &columnName : columnNames) {
                    columnValues.append(QString("'%1'").arg(row.value(columnName).toString().replace("'", "''")));
                }

                QString insertQuery = QString("INSERT INTO \"%1\" (%2) VALUES (%3)")
                                          .arg(tableName)
                                          .arg(columnNames.join(", "))
                                          .arg(columnValues.join(", "));

                if (!tableQuery.exec(insertQuery)) {
                    qDebug() << "Ошибка при вставке данных в таблицу:" << tableQuery.lastError().text();
                    db.rollback();
                    db.close();
                    continue;
                }
            }
        }

        if (!db.commit()) {
            qDebug() << "Ошибка при фиксации транзакции в базе данных:" << dbName << ":" << db.lastError().text();
            db.close();
            continue;
        }

        db.close();
    }

    localDb.close();
    qDebug() << "Данные успешно обновлены";
    return "Данные успешно обновлены";
}
