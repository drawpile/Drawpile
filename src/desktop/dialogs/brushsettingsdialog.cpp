// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/brushsettingsdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/keysequenceedit.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/toolmessage.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {

namespace {

class ShortcutLineEdit final : public QLineEdit {
public:
	ShortcutLineEdit(QPushButton *button)
		: QLineEdit()
		, m_button(button)
	{
		setReadOnly(true);
		setEnabled(false);
	}

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		QLineEdit::mousePressEvent(event);
		if(event->button() == Qt::LeftButton) {
			m_button->click();
		}
	}

private:
	QPushButton *m_button;
};

}


BrushPresetForm::BrushPresetForm(QWidget *parent)
	: QWidget(parent)
{
	QFormLayout *attachedLayout = new QFormLayout;
	setLayout(attachedLayout);

	m_presetShortcutButton = new QPushButton(tr("Change…"));
	connect(
		m_presetShortcutButton, &QPushButton::clicked, this,
		&BrushPresetForm::requestShortcutChange);

	m_presetShortcutEdit = new ShortcutLineEdit(m_presetShortcutButton);
	m_presetShortcutEdit->setAlignment(Qt::AlignCenter);
	m_presetShortcutEdit->setReadOnly(true);
	m_presetShortcutEdit->setEnabled(false);

	QHBoxLayout *shortcutLayout = new QHBoxLayout;
	shortcutLayout->setContentsMargins(0, 0, 0, 0);
	shortcutLayout->addWidget(m_presetShortcutEdit);
	shortcutLayout->addWidget(m_presetShortcutButton);
	attachedLayout->addRow(tr("Shortcut:"), shortcutLayout);

	utils::addFormSpacer(attachedLayout);

	QGridLayout *thumbnailLayout = new QGridLayout;

	m_presetThumbnailView = new QGraphicsView;
	m_presetThumbnailView->setFixedSize(64, 64);
	m_presetThumbnailView->setFrameShape(QFrame::NoFrame);
	m_presetThumbnailView->setFrameShadow(QFrame::Plain);
	m_presetThumbnailView->setLineWidth(0);
	thumbnailLayout->addWidget(m_presetThumbnailView, 0, 0, 2, 1);

	QGraphicsScene *scene = new QGraphicsScene(m_presetThumbnailView);
	m_presetThumbnailView->setScene(scene);

	m_presetThumbnailButton =
		new QPushButton(QIcon::fromTheme("document-open"), tr("Choose File…"));
	thumbnailLayout->addWidget(m_presetThumbnailButton, 0, 1);
	connect(
		m_presetThumbnailButton, &QPushButton::clicked, this,
		&BrushPresetForm::choosePresetThumbnailFile);

	QLabel *presetThumbnailLabel =
		new QLabel(tr("Will be resized to 64x64 pixels."));
	presetThumbnailLabel->setAlignment(Qt::AlignCenter);
	presetThumbnailLabel->setWordWrap(true);
	thumbnailLayout->addWidget(presetThumbnailLabel, 1, 1);

	attachedLayout->addRow(tr("Thumbnail:"), thumbnailLayout);

	m_presetLabelEdit = new QLineEdit;
	attachedLayout->addRow(tr("Add Label:"), m_presetLabelEdit);
	connect(
		m_presetLabelEdit, &QLineEdit::textChanged, this,
		&BrushPresetForm::renderPresetThumbnail);

	utils::addFormSpacer(attachedLayout);

	m_presetNameEdit = new QLineEdit;
	attachedLayout->addRow(tr("Name:"), m_presetNameEdit);
	connect(
		m_presetNameEdit, &QLineEdit::textChanged, this,
		&BrushPresetForm::presetNameChanged);

	m_presetDescriptionEdit = new QPlainTextEdit;
	attachedLayout->addRow(tr("Description:"), m_presetDescriptionEdit);
	connect(
		m_presetDescriptionEdit, &QPlainTextEdit::textChanged, this,
		&BrushPresetForm::emitPresetDescriptionChanged);

	utils::addFormSpacer(attachedLayout);

	m_takeableCheckBox =
		new QCheckBox(tr("Allow others in a session to use this brush"));
	attachedLayout->addRow(tr("Sharing:"), m_takeableCheckBox);
	connect(
		m_takeableCheckBox, &QCheckBox::clicked, this,
		&BrushPresetForm::takeableChanged);
}

QString BrushPresetForm::presetName() const
{
	return m_presetNameEdit->text();
}

void BrushPresetForm::setPresetName(const QString &presetName)
{
	if(presetName != m_presetNameEdit->text()) {
		QSignalBlocker blocker(m_presetNameEdit);
		m_presetNameEdit->setText(presetName);
		emit presetNameChanged(presetName);
	}
}

QString BrushPresetForm::presetDescription() const
{
	return m_presetDescriptionEdit->toPlainText();
}

void BrushPresetForm::setPresetDescription(const QString &presetDescription)
{
	if(presetDescription != m_presetDescriptionEdit->toPlainText()) {
		QSignalBlocker blocker(m_presetDescriptionEdit);
		m_presetDescriptionEdit->setPlainText(presetDescription);
		emit presetDescriptionChanged(presetDescription);
	}
}

QPixmap BrushPresetForm::presetThumbnail() const
{
	QGraphicsScene *scene = m_presetThumbnailView->scene();
	for(QGraphicsItem *item : scene->items()) {
		QGraphicsPixmapItem *pixmapItem =
			qgraphicsitem_cast<QGraphicsPixmapItem *>(item);
		if(pixmapItem) {
			return pixmapItem->pixmap();
		}
	}
	return QPixmap();
}

void BrushPresetForm::setPresetThumbnail(const QPixmap &presetThumbnail)
{
	if(presetThumbnail.cacheKey() != m_presetThumbnail.cacheKey()) {
		QSignalBlocker blocker(m_presetLabelEdit);
		m_presetLabelEdit->clear();
		showPresetThumbnail(presetThumbnail);
	}
}

void BrushPresetForm::setPresetShortcut(const QKeySequence &presetShortcut)
{
	QString text = presetShortcut.toString(QKeySequence::NativeText);
	m_presetShortcutEdit->setText(
		text.isEmpty() ? tr("No shortcut assigned") : text);
}

void BrushPresetForm::setChangeShortcutEnabled(bool enabled)
{
	m_presetShortcutButton->setEnabled(enabled);
}

void BrushPresetForm::setTakeable(bool takeable)
{
	m_takeableCheckBox->setChecked(takeable);
}

void BrushPresetForm::choosePresetThumbnailFile()
{
	FileWrangler::ImageOpenFn imageOpenCompleted =
		[this](QImage &img, const QString &) {
			QPixmap pixmap;
			if(!img.isNull() && pixmap.convertFromImage(img)) {
				showPresetThumbnail(pixmap);
			}
		};
	FileWrangler(this).openBrushThumbnail(imageOpenCompleted);
}

