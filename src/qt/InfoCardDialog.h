//InfoCardDialog.h by MFCoin developers
#pragma once
#include <QDialog>
#include <QLabel>
class InfoCardTextEdit;
class InfoCard;
class CertLogger;
class QPushButton;
class QTabWidget;
class QFormLayout;
class QLineEdit;
class QPlainTextEdit;

class InfoCardDialog: public QDialog {
	public:
		InfoCardDialog(InfoCard&info, CertLogger* logger, QWidget*parent=0);
		virtual void accept()override;

		QString text()const;
		void setText(const QString & s);
	protected:
		InfoCard& _info;
		CertLogger* _logger = 0;
		QPushButton* _okBtn = 0;
		QLabel* _id = new QLabel;
		InfoCardTextEdit* _text = 0;
		QFormLayout* _lay = 0;
		struct Item {
			QString _name;
			bool _multiline = false;
			QLineEdit* _line = 0;
			QPlainTextEdit* _text = 0;
			QString text()const;
			QWidget* widget()const;
		};
		QList<Item> _rows;
		void add(Item & row);
		void addExample(QTabWidget*tabs, int n);
		void enableOk();
		bool allValid()const;
};
