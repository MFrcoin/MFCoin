//InfoCardTextEdit.h by MFCoin developers
#pragma once
#include <QPlainTextEdit>
class QCompleter;

class InfoCardTextEdit: public QPlainTextEdit {
	public:
		InfoCardTextEdit();
		void setCompleter(QCompleter *c);
		QCompleter *completer()const;
	protected:
		virtual void keyPressEvent(QKeyEvent *e)override;
		virtual void focusInEvent(QFocusEvent *e)override;
		virtual void wheelEvent(QWheelEvent *e)override;
		void insertCompletion(const QString &completion);
		QString wordLeftFromCursor()const;
		QString textUnderCursor()const;
		bool cursorIsOnKeywordPosition()const;
		static bool isCommentStart(const QString & s, int pos);
		QCompleter *_c = 0;
		QStringList _keywords;
};
