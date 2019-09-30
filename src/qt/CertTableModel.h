//CertTableModel.h by MFCoin developers
#pragma once
#include <QAbstractTableModel>
#include <QDateTime>
#include <QDir>
class CertLogger;

class CertTableModel: public QAbstractTableModel {
	public:
	enum Columns {
		ColName,
		ColMail,
		ColInfoCardId,
		ColCertFile,
		ColMenu,
		ColCertCreated,
		//ColTemplateFile,

		ColEnd
	};
	enum CertType {
		EC,
		RSA
	};
	struct Item {
		QString _name;//like nickname
		QDir _dir;
		QString _mail;
		QString _InfoCardId;//optional
		QString _templateFile;//absolute path
		QString _baseName;//file name without path and extension
		QString _certPair;
		QDateTime _certCreated;
		QByteArray _templateLine;//it's parsed into name, mail and InfoCard
		
		QString logFilePath()const;
		QString loadFromTemplateFile(const QFileInfo & entry);//returns error, isEmpty() -> ok
		QString generateCert(CertLogger*logger, CertType certType, const QString & pass, QString & sha256)const;//returns error, isEmpty() -> ok
		QString removeFiles();//returns error, isEmpty() -> ok
		void installIntoSystem()const;
		QString pathByExt(const QString & extension)const;
		QString sha256FromCertificate(QString & sha256)const;//returns error, isEmpty() -> ok
	};
	QList<Item> _rows;
	
	CertTableModel(QObject*parent);
	void reload();
	int indexByFile(const QString & s)const;
	void removeRows(const QModelIndexList & rows);
	virtual int rowCount(const QModelIndex& index = QModelIndex())const override;
	virtual int columnCount(const QModelIndex& index = QModelIndex())const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole)const override;
	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
