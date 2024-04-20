// SPDX-License-Identifier: GPL-3.0-or-later
#include "sql_qt.h"
#include <QLoggingCategory>
extern "C" {
#include "sql.h"
#include <dpcommon/threading.h>
}

Q_LOGGING_CATEGORY(lcDpDatabase, "net.drawpile.database", QtWarningMsg)

namespace drawdance {

class MutexLocker final {
  public:
    MutexLocker(DP_Mutex *mutex) : m_mutex(mutex)
    {
        DP_MUTEX_MUST_LOCK(mutex);
    }

    ~MutexLocker()
    {
        DP_MUTEX_MUST_UNLOCK(m_mutex);
    }

  private:
    DP_Mutex *m_mutex;
};


Query::Query(Database &db) : m_db(db)
{
    m_db.lock();
}

Query::~Query()
{
    sqlite3_finalize(m_stmt);
    m_db.unlock();
}

bool Query::prepare(const char *sql)
{
    sqlite3_finalize(m_stmt);
    m_stmt = m_db.prepare(sql);
    return m_stmt != nullptr;
}

bool Query::prepare(const QString &sql)
{
    sqlite3_finalize(m_stmt);
    m_stmt = m_db.prepare(sql);
    return m_stmt != nullptr;
}

bool Query::bind(int pos, const Param &param)
{
    return m_db.bind(m_stmt, pos, param);
}

bool Query::bindAll(const QVector<Param> &params)
{
    int count = int(params.size());
    for (int i = 0; i < count; ++i) {
        if (!bind(i, params[i])) {
            return false;
        }
    }
    return true;
}

bool Query::execPrepared()
{
    int result = m_db.step(m_stmt);
    switch (result) {
    case SQLITE_ROW:
        m_ready = true;
        m_done = false;
        return true;
    case SQLITE_DONE:
        m_done = true;
        return true;
    default:
        m_done = true;
        return false;
    }
}

bool Query::next()
{
    if (m_done) {
        return false;
    }
    else if (m_ready) {
        m_ready = false;
        return true;
    }
    else {
        switch (m_db.step(m_stmt)) {
        case SQLITE_ROW:
            return true;
        case SQLITE_DONE:
            m_done = true;
            return false;
        default:
            m_done = true;
            return false;
        }
    }
}

qlonglong Query::lastInsertId()
{
    return m_db.lastInsertId();
}

qlonglong Query::numRowsAffected()
{
    return m_db.numRowsAffected();
}

bool Query::columnNull(int column)
{
    return sqlite3_column_type(m_stmt, column) == SQLITE_NULL;
}

bool Query::columnBool(int column)
{
    return sqlite3_column_int(m_stmt, column) != 0;
}

int Query::columnInt(int column)
{
    return sqlite3_column_int(m_stmt, column);
}

qlonglong Query::columnInt64(int column)
{
    return sqlite3_column_int64(m_stmt, column);
}

qreal Query::columnReal(int column)
{
    return sqlite3_column_double(m_stmt, column);
}

QByteArray Query::columnText8(int column, bool transient)
{
    const void *text = sqlite3_column_text(m_stmt, column);
    if (text) {
        int size = sqlite3_column_bytes(m_stmt, column);
        if (size > 0) {
            const char *ptr = reinterpret_cast<const char *>(text);
            return transient ? QByteArray::fromRawData(ptr, size)
                             : QByteArray(ptr, size);
        }
    }
    return QByteArray();
}

QString Query::columnText16(int column, bool transient)
{
    const void *text16 = sqlite3_column_text16(m_stmt, column);
    if (text16) {
        int len = sqlite3_column_bytes16(m_stmt, column) / 2;
        if (len > 0) {
            const QChar *ptr = reinterpret_cast<const QChar *>(text16);
            return transient ? QString::fromRawData(ptr, len)
                             : QString(ptr, len);
        }
    }
    return QString();
}

QByteArray Query::columnBlob(int column, bool transient)
{
    const void *blob = sqlite3_column_blob(m_stmt, column);
    if (blob) {
        int size = sqlite3_column_bytes(m_stmt, column);
        if (size > 0) {
            const char *ptr = reinterpret_cast<const char *>(blob);
            return transient ? QByteArray::fromRawData(ptr, size)
                             : QByteArray(ptr, size);
        }
    }
    return QByteArray();
}


Database::Database()
{
}

Database::~Database()
{
    close();
}

bool Database::open(const QString &path, const QString &humaneName)
{
    int init_result = DP_sql_init();
    if (init_result != SQLITE_OK) {
        qCCritical(lcDpDatabase, "Database %s: %s", qUtf8Printable(humaneName),
                   DP_error());
        return false;
    }

    if (!m_mutex) {
        m_mutex = DP_mutex_new_recursive();
        if (!m_mutex) {
            qCCritical(lcDpDatabase, "Database %s: %s",
                       qUtf8Printable(humaneName), DP_error());
            return false;
        }
    }

    MutexLocker locker(m_mutex);

    sqlite3 *db;
    int open_result =
        sqlite3_open_v2(qUtf8Printable(path), &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX
                            | SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_EXRESCODE,
                        NULL);
    if (open_result != SQLITE_OK) {
        return false;
    }

    const char *main_schema = sqlite3_db_name(db, 0);
    if (sqlite3_db_readonly(db, main_schema) > 0) {
        qCWarning(lcDpDatabase, "Database %s: main schema is read-only",
                  qUtf8Printable(humaneName));
        sqlite3_close(db);
        return false;
    }

    if (m_db) {
        int close_result = sqlite3_close_v2(m_db);
        qCWarning(lcDpDatabase,
                  "Database %s failed to close while opening database %s: %s",
                  qUtf8Printable(m_humaneName), qUtf8Printable(humaneName),
                  sqlite3_errstr(close_result));
    }

    m_db = db;
    m_humaneName = humaneName;
    return true;
}

bool Database::close()
{
    if (m_db) {
        MutexLocker locker(m_mutex);
        int result = sqlite3_close_v2(m_db);
        m_db = nullptr;
        if (result != SQLITE_OK) {
            qCWarning(lcDpDatabase, "Database %s failed to close: %s",
                      qUtf8Printable(m_humaneName), sqlite3_errstr(result));
            return false;
        }
    }
    return true;
}

void Database::lock()
{
    DP_MUTEX_MUST_LOCK(m_mutex);
}

void Database::unlock()
{
    DP_MUTEX_MUST_UNLOCK(m_mutex);
}

bool Database::enableWalMode()
{
    QString mode = readText16("pragma journal_mode = WAL");
    return QStringLiteral("wal").compare(mode, Qt::CaseInsensitive) == 0;
}

sqlite3_stmt *Database::prepare(const char *sql)
{
    sqlite3_stmt *stmt;
    int len = int(strlen(sql));
    int result = sqlite3_prepare_v3(m_db, sql, len, 0, &stmt, nullptr);
    if (result == SQLITE_OK) {
        return stmt;
    }
    else {
        qCWarning(lcDpDatabase,
                  "Database %s failed to prepare statement '%s': %s",
                  qUtf8Printable(m_humaneName), sql, getError(result));
        return nullptr;
    }
}

sqlite3_stmt *Database::prepare(const QString &sql)
{
    sqlite3_stmt *stmt;
    int result = sqlite3_prepare16_v3(m_db, sql.constData(),
                                      int(sql.size() * 2), 0, &stmt, nullptr);
    if (result == SQLITE_OK) {
        return stmt;
    }
    else {
        qCWarning(lcDpDatabase,
                  "Database %s failed to prepare statement '%s': %s",
                  qUtf8Printable(m_humaneName), qUtf8Printable(sql),
                  getError(result));
        return nullptr;
    }
}

bool Database::bind(sqlite3_stmt *stmt, int pos, const Query::Param &param)
{
    int result = bindValue(stmt, pos + 1, param.m_type, param.m_value);
    if (result == SQLITE_OK) {
        return true;
    }
    else {
        qCWarning(lcDpDatabase,
                  "Database %s failed to bind parameter %d in '%s': %s",
                  qUtf8Printable(m_humaneName), pos, sqlite3_sql(stmt),
                  getError(result));
        return false;
    }
}

int Database::step(sqlite3_stmt *stmt)
{
    int result = sqlite3_step(stmt);
    switch (result) {
    case SQLITE_ROW:
    case SQLITE_DONE:
        break;
    default:
        qCWarning(
            lcDpDatabase, "Database %s failed to execute statement '%s': %s",
            qUtf8Printable(m_humaneName), sqlite3_sql(stmt), getError(result));
        break;
    }
    return result;
}

qlonglong Database::lastInsertId()
{
    return sqlite3_last_insert_rowid(m_db);
}

qlonglong Database::numRowsAffected()
{
    return sqlite3_changes64(m_db);
}

int Database::readInt(const char *sql, int defaultValue)
{
    Query qry = query();
    return qry.exec(sql) && qry.next() ? qry.columnInt(0) : defaultValue;
}

int Database::readInt(const QString &sql, int defaultValue)
{
    Query qry = query();
    return qry.exec(sql) && qry.next() ? qry.columnInt(0) : defaultValue;
}

int Database::readInt(const char *sql, int defaultValue,
                      const QVector<Query::Param> &params)
{
    Query qry = query();
    return qry.exec(sql, params) && qry.next() ? qry.columnInt(0)
                                               : defaultValue;
}

int Database::readInt(const QString &sql, int defaultValue,
                      const QVector<Query::Param> &params)
{
    Query qry = query();
    return qry.exec(sql, params) && qry.next() ? qry.columnInt(0)
                                               : defaultValue;
}

QString Database::readText16(const char *sql)
{
    Query qry = query();
    return qry.exec(sql) && qry.next() ? qry.columnText16(0) : QString();
}

QString Database::readText16(const QString &sql)
{
    Query qry = query();
    return qry.exec(sql) && qry.next() ? qry.columnText16(0) : QString();
}

QString Database::readText16(const char *sql,
                             const QVector<Query::Param> &params)
{
    Query qry = query();
    return qry.exec(sql, params) && qry.next() ? qry.columnText16(0)
                                               : QString();
}

QString Database::readText16(const QString &sql,
                             const QVector<Query::Param> &params)
{
    Query qry = query();
    return qry.exec(sql, params) && qry.next() ? qry.columnText16(0)
                                               : QString();
}

QByteArray Database::readBlob(const char *sql)
{
    Query qry = query();
    return qry.exec(sql) && qry.next() ? qry.columnBlob(0) : QByteArray();
}

QByteArray Database::readBlob(const QString &sql)
{
    Query qry = query();
    return qry.exec(sql) && qry.next() ? qry.columnBlob(0) : QByteArray();
}

QByteArray Database::readBlob(const char *sql,
                              const QVector<Query::Param> &params)
{
    Query qry = query();
    return qry.exec(sql, params) && qry.next() ? qry.columnBlob(0)
                                               : QByteArray();
}

QByteArray Database::readBlob(const QString &sql,
                              const QVector<Query::Param> &params)
{
    Query qry = query();
    return qry.exec(sql, params) && qry.next() ? qry.columnBlob(0)
                                               : QByteArray();
}

int Database::bindValue(sqlite3_stmt *stmt, int i, Query::Param::Type type,
                        const QVariant &value)
{
    switch (type) {
    case Query::Param::Type::Null:
        return sqlite3_bind_null(stmt, i);
    case Query::Param::Type::Bool:
        return sqlite3_bind_int(stmt, i, value.toBool() ? 1 : 0);
    case Query::Param::Type::Int:
        return sqlite3_bind_int(stmt, i, value.toInt());
    case Query::Param::Type::Int64:
        return sqlite3_bind_int64(stmt, i, value.toLongLong());
    case Query::Param::Type::Real:
        return sqlite3_bind_double(stmt, i, value.toReal());
    case Query::Param::Type::Text8:
    case Query::Param::Type::Text16: {
        QByteArray b = type == Query::Param::Type::Text8
                         ? value.toByteArray()
                         : value.toString().toUtf8();
        return sqlite3_bind_text64(stmt, i, b.constData(),
                                   sqlite_uint64(b.size()), SQLITE_TRANSIENT,
                                   SQLITE_UTF8);
    }
    case Query::Param::Type::Blob: {
        QByteArray b = value.toByteArray();
        return sqlite3_bind_blob64(stmt, i, b.constData(),
                                   sqlite_uint64(b.size()), SQLITE_TRANSIENT);
    }
    }
    DP_UNREACHABLE();
}

const char *Database::getError(int result)
{
    if (result != SQLITE_MISUSE && m_db) {
        const char *error = sqlite3_errmsg(m_db);
        if (error) {
            return error;
        }
    }
    return sqlite3_errstr(result);
}


}