void BrushPresetForm::showPresetThumbnail(const QPixmap &thumbnail)
{
	m_presetThumbnail = thumbnail;
	m_scaledPresetThumbnail =
		thumbnail.size() == QSize(64, 64)
			? thumbnail
			: thumbnail.scaled(
				  64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	renderPresetThumbnail();
}

void BrushPresetForm::renderPresetThumbnail()
{
	QGraphicsScene *scene = m_presetThumbnailView->scene();
	scene->clear();
	scene->setSceneRect(QRect(0, 0, 64, 64));
	QString label = m_presetLabelEdit->text();
	if(label.trimmed().isEmpty()) {
		scene->addPixmap(m_scaledPresetThumbnail);
		emit presetThumbnailChanged(m_scaledPresetThumbnail);
	} else {
		QPixmap thumbnail = applyPresetThumbnailLabel(label);
		scene->addPixmap(thumbnail);
		emit presetThumbnailChanged(thumbnail);
	}
}

QPixmap BrushPresetForm::applyPresetThumbnailLabel(const QString &label)
{
	QPixmap thumbnail = m_scaledPresetThumbnail;
	QPainter painter(&thumbnail);
	qreal h = thumbnail.height();
	qreal y = h * 3.0 / 4.0;
	QRectF rect(0, y, thumbnail.width(), h - y);
	painter.fillRect(rect, palette().window());
	painter.setPen(palette().windowText().color());
	painter.drawText(
		rect, label, QTextOption(Qt::AlignHCenter | Qt::AlignBaseline));
	return thumbnail;
}

void BrushPresetForm::emitPresetDescriptionChanged()
{
	emit presetDescriptionChanged(m_presetDescriptionEdit->toPlainText());
}


struct BrushSettingsDialog::Private {
	struct MyPaintPage {
		KisDoubleSliderSpinBox *baseValueSpinner;
		QCheckBox *syncSamplesBox;
		QCheckBox *pixelPerfectBox;
		widgets::MyPaintInput *inputs[MYPAINT_BRUSH_INPUTS_COUNT];
		QLabel *constantLabel;
		QLabel *disabledLabel;
	};

	QPushButton *newBrushButton;
	QPushButton *overwriteBrushButton;
	QListWidget *categoryWidget;
	QStackedWidget *stackedWidget;
	BrushPresetForm *brushPresetForm;
	QComboBox *brushTypeCombo;
	QLabel *paintModeLabel;
	QComboBox *paintModeCombo;
	QStandardItemModel *paintModeModel;
	QStandardItem *paintModeIndirectWash;
	QStandardItem *paintModeIndirectNormal;
	QComboBox *brushModeCombo;
	QComboBox *eraseModeCombo;
	QCheckBox *eraseModeBox;
	QCheckBox *colorPickBox;
	QCheckBox *pixelPerfectBox;
	QCheckBox *preserveAlphaBox;
	KisSliderSpinBox *spacingSpinner;
	QComboBox *stabilizationModeCombo;
	KisSliderSpinBox *stabilizerSpinner;
	KisSliderSpinBox *smoothingSpinner;
	QLabel *stabilizerExplanationLabel;
	KisSliderSpinBox *classicSizeSpinner;
	Dynamics classicSizeDynamics;
	KisSliderSpinBox *classicSizeMinSpinner;
	widgets::CurveWidget *classicSizeCurve;
	KisSliderSpinBox *classicOpacitySpinner;
	Dynamics classicOpacityDynamics;
	KisSliderSpinBox *classicOpacityMinSpinner;
	widgets::CurveWidget *classicOpacityCurve;
	KisSliderSpinBox *classicHardnessSpinner;
	Dynamics classicHardnessDynamics;
	KisSliderSpinBox *classicHardnessMinSpinner;
	widgets::CurveWidget *classicHardnessCurve;
	KisSliderSpinBox *classicSmudgingSpinner;
	KisSliderSpinBox *classicColorPickupSpinner;
	QCheckBox *classicSmudgeAlphaBox;
	QCheckBox *classicSyncSamplesBox;
	Dynamics classicSmudgeDynamics;
	KisSliderSpinBox *classicSmudgingMinSpinner;
	widgets::CurveWidget *classicSmudgingCurve;
	KisSliderSpinBox *classicJitterSpinner;
	Dynamics classicJitterDynamics;
	KisSliderSpinBox *classicJitterMinSpinner;
	widgets::CurveWidget *classicJitterCurve;
	MyPaintPage myPaintPages[MYPAINT_BRUSH_SETTINGS_COUNT];
	BlendModeManager *blendModeManager;
	int presetPageIndex;
	int generalPageIndex;
	int classicSizePageIndex;
	int classicOpacityPageIndex;
	int classicHardnessPageIndex;
	int classicSmudgePageIndex;
	int classicJitterPageIndex;
	int mypaintPageIndexes[MYPAINT_BRUSH_SETTINGS_COUNT];
	DP_BrushShape lastShape;
	brushes::ActiveBrush brush;
	int globalSmoothing;
	int presetId = 0;
	bool useBrushSampleCount;
	bool presetAttached = true;
	bool compatibilityMode = false;
	bool updating = false;
};

BrushSettingsDialog::BrushSettingsDialog(QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	setWindowTitle(tr("Brush Editor"));
	resize(QSize{800, 600});
	buildDialogUi();
	d->lastShape = DP_BRUSH_SHAPE_COUNT;
	d->useBrushSampleCount = true;
	setPresetAttached(false, 0);
}

BrushSettingsDialog::~BrushSettingsDialog()
{
	delete d;
}


void BrushSettingsDialog::showPresetPage()
{
	d->categoryWidget->setCurrentRow(0);
	d->stackedWidget->setCurrentIndex(d->presetPageIndex);
}

void BrushSettingsDialog::showGeneralPage()
{
	d->categoryWidget->setCurrentRow(1);
	d->stackedWidget->setCurrentIndex(d->generalPageIndex);
}


bool BrushSettingsDialog::isPresetAttached() const
{
	return d->presetAttached;
}

int BrushSettingsDialog::presetId() const
{
	return d->presetId;
}


void BrushSettingsDialog::setPresetAttached(bool presetAttached, int presetId)
{
	utils::ScopedUpdateDisabler disabler(this);
	d->presetId = presetId;
	d->presetAttached = presetAttached;
	d->overwriteBrushButton->setEnabled(presetId > 0);
	d->brushPresetForm->setChangeShortcutEnabled(presetId > 0);
}

void BrushSettingsDialog::setPresetName(const QString &presetName)
{
	d->brushPresetForm->setPresetName(presetName);
}

void BrushSettingsDialog::setPresetDescription(const QString &presetDescription)
{
	d->brushPresetForm->setPresetDescription(presetDescription);
}

void BrushSettingsDialog::setPresetThumbnail(const QPixmap &presetThumbnail)
{
	d->brushPresetForm->setPresetThumbnail(presetThumbnail);
}

void BrushSettingsDialog::setPresetShortcut(const QKeySequence &presetShortcut)
{
	d->brushPresetForm->setPresetShortcut(presetShortcut);
}

void BrushSettingsDialog::setForceEraseMode(bool forceEraseMode)
{
	d->eraseModeBox->setEnabled(!forceEraseMode);
	if(forceEraseMode) {
		QScopedValueRollback<bool> rollback(d->updating, true);
		d->blendModeManager->setEraseMode(true);
	}
}

void BrushSettingsDialog::setStabilizerUseBrushSampleCount(
	bool useBrushSampleCount)
{
	d->useBrushSampleCount = useBrushSampleCount;
	d->stabilizationModeCombo->setEnabled(useBrushSampleCount);
	d->stabilizerSpinner->setEnabled(useBrushSampleCount);
	d->smoothingSpinner->setEnabled(useBrushSampleCount);
	updateStabilizerExplanationText();
}

void BrushSettingsDialog::setGlobalSmoothing(int smoothing)
{
	QSignalBlocker blocker{d->smoothingSpinner};
	d->smoothingSpinner->setValue(
		d->smoothingSpinner->value() - d->globalSmoothing + smoothing);
	d->globalSmoothing = smoothing;
}

void BrushSettingsDialog::setCompatibilityMode(bool compatibilityMode)
{
	if(compatibilityMode != d->compatibilityMode) {
		d->compatibilityMode = compatibilityMode;
		d->paintModeIndirectWash->setEnabled(!compatibilityMode);
		d->paintModeIndirectNormal->setEnabled(!compatibilityMode);
		updateUiFromActiveBrush(d->brush);
	}
}

void BrushSettingsDialog::updateUiFromActiveBrush(
	const brushes::ActiveBrush &brush)
{
	utils::ScopedUpdateDisabler disabler(this);
	QScopedValueRollback<bool> rollback(d->updating, true);
	QSignalBlocker blocker{this};
	d->brush = brush;
	d->blendModeManager->setCompatibilityMode(d->compatibilityMode);
	d->brushPresetForm->setTakeable(!d->brush.isConfidential());

	DP_BrushShape shape = brush.shape();
	bool shapeChanged = shape != d->lastShape;
	int prevStackIndex = d->stackedWidget->currentIndex();
	if(shapeChanged) {
		d->lastShape = shape;
		d->categoryWidget->clear();
		addCategory(
			tr("Brush"), tr("Brush metadata settings."), d->presetPageIndex);
		addCategory(
			tr("General"), tr("Core brush settings."), d->generalPageIndex);
		setComboBoxIndexByData(d->brushTypeCombo, int(shape));
	}

	switch(shape) {
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
	case DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND:
		if(shapeChanged) {
			addClassicCategories(shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND);
		}
		updateUiFromClassicBrush();
		break;
	case DP_BRUSH_SHAPE_MYPAINT:
		if(shapeChanged) {
			addMyPaintCategories();
		}
		updateUiFromMyPaintBrush();
		break;
	default:
		break;
	}

	if(shapeChanged) {
		if(prevStackIndex == d->presetPageIndex) {
			showPresetPage();
		} else {
			showGeneralPage();
		}
	}

	brushes::StabilizationMode stabilizationMode = brush.stabilizationMode();
	setComboBoxIndexByData(d->stabilizationModeCombo, int(stabilizationMode));
	d->stabilizerSpinner->setVisible(stabilizationMode == brushes::Stabilizer);
	d->stabilizerSpinner->setValue(brush.stabilizerSampleCount());
	d->smoothingSpinner->setVisible(stabilizationMode == brushes::Smoothing);
	d->smoothingSpinner->setValue(d->globalSmoothing + brush.smoothing());
	updateStabilizerExplanationText();
}


void BrushSettingsDialog::categoryChanged(
	QListWidgetItem *current, QListWidgetItem *)
{
	if(current) {
		d->stackedWidget->setCurrentIndex(current->data(Qt::UserRole).toInt());
	}
}


void BrushSettingsDialog::buildDialogUi()
{
	QVBoxLayout *dialogLayout = new QVBoxLayout;
	setLayout(dialogLayout);

	QHBoxLayout *splitLayout = new QHBoxLayout;
	dialogLayout->addLayout(splitLayout);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	dialogLayout->addLayout(buttonLayout);

	d->newBrushButton = new QPushButton(
		QIcon::fromTheme("list-add"), tr("Save as New Brush"), this);
	buttonLayout->addWidget(d->newBrushButton);
	connect(
		d->newBrushButton, &QPushButton::clicked, this,
		&BrushSettingsDialog::newBrushRequested);

	d->overwriteBrushButton = new QPushButton(
		QIcon::fromTheme("document-save"), tr("Overwrite Brush"), this);
	buttonLayout->addWidget(d->overwriteBrushButton);
	connect(
		d->overwriteBrushButton, &QPushButton::clicked, this,
		&BrushSettingsDialog::overwriteBrushRequested);

	QDialogButtonBox *buttonBox =
		new QDialogButtonBox{QDialogButtonBox::Close, this};
	buttonLayout->addWidget(buttonBox);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	d->categoryWidget = new QListWidget{this};
	splitLayout->addWidget(d->categoryWidget);
	d->categoryWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	d->categoryWidget->setSizePolicy(
		QSizePolicy::Preferred, QSizePolicy::Expanding);
	utils::bindKineticScrolling(d->categoryWidget);
	connect(
		d->categoryWidget, &QListWidget::currentItemChanged, this,
		&BrushSettingsDialog::categoryChanged);

	d->stackedWidget = new QStackedWidget{this};
	splitLayout->addWidget(d->stackedWidget);
	d->stackedWidget->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Expanding);

	d->presetPageIndex = d->stackedWidget->addWidget(buildPresetPageUi());
	d->generalPageIndex = d->stackedWidget->addWidget(buildGeneralPageUi());
	d->classicSizePageIndex =
		d->stackedWidget->addWidget(buildClassicSizePageUi());
	d->classicOpacityPageIndex =
		d->stackedWidget->addWidget(buildClassicOpacityPageUi());
	d->classicHardnessPageIndex =
		d->stackedWidget->addWidget(buildClassicHardnessPageUi());
	d->classicSmudgePageIndex =
		d->stackedWidget->addWidget(buildClassicSmudgingPageUi());
	d->classicJitterPageIndex =
		d->stackedWidget->addWidget(buildClassicJitterPageUi());
	for(int setting = 0; setting < MYPAINT_BRUSH_SETTINGS_COUNT; ++setting) {
		d->mypaintPageIndexes[setting] =
			shouldIncludeMyPaintSetting(setting)
				? d->stackedWidget->addWidget(buildMyPaintPageUi(setting))
				: -1;
	}
}

