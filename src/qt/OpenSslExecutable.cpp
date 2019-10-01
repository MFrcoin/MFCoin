//OpenSslExecutable.cpp by MFCoin developers
#include "OpenSslExecutable.h"
#include "OpenSslConfigWriter.h"
#include "Settings.h"
#include "CertLogger.h"
#include <QCoreApplication>
#include <QLineEdit>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QCompleter>
#include <QDirModel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QThread>

QString OpenSslExecutable::s_path;
OpenSslExecutable::OpenSslExecutable() {
	setWorkingDirectory(Settings::certDir().absolutePath());
	if(s_path.isEmpty())
		s_path = defaultPath();
}
QString OpenSslExecutable::defaultPath() {
	QString p;
#ifdef Q_OS_WIN
	p = "openssl.exe";
#else
	p = "openssl";
#endif
	p = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(p);
	return p;
}
bool OpenSslExecutable::seemsOk(const QString & path) {
	if(!path.contains("openssl", Qt::CaseInsensitive))
		return false;
	QFileInfo file(path);
	return file.exists() && file.isExecutable();
}
struct OpenSslExecutable::SpecifyPathDialog: public QDialog {
	QLineEdit* _path = {};
	QLabel* _statusLabel = 0;
	QPixmap _pixOk;
	QPixmap _pixFailed;
	QTimer _timerCheck;//user can download exe so check it periodically
	SpecifyPathDialog() {
		_pixOk = QPixmap(":/qt-project.org/styles/commonstyle/images/standardbutton-apply-32.png");
		_pixFailed = QPixmap(":/qt-project.org/styles/commonstyle/images/stop-24.png");
		auto lay = new QFormLayout(this);
		
		auto label = new QLabel(
				tr("No OpenSSL executable found, certificate creation will not work.<br/>\n"
				"Please download OpenSSL from <a href=\"https://wiki.openssl.org/index.php/Binaries\">www.openssl.org</a>"
					" and place it in folder specified below:<br>%1")
			.arg(qApp->applicationDirPath()));
		label->setOpenExternalLinks(true);
		lay->addRow(label);

		{
			auto lay2 = new QHBoxLayout;
			
			_path = new QLineEdit;
			QCompleter *completer = new QCompleter(this);
			auto fileSystem = new QDirModel(completer);
			fileSystem->setFilter(QDir::AllEntries|QDir::NoDotAndDotDot);
			completer->setModel(fileSystem);
			_path->setCompleter(completer);
			connect(_path, &QLineEdit::textChanged, this, &SpecifyPathDialog::onPathChanged);
			connect(&_timerCheck, &QTimer::timeout, this, &SpecifyPathDialog::onPathChanged);
			_timerCheck.setInterval(1000);
			_timerCheck.start();
			lay2->addWidget(_path);

			_statusLabel = new QLabel;
			lay2->addWidget(_statusLabel);

			auto browse = new QPushButton(tr("Browse"));
			browse->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-open-32.png"));
			lay2->addWidget(browse);
			connect(browse, &QAbstractButton::clicked, this, &SpecifyPathDialog::onBrowse);
			
			lay->addRow(tr("OpenSSL executable:"), lay2);

			_path->setText(s_path);
		}
		{
			auto box = new QDialogButtonBox;
			lay->addRow(box);
			auto ok = box->addButton(QDialogButtonBox::Ok);
			auto cancel = box->addButton(QDialogButtonBox::Cancel);
			ok->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-apply-32.png"));
			cancel->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-cancel-32.png"));
			connect(ok, &QAbstractButton::clicked, this, &QDialog::accept);
			connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
		}
	}
	void onBrowse() {
		QString s = QFileDialog::getOpenFileName(this, tr("Specify OpenSSL executable"), _path->text()
#ifdef Q_OS_WIN
			, tr("Executable (*.exe);;All files (*)")
#endif
			);
		if(!s.isEmpty())
			_path->setText(s);
		if(seemsOk(s))
			s_path = s;
	}
	void onPathChanged() {
		QString p = _path->text();
		bool ok = seemsOk(p);
		_statusLabel->setPixmap(ok ? _pixOk : _pixFailed);
		QString toolTip;
		if(ok) {
			toolTip = tr("Found!");
		} else {
			toolTip = tr("File not found");
			if(!p.contains("openssl", Qt::CaseInsensitive))
				toolTip = tr("Not openssl file");
			else if(!QFile::exists(p))
				;//use prev set value
			else if(!QFileInfo(p).isExecutable())
				toolTip = tr("File is not executable");
		}
		_statusLabel->setToolTip(toolTip);
	}
};
bool OpenSslExecutable::isFoundOrMessageBox() {
	if(s_path.isEmpty())
		s_path = defaultPath();
	if(seemsOk(s_path))
		return true;
#ifdef Q_OS_LINUX
	s_path = "/usr/bin/openssl";
	if(seemsOk(s_path))
		return true;
#endif
	SpecifyPathDialog dlg;
	dlg.exec();
	return seemsOk(s_path);
}
QString OpenSslExecutable::errorString()const {
	return QProcess::errorString() + '\n' + _strOutput;
}
QString OpenSslExecutable::exec(const QStringList & args) {
	if(!QFile::exists(cfgFilePath())) {
		//look for *cfg file in this repo
		return log(tr("Config file not found: %1") + cfgFilePath());
	}
	log(tr("Starting openssl ") + args.join(' '));
	_strOutput.clear();
	if(!QFile::exists(s_path)) {
		return log(tr("There is no file %1").arg(QDir::toNativeSeparators(s_path)));
	}
	start(s_path, args, QIODevice::ReadWrite);
	const int maxTimeout = 10000 * 1000;
	if(!waitForStarted(maxTimeout)) {
		return log(tr("Can't start: %1").arg(error()));
	}
	if(!_dataToWrite.isEmpty()) {
		write(_dataToWrite);
		_dataToWrite.clear();
		waitForBytesWritten();
		closeWriteChannel();
	}
	if(!waitForFinished(maxTimeout)) {
		return log(tr("Failed waiting to finish: %1").arg(error()));
	}
	readToMe();
	if(QProcess::NormalExit != exitStatus()) {
		return log(tr("Exit status = %1").arg(exitStatus()));
	}
	log(tr("Finished"));
	return QString();
}
void OpenSslExecutable::readToMe() {
	_strOutput += readAllStandardError();
	_strOutput += readAllStandardOutput();
}
bool OpenSslExecutable::existsOrExit(const QDir & dir, const QString & file) {
	if(dir.exists(file))
		return true;
	_strOutput += tr("File %1 does not exist").arg(file);
	return false;
}
QString OpenSslExecutable::cfgFilePath() {
	 return OpenSslConfigWriter::configPath();
}
bool OpenSslExecutable::deleteOrExit(QDir & dir, const QString & file, int tries) {
	for(int i = 0; i<tries; ++i) {
		if(i>0)
			QThread::msleep(50);
		dir.remove(file);
		if(!dir.exists(file))
			return true;
	}
	_strOutput += tr("File %1 can't be removed").arg(file);
	return false;
}
extern "C" int req_main(int argc, char **argv);
class Args: public QStringList {
	protected:
		QVector<char*> _argv;
		QList<QByteArray> _args;
	public:
		Args(const QStringList & args): QStringList(args) {
		}
		int argc()const {
			return count();
		}
		char ** argv() {
			_args.clear();
			for(const QString & s: *this) {
				_args << s.toUtf8();
			}
			_argv.clear();
			for(QByteArray & s: _args) {
				_argv << s.data();
			}
			return _argv.data();
		}
};

