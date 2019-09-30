//DiplomaWidget.h by MFCoin developers
#pragma once
#include <QTabWidget>
#include <QDialog>
class CheckDiplomaWidget;
class RegisterUniversityWidget;
class RegisterDiplomaWidget;

class DiplomaWidget: public QDialog {
	public:
		DiplomaWidget(QWidget*parent = nullptr);
		~DiplomaWidget();
		QString name()const;
		QString value()const;
	protected:
		QTabWidget* _tab = nullptr;
		CheckDiplomaWidget* _CheckDiplomaWidget = nullptr;
		RegisterUniversityWidget* _RegisterUniversityWidget = nullptr;
		RegisterDiplomaWidget* _RegisterDiplomaWidget = nullptr;
};