QWidget *BrushSettingsDialog::buildPresetPageUi()
{
	d->brushPresetForm = new BrushPresetForm;
	connect(
		d->brushPresetForm, &BrushPresetForm::requestShortcutChange, this,
		&BrushSettingsDialog::requestShortcutChange);
	connect(
		d->brushPresetForm, &BrushPresetForm::presetNameChanged, this,
		&BrushSettingsDialog::presetNameChanged);
	connect(
		d->brushPresetForm, &BrushPresetForm::presetDescriptionChanged, this,
		&BrushSettingsDialog::presetDescriptionChanged);
	connect(
		d->brushPresetForm, &BrushPresetForm::presetThumbnailChanged, this,
		&BrushSettingsDialog::presetThumbnailChanged);
	connect(
		d->brushPresetForm, &BrushPresetForm::takeableChanged, this,
		[this](bool takeable) {
			d->brush.setConfidential(!takeable);
			emitChange();
		});

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setWidget(d->brushPresetForm);
	utils::bindKineticScrolling(scroll);
	return scroll;
}

QWidget *BrushSettingsDialog::buildGeneralPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QFormLayout *layout = new QFormLayout;
	widget->setLayout(layout);

	d->brushTypeCombo = new QComboBox{widget};
	layout->addRow(tr("Brush Type:"), d->brushTypeCombo);
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_pixelround"), tr("Round Pixel Brush"),
		int(DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND));
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_square"), tr("Square Pixel Brush"),
		int(DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE));
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_round"), tr("Soft Round Brush"),
		int(DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND));
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_mypaint"), tr("MyPaint Brush"),
		int(DP_BRUSH_SHAPE_MYPAINT));
	connect(
		d->brushTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		makeBrushChangeCallbackArg<int>([this](int index) {
			int brushType = d->brushTypeCombo->itemData(index).toInt();
			switch(brushType) {
			case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
			case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
			case DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND:
				d->brush.setActiveType(brushes::ActiveBrush::CLASSIC);
				d->brush.classic().shape = DP_BrushShape(brushType);
				break;
			case DP_BRUSH_SHAPE_MYPAINT:
				d->brush.setActiveType(brushes::ActiveBrush::MYPAINT);
				break;
			default:
				qWarning("Unknown brush type %d", brushType);
				break;
			}
			emitChange();
		}));

	d->paintModeLabel = new QLabel{tr("Paint Mode:"), widget};
	d->paintModeCombo = new QComboBox{widget};
	layout->addRow(d->paintModeLabel, d->paintModeCombo);

	d->paintModeModel = new QStandardItemModel(d->paintModeCombo);

	QStandardItem *paintModeDirect = new QStandardItem(
		QIcon::fromTheme("drawpile_incremental_mode"), tr("Direct Build-Up"));
	paintModeDirect->setData(int(DP_PAINT_MODE_DIRECT), Qt::UserRole);
	d->paintModeModel->appendRow(paintModeDirect);

	QStandardItem *paintModeIndirectWash = new QStandardItem(
		QIcon::fromTheme("drawpile_wash_mode"), tr("Indirect Wash"));
	paintModeIndirectWash->setData(
		int(DP_PAINT_MODE_INDIRECT_WASH), Qt::UserRole);
	d->paintModeModel->appendRow(paintModeIndirectWash);

	QStandardItem *paintModeIndirectSoft = new QStandardItem(
		QIcon::fromTheme("drawpile_soft_mode"),
		tr("Indirect Soft (Drawpile 2.2)"));
	paintModeIndirectSoft->setData(
		int(DP_PAINT_MODE_INDIRECT_SOFT), Qt::UserRole);
	d->paintModeModel->appendRow(paintModeIndirectSoft);

	QStandardItem *paintModeIndirectNormal = new QStandardItem(
		QIcon::fromTheme("drawpile_indirect_mode"),
		tr("Indirect Build-Up (Drawpile 2.1)"));
	paintModeIndirectNormal->setData(
		int(DP_PAINT_MODE_INDIRECT_NORMAL), Qt::UserRole);
	d->paintModeModel->appendRow(paintModeIndirectNormal);

	d->paintModeCombo->setModel(d->paintModeModel);
	d->paintModeIndirectWash = paintModeIndirectWash;
	d->paintModeIndirectNormal = paintModeIndirectNormal;

	connect(
		d->paintModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		makeBrushChangeCallbackArg<int>([this](int index) {
			d->brush.setPaintMode(d->paintModeCombo->itemData(index).toInt());
			emitChange();
		}));

	d->brushModeCombo = new QComboBox;
	d->eraseModeCombo = new QComboBox;
	d->eraseModeCombo->hide();

	QHBoxLayout *modeLayout = new QHBoxLayout;
	modeLayout->setContentsMargins(0, 0, 0, 0);
	modeLayout->setSpacing(0);
	modeLayout->addWidget(d->brushModeCombo);
	modeLayout->addWidget(d->eraseModeCombo);
	layout->addRow(tr("Blend Mode:"), modeLayout);

	d->preserveAlphaBox = new QCheckBox{tr("Preserve alpha"), widget};
	d->preserveAlphaBox->setIcon(QIcon::fromTheme("drawpile_alpha_locked"));
	layout->addRow(d->preserveAlphaBox);

	d->eraseModeBox = new QCheckBox{tr("Eraser Mode"), widget};
	d->eraseModeBox->setIcon(QIcon::fromTheme("draw-eraser"));
	layout->addRow(d->eraseModeBox);

	d->colorPickBox =
		new QCheckBox{tr("Pick Initial Color from Layer"), widget};
	d->colorPickBox->setIcon(QIcon::fromTheme("color-picker"));
	layout->addRow(d->colorPickBox);
	connect(
		d->colorPickBox, &QCheckBox::clicked,
		makeBrushChangeCallbackArg<bool>([this](bool checked) {
			d->brush.classic().colorpick = checked;
			emitChange();
		}));

	d->pixelPerfectBox = buildPixelPerfectBox();
	layout->addRow(d->pixelPerfectBox);
	connect(
		d->pixelPerfectBox, &QCheckBox::clicked,
		makeBrushChangeCallbackArg<bool>([this](bool checked) {
			d->brush.setPixelPerfect(checked);
			emitChange();
		}));

	d->spacingSpinner = new KisSliderSpinBox{widget};
	layout->addRow(d->spacingSpinner);
	d->spacingSpinner->setRange(1, 999);
	d->spacingSpinner->setSingleStep(1);
	d->spacingSpinner->setExponentRatio(3.0);
	d->spacingSpinner->setPrefix(tr("Spacing: "));
	d->spacingSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(d->spacingSpinner);
	connect(
		d->spacingSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().spacing = value / 100.0;
			emitChange();
		}));

	d->stabilizationModeCombo = new QComboBox{widget};
	layout->addRow(tr("Stabilization Mode:"), d->stabilizationModeCombo);
	d->stabilizationModeCombo->addItem(
		tr("Time-Based Stabilizer"), int(brushes::Stabilizer));
	d->stabilizationModeCombo->addItem(
		tr("Average Smoothing"), int(brushes::Smoothing));
	connect(
		d->stabilizationModeCombo,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		makeBrushChangeCallbackArg<int>([this](int index) {
			d->brush.setStabilizationMode(brushes::StabilizationMode(
				d->stabilizationModeCombo->itemData(index).toInt()));
			emitChange();
		}));

	d->stabilizerSpinner = new KisSliderSpinBox{widget};
	layout->addRow(d->stabilizerSpinner);
	d->stabilizerSpinner->setRange(0, 1000);
	d->stabilizerSpinner->setPrefix(tr("Stabilizer: "));
	d->stabilizerSpinner->setSingleStep(1);
	d->stabilizerSpinner->setExponentRatio(3.0);
	kineticScroller->disableKineticScrollingOnWidget(d->stabilizerSpinner);
	connect(
		d->stabilizerSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.setStabilizerSampleCount(value);
			emitChange();
		}));

	d->smoothingSpinner = new KisSliderSpinBox{widget};
	layout->addRow(d->smoothingSpinner);
	d->smoothingSpinner->setRange(0, 20);
	d->smoothingSpinner->setPrefix(tr("Smoothing: "));
	d->smoothingSpinner->setSingleStep(1);
	kineticScroller->disableKineticScrollingOnWidget(d->smoothingSpinner);
	connect(
		d->smoothingSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.setSmoothing(value - d->globalSmoothing);
			emitChange();
		}));

	d->stabilizerExplanationLabel = new QLabel{widget};
	layout->addRow(d->stabilizerExplanationLabel);
	d->stabilizerExplanationLabel->setWordWrap(true);
	d->stabilizerExplanationLabel->setTextFormat(Qt::RichText);

	d->blendModeManager = BlendModeManager::initBrush(
		d->brushModeCombo, d->eraseModeCombo, d->preserveAlphaBox,
		d->eraseModeBox, this);
	dpApp().settings().bindAutomaticAlphaPreserve(
		d->blendModeManager, &BlendModeManager::setAutomaticAlphaPerserve);
	connect(
		d->blendModeManager, &BlendModeManager::blendModeChanged,
		[this](int blendMode, bool eraseMode) {
			if(!isUpdating()) {
				d->brush.classic().erase = eraseMode;
				d->brush.myPaint().brush().erase = eraseMode;
				d->brush.setBlendMode(blendMode, eraseMode);
				emitChange();
			}
		});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicSizePageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicSizeSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSizeSpinner);
	d->classicSizeSpinner->setExponentRatio(3.0);
	d->classicSizeSpinner->setRange(0, 1000);
	d->classicSizeSpinner->setPrefix(tr("Size: "));
	d->classicSizeSpinner->setSuffix(tr("px"));
	kineticScroller->disableKineticScrollingOnWidget(d->classicSizeSpinner);
	connect(
		d->classicSizeSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().size.max = value;
			emitChange();
		}));

	d->classicSizeDynamics = buildClassicDynamics(
		layout, &brushes::ClassicBrush::setSizeDynamicType,
		&brushes::ClassicBrush::setSizeMaxVelocity,
		&brushes::ClassicBrush::setSizeMaxDistance);

	d->classicSizeMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSizeMinSpinner);
	d->classicSizeMinSpinner->setExponentRatio(3.0);
	d->classicSizeMinSpinner->setRange(0, 1000);
	d->classicSizeMinSpinner->setPrefix(tr("Minimum Size: "));
	d->classicSizeMinSpinner->setSuffix(tr("px"));
	kineticScroller->disableKineticScrollingOnWidget(d->classicSizeMinSpinner);
	connect(
		d->classicSizeMinSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().size.min = value;
			emitChange();
		}));

	d->classicSizeCurve =
		new widgets::CurveWidget{tr("Input"), tr("Size"), false, widget};
	layout->addWidget(d->classicSizeCurve);
	buildClassicApplyToAllButton(d->classicSizeCurve);
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicSizeCurve->curveWidget());
	connect(
		d->classicSizeCurve, &widgets::CurveWidget::curveChanged,
		makeBrushChangeCallbackArg<const KisCubicCurve &>(
			[this](const KisCubicCurve &curve) {
				d->brush.classic().setSizeCurve(curve);
				emitChange();
			}));

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicOpacityPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicOpacitySpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicOpacitySpinner);
	d->classicOpacitySpinner->setRange(0, 100);
	d->classicOpacitySpinner->setPrefix(tr("Opacity: "));
	d->classicOpacitySpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(d->classicOpacitySpinner);
	connect(
		d->classicOpacitySpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().opacity.max = value / 100.0;
			emitChange();
		}));

	d->classicOpacityDynamics = buildClassicDynamics(
		layout, &brushes::ClassicBrush::setOpacityDynamicType,
		&brushes::ClassicBrush::setOpacityMaxVelocity,
		&brushes::ClassicBrush::setOpacityMaxDistance);

	d->classicOpacityMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicOpacityMinSpinner);
	d->classicOpacityMinSpinner->setRange(0, 100);
	d->classicOpacityMinSpinner->setPrefix(tr("Minimum Opacity: "));
	d->classicOpacityMinSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicOpacityMinSpinner);
	connect(
		d->classicOpacityMinSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().opacity.min = value / 100.0;
			emitChange();
		}));

	d->classicOpacityCurve =
		new widgets::CurveWidget{tr("Input"), tr("Opacity"), false, widget};
	layout->addWidget(d->classicOpacityCurve);
	buildClassicApplyToAllButton(d->classicOpacityCurve);
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicOpacityCurve->curveWidget());
	connect(
		d->classicOpacityCurve, &widgets::CurveWidget::curveChanged,
		makeBrushChangeCallbackArg<const KisCubicCurve &>(
			[this](const KisCubicCurve &curve) {
				d->brush.classic().setOpacityCurve(curve);
				emitChange();
			}));

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicHardnessPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicHardnessSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicHardnessSpinner);
	d->classicHardnessSpinner->setRange(0, 100);
	d->classicHardnessSpinner->setPrefix(tr("Hardness: "));
	d->classicHardnessSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(d->classicHardnessSpinner);
	connect(
		d->classicHardnessSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().hardness.max = value / 100.0;
			emitChange();
		}));

	d->classicHardnessDynamics = buildClassicDynamics(
		layout, &brushes::ClassicBrush::setHardnessDynamicType,
		&brushes::ClassicBrush::setHardnessMaxVelocity,
		&brushes::ClassicBrush::setHardnessMaxDistance);

	d->classicHardnessMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicHardnessMinSpinner);
	d->classicHardnessMinSpinner->setRange(0, 100);
	d->classicHardnessMinSpinner->setPrefix(tr("Minimum Hardness: "));
	d->classicHardnessMinSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicHardnessMinSpinner);
	connect(
		d->classicHardnessMinSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().hardness.min = value / 100.0;
			emitChange();
		}));

	d->classicHardnessCurve =
		new widgets::CurveWidget{tr("Input"), tr("Hardness"), false, widget};
	layout->addWidget(d->classicHardnessCurve);
	buildClassicApplyToAllButton(d->classicHardnessCurve);
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicHardnessCurve->curveWidget());
	connect(
		d->classicHardnessCurve, &widgets::CurveWidget::curveChanged,
		makeBrushChangeCallbackArg<const KisCubicCurve &>(
			[this](const KisCubicCurve &curve) {
				d->brush.classic().setHardnessCurve(curve);
				emitChange();
			}));

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicSmudgingPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicSmudgeAlphaBox = new QCheckBox(tr("Smudge with transparency"));
	d->classicSmudgeAlphaBox->setToolTip(
		tr("Enabling this will make smudging take the alpha channel into "
		   "account.\nDisabling it reverts the behavior to how it was before "
		   "Drawpile 2.3."));
	layout->addWidget(d->classicSmudgeAlphaBox);
	connect(
		d->classicSmudgeAlphaBox, &QCheckBox::clicked, this,
		[this](bool checked) {
			d->brush.classic().smudge_alpha = checked;
			emitChange();
		});

	d->classicSyncSamplesBox = buildSyncSamplesBox();
	layout->addWidget(d->classicSyncSamplesBox);
	connect(
		d->classicSyncSamplesBox, &QCheckBox::clicked, this,
		[this](bool checked) {
			d->brush.classic().syncSamples = checked;
			emitChange();
		});

	d->classicSmudgingSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSmudgingSpinner);
	d->classicSmudgingSpinner->setRange(0, 100);
	d->classicSmudgingSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(d->classicSmudgingSpinner);
	connect(
		d->classicSmudgingSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().smudge.max = value / 100.0;
			emitChange();
		}));

	d->classicColorPickupSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicColorPickupSpinner);
	d->classicColorPickupSpinner->setRange(1, 32);
	d->classicColorPickupSpinner->setPrefix(tr("Color Pickup: 1/"));
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicColorPickupSpinner);
	connect(
		d->classicColorPickupSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().resmudge = value;
			emitChange();
		}));

	d->classicSmudgeDynamics = buildClassicDynamics(
		layout, &brushes::ClassicBrush::setSmudgeDynamicType,
		&brushes::ClassicBrush::setSmudgeMaxVelocity,
		&brushes::ClassicBrush::setSmudgeMaxDistance);

	d->classicSmudgingMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSmudgingMinSpinner);
	d->classicSmudgingMinSpinner->setRange(0, 100);
	d->classicSmudgingMinSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicSmudgingMinSpinner);
	connect(
		d->classicSmudgingMinSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().smudge.min = value / 100.0;
			emitChange();
		}));

	d->classicSmudgingCurve =
		new widgets::CurveWidget{tr("Input"), QString(), false, widget};
	layout->addWidget(d->classicSmudgingCurve);
	buildClassicApplyToAllButton(d->classicSmudgingCurve);
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicSmudgingCurve->curveWidget());
	connect(
		d->classicSmudgingCurve, &widgets::CurveWidget::curveChanged,
		makeBrushChangeCallbackArg<const KisCubicCurve &>(
			[this](const KisCubicCurve &curve) {
				d->brush.classic().setSmudgeCurve(curve);
				emitChange();
			}));

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicJitterPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicJitterSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicJitterSpinner);
	d->classicJitterSpinner->setRange(0, 1000);
	d->classicJitterSpinner->setPrefix(tr("Jitter: "));
	d->classicJitterSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(d->classicJitterSpinner);
	connect(
		d->classicJitterSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().jitter.max = value / 100.0;
			emitChange();
		}));

	d->classicJitterDynamics = buildClassicDynamics(
		layout, &brushes::ClassicBrush::setJitterDynamicType,
		&brushes::ClassicBrush::setJitterMaxVelocity,
		&brushes::ClassicBrush::setJitterMaxDistance);

	d->classicJitterMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicJitterMinSpinner);
	d->classicJitterMinSpinner->setRange(0, 100);
	d->classicJitterMinSpinner->setPrefix(tr("Minimum Jitter: "));
	d->classicJitterMinSpinner->setSuffix(tr("%"));
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicJitterMinSpinner);
	connect(
		d->classicJitterMinSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([this](int value) {
			d->brush.classic().jitter.min = value / 100.0;
			emitChange();
		}));

	d->classicJitterCurve =
		new widgets::CurveWidget{tr("Input"), tr("Jitter"), false, widget};
	layout->addWidget(d->classicJitterCurve);
	buildClassicApplyToAllButton(d->classicJitterCurve);
	kineticScroller->disableKineticScrollingOnWidget(
		d->classicJitterCurve->curveWidget());
	connect(
		d->classicJitterCurve, &widgets::CurveWidget::curveChanged,
		makeBrushChangeCallbackArg<const KisCubicCurve &>(
			[this](const KisCubicCurve &curve) {
				d->brush.classic().setJitterCurve(curve);
				emitChange();
			}));

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QComboBox *BrushSettingsDialog::buildClassicTypeCombo()
{
	// Qt fails to load these translations if this is not in its own function
	// for some reason. I really don't know what's going on, but whatever.
	QComboBox *typeCombo = new QComboBox;
	typeCombo->addItem(tr("No dynamics"), int(DP_CLASSIC_BRUSH_DYNAMIC_NONE));
	typeCombo->addItem(
		tr("Pressure dynamics"), int(DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE));
	typeCombo->addItem(
		tr("Velocity dynamics"), int(DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY));
	typeCombo->addItem(
		tr("Distance dynamics"), int(DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE));
	return typeCombo;
}

