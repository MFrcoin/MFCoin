//InfoCardHighlighter.h by MFCoin developers
#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class InfoCardHighlighter: public QSyntaxHighlighter {
	public:
		InfoCardHighlighter(QTextDocument *parent);
		virtual void highlightBlock(const QString &text)override;
		QTextCharFormat _key;
		QTextCharFormat _comment;
	protected:
		void highlightKeys(const QString &text);
		void highlightComments(const QString &text);
};
