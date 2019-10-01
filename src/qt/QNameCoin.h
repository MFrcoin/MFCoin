#pragma once
#include <QObject>
class UniValue;
class RandPayRequest;

//Qt NameCoin interface
class QNameCoin: public QObject {//for tr()
    public:
        static bool isMyName(const QString & name);
        static bool nameActive(const QString & name);
        static QStringList myNames(bool sortByLessParts = true);
        static QStringList myNamesStartingWith(const QString & prefix);

		struct SignMessageRet {
			QString signature;
			QString address;
			QString error;
			bool isError()const { return !error.isEmpty(); }
		};
		static SignMessageRet signMessageByName(const QString& name, const QString& message);
		static UniValue signMessageByAddress(const QString& address, const QString& message);//may throw
		static UniValue nameShow(const QString& name);//may throw
		struct NVPair {
			QString name;
			QByteArray value;
		};
		static QList<NVPair> nameScan(const QString& prefix, int maxCount = 0);

        static QString numberLikeBase64(quint64 n);
        static QString currentSecondsPseudoBase64();
        static QString errorToString(UniValue& v);
		static QString toString(const std::exception& e);

		static QString randPayCreateTx(const RandPayRequest & r, QString & error);
};
