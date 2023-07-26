// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_WIDGETS_RECENTSCROLL_H
#define DESKTOP_WIDGETS_RECENTSCROLL_H

#include "desktop/utils/recents.h"
#include <QScrollArea>
#include <QVector>

class QHBoxLayout;
class QLabel;
class QToolButton;
class QVBoxLayout;

namespace widgets {

class RecentScrollEntry final : public QWidget {
	Q_OBJECT
public:
	static RecentScrollEntry *ofNoFiles();
	static RecentScrollEntry *ofFile(const utils::Recents::File &file);

	static RecentScrollEntry *ofNoHosts();
	static RecentScrollEntry *ofHost(const utils::Recents::Host &rh);

signals:
	void clicked();
	void doubleClicked();
	void deleteRequested();

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
	RecentScrollEntry();

	void initOpacity(double opacity);
	void initDeleteButton(int iconSize);

	void setText(const QString &text);

	static QString makeWrappable(const QString &s);

	QHBoxLayout *m_layout;
	QLabel *m_text;
	QToolButton *m_deleteButton;
};

class RecentScroll final : public QScrollArea {
	Q_OBJECT
public:
	static constexpr int THUMBNAIL_WIDTH = 112;
	static constexpr int THUMBNAIL_HEIGHT = 63;

	enum class Mode { Files, Join };

	explicit RecentScroll(Mode mode, QWidget *parent = nullptr);

signals:
	void clicked(const QString &value);
	void doubleClicked(const QString &value);

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void updateFiles();
	void updateHosts();

private:
	void addEntry(RecentScrollEntry *entry);
	void clearEntries();

	Mode m_mode;
	QWidget *m_content;
	QVBoxLayout *m_layout;
	QVector<RecentScrollEntry *> m_entries;
};

}

#endif
