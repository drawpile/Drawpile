// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_ASYNCTASKRUNNABLE
#define LIBCLIENT_UTILS_ASYNCTASKRUNNABLE
#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <QVector>

class AsyncTaskRunnable;

enum class AsyncTaskStatus { Ready, Ok, Cancelled, Error };

class AsyncTaskResult final {
public:
	explicit AsyncTaskResult(
		AsyncTaskStatus status, bool proceed, const QString &errorMessage)
		: m_errorMessage(errorMessage)
		, m_status(status)
		, m_proceed(proceed)
	{
	}

	AsyncTaskResult()
		: AsyncTaskResult(AsyncTaskStatus::Ready, true, QString())
	{
	}

	static AsyncTaskResult ok(bool proceed = true)
	{
		return AsyncTaskResult(AsyncTaskStatus::Ok, proceed, QString());
	}

	static AsyncTaskResult cancelled(bool proceed = false)
	{
		return AsyncTaskResult(AsyncTaskStatus::Cancelled, proceed, QString());
	}

	static AsyncTaskResult errorStop(const QString &errorMessage)
	{
		return AsyncTaskResult(AsyncTaskStatus::Error, false, errorMessage);
	}

	static AsyncTaskResult errorProceed(const QString &errorMessage)
	{
		return AsyncTaskResult(AsyncTaskStatus::Error, true, errorMessage);
	}

	AsyncTaskStatus status() const { return m_status; }
	bool proceed() { return m_proceed; }
	const QString &errorMessage() const { return m_errorMessage; }

private:
	QString m_errorMessage;
	AsyncTaskStatus m_status;
	bool m_proceed;
};

class AsyncTask {
	friend class AsyncTaskRunnable;
	Q_DISABLE_COPY_MOVE(AsyncTask)

public:
	AsyncTask() = default;
	virtual ~AsyncTask() = default;

	const AsyncTaskResult &result() const { return m_result; }

	virtual AsyncTaskResult run() = 0;

	virtual void cancel() {}

protected:
	bool isCancelled() const;
	void emitProgress(double percent);

private:
	AsyncTaskRunnable *m_parent = nullptr;
	AsyncTaskResult m_result;
};

class AsyncTaskRunnable final : public QObject, public QRunnable {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(AsyncTaskRunnable)
public:
	explicit AsyncTaskRunnable(
		const QVector<AsyncTask *> &tasks, QObject *parent = nullptr);

	~AsyncTaskRunnable();

	AsyncTask *taskAt(int index) { return m_tasks[index]; }
	int taskCount() const { return m_tasks.size(); }

	void run() override;

	void cancel();
	bool isCancelled() const { return m_cancelled.loadRelaxed() != 0; }

	void emitProgress(double percent);

Q_SIGNALS:
	void progress(int percent);
	void taskFinished(int index);
	void runFinished(int count);

private:
	QVector<AsyncTask *> m_tasks;
	QAtomicInt m_atomicTaskIndex;
	QAtomicInt m_cancelled;
	int m_currentTaskIndex = 0;
};

#endif