BrushSettingsDialog::Dynamics BrushSettingsDialog::buildClassicDynamics(
	QVBoxLayout *layout,
	void (brushes::ClassicBrush::*setType)(DP_ClassicBrushDynamicType),
	void (brushes::ClassicBrush::*setVelocity)(float),
	void (brushes::ClassicBrush::*setDistance)(float))
{
	QComboBox *typeCombo = buildClassicTypeCombo();
	connect(
		typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		makeBrushChangeCallbackArg<int>([this, typeCombo, setType](int index) {
			int type = typeCombo->itemData(index).toInt();
			(d->brush.classic().*setType)(DP_ClassicBrushDynamicType(type));
			emitChange();
		}));

	KisSliderSpinBox *velocitySlider = new KisSliderSpinBox;
	velocitySlider->setRange(1, 1000);
	velocitySlider->setPrefix(tr("Maximum Velocity: "));
	connect(
		velocitySlider, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([=](int value) {
			(d->brush.classic().*setVelocity)(float(value) / 100.0f);
			emitChange();
		}));

	KisSliderSpinBox *distanceSlider = new KisSliderSpinBox;
	distanceSlider->setRange(1, 10000);
	distanceSlider->setExponentRatio(3.0);
	distanceSlider->setPrefix(tr("Maximum Distance: "));
	connect(
		distanceSlider, QOverload<int>::of(&QSpinBox::valueChanged),
		makeBrushChangeCallbackArg<int>([=](int value) {
			(d->brush.classic().*setDistance)(float(value));
			emitChange();
		}));

	QPushButton *applyVelocityToAllButton =
		new QPushButton(QIcon::fromTheme("fill-color"), tr("Apply to All"));
	applyVelocityToAllButton->setToolTip(
		tr("Set the maximum velocity for Size, Opacity, Hardness, Smudging and "
		   "Jitter at once."));
	connect(
		applyVelocityToAllButton, &QPushButton::clicked,
		makeBrushChangeCallback([=]() {
			brushes::ClassicBrush &b = d->brush.classic();
			float maxVelocity = float(velocitySlider->value()) / 100.0f;
			b.setSizeMaxVelocity(maxVelocity);
			b.setOpacityMaxVelocity(maxVelocity);
			b.setHardnessMaxVelocity(maxVelocity);
			b.setSmudgeMaxVelocity(maxVelocity);
			b.setJitterMaxVelocity(maxVelocity);
			emitChange();
			ToolMessage::showText(
				tr("Maximum velocity set for all settings in this brush."));
		}));

	QPushButton *applyDistanceToAllButton =
		new QPushButton(QIcon::fromTheme("fill-color"), tr("Apply to All"));
	applyDistanceToAllButton->setToolTip(
		tr("Set the maximum distance for Size, Opacity, Hardness, Smudging and "
		   "Jitter at once."));
	connect(
		applyDistanceToAllButton, &QPushButton::clicked,
		makeBrushChangeCallback([=]() {
			brushes::ClassicBrush &b = d->brush.classic();
			float maxDistance = float(distanceSlider->value());
			b.setSizeMaxDistance(maxDistance);
			b.setOpacityMaxDistance(maxDistance);
			b.setHardnessMaxDistance(maxDistance);
			b.setSmudgeMaxDistance(maxDistance);
			b.setJitterMaxDistance(maxDistance);
			emitChange();
			ToolMessage::showText(
				tr("Maximum distance set for all settings in this brush."));
		}));

	QGridLayout *grid = new QGridLayout;
	grid->setColumnStretch(0, 1);
	grid->addWidget(typeCombo, 0, 0, 1, 2);
	grid->addWidget(velocitySlider, 1, 0);
	grid->addWidget(distanceSlider, 2, 0);
	grid->addWidget(applyVelocityToAllButton, 1, 1);
	grid->addWidget(applyDistanceToAllButton, 2, 1);
	layout->addLayout(grid);

	return {
		typeCombo, velocitySlider, distanceSlider, applyVelocityToAllButton,
		applyDistanceToAllButton};
}

