// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_VIEWSTATUS_H
#define DESKTOP_WIDGETS_VIEWSTATUS_H
#include <QWidget>

class QComboBox;
class QMenu;

namespace widgets {

class GroupedToolButton;
class KisAngleGauge;
class ZoomSlider;

class ViewStatus final : public QWidget {
	Q_OBJECT
public:
	ViewStatus(QWidget *parent = nullptr);

	void setActions(
		QAction *flip, QAction *mirror, QAction *rotationReset,
		const QVector<QAction *> &zoomActions);

public slots:
	void setTransformation(qreal zoom, qreal angle);

signals:
	void zoomStepped(int steps);
	void zoomChanged(qreal newZoom);
	void angleChanged(qreal newAngle);

protected:
	void changeEvent(QEvent *event) override;

private slots:
	void zoomSliderChanged(double value);
	void angleBoxChanged(const QString &text);

private:
	void updatePalette();

	ZoomSlider *m_zoomSlider;
	KisAngleGauge *m_compass;
	QComboBox *m_angleBox;
	widgets::GroupedToolButton *m_viewFlip;
	widgets::GroupedToolButton *m_viewMirror;
	widgets::GroupedToolButton *m_rotationReset;
	widgets::GroupedToolButton *m_zoomPreset;
	QMenu *m_zoomsMenu;
	bool m_updating = false;
};

}

#endif
