//InfoCardDialog.cpp by MFCoin developers
#include "InfoCardDialog.h"
#include "InfoCardExample.h"
#include "InfoCardTextEdit.h"
#include "InfoCard.h"
#include "InfoCardTextEdit.h"
#include "InfoCard.h"
#include "CertLogger.h"
#include <QPushButton>
#include <QTabWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDialogButtonBox>

InfoCardDialog::InfoCardDialog(InfoCard&info, CertLogger* logger, QWidget*parent):
	QDialog(parent),
	_info(info),
	_logger(logger)
{
	setWindowTitle(tr("Edit InfoCard"));
#if QT_VERSION > QT_VERSION_CHECK(5, 9, 0)
	setWindowFlag(Qt::WindowContextHelpButtonHint, false);
	setWindowFlag(Qt::WindowMaximizeButtonHint, true);
#endif
	auto lay = new QVBoxLayout(this);
	auto tabs = new QTabWidget;
	lay->addWidget(tabs);
	_text = new InfoCardTextEdit;
	tabs->addTab(_text, tr("Text"));
	addExample(tabs, 1);
	addExample(tabs, 0);

	auto box = new QDialogButtonBox;
	lay->addWidget(box);
	_okBtn = box->addButton(QDialogButtonBox::Ok);
	_okBtn->setShortcut(QKeySequence("Ctrl+Enter"));
	auto cancel = box->addButton(QDialogButtonBox::Cancel);
	_okBtn->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/floppy-32.png"));
	cancel->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-cancel-32.png"));
	connect(_okBtn, &QAbstractButton::clicked, this, &QDialog::accept);
	connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
	connect(_text, &QPlainTextEdit::textChanged, this, &InfoCardDialog::enableOk);
	_okBtn->setEnabled(false);
	if(parent) {
		QWidget*topMost = parent;
		for(; parent; parent = parent->parentWidget())
			topMost = parent;
		resize(topMost->sizeHint());
	}
	setText(info._text);
}
void InfoCardDialog::addExample(QTabWidget*tabs, int n) {
	QString text = n>0 ? InfoCardExample::user : InfoCardExample::corporate;
	QString tabName = n>0 ? tr("User card example"): tr("Corporate card example");
	auto w = new InfoCardTextEdit;
	w->setPlainText(text);
	w->setReadOnly(true);
	tabs->addTab(w, tabName);
}
bool InfoCardDialog::allValid()const {
	return !_text->toPlainText().trimmed().isEmpty();
}
void InfoCardDialog::enableOk() {
	_okBtn->setEnabled(allValid());
}
void InfoCardDialog::accept() {
	if(!allValid())
		return;
	QDialog::accept();
	_info._text = text();
	_info.save();
	_info.parse();
	auto err = _info.encrypt(_logger);
}
QString InfoCardDialog::Item::text()const {
	if(!_multiline && _line) {
		return _line->text();
	}
	if(_multiline && _text) {
		return _text->toPlainText();
	}
	Q_ASSERT(0);
	return QString();
}
QWidget* InfoCardDialog::Item::widget()const {
	if(_multiline)
		return _line;
	return _text;
}
void InfoCardDialog::add(Item & row) {
	if(row._multiline)
		row._line = new QLineEdit;
	else
		row._text = new QPlainTextEdit;
	_rows << row;
	_lay->addRow(row._name, row.widget());
}
QString InfoCardDialog::text()const {
	return _text->toPlainText();
}
void InfoCardDialog::setText(const QString & s) {
	_text->setPlainText(s);
}