void BrushSettingsDialog::buildClassicApplyToAllButton(
	widgets::CurveWidget *curve)
{
	QPushButton *applyToAllButton =
		new QPushButton{QIcon::fromTheme("fill-color"), tr("Apply to All")};
	applyToAllButton->setToolTip(
		tr("Set this curve for Size, Opacity, Hardness and Smudging at once."));
	curve->addButton(applyToAllButton);
	connect(
		applyToAllButton, &QPushButton::clicked, makeBrushChangeCallback([=]() {
			applyCurveToAllClassicSettings(curve->curve());
			ToolMessage::showText(
				tr("Curve set for all settings in this brush."));
		}));
}

QWidget *BrushSettingsDialog::buildMyPaintPageUi(int setting)
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);
	utils::KineticScroller *kineticScroller =
		utils::bindKineticScrolling(scroll);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	const MyPaintBrushSettingInfo *settingInfo =
		mypaint_brush_setting_info(MyPaintBrushSetting(setting));
	Private::MyPaintPage &page = d->myPaintPages[setting];

	page.syncSamplesBox = nullptr;
	page.pixelPerfectBox = nullptr;
	if(isSmudgeMyPaintSetting(setting)) {
		page.syncSamplesBox = buildSyncSamplesBox();
		layout->addWidget(page.syncSamplesBox);
		connect(
			page.syncSamplesBox, &QCheckBox::clicked, this,
			[this](bool checked) {
				d->brush.myPaint().setSyncSamples(checked);
				emitChange();
			});
	} else if(isPixelPerfectMyPaintSetting(setting)) {
		page.pixelPerfectBox = buildPixelPerfectBox();
		layout->addWidget(page.pixelPerfectBox);
		connect(
			page.pixelPerfectBox, &QCheckBox::clicked, this,
			[this](bool checked) {
				d->brush.myPaint().setPixelPerfect(checked);
				emitChange();
			});
	}

	page.baseValueSpinner = new KisDoubleSliderSpinBox{widget};
	layout->addWidget(page.baseValueSpinner);
	page.baseValueSpinner->setPrefix(tr("Value: "));
	page.baseValueSpinner->setRange(settingInfo->min, settingInfo->max, 2);
	kineticScroller->disableKineticScrollingOnWidget(page.baseValueSpinner);
	connect(
		page.baseValueSpinner,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		makeBrushChangeCallbackArg<double>([=](double value) {
			d->brush.myPaint().settings().mappings[setting].base_value = value;
			emitChange();
		}));

	if(settingInfo->constant) {
		page.constantLabel = new QLabel{tr("No brush dynamics."), widget};
		page.constantLabel->setWordWrap(true);
		layout->addWidget(page.constantLabel);
	} else {
		for(int input = 0; input < MYPAINT_BRUSH_INPUTS_COUNT; ++input) {
			layout->addWidget(buildMyPaintInputUi(
				setting, input, settingInfo, kineticScroller));
		}
	}

	switch(getMyPaintCondition(setting)) {
	case MyPaintCondition::IndirectDisabled:
		page.disabledLabel =
			new QLabel(tr("Not available in indirect paint modes."));
		break;
	case MyPaintCondition::BlendOrIndirectDisabled:
		page.disabledLabel =
			new QLabel(tr("Not available in indirect paint modes or when using "
						  "a blend mode other than Normal."));
		break;
	case MyPaintCondition::ComparesAlphaOrIndirectDisabled:
		page.disabledLabel =
			new QLabel(tr("Not available in indirect paint modes or when using "
						  "the Marker or Greater Density blend modes."));
		break;
	default:
		page.disabledLabel = nullptr;
	}

	if(page.disabledLabel) {
		page.disabledLabel->setWordWrap(true);
		layout->addWidget(page.disabledLabel);
	}

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

