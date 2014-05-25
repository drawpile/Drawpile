/********************************************************************************
** Form generated from reading UI file 'color_dialog.ui'
**
** Created by: Qt User Interface Compiler version 5.2.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COLOR_DIALOG_H
#define UI_COLOR_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include "color_preview.hpp"
#include "color_wheel.hpp"
#include "gradient_slider.hpp"
#include "hue_slider.hpp"

QT_BEGIN_NAMESPACE

class Ui_Color_Dialog
{
public:
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    Color_Wheel *wheel;
    Color_Preview *preview;
    QGridLayout *gridLayout;
    Gradient_Slider *slide_value;
    QLabel *label_7;
    QLabel *label_6;
    Gradient_Slider *slide_saturation;
    QLabel *label_8;
    QLabel *label_3;
    Gradient_Slider *slide_alpha;
    Gradient_Slider *slide_red;
    Gradient_Slider *slide_green;
    QLabel *label_5;
    QLabel *label_2;
    QLabel *label_alpha;
    QLabel *label;
    Gradient_Slider *slide_blue;
    QSpinBox *spin_hue;
    QSpinBox *spin_saturation;
    QSpinBox *spin_value;
    QSpinBox *spin_red;
    QSpinBox *spin_green;
    QSpinBox *spin_blue;
    QSpinBox *spin_alpha;
    QLineEdit *edit_hex;
    QFrame *line_alpha;
    QFrame *line;
    QFrame *line_3;
    Hue_Slider *slide_hue;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *Color_Dialog)
    {
        if (Color_Dialog->objectName().isEmpty())
            Color_Dialog->setObjectName(QStringLiteral("Color_Dialog"));
        Color_Dialog->resize(491, 313);
        QIcon icon;
        QString iconThemeName = QStringLiteral("format-fill-color");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon = QIcon::fromTheme(iconThemeName);
        } else {
            icon.addFile(QStringLiteral(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        Color_Dialog->setWindowIcon(icon);
        verticalLayout_2 = new QVBoxLayout(Color_Dialog);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        wheel = new Color_Wheel(Color_Dialog);
        wheel->setObjectName(QStringLiteral("wheel"));
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(wheel->sizePolicy().hasHeightForWidth());
        wheel->setSizePolicy(sizePolicy);

        verticalLayout->addWidget(wheel);

        preview = new Color_Preview(Color_Dialog);
        preview->setObjectName(QStringLiteral("preview"));
        preview->setProperty("display_mode", QVariant(Color_Preview::SplitColor));

        verticalLayout->addWidget(preview);


        horizontalLayout->addLayout(verticalLayout);

        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        slide_value = new Gradient_Slider(Color_Dialog);
        slide_value->setObjectName(QStringLiteral("slide_value"));
        slide_value->setMaximum(255);
        slide_value->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slide_value, 2, 1, 1, 1);

        label_7 = new QLabel(Color_Dialog);
        label_7->setObjectName(QStringLiteral("label_7"));

        gridLayout->addWidget(label_7, 1, 0, 1, 1);

        label_6 = new QLabel(Color_Dialog);
        label_6->setObjectName(QStringLiteral("label_6"));

        gridLayout->addWidget(label_6, 0, 0, 1, 1);

        slide_saturation = new Gradient_Slider(Color_Dialog);
        slide_saturation->setObjectName(QStringLiteral("slide_saturation"));
        slide_saturation->setMaximum(255);
        slide_saturation->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slide_saturation, 1, 1, 1, 1);

        label_8 = new QLabel(Color_Dialog);
        label_8->setObjectName(QStringLiteral("label_8"));

        gridLayout->addWidget(label_8, 10, 0, 1, 1);

        label_3 = new QLabel(Color_Dialog);
        label_3->setObjectName(QStringLiteral("label_3"));

        gridLayout->addWidget(label_3, 6, 0, 1, 1);

        slide_alpha = new Gradient_Slider(Color_Dialog);
        slide_alpha->setObjectName(QStringLiteral("slide_alpha"));
        slide_alpha->setMaximum(255);
        slide_alpha->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slide_alpha, 8, 1, 1, 1);

        slide_red = new Gradient_Slider(Color_Dialog);
        slide_red->setObjectName(QStringLiteral("slide_red"));
        slide_red->setMaximum(255);
        slide_red->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slide_red, 4, 1, 1, 1);

        slide_green = new Gradient_Slider(Color_Dialog);
        slide_green->setObjectName(QStringLiteral("slide_green"));
        slide_green->setMaximum(255);
        slide_green->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slide_green, 5, 1, 1, 1);

        label_5 = new QLabel(Color_Dialog);
        label_5->setObjectName(QStringLiteral("label_5"));

        gridLayout->addWidget(label_5, 2, 0, 1, 1);

        label_2 = new QLabel(Color_Dialog);
        label_2->setObjectName(QStringLiteral("label_2"));

        gridLayout->addWidget(label_2, 5, 0, 1, 1);

        label_alpha = new QLabel(Color_Dialog);
        label_alpha->setObjectName(QStringLiteral("label_alpha"));

        gridLayout->addWidget(label_alpha, 8, 0, 1, 1);

        label = new QLabel(Color_Dialog);
        label->setObjectName(QStringLiteral("label"));

        gridLayout->addWidget(label, 4, 0, 1, 1);

        slide_blue = new Gradient_Slider(Color_Dialog);
        slide_blue->setObjectName(QStringLiteral("slide_blue"));
        slide_blue->setMaximum(255);
        slide_blue->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slide_blue, 6, 1, 1, 1);

        spin_hue = new QSpinBox(Color_Dialog);
        spin_hue->setObjectName(QStringLiteral("spin_hue"));
        spin_hue->setWrapping(true);
        spin_hue->setMaximum(359);

        gridLayout->addWidget(spin_hue, 0, 2, 1, 1);

        spin_saturation = new QSpinBox(Color_Dialog);
        spin_saturation->setObjectName(QStringLiteral("spin_saturation"));
        spin_saturation->setMaximum(255);

        gridLayout->addWidget(spin_saturation, 1, 2, 1, 1);

        spin_value = new QSpinBox(Color_Dialog);
        spin_value->setObjectName(QStringLiteral("spin_value"));
        spin_value->setMaximum(255);

        gridLayout->addWidget(spin_value, 2, 2, 1, 1);

        spin_red = new QSpinBox(Color_Dialog);
        spin_red->setObjectName(QStringLiteral("spin_red"));
        spin_red->setMaximum(255);

        gridLayout->addWidget(spin_red, 4, 2, 1, 1);

        spin_green = new QSpinBox(Color_Dialog);
        spin_green->setObjectName(QStringLiteral("spin_green"));
        spin_green->setMaximum(255);

        gridLayout->addWidget(spin_green, 5, 2, 1, 1);

        spin_blue = new QSpinBox(Color_Dialog);
        spin_blue->setObjectName(QStringLiteral("spin_blue"));
        spin_blue->setMaximum(255);

        gridLayout->addWidget(spin_blue, 6, 2, 1, 1);

        spin_alpha = new QSpinBox(Color_Dialog);
        spin_alpha->setObjectName(QStringLiteral("spin_alpha"));
        spin_alpha->setMaximum(255);

        gridLayout->addWidget(spin_alpha, 8, 2, 1, 1);

        edit_hex = new QLineEdit(Color_Dialog);
        edit_hex->setObjectName(QStringLiteral("edit_hex"));
        QFont font;
        font.setFamily(QStringLiteral("Monospace"));
        edit_hex->setFont(font);

        gridLayout->addWidget(edit_hex, 10, 1, 1, 2);

        line_alpha = new QFrame(Color_Dialog);
        line_alpha->setObjectName(QStringLiteral("line_alpha"));
        line_alpha->setFrameShape(QFrame::HLine);
        line_alpha->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line_alpha, 7, 0, 1, 3);

        line = new QFrame(Color_Dialog);
        line->setObjectName(QStringLiteral("line"));
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line, 3, 0, 1, 3);

        line_3 = new QFrame(Color_Dialog);
        line_3->setObjectName(QStringLiteral("line_3"));
        line_3->setFrameShape(QFrame::HLine);
        line_3->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line_3, 9, 0, 1, 3);

        slide_hue = new Hue_Slider(Color_Dialog);
        slide_hue->setObjectName(QStringLiteral("slide_hue"));
        slide_hue->setMinimum(1);
        slide_hue->setMaximum(359);

        gridLayout->addWidget(slide_hue, 0, 1, 1, 1);


        horizontalLayout->addLayout(gridLayout);


        verticalLayout_2->addLayout(horizontalLayout);

        buttonBox = new QDialogButtonBox(Color_Dialog);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setStandardButtons(QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Ok|QDialogButtonBox::Reset);

        verticalLayout_2->addWidget(buttonBox);


        retranslateUi(Color_Dialog);
        QObject::connect(slide_saturation, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(set_hsv()));
        QObject::connect(slide_value, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(set_hsv()));
        QObject::connect(slide_red, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(set_rgb()));
        QObject::connect(slide_green, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(set_rgb()));
        QObject::connect(slide_blue, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(set_rgb()));
        QObject::connect(slide_alpha, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(update_widgets()));
        QObject::connect(wheel, SIGNAL(colorSelected(QColor)), Color_Dialog, SLOT(update_widgets()));
        QObject::connect(slide_saturation, SIGNAL(valueChanged(int)), spin_saturation, SLOT(setValue(int)));
        QObject::connect(spin_saturation, SIGNAL(valueChanged(int)), slide_saturation, SLOT(setValue(int)));
        QObject::connect(slide_value, SIGNAL(valueChanged(int)), spin_value, SLOT(setValue(int)));
        QObject::connect(spin_value, SIGNAL(valueChanged(int)), slide_value, SLOT(setValue(int)));
        QObject::connect(slide_red, SIGNAL(valueChanged(int)), spin_red, SLOT(setValue(int)));
        QObject::connect(spin_red, SIGNAL(valueChanged(int)), slide_red, SLOT(setValue(int)));
        QObject::connect(slide_green, SIGNAL(valueChanged(int)), spin_green, SLOT(setValue(int)));
        QObject::connect(spin_green, SIGNAL(valueChanged(int)), slide_green, SLOT(setValue(int)));
        QObject::connect(slide_alpha, SIGNAL(valueChanged(int)), spin_alpha, SLOT(setValue(int)));
        QObject::connect(spin_alpha, SIGNAL(valueChanged(int)), slide_alpha, SLOT(setValue(int)));
        QObject::connect(slide_blue, SIGNAL(valueChanged(int)), spin_blue, SLOT(setValue(int)));
        QObject::connect(spin_blue, SIGNAL(valueChanged(int)), slide_blue, SLOT(setValue(int)));
        QObject::connect(slide_hue, SIGNAL(valueChanged(int)), spin_hue, SLOT(setValue(int)));
        QObject::connect(spin_hue, SIGNAL(valueChanged(int)), slide_hue, SLOT(setValue(int)));
        QObject::connect(slide_hue, SIGNAL(valueChanged(int)), Color_Dialog, SLOT(set_hsv()));
        QObject::connect(buttonBox, SIGNAL(accepted()), Color_Dialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), Color_Dialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(Color_Dialog);
    } // setupUi

    void retranslateUi(QDialog *Color_Dialog)
    {
        Color_Dialog->setWindowTitle(QApplication::translate("Color_Dialog", "Select Color", 0));
        label_7->setText(QApplication::translate("Color_Dialog", "Saturation", 0));
        label_6->setText(QApplication::translate("Color_Dialog", "Hue", 0));
        label_8->setText(QApplication::translate("Color_Dialog", "Hex", 0));
        label_3->setText(QApplication::translate("Color_Dialog", "Blue", 0));
        label_5->setText(QApplication::translate("Color_Dialog", "Value", 0));
        label_2->setText(QApplication::translate("Color_Dialog", "Green", 0));
        label_alpha->setText(QApplication::translate("Color_Dialog", "Alpha", 0));
        label->setText(QApplication::translate("Color_Dialog", "Red", 0));
    } // retranslateUi

};

namespace Ui {
    class Color_Dialog: public Ui_Color_Dialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COLOR_DIALOG_H
