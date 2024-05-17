// SPDX-License-Identifier: GPL-2.0-or-later
// Based on KisOutlineGenerator from Krita
// SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
// SPDX-FileCopyrightText: 2007, 2010 Sven Langkamp <sven.langkamp@gmail.com>
// Outline algorithm based of the limn of fontutils
// SPDX-FileCopyrightText: 1992 Karl Berry <karl@cs.umb.edu>
// SPDX-FileCopyrightText: 1992 Kathryn Hargreaves <letters@cs.umb.edu>
#ifndef LIBCLIENT_UTILS_SELECTIONOUTLINEGENERATOR_H
#define LIBCLIENT_UTILS_SELECTIONOUTLINEGENERATOR_H
#include <QAtomicInt>
#include <QImage>
#include <QObject>
#include <QPainterPath>
#include <QRunnable>

// Qt5 doesn't have a metatype registered for QPainterPath.
struct SelectionOutlinePath {
	QPainterPath value;
};
Q_DECLARE_METATYPE(SelectionOutlinePath)

class SelectionOutlineGenerator : public QObject, public QRunnable {
	Q_OBJECT
public:
	explicit SelectionOutlineGenerator(
		unsigned int executionId, const QImage &mask, bool simple, int xOffset,
		int yOffset, QObject *parent = nullptr);

	void run() override;

public slots:
	void cancel();

signals:
	void outlineGenerated(
		unsigned int executionId, const SelectionOutlinePath &path);

private:
	const unsigned int m_executionId;
	const QImage m_mask;
	const bool m_simple;
	const int m_xOffset;
	const int m_yOffset;
	QAtomicInt m_running = 1;
};


#endif