widgets::MyPaintInput *BrushSettingsDialog::buildMyPaintInputUi(
	int setting, int input, const MyPaintBrushSettingInfo *settingInfo,
	utils::KineticScroller *kineticScroller)
{
	widgets::MyPaintInput *inputWidget = new widgets::MyPaintInput{
		getMyPaintInputTitle(input), getMyPaintInputDescription(input),
		getMyPaintSettingTitle(setting), settingInfo,
		mypaint_brush_input_info(MyPaintBrushInput(input))};
	d->myPaintPages[setting].inputs[input] = inputWidget;
	connect(
		inputWidget, &widgets::MyPaintInput::controlPointsChanged,
		makeBrushChangeCallback([this, setting, input]() {
			widgets::MyPaintInput *otherWidget =
				d->myPaintPages[setting].inputs[input];
			brushes::MyPaintBrush &mypaint = d->brush.myPaint();
			mypaint.setCurve(setting, input, otherWidget->myPaintCurve());
			mypaint.settings().mappings[setting].inputs[input] =
				otherWidget->controlPoints();
			emitChange();
		}));
	connect(
		inputWidget, &widgets::MyPaintInput::curveWidgetsConstructed, this,
		[kineticScroller, inputWidget] {
			kineticScroller->disableKineticScrollingOnWidget(
				inputWidget->ySpinner());
			kineticScroller->disableKineticScrollingOnWidget(
				inputWidget->xMinSpinner());
			kineticScroller->disableKineticScrollingOnWidget(
				inputWidget->xMaxSpinner());
			kineticScroller->disableKineticScrollingOnWidget(
				inputWidget->curveWidget()->curveWidget());
		});
	return inputWidget;
}

QCheckBox *BrushSettingsDialog::buildSyncSamplesBox()
{
	QCheckBox *box =
		new QCheckBox(tr("Synchronize smudging (slower, but more accurate)"));
	box->setToolTip(
		tr("This will make the brush to wait for its own stroke to finish to "
		   "allow it to accurately smudge with itself.\nIf fast strokes cause "
		   "artifacts when smudging, enabling this can help."));
	return box;
}

QCheckBox *BrushSettingsDialog::buildPixelPerfectBox()
{
	QCheckBox *box = new QCheckBox(tr("Pixel-perfect"));
	box->setIcon(QIcon::fromTheme("drawpile_pixelperfect"));
	box->setToolTip(
		tr("Prevents L-shaped curves, mostly useful for small pixel brushes."));
	return box;
}

