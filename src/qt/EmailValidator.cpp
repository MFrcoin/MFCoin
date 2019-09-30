//EmailValidator.cpp by MFCoin developers
#include "EmailValidator.h"

void EmailValidator::fixup(QString &input)const {
	input.remove(' ');
	input.remove('\t');
	input.remove('\r');
	input.remove('\n');
}
bool EmailValidator::isAsciiAlpha(QChar ch) {//static
	return ('a' <= ch && ch <= 'z') ||
		('A'<= ch && ch <= 'Z');
}
QValidator::State EmailValidator::checkLocal(const QString & str, int & pos)const {
	pos = 0;
	const QString allowed = "_-.!#$%&'*"
		//"+-/=?^_`{|}~" //characters after + are valid but ignored
		;
	if(str.isEmpty()) {
		return set(tr("Empty name before @ character"));
	}
	int i = 0;
	pos = 0;
	if(str[0] == '.') {
		return set(tr("Dot (.) can't be at the start of email"));
	}
	pos = str.count() - 1;
	if(str[pos] == '.') {
		return set(tr("Dot (.) can't be at the end of you email name"));
	}
	for(pos = 0; pos < str.length(); ++pos) {
		QChar ch = str[pos];
		if(isAsciiAlpha(ch) || ch.isDigit() || allowed.contains(ch))
			continue;
		return set(tr("Invalid character ") + ch);
	}
	return set(QValidator::Acceptable);
}
QValidator::State EmailValidator::checkDomain(const QString & str, int & pos)const {
	pos = 0;
	if(str.isEmpty()) {
		return set(tr("Domain part after @ is empty"));
	}
	if(str[0] == '-') {
		return set(tr("Hyphen '-' can't be at the start of domain"));
	}
	pos = str.count() - 1;
	if(str[pos] == '-') {
		return set(tr("Hyphen '-' can't be at the end of domain"));
	}
	QStringList parts = str.split('.', QString::KeepEmptyParts);
	if(parts.count()<2) {
		return set(tr("Single domain without subdomains?"));
	}
	pos = 0;
	for(QString part : parts) {
		if(part.isEmpty()) {
			return set(tr("empty domain part .."));
		}
		for(int i = 0; i < part.length(); ++i, ++pos) {
			QChar ch = part.at(i);
			if(isAsciiAlpha(ch) || ch.isDigit() || ch == '-')
				continue;
			return set(tr("Unsupported character ") + ch);
		}
		pos++;
	}
	return set(QValidator::Acceptable);
}
EmailValidator::State EmailValidator::validate(QString &str, int &pos2)const {
	if(str.isEmpty())
		return set("Empty email");
	int pos = 0;
	const int indexA = str.indexOf('@');
	if(-1 == indexA) {
		return set(tr("No '@' character"));
	}
	if(str.count('@') != 1)
		return set(tr("More than one '@' characters"));
	QString local = str.left(indexA);
	QString domain = str.mid(indexA + 1);
	auto ret = checkLocal(local, pos);
	if(ret!=Acceptable)
		return ret;
	ret = checkDomain(domain, pos);
	if(ret != Acceptable) {
		pos += local.length() + 1;
		return ret;
	}
	return Acceptable;
}
