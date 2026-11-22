# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'widget.ui'
##
## Created by: Qt User Interface Compiler version 6.12.0
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QHBoxLayout, QLabel, QPushButton,
    QSizePolicy, QSpinBox, QTextEdit, QVBoxLayout,
    QWidget)

class Ui_Widget(object):
    def setupUi(self, Widget):
        if not Widget.objectName():
            Widget.setObjectName(u"Widget")
        Widget.resize(735, 584)
        self.horizontalLayout_10 = QHBoxLayout(Widget)
        self.horizontalLayout_10.setObjectName(u"horizontalLayout_10")
        self.verticalLayout_2 = QVBoxLayout()
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.horizontalLayout_3 = QHBoxLayout()
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.temperature = QLabel(Widget)
        self.temperature.setObjectName(u"temperature")

        self.horizontalLayout_3.addWidget(self.temperature)

        self.temperature_value_label = QLabel(Widget)
        self.temperature_value_label.setObjectName(u"temperature_value_label")

        self.horizontalLayout_3.addWidget(self.temperature_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_3)

        self.horizontalLayout_4 = QHBoxLayout()
        self.horizontalLayout_4.setObjectName(u"horizontalLayout_4")
        self.light = QLabel(Widget)
        self.light.setObjectName(u"light")

        self.horizontalLayout_4.addWidget(self.light)

        self.light_value_label = QLabel(Widget)
        self.light_value_label.setObjectName(u"light_value_label")

        self.horizontalLayout_4.addWidget(self.light_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_4)

        self.horizontalLayout_5 = QHBoxLayout()
        self.horizontalLayout_5.setObjectName(u"horizontalLayout_5")
        self.fan_rpm = QLabel(Widget)
        self.fan_rpm.setObjectName(u"fan_rpm")

        self.horizontalLayout_5.addWidget(self.fan_rpm)

        self.fan_rpm_value_label = QLabel(Widget)
        self.fan_rpm_value_label.setObjectName(u"fan_rpm_value_label")

        self.horizontalLayout_5.addWidget(self.fan_rpm_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_5)

        self.horizontalLayout_6 = QHBoxLayout()
        self.horizontalLayout_6.setObjectName(u"horizontalLayout_6")
        self.led_state = QLabel(Widget)
        self.led_state.setObjectName(u"led_state")

        self.horizontalLayout_6.addWidget(self.led_state)

        self.led_state_value_label = QLabel(Widget)
        self.led_state_value_label.setObjectName(u"led_state_value_label")

        self.horizontalLayout_6.addWidget(self.led_state_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_6)

        self.horizontalLayout_7 = QHBoxLayout()
        self.horizontalLayout_7.setObjectName(u"horizontalLayout_7")
        self.cmd_result = QLabel(Widget)
        self.cmd_result.setObjectName(u"cmd_result")

        self.horizontalLayout_7.addWidget(self.cmd_result)

        self.cmd_result_value_label = QLabel(Widget)
        self.cmd_result_value_label.setObjectName(u"cmd_result_value_label")

        self.horizontalLayout_7.addWidget(self.cmd_result_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_7)

        self.horizontalLayout_8 = QHBoxLayout()
        self.horizontalLayout_8.setObjectName(u"horizontalLayout_8")
        self.online = QLabel(Widget)
        self.online.setObjectName(u"online")

        self.horizontalLayout_8.addWidget(self.online)

        self.online_value_label = QLabel(Widget)
        self.online_value_label.setObjectName(u"online_value_label")

        self.horizontalLayout_8.addWidget(self.online_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_8)

        self.horizontalLayout_9 = QHBoxLayout()
        self.horizontalLayout_9.setObjectName(u"horizontalLayout_9")
        self.label_6 = QLabel(Widget)
        self.label_6.setObjectName(u"label_6")

        self.horizontalLayout_9.addWidget(self.label_6)

        self.protocol_version_value_label = QLabel(Widget)
        self.protocol_version_value_label.setObjectName(u"protocol_version_value_label")

        self.horizontalLayout_9.addWidget(self.protocol_version_value_label)


        self.verticalLayout_2.addLayout(self.horizontalLayout_9)

        self.update_button = QPushButton(Widget)
        self.update_button.setObjectName(u"update_button")

        self.verticalLayout_2.addWidget(self.update_button)


        self.horizontalLayout_10.addLayout(self.verticalLayout_2)

        self.verticalLayout_3 = QVBoxLayout()
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.verticalLayout = QVBoxLayout()
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.rpm_set = QSpinBox(Widget)
        self.rpm_set.setObjectName(u"rpm_set")
        self.rpm_set.setMaximum(3000)

        self.horizontalLayout.addWidget(self.rpm_set)

        self.set_rpm_button = QPushButton(Widget)
        self.set_rpm_button.setObjectName(u"set_rpm_button")

        self.horizontalLayout.addWidget(self.set_rpm_button)


        self.verticalLayout.addLayout(self.horizontalLayout)

        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.led_on_button = QPushButton(Widget)
        self.led_on_button.setObjectName(u"led_on_button")

        self.horizontalLayout_2.addWidget(self.led_on_button)

        self.led_off_button = QPushButton(Widget)
        self.led_off_button.setObjectName(u"led_off_button")

        self.horizontalLayout_2.addWidget(self.led_off_button)


        self.verticalLayout.addLayout(self.horizontalLayout_2)


        self.verticalLayout_3.addLayout(self.verticalLayout)

        self.log_textedit = QTextEdit(Widget)
        self.log_textedit.setObjectName(u"log_textedit")
        self.log_textedit.setMaximumSize(QSize(641, 401))

        self.verticalLayout_3.addWidget(self.log_textedit)


        self.horizontalLayout_10.addLayout(self.verticalLayout_3)


        self.retranslateUi(Widget)

        QMetaObject.connectSlotsByName(Widget)
    # setupUi

    def retranslateUi(self, Widget):
        Widget.setWindowTitle(QCoreApplication.translate("Widget", u"Form", None))
        self.temperature.setText(QCoreApplication.translate("Widget", u"Temperature", None))
        self.temperature_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.light.setText(QCoreApplication.translate("Widget", u"Light", None))
        self.light_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.fan_rpm.setText(QCoreApplication.translate("Widget", u"Fan RPM", None))
        self.fan_rpm_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.led_state.setText(QCoreApplication.translate("Widget", u"LED State", None))
        self.led_state_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.cmd_result.setText(QCoreApplication.translate("Widget", u"CMD Result", None))
        self.cmd_result_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.online.setText(QCoreApplication.translate("Widget", u"Online", None))
        self.online_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.label_6.setText(QCoreApplication.translate("Widget", u"Protocol Version", None))
        self.protocol_version_value_label.setText(QCoreApplication.translate("Widget", u"0", None))
        self.update_button.setText(QCoreApplication.translate("Widget", u"Update", None))
        self.set_rpm_button.setText(QCoreApplication.translate("Widget", u"Set RPM", None))
        self.led_on_button.setText(QCoreApplication.translate("Widget", u"LED ON", None))
        self.led_off_button.setText(QCoreApplication.translate("Widget", u"LED OFF", None))
    # retranslateUi

