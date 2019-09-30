//NameValueLineEdits.cpp by MFCoin developers
#include "NameValueLineEdits.h"
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QLabel>
#include <QDebug>
#include "SelectableTextEdit.h"
#include "QNameCoin.h"
#include "NameCoinStrings.h"

NameValueLineEdits::NameValueLineEdits() {
	_name = new SelectableLineEdit;
	_value = new SelectableLineEdit;
	_valueMuti = new SelectableTextEdit;

	auto form = new QFormLayout(this);
	form->setMargin(0);//usually this widget is embedded to other so no need
	auto label = new QLabel(tr("Resulting name and value to put into blockchain:"));
	label->setWordWrap(true);
	form->addRow(label);
	{
		auto w = new QWidget;
		auto lay = new QHBoxLayout(w);
		lay->setSpacing(0);
		lay->setMargin(0);

		_availability = new QLabel;
		lay->addWidget(_availability);

		QString namePlaceholder = tr("This field will contain name to insert to 'Manage names' panel");
		_name->setPlaceholderText(namePlaceholder);
		_name->setToolTip(tr("Read-only") + ". " +  namePlaceholder);
		_name->setReadOnly(true);
		lay->addWidget(_name);
		
		auto copy = new QToolButton;
		copy->setText(tr("Copy to clipboard"));
		_name->_buddyToShowToolip = copy;
		connect(copy, &QAbstractButton::clicked, this, [=] () {
			_name->copyAndShowTooltip(_value);
		});
		lay->addWidget(copy);
		form->addRow(tr("Name:"), w);
	}
	auto layValue = new QVBoxLayout;
	{
		_w1Line = new QWidget;
		auto lay = new QHBoxLayout(_w1Line);
		lay->setSpacing(0);
		lay->setMargin(0);

		_value->setReadOnly(true);
		_value->setPlaceholderText(tr("This field will contain value to insert to 'Manage names' panel"));
		lay->addWidget(_value);

		auto copy = new QToolButton;
		copy->setText(tr("Copy to clipboard"));
		connect(copy, &QAbstractButton::clicked, this, [=]() {
			_value->copyAndShowTooltip(_name);
		});
		lay->addWidget(copy);
		layValue->addWidget(_w1Line);
	}
	{
		_wMultiLine = new QWidget;
		auto lay = new QHBoxLayout(_wMultiLine);
		lay->setSpacing(0);
		lay->setMargin(0);

		_valueMuti->setReadOnly(true);
		_valueMuti->setPlaceholderText(tr("This field will contain value to insert to 'Manage names' panel"));
		lay->addWidget(_valueMuti);

		auto copy = new QToolButton;
		copy->setText(tr("Copy to clipboard"));
		_valueMuti->_buddyToShowToolip = copy;
		connect(copy, &QAbstractButton::clicked, this, [=]() {
			_valueMuti->copyAndShowTooltip(_name);
		});
		lay->addWidget(copy);
		layValue->addWidget(_wMultiLine);
	}
	form->addRow(tr("Value:"), layValue);
	if(_multiline)
		_w1Line->hide();
	else
		_wMultiLine->hide();
}
void NameValueLineEdits::setName(const QString & name) {
	_name->setText(name);
	if(name.isEmpty()) {
		_availability->setText({});
		_availability->setToolTip({});
		QToolTip::hideText();
		return;
	}
	_availability->show();
	QString text;
	if(QNameCoin::isMyName(name)) {
	   text = NameCoinStrings::trNameIsMy(name);
	} else if(QNameCoin::nameActive(name)) {
		text = NameCoinStrings::trNameAlreadyRegistered(name, false);
	} else {
		text = NameCoinStrings::trNameIsFree(name);
	}
	_availability->setText(text);
	//if(tooltip!=_availability->toolTip()) {
	//	_availability->setToolTip(tooltip);
	//	QToolTip::showText(_availability->mapToGlobal(_availability->rect().topLeft()), tooltip);
	//}
}
void NameValueLineEdits::setValuePlaceholder(const QString & s) {
	_value->setPlaceholderText(s);
	_valueMuti->setPlaceholderText(s);
}
void NameValueLineEdits::copyValue() {
	if(_multiline) {
		_valueMuti->copyAndShowTooltip(_name);
		return;
	}
	_value->copyAndShowTooltip(_name);
}
void NameValueLineEdits::setValue(const QString & s) {
	_value->setText(s);
	_valueMuti->setPlainText(s);
}
void NameValueLineEdits::setValueMultiline(bool multi) {
	if(_multiline==multi)
		return;
	_multiline = multi;
	_wMultiLine->setVisible(_multiline);
	_w1Line->setVisible(!_multiline);
}
void NameValueLineEdits::setValueReadOnly(bool b) {
	_value->setReadOnly(b);
	_valueMuti->setReadOnly(b);
}
QString NameValueLineEdits::name()const {
	return _name->text();
}
QString NameValueLineEdits::value()const {
	if(_multiline)
		return _valueMuti->toPlainText();
	return _value->text();
}
QLabel* NameValueLineEdits::availabilityLabel()const {
	return _availability;
}
