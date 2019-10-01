//InfoCardTextEdit.cpp by MFCoin developers
#include "InfoCardTextEdit.h"
#include "InfoCardHighlighter.h"
#include <QCompleter>
#include <QStringListModel>
#include <QTextBlock>
#include <QScrollBar>
#include <QAbstractItemView>

InfoCardTextEdit::InfoCardTextEdit() {
	setWordWrapMode(QTextOption::NoWrap);
	new InfoCardHighlighter(document());

	auto completer = new QCompleter(this);
	_keywords = QString("Import Alias FirstName LastName HomeAddress HomePhone CellPhone Gender Birthdate Email WEB Facebook Twitter MFC BTC").split(' ');
	qSort(_keywords);
	auto model = new QStringListModel(_keywords, this);
    completer->setModel(model);
    completer->setFilterMode(Qt::MatchContains);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    //completer->setWrapAround(false);
    setCompleter(completer);

	//commented cause Qt bug - somtimes it get zero width tab after text like "MFC\tA"
	//qreal w = QFontMetricsF(font()).width(QLatin1Char('x')) * 4;
	//if(w>0)
	//	setTabStopDistance(w);
}
void InfoCardTextEdit::setCompleter(QCompleter *completer) {
    if (_c)
        QObject::disconnect(_c, 0, this, 0);
    _c = completer;
    if (!_c)
        return;

    _c->setWidget(this);
    _c->setCompletionMode(QCompleter::PopupCompletion);
    _c->setCaseSensitivity(Qt::CaseInsensitive);
    connect(_c, static_cast<void(QCompleter::*)(const QString&)>(&QCompleter::activated), this, &InfoCardTextEdit::insertCompletion);
}
QCompleter *InfoCardTextEdit::completer() const {
    return _c;
}
void InfoCardTextEdit::insertCompletion(const QString& completion) {
    if (_c->widget() != this)
        return;
    QTextCursor tc = textCursor();
	tc.select(QTextCursor::WordUnderCursor);
	tc.removeSelectedText();
	tc.insertText(completion);
}
QString InfoCardTextEdit::textUnderCursor()const {
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}
QString InfoCardTextEdit::wordLeftFromCursor()const {
    QTextCursor tc = textCursor();
	QTextBlock block = tc.block();
	QString s = block.text();
	const int cur = tc.positionInBlock();
	return s.left(cur).trimmed();
}
bool InfoCardTextEdit::cursorIsOnKeywordPosition()const {
    QTextCursor tc = textCursor();
	QTextBlock block = tc.block();
	QString s = block.text();
	const int cur = tc.positionInBlock();
	if(0==cur)
		return true;
	for(int i = 0; i < s.count(); ++i) {
		if(s[i].isSpace() || isCommentStart(s, i))
			return false;
		if(i + 1==cur)
			return true;
	}
	return false;
}
bool InfoCardTextEdit::isCommentStart(const QString & s, int pos) {
	if(s.isEmpty())
		return false;
	if(s[pos] == '#') {
		if(pos>0 && s[pos-1]=='\\')
			return false;
		return true;
	}
	return false;
}
void InfoCardTextEdit::wheelEvent(QWheelEvent *e) {
	if(e->modifiers() == Qt::ControlModifier && !isReadOnly() && e->buttons()==Qt::NoButton) {
		//read-only is processed by default
		e->accept();
		QPoint numDegrees = e->angleDelta() / 8;
		if(numDegrees.y()>0)
			zoomIn();
		else
			zoomOut();
		return;
	}
	QPlainTextEdit::wheelEvent(e);
}
void InfoCardTextEdit::focusInEvent(QFocusEvent *e) {
    if (_c)
        _c->setWidget(this);
    QPlainTextEdit::focusInEvent(e);
}
void InfoCardTextEdit::keyPressEvent(QKeyEvent *e) {
	if (_c && _c->popup()->isVisible()) {
	// The following keys are forwarded by the completer to the widget
	switch (e->key()) {
		case Qt::Key_Enter:
		case Qt::Key_Return:
		case Qt::Key_Escape:
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
			e->ignore();
			return; // let the completer do default behavior
		}
	}

	bool isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space); // CTRL+Space
	if (!_c || !isShortcut) // do not process the shortcut when we have a completer
		QPlainTextEdit::keyPressEvent(e);

	const bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
	if (!_c || (ctrlOrShift && e->text().isEmpty()))
		return;

	bool hasModifier = (e->modifiers() != Qt::NoModifier) && !ctrlOrShift;

	bool hide = !cursorIsOnKeywordPosition() && !isShortcut && (!hasModifier || e->text().isEmpty());
	if(!hide) {
		if(_keywords.contains(textUnderCursor()))
			hide = true;
		else if(//is multiline selection with paragraph separator
			textCursor().selectedText().contains(QChar(0x2029)))
		{
			hide = true;
		}
			
	}
	if(hide) {
		_c->popup()->hide();
		return;
	}

	QString completionPrefix = wordLeftFromCursor();
	if (completionPrefix != _c->completionPrefix()) {
		_c->setCompletionPrefix(completionPrefix);
		_c->popup()->setCurrentIndex(_c->completionModel()->index(0, 0));
	}
	QRect cr = cursorRect();
	cr.setWidth(_c->popup()->sizeHintForColumn(0)
				+ _c->popup()->verticalScrollBar()->sizeHint().width());
	_c->complete(cr); // popup it up
}
