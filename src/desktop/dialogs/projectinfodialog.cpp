// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/project.h>
}
#include "desktop/dialogs/projectinfodialog.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/utils/scopedoverridecursor.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dialogs {

ProjectInfoDialog::ProjectInfoDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Project Information"));
	setWindowModality(Qt::NonModal);
	resize(800, 600);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	m_statusLabel = new QLabel;
	m_statusLabel->setWordWrap(true);
	m_statusLabel->setTextFormat(Qt::RichText);
	layout->addWidget(m_statusLabel);

	QTabWidget *tabs = new QTabWidget;
	layout->addWidget(tabs, 1);

	m_headerTable = new QTableWidget(0, 2);
	m_headerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_headerTable->verticalHeader()->hide();
	m_headerTable->setHorizontalHeaderLabels(
		{QStringLiteral("Key"), QStringLiteral("Value")});
	tabs->addTab(m_headerTable, QStringLiteral("General"));
	utils::bindKineticScrolling(m_headerTable);

	m_sessionsTable = new QTableWidget(0, 10);
	m_sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_sessionsTable->verticalHeader()->hide();
	m_sessionsTable->setHorizontalHeaderLabels(
		{QStringLiteral("Thumbnail"), QStringLiteral("Session ID"),
		 QStringLiteral("Source Type"), QStringLiteral("Source Param"),
		 QStringLiteral("Protocol"), QStringLiteral("Flags"),
		 QStringLiteral("Opened At"), QStringLiteral("Closed At"),
		 QStringLiteral("Messages"), QStringLiteral("Snapshots")});
	tabs->addTab(m_sessionsTable, QStringLiteral("Sessions"));
	utils::bindKineticScrolling(m_sessionsTable);

	m_snapshotsTable = new QTableWidget(0, 7);
	m_snapshotsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_snapshotsTable->verticalHeader()->hide();
	m_snapshotsTable->setHorizontalHeaderLabels(
		{QStringLiteral("Thumbnail"), QStringLiteral("Snapshot ID"),
		 QStringLiteral("Session ID"), QStringLiteral("Flags"),
		 QStringLiteral("Taken At"), QStringLiteral("Sequence ID"),
		 QStringLiteral("Messages")});
	tabs->addTab(m_snapshotsTable, QStringLiteral("Snapshots"));
	utils::bindKineticScrolling(m_snapshotsTable);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	layout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this, &ProjectInfoDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this, &ProjectInfoDialog::reject);
}

void ProjectInfoDialog::loadProjectInfo(const QString &path)
{
	utils::ScopedOverrideCursor overrideCursor;

	m_statusLabel->setText(QStringLiteral("<strong>%1</strong> - Loading…")
							   .arg(path.toHtmlEscaped()));
	m_headerTable->clearContents();
	m_sessionsTable->clearContents();
	m_snapshotsTable->clearContents();
	m_headerTable->setRowCount(0);
	m_sessionsTable->setRowCount(0);
	m_snapshotsTable->setRowCount(0);
	setEnabled(false);
	QCoreApplication::processEvents();

	utils::ScopedUpdateDisabler updateDisabler(this);

	QString errorMessage;
	bool ok = queryProjectInfo(path, errorMessage);
	m_statusLabel->setText(
		QStringLiteral("<strong>%1</strong> - %2")
			.arg(
				path.toHtmlEscaped(),
				ok ? QStringLiteral("OK") : errorMessage.toHtmlEscaped()));
	m_headerTable->resizeColumnsToContents();
	m_sessionsTable->resizeColumnsToContents();
	m_snapshotsTable->resizeColumnsToContents();

	setEnabled(true);
}

bool ProjectInfoDialog::queryProjectInfo(
	const QString &path, QString &outErrorMessage)
{
	DP_Project *prj;
	{
		DP_ProjectOpenResult openResult = DP_project_open(
			path.toUtf8().constData(),
			DP_PROJECT_OPEN_EXISTING | DP_PROJECT_OPEN_READ_ONLY);
		prj = openResult.project;
		if(!prj) {
			outErrorMessage = QStringLiteral("Error %1 opening project: %2")
								  .arg(openResult.error)
								  .arg(QString::fromUtf8(DP_error()));
			return false;
		}
	}

	int queryResult = DP_project_info(
		prj,
		DP_PROJECT_INFO_FLAG_HEADER | DP_PROJECT_INFO_FLAG_SESSIONS |
			DP_PROJECT_INFO_FLAG_SNAPSHOTS,
		&ProjectInfoDialog::onProjectInfoCallback, this);
	if(queryResult != 0) {
		outErrorMessage = QStringLiteral("Error %1 querying project info: %2")
							  .arg(queryResult)
							  .arg(QString::fromUtf8(DP_error()));
		if(!DP_project_close(prj)) {
			outErrorMessage +=
				QStringLiteral(" (Also error closing project: %1)")
					.arg(QString::fromUtf8(DP_error()));
		}
		return false;
	}

	if(!DP_project_close(prj)) {
		outErrorMessage = QStringLiteral("Error closing project: %1")
							  .arg(QString::fromUtf8(DP_error()));
		return false;
	}

	return true;
}

void ProjectInfoDialog::onProjectInfo(const DP_ProjectInfo &info)
{
	switch(info.type) {
	case DP_PROJECT_INFO_TYPE_HEADER:
		onProjectInfoHeader(info.header);
		break;
	case DP_PROJECT_INFO_TYPE_SESSION:
		onProjectInfoSession(info.session);
		break;
	case DP_PROJECT_INFO_TYPE_SNAPSHOT:
		onProjectInfoSnapshot(info.snapshot);
		break;
	default:
		qWarning("Unknown project info type %d", int(info.type));
		break;
	}
}

