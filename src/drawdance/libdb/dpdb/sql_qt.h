// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPDB_SQL_QT_H
#define DPDB_SQL_QT_H
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVector>

struct DP_Mutex;
struct sqlite3;
struct sqlite3_stmt;

namespace drawdance {

class Database;

class Query final {
  public:
    class Param {
        friend class Database;

      public:
        Param() : Param(Type::Null, QVariant())
        {
        }

        Param(bool value) : Param(Type::Bool, value)
        {
        }

        Param(int value) : Param(Type::Int, value)
        {
        }

        Param(qlonglong value) : Param(Type::Int64, value)
        {
        }

        Param(qreal value) : Param(Type::Real, value)
        {
        }

        Param(const char *value) : Param(Type::Text8, QByteArray(value))
        {
        }

        Param(const QString &value) : Param(Type::Text16, value)
        {
        }

        Param(const QByteArray &value) : Param(Type::Blob, value)
        {
        }

      private:
        enum class Type {
            Null,
            Bool,
            Int,
            Int64,
            Real,
            Text8,
            Text16,
            Blob,
        };

        Param(Type type, const QVariant &value) : m_type(type), m_value(value)
        {
        }

        const Type m_type;
        const QVariant m_value;
    };

    Query(Database &db);
    ~Query();

    Query(const Query &) = delete;
    Query(Query &&) = delete;
    Query &operator=(const Query &) = delete;
    Query &operator=(Query &&) = delete;

    bool exec(const char *sql)
    {
        return prepare(sql) && execPrepared();
    }

    bool exec(const QString &sql)
    {
        return prepare(sql) && execPrepared();
    }

    bool exec(const char *sql, const QVector<Param> &params)
    {
        return prepare(sql) && bindAll(params) && execPrepared();
    }

    bool exec(const QString &sql, const QVector<Param> &params)
    {
        return prepare(sql) && bindAll(params) && execPrepared();
    }

    bool prepare(const char *sql);
    bool prepare(const QString &sql);
    bool bind(int pos, const Param &param);
    bool bindAll(const QVector<Param> &params);
    bool execPrepared();
    bool next();

    qlonglong lastInsertId();
    qlonglong numRowsAffected();
    bool columnNull(int column);
    bool columnBool(int column);
    int columnInt(int column);
    qlonglong columnInt64(int column);
    qreal columnReal(int column);
    QByteArray columnText8(int column, bool transient = false);
    QString columnText16(int column, bool transient = false);
    QByteArray columnBlob(int column, bool transient = false);

  private:
    Database &m_db;
    sqlite3_stmt *m_stmt = nullptr;
    bool m_ready = false;
    bool m_done = true;
};

class Database final {
  public:
    Database();
    ~Database();

    Database(const Database &) = delete;
    Database(Database &&) = delete;
    Database &operator=(const Database &) = delete;
    Database &operator=(Database &&) = delete;

    bool open(const QString &path, const QString &humaneName);
    bool close();

    bool isOpen() const
    {
        return m_db != nullptr;
    }

    const QString &humaneName() const
    {
        return m_humaneName;
    }

    Query query()
    {
        return Query(*this);
    }

    void lock();
    void unlock();

    bool enableWalMode();

    sqlite3_stmt *prepare(const char *sql);
    sqlite3_stmt *prepare(const QString &sql);
    bool bind(sqlite3_stmt *stmt, int pos, const Query::Param &param);
    int step(sqlite3_stmt *stmt);

    qlonglong lastInsertId();
    qlonglong numRowsAffected();

    int readInt(const char *sql, int defaultValue = 0);
    int readInt(const QString &sql, int defaultValue = 0);
    int readInt(const char *sql, int defaultValue,
                const QVector<Query::Param> &params);
    int readInt(const QString &sql, int defaultValue,
                const QVector<Query::Param> &params);

    QString readText16(const char *sql);
    QString readText16(const QString &sql);
    QString readText16(const char *sql, const QVector<Query::Param> &params);
    QString readText16(const QString &sql, const QVector<Query::Param> &params);

    QByteArray readBlob(const char *sql);
    QByteArray readBlob(const QString &sql);
    QByteArray readBlob(const char *sql, const QVector<Query::Param> &params);
    QByteArray readBlob(const QString &sql,
                        const QVector<Query::Param> &params);

  private:
    int bindValue(sqlite3_stmt *stmt, int i, Query::Param::Type type,
                  const QVariant &value);

    const char *getError(int result);

    sqlite3 *m_db = nullptr;
    DP_Mutex *m_mutex = nullptr;
    QString m_humaneName;
};

}

#endif
