//InfoCardHighlighter.cpp by MFCoin developers
#include "InfoCardHighlighter.h"

InfoCardHighlighter::InfoCardHighlighter(QTextDocument *parent): QSyntaxHighlighter(parent) {
    _comment.setForeground(Qt::darkGreen);
    _key.setForeground(Qt::darkRed);
}
void InfoCardHighlighter::highlightBlock(const QString &text) {
	highlightKeys(text);
	highlightComments(text);
}
void InfoCardHighlighter::highlightComments(const QString &s) {
	for(int i = 0; i!=-1 && i<s.count(); ) {
		i = s.indexOf('#', i);
		if(-1==i)
			break;
		if(i > 0 && s[i - 1] == '\\') {//escape character
			++i;
			continue;
		}
		int end = s.indexOf('\n', i);
		if(-1==end)
			end = s.count();
		setFormat(i, end-i, _comment);
		i = end;
	}
}
void InfoCardHighlighter::highlightKeys(const QString &s) {
	for(int i = 0; i!=-1 && i<s.count(); ++i) {
		if(s[i].isSpace()) {//lines starting from spaces are values of prev keyword
			i = s.indexOf('\n', i+1);
			if(-1==i)
				break;
			i++;
			continue;
		}
		if(i>=s.length())
			break;
		int end = i+1;
		while(end < s.length()) {
			if(s[end++].isSpace())
				break;
		}
		setFormat(i, end-i, _key);
		i = s.indexOf('\n', i+1);
		if(-1==i)
			break;
	}
}
