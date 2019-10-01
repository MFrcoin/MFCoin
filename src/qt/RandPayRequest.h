//RandPayRequest.h by MFCoin developers
#pragma once
#include <QUrlQuery>
#include <QObject>
class QNetworkRequest;
class PaymentServer;

class RandPayRequest: public QUrlQuery {
	// example query:
	// mfcoin://randpay?amount=DOUBLE&chap=chap_hex&risk=INT[&timeout=INT]&submit=ESCAPED_URI
	public:
		static bool isRandPayUrl(const QUrl&u);
		static void process(const QUrl & url, PaymentServer* server);
		static void showSuccess(const QNetworkRequest & r);
		double _amount = 0;
		QString _chap;//short for 'challenge protocol'
		quint64 _risk = 0;
		quint64 _timeout = 0;//opional
		QUrl _submit;
	
	protected:
		PaymentServer* _server = 0;
		QString _error;//empty -> no error
		RandPayRequest(const QUrl & u, PaymentServer* server);
		bool isAmountEnough()const;
		static QString tr(const char *str) { return QObject::tr(str); }

		quint64 timeoutFromSettings();
		void correctTimeout();
		QString parse();
		void readConfig();
		void process_(PaymentServer* server);
		bool needConfirmation()const;
		bool askConfirmation()const;
		bool makePayment();
		QString checkAmountInWallet()const;
		static void emitMessage(PaymentServer* server, const QString & s);

		struct Dialog;
		friend class Dialog;
		struct Config {
			quint64 _timeout = 30;//secs by default
			double _maxAmount = 0;
			double _maxPayment = 0;
			bool _defaultYes = false;
		} _config;
};
