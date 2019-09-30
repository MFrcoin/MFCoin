//InfoCardsWidget.h by MFCoin developers
#pragma once
#include <QDialog>
class InfoCardTableView;
class QAbstractButton;
class InfoCardTableModel;
class CertLogger;

class InfoCardsWidget: public QDialog {
	public:
		InfoCardsWidget(QWidget*parent=0);
		QString name()const;
		QString value()const;
	protected:
		void onDelete();
		void onCreate();
		void enableButtons();
		static QString randName();

		QAbstractButton* _btnDelete = 0;
		QAbstractButton* _btnGenerate = 0;
		QAbstractButton* _btnOpenFolder = 0;
		InfoCardTableView* _view = 0;
		CertLogger* _logger = 0;
};
