// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPDB_SQL_QT_H
#define DPDB_SQL_QT_H
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVector>
#include <functional>
#include <optional>

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

        Param(const std::nullopt_t &) : Param()
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

        static Param null()
        {
            return Param();
        }

        template <typename T>
        static Param fromOptional(const std::optional<T> &value)
        {
            return value.has_value() ? Param(value.value()) : null();
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

        Type m_type;
        QVariant m_value;
    };

    explicit Query(Database &db, bool lock);
    ~Query();

    Query(const Query &) = delete;
    Query(Query &&) = delete;
    Query &operator=(const Query &) = delete;
    Query &operator=(Query &&) = delete;

    bool enableWalMode();
    bool setForeignKeysEnabled(bool enabled);

    bool tx(const std::function<bool()> &block);
    bool isInTx() const;

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

    bool prepare(const char *sql, unsigned int flags = 0);
    bool prepare(const QString &sql, unsigned int flags = 0);
    bool bind(int pos, const Param &param);
    bool bindAll(const QVector<Param> &params);
    bool execPrepared();
    bool next();

    qlonglong lastInsertId() const;
    qlonglong numRowsAffected() const;
    bool columnNull(int column) const;
    bool columnBool(int column) const;
    int columnInt(int column) const;
    qlonglong columnInt64(int column) const;
    qreal columnReal(int column) const;
    QByteArray columnText8(int column, bool transient = false) const;
    QString columnText16(int column, bool transient = false) const;
    QByteArray columnBlob(int column, bool transient = false) const;

  private:
    void resetStatement();
    void resetQueryState()
    {
        m_started = false;
        m_ready = false;
        m_done = true;
    }

    Database &m_db;
    sqlite3_stmt *m_stmt = nullptr;
    bool m_started = false;
    bool m_ready = false;
    bool m_done = true;
    const bool m_lock;
};

class Database final {
  public:
    static constexpr unsigned int PREPARE_PERSISTENT = 0x1;

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
        return Query(*this, true);
    }

    Query queryWithoutLock()
    {
        return Query(*this, false);
    }

    bool tx(const std::function<bool(Query &)> &block);
    bool txWithoutLock(const std::function<bool(Query &)> &block);
    bool isInTx() const;

    void lock();
    void unlock();

    sqlite3_stmt *prepare(const char *sql, unsigned int flags = 0);
    sqlite3_stmt *prepare(const QString &sql, unsigned int flags = 0);
    bool bind(sqlite3_stmt *stmt, int pos, const Query::Param &param);
    int step(sqlite3_stmt *stmt);

    qlonglong lastInsertId() const;
    qlonglong numRowsAffected() const;

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

class DatabaseLocker final {
  public:
    DatabaseLocker(Database &db) : m_db(db)
    {
        m_db.lock();
    }

    ~DatabaseLocker()
    {
        m_db.unlock();
    }

  private:
    Database &m_db;
};

}

#endif