void ProjectInfoDialog::onProjectInfoHeader(const DP_ProjectInfoHeader &info)
{
	addRow(
		m_headerTable, {new QTableWidgetItem(QStringLiteral("path")),
						new QTableWidgetItem(QString::fromUtf8(info.path))});
	addRow(
		m_headerTable,
		{new QTableWidgetItem(QStringLiteral("application_id")),
		 new QTableWidgetItem(QString::number(info.application_id))});
	addRow(
		m_headerTable,
		{new QTableWidgetItem(QStringLiteral("user_version")),
		 new QTableWidgetItem(QString::number(info.user_version))});
}

void ProjectInfoDialog::onProjectInfoSession(const DP_ProjectInfoSession &info)
{
	QString sourceTypeName;
	switch(info.source_type) {
	case DP_PROJECT_SOURCE_BLANK:
		sourceTypeName = QStringLiteral("blank");
		break;
	case DP_PROJECT_SOURCE_FILE:
		sourceTypeName = QStringLiteral("file");
		break;
	case DP_PROJECT_SOURCE_SESSION:
		sourceTypeName = QStringLiteral("session");
		break;
	default:
		sourceTypeName = QStringLiteral("unknown");
		break;
	}

	QStringList flagNames;
	if(info.flags & DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED) {
		flagNames.append(QStringLiteral("project_closed"));
	}

	addRow(
		m_sessionsTable,
		{
			loadThumbnail(info.thumbnail_data, info.thumbnail_size),
			new QTableWidgetItem(QString::number(info.session_id)),
			new QTableWidgetItem(QStringLiteral("%1 (%2)")
									 .arg(info.source_type)
									 .arg(sourceTypeName)),
			new QTableWidgetItem(QString::fromUtf8(info.source_param)),
			new QTableWidgetItem(QString::fromUtf8(info.protocol)),
			new QTableWidgetItem(
				QStringLiteral("0x%1 (%2)")
					.arg(info.flags, 0, 16)
					.arg(flagNames.join(QStringLiteral(", ")))),
			loadTimestamp(info.opened_at),
			loadTimestamp(info.closed_at),
			new QTableWidgetItem(QString::number(info.message_count)),
			new QTableWidgetItem(QString::number(info.snapshot_count)),
		});
}

void ProjectInfoDialog::onProjectInfoSnapshot(
	const DP_ProjectInfoSnapshot &info)
{
	QStringList flagNames;
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE) {
		flagNames.append(QStringLiteral("complete"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT) {
		flagNames.append(QStringLiteral("persistent"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_CANVAS) {
		flagNames.append(QStringLiteral("canvas"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_AUTOSAVE) {
		flagNames.append(QStringLiteral("autosave"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_HAS_MESSAGES) {
		flagNames.append(QStringLiteral("has_messages"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_HAS_SUBLAYERS) {
		flagNames.append(QStringLiteral("has_sublayers"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_HAS_SELECTIONS) {
		flagNames.append(QStringLiteral("has_selections"));
	}
	if(info.flags & DP_PROJECT_SNAPSHOT_FLAG_NULL_CANVAS) {
		flagNames.append(QStringLiteral("null_canvas"));
	}

	addRow(
		m_snapshotsTable,
		{
			loadThumbnail(info.thumbnail_data, info.thumbnail_size),
			new QTableWidgetItem(QString::number(info.snapshot_id)),
			new QTableWidgetItem(QString::number(info.session_id)),
			new QTableWidgetItem(
				QStringLiteral("0x%1 (%2)")
					.arg(info.flags, 0, 16)
					.arg(flagNames.join(QStringLiteral(", ")))),
			loadTimestamp(info.taken_at),
			new QTableWidgetItem(QString::number(info.metadata_sequence_id)),
			new QTableWidgetItem(QString::number(info.snapshot_message_count)),
		});
}

void ProjectInfoDialog::onProjectInfoCallback(
	void *user, const DP_ProjectInfo *info)
{
	static_cast<ProjectInfoDialog *>(user)->onProjectInfo(*info);
}

void ProjectInfoDialog::addRow(
	QTableWidget *table, const QVector<QTableWidgetItem *> items)
{
	int row = table->rowCount();
	table->insertRow(row);
	int count = items.size();
	for(int i = 0; i < count; ++i) {
		table->setItem(row, i, items[i]);
	}
}

QTableWidgetItem *
ProjectInfoDialog::loadThumbnail(const unsigned char *data, size_t size)
{
	if(data && size > 0) {
		QPixmap pixmap;
		if(pixmap.loadFromData(data, uint(size))) {
			return new QTableWidgetItem(QIcon(pixmap), QStringLiteral("yes"));
		} else {
			return new QTableWidgetItem(QStringLiteral("error"));
		}
	} else {
		return new QTableWidgetItem(QStringLiteral("no"));
	}
}

QTableWidgetItem *ProjectInfoDialog::loadTimestamp(double timestamp)
{
	if(timestamp > 0.0) {
		return new QTableWidgetItem(
			QDateTime::fromMSecsSinceEpoch(qint64(timestamp * 1000.0))
				.toString(Qt::ISODateWithMs));
	} else {
		return new QTableWidgetItem();
	}
}

}
