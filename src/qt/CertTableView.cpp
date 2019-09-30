//CertTableView.cpp by MFCoin developers
#include "CertTableView.h"
#include "CertTableModel.h"
#include "CertLogger.h"
#include "ShellImitation.h"
#include "Settings.h"
#include <QToolBar>
#include <QHeaderView>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QAction>

CertTableView::CertTableView(CertLogger*log): _logger(log) {
	horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	_model = new CertTableModel(this);
	setModel(_model);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &CertTableView::reloadLog);
	
	recreateButtons();
	connect(_model, &CertTableModel::modelReset, this, &CertTableView::recreateButtons);
	connect(this, &CertTableView::doubleClicked, this, &CertTableView::installSelectedIntoSystem);
}
CertTableModel* CertTableView::model()const {
	return _model;
}
void CertTableView::reloadLog() {
	QString path = selectedLogPath();
	_logger->setFile(path);
}
void CertTableView::recreateButtons() {
	for(int row = 0; row < _model->rowCount(); ++row) {
		auto w = new QToolBar;
		setIndexWidget(_model->index(row, CertTableModel::ColMenu), w);
		
		auto gen = new QAction(tr("Generate again"), w);
		gen->setToolTip(tr("Generate again\nRegenerate certificate (for same nickname and email) if it has been expired or has been compromised"));
		gen->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/refresh-24.png"));
		connect(gen, &QAction::triggered, this, &CertTableView::generateCertByButton);
		w->addAction(gen);

		auto show = new QAction(tr("Show in explorer"), w);
		show->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-open-32.png"));
		connect(show, &QAction::triggered, this, &CertTableView::showInExplorer);
		w->addAction(show);
	}
}
struct CertTableView::Dialog: public QDialog {
	QComboBox* _certType = new QComboBox;
	QLineEdit* _pass = new QLineEdit;
	QLineEdit* _pass2 = new QLineEdit;
	QLabel * _labelError = new QLabel;
	Dialog(QWidget*parent): QDialog(parent) {
		setWindowTitle(tr("New certificate"));
		//setWindowFlag(Qt::WindowContextHelpButtonHint, false);
		auto lay = new QVBoxLayout(this);
		auto form = new QFormLayout;
		lay->addLayout(form);

		_certType->addItem("EC", (int)CertTableModel::EC);
		_certType->addItem("RSA", (int)CertTableModel::RSA);
		form->addRow(tr("Certificate type:"), _certType);

		_pass->setEchoMode(QLineEdit::Password);
		_pass2->setEchoMode(QLineEdit::Password);
		form->addRow(new QLabel(tr("Enter password for certificate package.\n"
			"You will use this password when installing certificate into browser:")));
		form->addRow(tr("Password:"), _pass);
		form->addRow(tr("Password again:"), _pass2);
		form->addRow(_labelError);
		_labelError->hide();
		{
			auto box = new QDialogButtonBox;
			lay->addWidget(box);
			auto ok = box->addButton(QDialogButtonBox::Ok);
			auto cancel = box->addButton(QDialogButtonBox::Cancel);
			ok->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-apply-32.png"));
			cancel->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-cancel-32.png"));
			connect(ok, &QAbstractButton::clicked, this, &QDialog::accept);
			connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
		}
		_pass->setFocus();
	}
	void showError(const QString & str) {
		QString str2 = QString("<font color='red'>%1</font>").arg(str.toHtmlEscaped());
		str2.replace("\n", "<br>\n");
		_labelError->setText(str2);
		_labelError->show();
	}
	void accept()override {
		if(_pass->text().isEmpty()) {
			showError(tr("Empty password"));
			return;
		}
		if(_pass->text() != _pass2->text()) {
			showError(tr("Passwords differ"));
			return;
		}
		QDialog::accept();
	}
};
void CertTableView::showInGraphicalShell(QWidget *parent, const QString &pathIn) {
#ifdef Q_OS_WIN
	const QString explorer = "explorer.exe";
	QString param;
	if(!QFileInfo(pathIn).isDir())
		param = QLatin1String("/select,");
	param += QDir::toNativeSeparators(pathIn);
	QString command = explorer + " " + param;
	if(!QProcess::startDetached(command)) {
		QMessageBox::warning(parent,
			tr("Launching Windows Explorer failed"),
			tr("Could not find explorer.exe in path to launch Windows Explorer."));
	}
	return;
#endif
#if defined(Q_OS_MAC)
	Q_UNUSED(parent)
	QStringList scriptArgs;
	scriptArgs << QLatin1String("-e")
		<< QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
		.arg(pathIn);
	QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
	scriptArgs.clear();
	scriptArgs << QLatin1String("-e")
		<< QLatin1String("tell application \"Finder\" to activate");
	QProcess::execute("/usr/bin/osascript", scriptArgs);
	return;
#endif
	// we can't select a file here, because no file browser really supports it
	const QFileInfo fileInfo(pathIn);
	QString dir = fileInfo.isDir() ? fileInfo.absolutePath() : fileInfo.dir().absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
int CertTableView::rowFromAction(QAction*a) {
	auto w = a->parentWidget();
	Q_ASSERT(w);
	if(!w)
		return -1;
	const int rows = _model->rowCount();
	for(int row = 0; row < rows; ++row) {
		auto w2 = indexWidget(_model->index(row, _model->ColMenu));
		if(w==w2)
			return row;
	}
	Q_ASSERT(0);
	return -1;
}
void CertTableView::showInExplorer() {
	int nRow = -1;
	if(auto action = qobject_cast<QAction*>(sender())) {
		nRow = rowFromAction(action);
	} else {
		nRow = selectedRow();
	}
	QString path;
	if(nRow>=0 && nRow < _model->rowCount()) {
		const auto & row = _model->_rows[nRow];
		path = row._certPair;
		if(path.isEmpty() || !QFile::exists(path))
			path = row._templateFile;
	}
	if(path.isEmpty())
		path = Settings::certDir().absolutePath();
	showInGraphicalShell(this, path);
}
void CertTableView::generateCertByButton() {
	auto b = qobject_cast<QAction*>(sender());
	Q_ASSERT(b);
	if(!b)
		return;
	int nRow = rowFromAction(b);
	if(nRow<0 || nRow >= _model->rowCount())
		return;
	clearSelection();
	selectRow(nRow);
	generateCertForSelectedRow();
}
int CertTableView::selectedRow()const {
	auto rows = selectionModel()->selectedIndexes();
	if(!rows.isEmpty())
		return rows[0].row();
	return -1;
}
QString CertTableView::selectedLogPath() {
	int nRow = selectedRow();
	if(-1 == nRow)
		return QString();
	const auto & row = _model->_rows[nRow];
	return row.logFilePath();
}
void CertTableView::generateCertForSelectedRow() {
	int nRow = selectedRow();
	if(-1==nRow || !isEnabled())
		return;
	const auto & row = _model->_rows[nRow];
	Dialog dlg(this);
	if(dlg.exec()!=QDialog::Accepted)
		return;
	setEnabled(false);//prevent selection change to prevent selected log file change
	auto certType = (CertTableModel::CertType)dlg._certType->currentData().toInt();
	QString sha256;
	QString msg = row.generateCert(_logger, certType, dlg._pass->text(), sha256);
	setEnabled(true);
	if(_logger && !msg.isEmpty()) {
		_logger->append(msg);
		return;
	}
	if(QMessageBox::question(this, tr("EmerSSL certificate installation"),
		tr("Do you want to install newly created certificate into system?"))
		== QMessageBox::Yes)
	{
		row.installIntoSystem();
	}
	QString file = row._templateFile;
	_model->reload();
	selectRow(_model->indexByFile(file));
}
void CertTableView::installSelectedIntoSystem() {
	int nRow = selectedRow();
	if(-1 == nRow)
		return;
	const auto & row = _model->_rows[nRow];
	row.installIntoSystem();
}
