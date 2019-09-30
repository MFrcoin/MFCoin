//PhoneNumberLineEdit.cpp by MFCoin developers
#include "PhoneNumberLineEdit.h"
#include <QValidator>

struct PhoneValidator: public QValidator {
	//E.164 format https://en.wikipedia.org/wiki/E.164
	PhoneValidator(QObject*parent = 0): QValidator(parent) {}
	virtual void fixup(QString &input) const override {
		input = input.trimmed();
	}
	virtual State validate(QString &input, int &pos) const override {
		input = input.trimmed();
		for(QChar& c : input) {
			if(c=='+' || c==' ' || c=='-' || c=='('|| c==')' || c=='\t' || c.isDigit())
				continue;
			return Invalid;
		}
		return Acceptable;
	}
};
PhoneNumberLineEdit::PhoneNumberLineEdit() {
	setValidator(new PhoneValidator(this));
	setPlaceholderText(tr("Like +124-000-000"));
}
QString PhoneNumberLineEdit::toPhoneNumber()const {
	QString s = text();
	s.remove(' ');
	s.remove('\t');
	s.remove('-');
	s.remove('(');
	s.remove(')');
	return s;
}
