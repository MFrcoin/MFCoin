//DpoRegisterDocWidget.cpp by MFCoin developers
#include "DpoRegisterDocWidget.h"
#include "SelectableLineEdit.h"
#include "signverifymessagedialog.h"
#include "QNameCoin.h"
#include <QFormLayout>
#include <QSettings>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>
#include <QMessageBox>
#include <QComboBox>
#include <QCryptographicHash>

DpoRegisterDocWidget::DpoRegisterDocWidget() {
	setWindowTitle(tr("3) Document registration"));

	_NVPair = new NameValueLineEdits;
	_editHash = new QLineEdit;
	_chooseRoot = new QComboBox;
	_editFile = new QLineEdit;
	_editDocName = new QLineEdit;
	_editSignature = new SelectableLineEdit;

	auto lay = new QVBoxLayout(this);

	QStringList names = QNameCoin::myNames();
	connect(_chooseRoot, &QComboBox::currentTextChanged, this, &DpoRegisterDocWidget::recalcValue);
	_chooseRoot->addItems(names);
	if(names.isEmpty()) {
		addText(lay, tr("You didn't register any name in previous tab.\n"
			"Please register, wait until the record is accepted by the blockchain and return here."));
	} else {
		addText(lay, tr("Choose your identity - registered record ,\n"
			"like dpo:Organization:YourName, or infocard etc. This name will sign message."));
	}
	lay->addWidget(_chooseRoot);

	addText(lay, tr("Choose a file to add to blockchain:"));
	{
		auto lay2 = new QHBoxLayout;
		lay->addLayout(lay2);

		_editFile->setReadOnly(true);
		_editFile->setPlaceholderText(tr("Click button to the right"));
		lay2->addWidget(_editFile);

		auto open = new QPushButton(tr("Open file..."));
		open->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-open-32.png"));
		connect(open, &QPushButton::clicked, this, &DpoRegisterDocWidget::openFileDialog);
		lay2->addWidget(open);
	}

	_editHash->setEnabled(false);
	_editHash->hide();
	connect(_editHash, &QLineEdit::textChanged, this, &DpoRegisterDocWidget::recalcValue);
	lay->addWidget(_editHash);
	//lay->addWidget(_NVPair->availabilityLabel());

	addText(lay, tr("You can change default document name:"));
	lay->addWidget(_editDocName);

	addText(lay, tr("Your signature with selected identity:"));
	_editSignature->setReadOnly(true);
	lay->addWidget(_editSignature);

	_NVPair->setValueMultiline(true);
	_NVPair->hide();
	lay->addWidget(_NVPair);
	lay->addStretch();
}
void DpoRegisterDocWidget::openFileDialog() {
	QString path = QFileDialog::getOpenFileName(this, tr("Choose file to register in blockchain"), QString(),
		tr("Adobe pdf (*.pdf);;All files (*)"));
	if(path.isEmpty())
		return;

	_editFile->setText(path);
	_editDocName->setText(QFileInfo(path).completeBaseName());

	QFile file(path);
	if(!file.open(QFile::ReadOnly)) {
		QMessageBox::critical(this, qApp->applicationDisplayName(),
			tr("Can't open file %1 for reading: %2").arg(path, file.errorString()));
		return;
	}
	auto arr = file.readAll();
	auto hash = QCryptographicHash::hash(arr, QCryptographicHash::Sha256);
	_editHash->setText(hash.toHex());
}
void DpoRegisterDocWidget::showError(const QString & s) {
	_NVPair->availabilityLabel()->setText(s);
}
void DpoRegisterDocWidget::recalcValue() {
	_editSignature->setText({});
	_NVPair->setName({});
	_NVPair->setValue({});

	const QString root = _chooseRoot->currentText().trimmed();
	QString docHash = _editHash->text().trimmed();
	if(!docHash.isEmpty())
		docHash.prepend("doc:");
	if(docHash.isEmpty() || root.isEmpty())
		return;

	auto sig = QNameCoin::signMessageByName(root, docHash);
	if(sig.isError()) {
		showError(sig.error);
		return;
	}
	_editSignature->setText(sig.signature);
	_NVPair->setName(docHash);
	QString value = QString("SIG=%1|%2\n").arg(root, sig.signature);
	QString docName = _editDocName->text().trimmed();
	if(docName.isEmpty())
		docName = QFileInfo(_editFile->text()).completeBaseName();
	if(docName.isEmpty())
		docName = QFileInfo(_editFile->text()).fileName();
	value += "name=" + docName;
	_NVPair->setValue(value);
}
QLabel* DpoRegisterDocWidget::addText(QBoxLayout*lay, const QString& s) {
	auto ret = new QLabel(s);
	lay->addWidget(ret);
	return ret;
}