void BrushSettingsDialog::applyCurveToAllClassicSettings(
	const KisCubicCurve &curve)
{
	d->brush.classic().setSizeCurve(curve);
	d->brush.classic().setOpacityCurve(curve);
	d->brush.classic().setHardnessCurve(curve);
	d->brush.classic().setSmudgeCurve(curve);
	d->brush.classic().setJitterCurve(curve);
	emitChange();
}


void BrushSettingsDialog::addCategory(
	const QString &text, const QString &toolTip, int pageIndex)
{
	QListWidgetItem *item = new QListWidgetItem{d->categoryWidget};
	item->setText(text);
	item->setData(Qt::UserRole, pageIndex);
	item->setToolTip(toolTip);
	d->categoryWidget->addItem(item);
}

void BrushSettingsDialog::addClassicCategories(bool withHardness)
{
	addCategory(
		tr("Size"), tr("The radius of the brush."), d->classicSizePageIndex);
	addCategory(
		tr("Opacity"),
		tr("Opaqueness of the brush, 0% is transparent, 100% fully opaque."),
		d->classicOpacityPageIndex);
	if(withHardness) {
		addCategory(
			tr("Hardness"), tr("Edge hardness, 0% is blurry, 100% is sharp."),
			d->classicHardnessPageIndex);
	}
	addCategory(
		tr("Smudging"), tr("Blending of colors on the layer being drawn on."),
		d->classicSmudgePageIndex);
	addCategory(
		tr("Jitter"), tr("Randomized offsets in the stroke center."),
		d->classicJitterPageIndex);
}

void BrushSettingsDialog::addMyPaintCategories()
{
	for(int setting = 0; setting < MYPAINT_BRUSH_SETTINGS_COUNT; ++setting) {
		if(shouldIncludeMyPaintSetting(setting)) {
			addCategory(
				getMyPaintSettingTitle(setting),
				getMyPaintSettingDescription(setting),
				d->mypaintPageIndexes[setting]);
		}
	}
}


void BrushSettingsDialog::updateUiFromClassicBrush()
{
	const brushes::ClassicBrush &classic = d->brush.classic();

	int brushMode = DP_classic_brush_blend_mode(&classic);
	d->blendModeManager->setMyPaint(false);
	d->blendModeManager->selectBlendMode(brushMode);

	d->colorPickBox->setChecked(classic.colorpick);
	d->colorPickBox->setEnabled(
		brushMode != DP_BLEND_MODE_ERASE &&
		brushMode != DP_BLEND_MODE_ERASE_PRESERVE);
	d->colorPickBox->setVisible(true);

	bool isPixelBrush = classic.shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND ||
						classic.shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
	d->pixelPerfectBox->setChecked(classic.pixel_perfect);
	d->pixelPerfectBox->setEnabled(isPixelBrush);
	d->pixelPerfectBox->setVisible(isPixelBrush);

	bool forceDirectMode =
		canvas::blendmode::directOnly(brushMode) || classic.smudge.max > 0.0f;
	setComboBoxIndexByData(
		d->paintModeCombo,
		forceDirectMode ? DP_PAINT_MODE_DIRECT : int(classic.paint_mode));
	d->paintModeCombo->setDisabled(forceDirectMode);

	d->spacingSpinner->setValue(classic.spacing * 100.0 + 0.5);
	d->spacingSpinner->setVisible(true);

	d->classicSizeSpinner->setValue(classic.size.max);
	bool haveSizeDynamics = updateClassicBrushDynamics(
		d->classicSizeDynamics, classic.size_dynamic);
	d->classicSizeMinSpinner->setValue(classic.size.min);
	d->classicSizeMinSpinner->setEnabled(haveSizeDynamics);
	d->classicSizeCurve->setCurve(classic.sizeCurve());
	d->classicSizeCurve->setEnabled(haveSizeDynamics);

	d->classicOpacitySpinner->setValue(classic.opacity.max * 100.0 + 0.5);
	bool haveOpacityDynamics = updateClassicBrushDynamics(
		d->classicOpacityDynamics, classic.opacity_dynamic);
	d->classicOpacityMinSpinner->setValue(classic.opacity.min * 100.0 + 0.5);
	d->classicOpacityMinSpinner->setEnabled(haveOpacityDynamics);
	d->classicOpacityCurve->setCurve(classic.opacityCurve());
	d->classicOpacityCurve->setEnabled(haveOpacityDynamics);

	d->classicHardnessSpinner->setValue(classic.hardness.max * 100.0 + 0.5);
	bool haveHardnessDynamics = updateClassicBrushDynamics(
		d->classicHardnessDynamics, classic.hardness_dynamic);
	d->classicHardnessMinSpinner->setValue(classic.hardness.min * 100.0 + 0.5);
	d->classicHardnessMinSpinner->setEnabled(haveHardnessDynamics);
	d->classicHardnessCurve->setCurve(classic.hardnessCurve());
	d->classicHardnessCurve->setEnabled(haveHardnessDynamics);

	bool smudgeAlpha = classic.smudge_alpha && !d->compatibilityMode;
	d->classicSmudgingSpinner->setValue(classic.smudge.max * 100.0 + 0.5);
	d->classicSmudgingSpinner->setPrefix(
		smudgeAlpha ? tr("Smudging: ") : tr("Blending: "));
	d->classicColorPickupSpinner->setValue(classic.resmudge);
	d->classicSmudgeAlphaBox->setChecked(classic.smudge_alpha);
	d->classicSyncSamplesBox->setChecked(classic.syncSamples);
	bool haveSmudgeDynamics = updateClassicBrushDynamics(
		d->classicSmudgeDynamics, classic.smudge_dynamic);
	d->classicSmudgingMinSpinner->setValue(classic.smudge.min * 100.0 + 0.5);
	d->classicSmudgingMinSpinner->setEnabled(haveSmudgeDynamics);
	d->classicSmudgingMinSpinner->setPrefix(
		smudgeAlpha ? tr("Minimum Smudging: ") : tr("Minimum Blending: "));
	d->classicSmudgingCurve->setCurve(classic.smudgeCurve());
	d->classicSmudgingCurve->setEnabled(haveSmudgeDynamics);
	d->classicSmudgingCurve->setAxisTitleLabelY(
		smudgeAlpha ? tr("Smudging") : tr("Blending"));

	d->classicJitterSpinner->setValue(classic.jitter.max * 100.0 + 0.5);
	bool haveJitterDynamics = updateClassicBrushDynamics(
		d->classicJitterDynamics, classic.jitter_dynamic);
	d->classicJitterMinSpinner->setValue(classic.jitter.min * 100.0 + 0.5);
	d->classicJitterMinSpinner->setEnabled(haveJitterDynamics);
	d->classicJitterCurve->setCurve(classic.jitterCurve());
	d->classicJitterCurve->setEnabled(haveJitterDynamics);
}

bool BrushSettingsDialog::updateClassicBrushDynamics(
	Dynamics &dynamics, const DP_ClassicBrushDynamic &brush)
{
	DP_ClassicBrushDynamicType type = brush.type;
	setComboBoxIndexByData(dynamics.typeCombo, type);

	dynamics.velocitySlider->setValue(int(brush.max_velocity * 100.0f + 0.5));
	bool isVelocity = type == DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY;
	dynamics.velocitySlider->setEnabled(isVelocity);
	dynamics.velocitySlider->setVisible(isVelocity);
	dynamics.applyVelocityToAllButton->setEnabled(isVelocity);
	dynamics.applyVelocityToAllButton->setVisible(isVelocity);

	dynamics.distanceSlider->setValue(int(brush.max_distance + 0.5));
	bool isDistance = type == DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE;
	dynamics.distanceSlider->setEnabled(isDistance);
	dynamics.distanceSlider->setVisible(isDistance);
	dynamics.applyDistanceToAllButton->setEnabled(isDistance);
	dynamics.applyDistanceToAllButton->setVisible(isDistance);

	return type != DP_CLASSIC_BRUSH_DYNAMIC_NONE;
}