bool OpenSslExecutable::generateKeyAndCertificateRequest(const QString & baseName, const QString & subj) {
	log(tr("Generate key and certificate request:"));
	const QString keyFile = baseName + ".key";
	const QString csrFile = baseName + ".csr";
	QDir dir = workingDirectory();
	if(!deleteOrExit(dir, keyFile))
		return false;
	if(!deleteOrExit(dir, csrFile))
		return false;
	Args args = QString("req -config $CFG -new -newkey rsa:2048 -nodes -keyout $KEY -subj $SUBJ -out $CSR").split(' ');
	args.replaceInStrings("$KEY", keyFile);
	args.replaceInStrings("$CSR", csrFile);
	args.replaceInStrings("$SUBJ", subj);
	args.replaceInStrings("$CFG", cfgFilePath());
	if(!exec(args).isEmpty() || exitCode() != 0)
		return false;
	return existsOrExit(dir, keyFile) && existsOrExit(dir, csrFile);
}
bool OpenSslExecutable::generateCertificate(const QString & baseName, const QString & configDir) {
	log(tr("Generate certificate:"));
	const QString csrFile = baseName + ".csr";
	const QString crtFile = baseName + ".crt";
	QDir dir = workingDirectory();
	if(!existsOrExit(dir, csrFile))
		return false;
	if(!deleteOrExit(dir, crtFile))
		return false;
	QStringList args = QString("ca -config $CA_DIR/ca.config -in $IN -out $OUT -batch").split(' ');
	args.replaceInStrings("$CA_DIR", configDir);
	args.replaceInStrings("$IN", csrFile);
	args.replaceInStrings("$OUT", crtFile);
	if(!exec(args).isEmpty() || exitCode() != 0)
		return false;
	return existsOrExit(dir, crtFile);
}
static const QString passKeyName = "b20bdb78a28343488aace4fc75dd47cf";
bool OpenSslExecutable::createCertificatePair(const QString & baseName, const QString & configDir, const QString & pass) {
	log(tr("Create certificate pair:"));
	const QString keyFile = baseName + ".key";
	const QString crtFile = baseName + ".crt";
	const QString p12 = baseName + ".p12";
	QDir dir = workingDirectory();
	if(!existsOrExit(dir, keyFile))
		return false;
	if(!existsOrExit(dir, crtFile))
		return false;
	dir.remove(p12);
	//OpenSSL password options: https://www.openssl.org/docs/man1.1.0/apps/openssl.html#Pass-Phrase-Options
	QStringList args = QString("pkcs12 -export -in $CRT -inkey $KEY -certfile $CA_DIR/mfcssl_ca.crt -out $P12 -passout env:%1")
		.arg(passKeyName)
		.split(' ');
	for(auto & s: args) {
		if(s == "$CRT")
			s = crtFile;
		else if(s == "$KEY")
			s = keyFile;
		else if(s == "$P12")
			s = p12;
		else if(s.startsWith("$CA_DIR"))
			s.replace("$CA_DIR", configDir);
	}
	{
		auto env = systemEnvironment();
		env << passKeyName + "=" + pass;
		setEnvironment(env);
	}
	if(!exec(args).isEmpty())
		return false;
	if(exitCode() != 0)
		return false;
	return existsOrExit(dir, p12);
}
QString OpenSslExecutable::log(const QString & s) {
	if(_logger) {
		_logger->append(s);
	}
	return s;
}
void OpenSslExecutable::setLogger(CertLogger*l) {
	_logger = l;
}
bool OpenSslExecutable::encryptInfocardAes(const QByteArray& data, const QString & outFile, const QString & pass) {
	_dataToWrite = data;
	QStringList args = QString("enc -aes-256-cbc -salt -out $OUTF -pass env:%1").arg(passKeyName).split(' ');
	args.replaceInStrings("$OUTF", outFile);
	{
		auto env = systemEnvironment();
		env << passKeyName + "=" + pass;
		setEnvironment(env);
	}
	if(!exec(args).isEmpty())
		return false;
	if(exitCode() != 0)
		return false;
	return true;
}
