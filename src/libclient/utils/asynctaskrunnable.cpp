// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/asynctaskrunnable.h"


bool AsyncTask::isCancelled() const
{
	return m_parent->isCancelled();
}

void AsyncTask::emitProgress(double percent)
{
	m_parent->emitProgress(qBound(0.0, percent, 100.0));
}


AsyncTaskRunnable::AsyncTaskRunnable(
	const QVector<AsyncTask *> &tasks, QObject *parent)
	: QObject(parent)
	, m_tasks(tasks)
{
	for(AsyncTask *task : m_tasks) {
		task->m_parent = this;
	}
}

AsyncTaskRunnable::~AsyncTaskRunnable()
{
	for(AsyncTask *task : m_tasks) {
		delete task;
	}
}

void AsyncTaskRunnable::run()
{
	int taskCount = m_tasks.size();
	m_currentTaskIndex = 0;
	while(m_currentTaskIndex < taskCount && !isCancelled()) {
		m_atomicTaskIndex.storeRelaxed(m_currentTaskIndex);
		AsyncTask *task = m_tasks[m_currentTaskIndex];
		task->m_result = task->run();
		emitProgress(100.0);
		Q_EMIT taskFinished(m_currentTaskIndex);
		++m_currentTaskIndex;
		if(!task->m_result.proceed()) {
			break;
		}
	}
	Q_EMIT runFinished(m_currentTaskIndex);
}

void AsyncTaskRunnable::cancel()
{
	m_cancelled.storeRelaxed(1);
	int taskIndex = m_atomicTaskIndex.loadRelaxed();
	if(taskIndex >= 0 && taskIndex < m_tasks.size()) {
		m_tasks[taskIndex]->cancel();
	}
}

void AsyncTaskRunnable::emitProgress(double percent)
{
	int taskCount = m_tasks.size();
	double actualProgress;
	if(taskCount > 1) {
		actualProgress = (double(m_currentTaskIndex * 100) + percent) /
						 double(taskCount * 100) * 100.0;
	} else {
		actualProgress = percent;
		Q_EMIT progress(percent);
	}
	Q_EMIT progress(qBound(0, qRound(actualProgress), 100));
}
