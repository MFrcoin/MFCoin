//ValidatorToLabel.h by MFCoin developers
#pragma once
#include <QValidator>
class QLabel;

class ValidatorToLabel: public QValidator {
	public:
		ValidatorToLabel(QObject*parent): QValidator(parent) {}
		void setErrorLabel(QLabel*label);
		QValidator::State set(QValidator::State state, QString str = QString())const;
		QValidator::State set(const QString & str)const { return set(QValidator::Intermediate, str); }
		QValidator::State setOk()const { return set(QValidator::Acceptable); }
	protected:
		QLabel* _labelError = 0;//if set - show error there
};
