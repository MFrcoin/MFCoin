#include "NameCoinStrings.h"
#include "QNameCoin.h"

const QChar NameCoinStrings::charCheckOk = 0x2705;
const QChar NameCoinStrings::charX = 0x274C;
QChar NameCoinStrings::charBy(bool ok) {
	return ok ? charCheckOk : charX;
}
QString NameCoinStrings::trNameNotFound(const QString & name) {
	return charBy(false) + tr(" This name is not found (%1)").arg(name);
}
QString NameCoinStrings::trNameIsFree(const QString & name, bool ok) {
	return charBy(ok) + tr(" This name is free (%1)").arg(name);
}
QString NameCoinStrings::trNameAlreadyRegistered(const QString & name, bool ok) {
	return charBy(ok) + tr(" This name is already registered in blockchain (%1)").arg(name);
}
QString NameCoinStrings::nameExistOrError(const QString & name) {
	if(name.isEmpty())
		return {};

	if(QNameCoin::nameActive(name))
		return charBy(true) + tr(" This name is registered (%1)").arg(name);

	return trNameNotFound(name);
}
QString NameCoinStrings::nameExistOrError(const QString & name, const QString & prefix) {
	if(prefix.isEmpty())
		return nameExistOrError(name);

	if(name.isEmpty())
		return {};

	if(!name.startsWith(prefix))
		return charBy(false) + tr(" Name must start with '%1' prefix").arg(prefix);

	if(name==prefix)
		return charBy(false) + tr(" Enter name");

	if(QNameCoin::nameActive(name))
		return charBy(true) + tr(" This name is registered (%1)").arg(name);

	return trNameNotFound(name);
}
QString NameCoinStrings::trNameIsMy(const QString & name) {
	return charCheckOk + tr(" You are owner of this name and can change it (%1)").arg(name);
}
