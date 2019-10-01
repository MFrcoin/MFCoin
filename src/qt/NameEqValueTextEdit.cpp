//NameEqValueTextEdit.cpp by MFCoin developers
#include "NameEqValueTextEdit.h"
#include <QTextDocument>
#include <QSyntaxHighlighter>
#include <QSize>

class NameEqValueTextEdit::Highlighter: public QSyntaxHighlighter {
	public:
		Highlighter(QTextDocument *parent): QSyntaxHighlighter(parent) {
			_key.setForeground(Qt::darkRed);
		}
		virtual void highlightBlock(const QString &text)override;
		QTextCharFormat _key;
	protected:
		void highlightKeys(const QString &text);
};
void NameEqValueTextEdit::Highlighter::highlightBlock(const QString &text) {
	highlightKeys(text);
}
void NameEqValueTextEdit::Highlighter::highlightKeys(const QString &s) {
	for(int i = 0; i!=-1 && i<s.count(); ++i) {
		int endOfLine = s.indexOf('\n', i+1);
		if(-1==endOfLine)
			endOfLine = s.length()-1;
		int endOfName = s.indexOf('=', i);
		if(endOfName == -1)
			endOfName = endOfLine + 1;

		setFormat(i, endOfName - i, _key);
		i = endOfLine + 1;
	}
}
NameEqValueTextEdit::NameEqValueTextEdit() {
	new Highlighter(document());
}
QSize NameEqValueTextEdit::sizeHint()const {
	auto s = QPlainTextEdit::sizeHint();
	s.setHeight(30);
	return s;
}
