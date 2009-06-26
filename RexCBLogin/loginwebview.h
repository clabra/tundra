#ifndef LOGINWEBVIEW_H
#define LOGINWEBVIEW_H

#include "ui_loginwebview.h"
#include "loadprogress.h"

#include <QtGui>
#include <QWidget>
#include <QGraphicsView>
#include <QNetworkReply>

class LoginWebView 
	: public QWidget
{
	Q_OBJECT

public:
	LoginWebView(QWidget *parent = 0, QGraphicsView *view = 0);
	Ui_LoginWebView *ui;
	LoadProgressPanel *loadSplash;
	void loadAutoLoginPage();

public slots:
	void loadStarted();
	void loadInProgress(int progress);
	void loadFinished(bool success);

private:
	void connectSignals();
	QGraphicsView *myview;

};

#endif LOGINWEBVIEW_H