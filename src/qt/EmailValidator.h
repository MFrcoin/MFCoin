//EmailValidator.h by MFCoin developers
#pragma once
#include "ValidatorToLabel.h"

class EmailValidator: public ValidatorToLabel {
	public:
		EmailValidator(QObject*parent):ValidatorToLabel(parent) {}
		virtual void fixup(QString &input)const override;
		virtual State validate(QString &str, int &pos)const override;
		//this validator is not in fully acordance to
		//https://en.wikipedia.org/wiki/Email_address#Syntax
		//it's more strict to restrict users to rules that is common for email providers
	protected:
		//neurocod@gmail.com - neurocod is local, gmail.com is domain
		static bool isAsciiAlpha(QChar ch);
		QValidator::State checkLocal(const QString & str, int & pos)const;
		QValidator::State checkDomain(const QString & str, int & pos)const;
};