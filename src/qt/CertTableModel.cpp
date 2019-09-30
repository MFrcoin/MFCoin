//CertTableModel.cpp by MFCoin developers
#include "CertTableModel.h"
#include "ShellImitation.h"
#include "Settings.h"
#include "CertLogger.h"
#include "OpenSslExecutable.h"
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QMessageBox>

QString CertTableModel::Item::logFilePath()const {
	return pathByExt("log");
}
QString CertTableModel::Item::loadFromTemplateFile(const QFileInfo & entry) {//QString::isEmpty -> ok
	//file example: /CN=Konstantine Kozachuk/emailAddress=neurocod@gmail.com/UID=123
	_templateFile = entry.filePath();
	_dir = entry.dir();
	_baseName = entry.baseName();
	QFile file(_templateFile);
	if(!file.open(QFile::ReadOnly))
		return file.errorString();
	const QByteArray arr = file.readAll();
	const QString str = arr;
	auto lines = str.split('\n', QString::SkipEmptyParts);
	if(lines.count()!=1)
		return tr("Invalid file format: more than 1 non-empty line in %1")
		.arg(QDir::toNativeSeparators(_templateFile));
	const QString line = lines[0];
	_templateLine.append(line.toUtf8());
	auto parts = line.split('/', QString::SkipEmptyParts);
	bool formatOk = parts.size() == 2 || parts.size() == 3;
	if(!formatOk)
		return tr("Invalid format of file %1: %2 parts, looking for 2 or 3")
		.arg(QDir::toNativeSeparators(_templateFile))
		.arg(parts.size());
	for(QString part: parts) {
		if(!part.contains('='))
			return tr("Invalid format: must be key=value in %1")
			.arg(QDir::toNativeSeparators(_templateFile));
		auto kv = part.split('=', QString::SkipEmptyParts);
		if(kv.size() != 2)
			return tr("Invalid format: must be key=value in %1")
			.arg(QDir::toNativeSeparators(_templateFile));
		const QString key = kv.first();
		const QString value = kv.last();
		if(key == "CN") {
			_name = value;
		} else if(key == "emailAddress") {
			_mail = value;
		} else if(key == "UID") {
			_InfoCardId = value;
		} else {
			return tr("Unknown key %1 in %2").arg(key)
				.arg(QDir::toNativeSeparators(_templateFile));
		}
	}
	_name = _name.trimmed();
	_mail = _mail.trimmed();
	if(_name.isEmpty())
		return tr("Invalid format: empty name in %1")
			.arg(QDir::toNativeSeparators(_templateFile));
	if(_mail.isEmpty())
		return tr("Invalid format: empty email in %1")
			.arg(QDir::toNativeSeparators(_templateFile));
	_certPair = pathByExt("p12");
	QFileInfo cert(_certPair);
	if(cert.exists()) {
		_certCreated = cert.lastModified();
	}
	return QString();
}
QString CertTableModel::Item::sha256FromCertificate(QString & sha256)const {
	QString path = pathByExt("crt");
	QFile file(path);
	if(!file.open(QFile::ReadOnly)) {
		return tr("Can't open %1").arg(path);
	}
	QByteArray cert = file.readAll();
	const QByteArray sectionBegin = "-----BEGIN CERTIFICATE-----";
	const QByteArray sectionEnd = "-----END CERTIFICATE-----";
	int i1 = cert.indexOf(sectionBegin);
	int i2 = cert.indexOf(sectionEnd);
	if(-1==i1 || -1==i2 || i1>i2) {
		return tr("Wrong certificate file format %1").arg(path);
	}
	i1 += sectionBegin.count();
	cert = cert.mid(i1, i2-i1);
	cert.replace('\n', "");
	cert.replace('\r', "");
	QByteArray h = QCryptographicHash::hash(QByteArray::fromBase64(cert), QCryptographicHash::Sha256);
	sha256 = QString::fromLatin1(h.toHex());
	sha256 = sha256.toLower();
	return {};
}
using Shell = ShellImitation;
QString CertTableModel::Item::generateCert(CertLogger*logger, CertType ctype, const QString & pass, QString & sha256)const {//QString::isEmpty -> ok
	QString certType;
	if(ctype == EC) {
		certType = "EC";
	} else if(ctype == RSA) {
		certType = "RSA";
	} else {
		Q_ASSERT(0);
	}
	const QString CA_DIR = Settings::certDir().absoluteFilePath("CA-" + certType);

	const QDir cur = Settings::certDir();
	QDir db = cur.absoluteFilePath("db");
	QString err;
	Shell shell(logger);
	if(logger) {
		logger->clear();
		logger->append(QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss"));
	}
	if(!shell.removeRecursiveFilesOnly(db, err)
	|| !shell.mkpath(cur, "db/certs", err)
	|| !shell.mkpath(cur, "db/newcerts", err)
	|| !shell.touch(db, "index.txt", err)
	|| !shell.write(db.absoluteFilePath("serial"), _baseName.toLatin1(), err))
		return err;
	OpenSslExecutable openssl;
	openssl.setLogger(logger);
	if(!openssl.generateKeyAndCertificateRequest(_baseName, _templateLine)
		|| !openssl.generateCertificate(_baseName, CA_DIR)
		|| !openssl.createCertificatePair(_baseName, CA_DIR, pass))
	{
		return openssl.errorString();
	}
	
	shell.maybeLog(tr("For security reasons, removing:"));
	shell.remove(pathByExt("key"));
	shell.remove(pathByExt("csr"));

	shell.maybeLog(tr("sha256 from certificate..."));
	QString error = sha256FromCertificate(sha256);
	if(!error.isEmpty())
		return error;

	openssl.log("_______________________");
	openssl.log(QObject::tr("Please, deposit into MFCoin 'Manage names' tab:\n"
		"Name of key (whole next line):\nssl:%1\n"
		"Value:\nsha256=%2").arg(_baseName).arg(sha256));
	openssl.log("_______________________");
	if(logger)
		logger->find("ssl:"+_baseName, QTextDocument::FindBackward);
	return QString();
}
QString CertTableModel::Item::pathByExt(const QString & extension)const {
	return _dir.absoluteFilePath(_baseName + '.' + extension);
}
void CertTableModel::Item::installIntoSystem()const {
	if(!_certPair.isEmpty() && QFile::exists(_certPair)) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(_certPair));
	}
}
QString CertTableModel::Item::removeFiles() {
	for(auto ext: QString("crt|csr|key|p12|tpl|log").split('|')) {
		QString path = pathByExt(ext);
		if(QFile::exists(path)) {
			if(!QFile::remove(path)) {
				return QObject::tr("Can't remove %1").arg(path);
			}
		}
	}
	return QString();
}
CertTableModel::CertTableModel(QObject*parent): QAbstractTableModel(parent) {
	reload();
}
void CertTableModel::removeRows(const QModelIndexList & rows) {
	if(rows.isEmpty())
		return;
	int row = rows[0].row();
	if(row < 0 || row >= _rows.count()) {
		Q_ASSERT(0);
		return;
	}
	Item & r = _rows[row];
	QString error = r.removeFiles();
	if(!error.isEmpty()) {
		reload();
		QMessageBox::critical(0, tr("Error"), tr("Error removing files: %1").arg(error));
		return;
	}
	beginRemoveRows(QModelIndex(), row, row);
	_rows.removeAt(row);
	endRemoveRows();
}
void CertTableModel::reload() {
	beginResetModel();
	_rows.clear();
	QDir dir = Settings::certDir();
	const QFileInfoList list = dir.entryInfoList(QStringList() << "*.tpl", QDir::Files, QDir::Name);
	for(const QFileInfo & entry : list) {
		Item item;
		const QString code = item.loadFromTemplateFile(entry);
		if(code.isEmpty()) {
			_rows << item;
		} else {
			QMessageBox::critical(0, tr("Can't load template file"), code);
		}
	}
	endResetModel();
}
int CertTableModel::rowCount(const QModelIndex& index)const {
	return _rows.count();
}
int CertTableModel::columnCount(const QModelIndex& index)const {
	return ColEnd;
}
QVariant CertTableModel::headerData(int section, Qt::Orientation orientation, int role)const {
	if(orientation == Qt::Vertical) {
		if(role == Qt::DisplayRole || role == Qt::ToolTipRole)
			return section + 1;
	} else {
		if(role == Qt::DisplayRole || role == Qt::ToolTipRole) {
			static_assert(ColEnd == 6, "update switch");
			switch(section) {
				case ColName: return tr("Name");
				case ColMail: return tr("email");
				case ColInfoCardId: return tr("InfoCard id");
				case ColMenu: return tr("Menu");
				case ColCertFile: return tr("certificate file");
				case ColCertCreated: return tr("Cert updated");
				//case ColTemplateFile: return tr("Template file");
			}
		}
	}
	return QVariant();
}
QVariant CertTableModel::data(const QModelIndex &index, int role) const {
	int row = index.row();
	if(row<0 || row >= rowCount())
		return QVariant();
	const auto & item = _rows.at(row);
	if(role == Qt::DisplayRole || role == Qt::ToolTipRole) {
		static_assert(ColEnd == 6, "update switch");
		switch(index.column()) {
			case ColName: return item._name;
			case ColMail: return item._mail;
			case ColInfoCardId: return item._InfoCardId;
			case ColCertFile: return item._baseName + ".p12";
			case ColMenu: return QVariant();
			case ColCertCreated: return item._certCreated.toString("yyyy.MM.dd HH:mm:ss");
			//case ColTemplateFile: return item._templateFile;
		}
	}
	return QVariant();
}
int CertTableModel::indexByFile(const QString & s)const {
	for(int i = 0; i < _rows.count(); ++i) {
		const auto & row = _rows[i];
		if(row._templateFile==s || row._certPair == s)
			return i;
	}
	return -1;
}
