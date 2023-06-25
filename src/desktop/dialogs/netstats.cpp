// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpcommon/memory_pool.h>
#include <dpengine/tile.h>
}

#include "desktop/dialogs/netstats.h"
#include "libclient/drawdance/global.h"
#include "ui_netstats.h"

#include <QTimer>

namespace dialogs {

NetStats::NetStats(QWidget *parent)
	: QDialog(parent)
	, m_ui(new Ui_NetStats)
	, m_updateMemoryTimer{new QTimer{this}}
{
	m_ui->setupUi(this);
	m_updateMemoryTimer->setInterval(5000);
	m_updateMemoryTimer->setTimerType(Qt::CoarseTimer);
	connect(
		m_updateMemoryTimer, &QTimer::timeout, this,
		&NetStats::updateMemoryUsagePeriodic);
	setDisconnected();
}

NetStats::~NetStats()
{
	delete m_ui;
}

void NetStats::updateMemoryUsage()
{
	DP_MemoryPoolStatistics mps = DP_tile_memory_usage();
	size_t tileElementsTotal = mps.buckets_len * mps.bucket_el_count;
	size_t tileElementsUsed = tileElementsTotal - mps.el_free;
	size_t tileBytesTotal = tileElementsTotal * mps.el_size;
	size_t tileBytesUsed = tileElementsUsed * mps.el_size;
	m_ui->tilesLabel->setText(
		QStringLiteral("%1 / %2").arg(tileElementsUsed).arg(tileElementsTotal));
	m_ui->tileMemoryLabel->setText(QStringLiteral("%1 / %2").arg(
		formatDataSize(tileBytesUsed), formatDataSize(tileBytesTotal)));

	drawdance::DrawContextPoolStatistics dpcs =
		drawdance::DrawContextPool::statistics();
	m_ui->contextsLabel->setText(QStringLiteral("%1 / %2")
									 .arg(dpcs.contextsUsed)
									 .arg(dpcs.contextsTotal));
	m_ui->contextMemoryLabel->setText(QStringLiteral("%1 / %2").arg(
		formatDataSize(dpcs.bytesUsed), formatDataSize(dpcs.bytesTotal)));

	if(!m_updateMemoryTimer->isActive()) {
		m_updateMemoryTimer->start();
	}
}

void NetStats::setRecvBytes(int bytes)
{
	m_ui->recvLabel->setText(formatDataSize(bytes));
}

void NetStats::setSentBytes(int bytes)
{
	m_ui->sentLabel->setText(formatDataSize(bytes));
}

void NetStats::setCurrentLag(int lag)
{
	m_ui->lagLabel->setText(QStringLiteral("%1 ms").arg(lag));
}

void NetStats::setDisconnected()
{
	m_ui->lagLabel->setText(tr("not connected"));
}

void NetStats::updateMemoryUsagePeriodic()
{
	if(isVisible()) {
		updateMemoryUsage();
	} else {
		m_updateMemoryTimer->stop();
	}
}

QString NetStats::formatDataSize(size_t bytes)
{
	return QLocale::c().formattedDataSize(bytes);
}

}
