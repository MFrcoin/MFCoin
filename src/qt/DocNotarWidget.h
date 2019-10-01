//DocNotarWidget.h by MFCoin developers
#pragma once
#include <QTabWidget>
#include <QDialog>
class DpoCreateRootWidget;
class DpoCreateRecordWidget;
class DpoRegisterDocWidget;

class DocNotarWidget: public QDialog {
	public:
		DocNotarWidget(QWidget*parent = 0);
		~DocNotarWidget();
		QString name()const;
		QString value()const;
	protected:
		QTabWidget* _tab = 0;

		DpoCreateRootWidget* _createRoot = 0;
		DpoCreateRecordWidget* _createRecord = 0;
		DpoRegisterDocWidget* _registerDoc = 0;
};
