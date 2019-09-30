//RegisterDiplomaWidget.cpp by MFCoin developers
#include "RegisterDiplomaWidget.h"
#include "QNameCoin.h"
#include "../rpc/server.h"
#include <QIcon>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include "YearSpinBox.h"
#include "NameEqValueTextEdit.h"

RegisterDiplomaWidget::RegisterDiplomaWidget() {
	setWindowTitle(tr("Register diploma"));
	setWindowIcon(QIcon(":/icons/Trusted-diploma-16-monochrome.png"));

	_NVPair = new NameValueLineEdits;
	_NVPair->setValueMultiline(true);

	_chooseRoot = new QComboBox;
	_lineAddress = new QLabel;
	_lineSerial = new QLineEdit;
	_lineName = new QLineEdit;
	_yearGraduation = new YearSpinBox;
	_yearAdmission = new YearSpinBox;
	_editOther = new NameEqValueTextEdit;
	_editOther->setPlaceholderText("Format: key=value (like 'specialization=physics'), each 'name=value' pair on a new line");
	connect(_editOther, &QPlainTextEdit::textChanged, this, &RegisterDiplomaWidget::recalcValue);
	_lineSerial->setText(QNameCoin::currentSecondsPseudoBase64());
	connect(_lineSerial, &QLineEdit::textChanged, this, &RegisterDiplomaWidget::recalcValue);
	connect(_lineName, &QLineEdit::textChanged, this, &RegisterDiplomaWidget::recalcValue);
	connect(_yearGraduation, static_cast<void (QSpinBox::*)(const QString&)>(&QSpinBox::valueChanged), this, &RegisterDiplomaWidget::recalcValue);
	connect(_yearAdmission, static_cast<void (QSpinBox::*)(const QString&)>(&QSpinBox::valueChanged), this, &RegisterDiplomaWidget::recalcValue);

	auto lay = new QVBoxLayout(this);
	auto form = new QFormLayout;

	QStringList names = QNameCoin::myNamesStartingWith("dpo:");
	connect(_chooseRoot, &QComboBox::currentTextChanged, this, &RegisterDiplomaWidget::recalcValue);
	_chooseRoot->addItems(names);
	if(names.isEmpty()) {
		addText(lay, tr("You didn't register any university.\n"
						"Please register, wait until the record is accepted by the blockchain and return here."));
	} else {
		addText(lay, tr("Choose university root record:"));
	}
	lay->addWidget(_chooseRoot);
	form->addRow(tr("Address from that record to sign message:"), _lineAddress);
	form->addRow(tr("Proposed serial number:"), _lineSerial);

	lay->addLayout(form);
	form->addRow(_NVPair->availabilityLabel());
	form->addRow(tr("Student first and last name"), _lineName);
	form->addRow(tr("Admission year"), _yearAdmission);
	form->addRow(tr("... or gaduation year:"), _yearGraduation);
	_yearAdmission->setToolTip(tr("Specify one or both admission/graduation year"));
	_yearGraduation->setToolTip(_yearAdmission->toolTip());
	addText(lay, tr("Any other data, like diploma serial number etc:"));
	lay->addWidget(_editOther);
	lay->addWidget(_NVPair);
	_NVPair->hide();
	if(!_NVPair->isVisible())
		lay->addStretch();
}
QLabel* RegisterDiplomaWidget::addText(QBoxLayout*lay, const QString& s) {
	auto ret = new QLabel(s);
	lay->addWidget(ret);
	return ret;
}
void RegisterDiplomaWidget::recalcValue() {
	_NVPair->setName({});
	_NVPair->setValue({});
	_lineAddress->setText({});
	const QString root = _chooseRoot->currentText().trimmed();
	const QString serial= _lineSerial->text().trimmed();
	if(root.isEmpty() || serial.isEmpty())
		return;
	const QString name = root + ':' + serial;

	auto sig = QNameCoin::signMessageByName(root, name);
	if(sig.isError()) {
		showError(sig.error);
		return;
	}
	if(sig.address.isEmpty()) {
		_lineAddress->setText(tr("Can't get address"));
		return;
	}
	_lineAddress->setText(sig.address);
	QString value = "Signature=" + sig.signature;
	if(!_yearGraduation->text().isEmpty())
		value += QString("\ngraduation_year=%1").arg(_yearGraduation->text());
	if(!_yearAdmission->text().isEmpty())
		value += QString("\nadmission_year=%1").arg(_yearAdmission->text());
	QString student = _lineName->text().trimmed();
	if(!student.isEmpty())
		value += QString("\nname=%1").arg(student);

	value += '\n' + _editOther->toPlainText().trimmed();
	value = value.trimmed();
	_NVPair->setName(name);
	_NVPair->setValue(value);
}
void RegisterDiplomaWidget::showError(const QString & s) {
	_NVPair->availabilityLabel()->setText(s);
}
