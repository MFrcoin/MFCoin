//ValidatorToLabel.cpp by MFCoin developers
#include "ValidatorToLabel.h"
#include <QLabel>

QValidator::State ValidatorToLabel::set(QValidator::State state, QString str)const {
	if(str.isEmpty()) {
		if(_labelError)
			_labelError->hide();
		return state;
	}
	if(_labelError) {
		str = QString("<font color='red'>%1</font>").arg(str.toHtmlEscaped());
		str.replace("\n", "<br>\n");
		_labelError->setText(str);
		_labelError->show();
	}
	return state;
}
void ValidatorToLabel::setErrorLabel(QLabel*label) {
	_labelError = label;
	if(label && label->text().isEmpty())
		_labelError->hide();
}
