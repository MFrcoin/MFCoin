//InfoCard.cpp by MFCoin developers
#include "InfoCard.h"
#include "ShellImitation.h"
#include "OpenSslExecutable.h"
#include "../quazip/JlCompress.h"
#include <QCryptographicHash>
#include <QDateTime>

InfoCard::InfoCard(const QString& fileName): _fileName(fileName) {
	QFileInfo entry(fileName);
	_baseName = entry.baseName();
	_dir = entry.dir();
}
QString InfoCard::tr(const char *t) {
	return QObject::tr(t);
}
QString InfoCard::indexAndPassFromText(QString & index, QString & pass) {
	if(_text.startsWith("#!info:")) {
		int i = _text.indexOf('\n');
		if(-1==i)
			i = _text.length();
		auto s = _text.left(i);
		auto parts = s.split(':');
		if(parts.size() == 3) {
			index = parts[1];
			pass = parts[2];
			return {};
		}
	}
	QByteArray hash = QCryptographicHash::hash(_text.toUtf8(), QCryptographicHash::Sha256);
	QByteArray sha256 = hash.toHex().toLower();
	index = sha256.left(16);//64bit indrx
	pass = sha256.mid(20, 30);//120bit password

	QString line = "#!info:" + index + ':' + pass + '\n';
	_text.prepend(line);
	return save();
}
QString InfoCard::load() {
	QFile file(_fileName);
	if(!file.open(QFile::ReadOnly)) {
		return tr("Can't open %1").arg(QDir::toNativeSeparators(_fileName));
	}
	QByteArray arr = file.readAll();
	_text = QString::fromUtf8(arr);
	parse();
	return {};
}
QString InfoCard::save()const {
	QFile file(_fileName);
	if(!file.open(QFile::WriteOnly)) {
		return tr("Can't open %1 for writing: %2")
			.arg(QDir::toNativeSeparators(_fileName)).arg(file.errorString());
	}
	auto arr = _text.toUtf8();
	const qint64 written = file.write(arr);
	if(written!=arr.count())
		return tr("Can't write all data to %1: %2")
			.arg(QDir::toNativeSeparators(_fileName)).arg(file.errorString());
	return {};
}
void InfoCard::removeComments(QString & text) {
	QStringList lines = text.split('\n');
	QStringList lines2;
	for(auto & line: lines) {
		removeComment(line);
		if(line.trimmed().isEmpty())
			continue;
		lines2.append(line);
	}
	text = lines2.join('\n');
}
QString InfoCard::pathByExt(const QString & extension)const {
	return _dir.absoluteFilePath(_baseName + '.' + extension);
}
QString InfoCard::encrypt(CertLogger*logger) {
	return "n impl";
	if(logger) {
		logger->clear(true);
		logger->append(tr("Running file encryption at ") + QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss"));
	}
	QString index, pass;
	auto err = indexAndPassFromText(index, pass);
	if(!err.isEmpty())
		return err;
	QString clean = _text;
	removeComments(clean);
	QString cleanFileName = pathByExt("txt");
	if(!Shell(logger).write(cleanFileName, clean.toUtf8(), err))
		return err;
	const QString pathZip = pathByExt("zip");
	if(!JlCompress::compressFile(pathZip, cleanFileName)) {
		Shell(logger).maybeLog(tr("Can't compress file %1 to %2").arg(cleanFileName).arg(pathZip));
		return err;
	}
	OpenSslExecutable openssl;
	openssl.setLogger(logger);
	QString infozFile = pathByExt("infoz");
	/*if(!openssl.encryptInfocardAes(cleanFileName, infozFile, pass))
		return openssl.errorString();
	openssl.log("_______________________");
	openssl.log(tr("Please, deposit into MFCoin 'Manage names' tab:\n"
		"Name of key (whole next line):\ninfo:%1\n"
		"Value - import file %2\n").arg(index).arg(infozFile));
	if(logger) {
		logger->find("info:" + index, QTextDocument::FindBackward);
		logger->setFocus();
	}
	openssl.log(tr("To link MFCSSL Certificate to this info file, create certificate in Certificates tab and use value for UID: info:%1:%2")
		.arg(index).arg(pass));
	openssl.log("_______________________");
	return {};*/
}
void InfoCard::parse() {
	QString text = _text;
	text.replace("\r\n", "\n");
	text.replace('\r', '\n');
	text.replace('\t', ' ');
	auto lines = text.split('\n', QString::SkipEmptyParts);
	QString lastKey;
	for(QString & line: lines) {
		parseLine(line, lastKey);
	}
}
void InfoCard::add(const QString & key_, const QString & value_, bool replace) {
	QString key = key_.trimmed();
	QString value = value_.trimmed();
	if(key.endsWith('+')) {
		key.truncate(key.length()-2);
		replace = false;
	}
	for(QChar c: key) {
		Q_ASSERT(!c.isSpace());
		if(c.isSpace())
			return;
	}
	if(replace)
		_values.insert(key, value);
	else
		_values.insertMulti(key, value);
}
void InfoCard::parseLine(QString & line, QString & lastKey) {
	removeComment(line);
	if(line.isEmpty())
		return;
	if(line[0].isSpace()) {
		if(lastKey.isEmpty())
			return;
		line = line.trimmed();
		if(line.isEmpty())
			return;
		add(lastKey, line, false);
		return;
	}
	for(int i = 1; i < line.count(); ++i) {
		if(line[i].isSpace()) {
			lastKey = line.left(i);
			QString val = line.mid(i).trimmed();
			if(!val.isEmpty())
				add(lastKey, val, true);
			break;
		}
	}
}
void InfoCard::removeComment(QString & line) {
	for(int i = line.indexOf('#'); i < line.count(); ) {
		if(-1==i)
			return;
		if(i>0 && line[i-1]=='\\') {//escaped
			++i;
			i = line.indexOf('#', i);
			continue;
		}
		line.truncate(i);
		return;
	}
}