void BrushSettingsDialog::updateUiFromMyPaintBrush()
{
	const DP_MyPaintBrush &brush = d->brush.myPaint().constBrush();
	d->colorPickBox->setVisible(false);
	d->pixelPerfectBox->setVisible(true);
	d->pixelPerfectBox->setChecked(brush.pixel_perfect);
	d->spacingSpinner->setVisible(false);

	int brushMode = DP_mypaint_brush_blend_mode(&brush);
	d->blendModeManager->setMyPaint(true);
	d->blendModeManager->selectBlendMode(brushMode);

	bool forceDirectMode = canvas::blendmode::directOnly(brushMode);
	setComboBoxIndexByData(
		d->paintModeCombo,
		forceDirectMode ? DP_PAINT_MODE_DIRECT : int(brush.paint_mode));
	d->paintModeCombo->setDisabled(forceDirectMode);

	d->preserveAlphaBox->setChecked(
		canvas::blendmode::presentsAsAlphaPreserving(brushMode));
	d->eraseModeBox->setChecked(brush.erase);

	for(int setting = 0; setting < MYPAINT_BRUSH_SETTINGS_COUNT; ++setting) {
		if(shouldIncludeMyPaintSetting(setting)) {
			updateMyPaintSettingPage(setting);
		}
	}
}

void BrushSettingsDialog::updateMyPaintSettingPage(int setting)
{
	brushes::MyPaintBrush &mypaint = d->brush.myPaint();
	const DP_MyPaintSettings &settings = mypaint.constSettings();

	const DP_MyPaintMapping &mapping = settings.mappings[setting];
	Private::MyPaintPage &page = d->myPaintPages[setting];
	page.baseValueSpinner->setValue(mapping.base_value);

	bool enabled;
	const DP_MyPaintBrush &brush = mypaint.constBrush();
	switch(getMyPaintCondition(setting)) {
	case MyPaintCondition::IndirectDisabled:
		enabled = brush.paint_mode == DP_PAINT_MODE_DIRECT;
		break;
	case MyPaintCondition::BlendOrIndirectDisabled:
		enabled = brush.paint_mode == DP_PAINT_MODE_DIRECT &&
				  DP_mypaint_brush_blend_mode(&brush) == DP_BLEND_MODE_NORMAL;
		break;
	case MyPaintCondition::ComparesAlphaOrIndirectDisabled:
		enabled = brush.paint_mode == DP_PAINT_MODE_DIRECT &&
				  !canvas::blendmode::comparesAlpha(
					  int(DP_mypaint_brush_blend_mode(&brush)));
		break;
	default:
		enabled = true;
		break;
	}

	bool pixelPerfect;
	if(page.pixelPerfectBox) {
		pixelPerfect = mypaint.isPixelPerfect();
		page.pixelPerfectBox->setChecked(pixelPerfect);
		page.pixelPerfectBox->setVisible(enabled);
	} else {
		pixelPerfect = false;
	}

	bool visible = enabled && !pixelPerfect;
	page.baseValueSpinner->setVisible(visible);

	const MyPaintBrushSettingInfo *settingInfo =
		mypaint_brush_setting_info(MyPaintBrushSetting(setting));
	if(settingInfo->constant) {
		page.constantLabel->setVisible(visible);
	} else {
		for(int input = 0; input < MYPAINT_BRUSH_INPUTS_COUNT; ++input) {
			widgets::MyPaintInput *inputWidget = page.inputs[input];
			brushes::MyPaintCurve curve = mypaint.getCurve(setting, input);
			if(curve.isValid()) {
				inputWidget->setMyPaintCurve(curve);
			} else {
				inputWidget->setControlPoints(mapping.inputs[input]);
				mypaint.setCurve(setting, input, inputWidget->myPaintCurve());
			}
			inputWidget->setVisible(visible);
		}
	}

	if(page.disabledLabel) {
		page.disabledLabel->setVisible(!enabled);
	}

	if(page.syncSamplesBox) {
		page.syncSamplesBox->setChecked(mypaint.isSyncSamples());
		page.syncSamplesBox->setVisible(enabled);
	}
}

void BrushSettingsDialog::updateStabilizerExplanationText()
{
	QString text;
	if(d->useBrushSampleCount) {
		switch(d->brush.stabilizationMode()) {
		case brushes::Stabilizer:
			text = tr(
				"Slows down the stroke to stabilize it over time. High values "
				"give very smooth lines, but they will draw slowly. When you "
				"stop moving, the line will catch up your cursor. Tablet "
				"smoothing from the input preferences applies as well.");
			break;
		case brushes::Smoothing:
			text = tr("Simply averages a number of inputs. Feels faster than "
					  "the time-based stabilizer, but not as smooth and won't "
					  "catch up to your cursor when you stop moving. Overrides "
					  "tablet smoothing from the input preferences.");
			break;
		default:
			text = tr("Unknown stabilization mode.");
			break;
		}
	} else {
		text = tr(
			"Synchronizing stabilization settings with brushes is disabled.");
	}
	d->stabilizerExplanationLabel->setText(text);
}

void BrushSettingsDialog::emitChange()
{
	emit brushSettingsChanged(d->brush);
}


bool BrushSettingsDialog::isUpdating() const
{
	return d->updating;
}


void BrushSettingsDialog::setComboBoxIndexByData(QComboBox *combo, int data)
{
	int count = combo->count();
	for(int i = 0; i < count; ++i) {
		if(combo->itemData(i).toInt() == data) {
			combo->setCurrentIndex(i);
			break;
		}
	}
}

bool BrushSettingsDialog::shouldIncludeMyPaintSetting(int setting)
{
	switch(setting) {
	// The brush settings dialog is not the place to set the brush color.
	case MYPAINT_BRUSH_SETTING_COLOR_H:
	case MYPAINT_BRUSH_SETTING_COLOR_S:
	case MYPAINT_BRUSH_SETTING_COLOR_V:
	// This is for storing the color in the brush, which we don't do.
	case MYPAINT_BRUSH_SETTING_RESTORE_COLOR:
	// Spectral painting is not supported in Drawpile.
	case MYPAINT_BRUSH_SETTING_PAINT_MODE:
	// This is a stabilizer, but Drawpile has its own one.
	case MYPAINT_BRUSH_SETTING_SLOW_TRACKING:
		return false;
	default:
		return true;
	}
}

BrushSettingsDialog::MyPaintCondition
BrushSettingsDialog::getMyPaintCondition(int setting)
{
	switch(setting) {
	// Opacity linearization is supposed to compensate for direct drawing mode,
	// in indirect mode its behavior is just really wrong, so we disable it. The
	// same goes for blend modes like Greater, which effectively do indirect
	// drawing directly on top of the layer.
	case MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE:
		return MyPaintCondition::ComparesAlphaOrIndirectDisabled;
	// Indirect mode can't smudge.
	case MYPAINT_BRUSH_SETTING_SMUDGE:
	case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH:
	case MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG:
	case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH_LOG:
	case MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET:
	case MYPAINT_BRUSH_SETTING_SMUDGE_TRANSPARENCY:
		return MyPaintCondition::IndirectDisabled;
	// Indirect mode also can't use multiple blend modes, nor can you use them
	// when you picked some non-Normal blend mode, since you get that instead.
	case MYPAINT_BRUSH_SETTING_ERASER:
	case MYPAINT_BRUSH_SETTING_LOCK_ALPHA:
	case MYPAINT_BRUSH_SETTING_COLORIZE:
	case MYPAINT_BRUSH_SETTING_POSTERIZE:
	case MYPAINT_BRUSH_SETTING_POSTERIZE_NUM:
		return MyPaintCondition::BlendOrIndirectDisabled;
	default:
		return MyPaintCondition::AlwaysEnabled;
	}
}

bool BrushSettingsDialog::isSmudgeMyPaintSetting(int setting)
{
	switch(setting) {
	case MYPAINT_BRUSH_SETTING_SMUDGE:
	case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH:
	case MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG:
	case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH_LOG:
	case MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET:
	case MYPAINT_BRUSH_SETTING_SMUDGE_TRANSPARENCY:
		return true;
	default:
		return false;
	}
}

bool BrushSettingsDialog::isPixelPerfectMyPaintSetting(int setting)
{
	switch(setting) {
	case MYPAINT_BRUSH_SETTING_HARDNESS:
	case MYPAINT_BRUSH_SETTING_ANTI_ALIASING:
	case MYPAINT_BRUSH_SETTING_SNAP_TO_PIXEL:
		return true;
	default:
		return false;
	}
}

void BrushSettingsDialog::requestShortcutChange()
{
	emit shortcutChangeRequested(d->presetId);
}

}
