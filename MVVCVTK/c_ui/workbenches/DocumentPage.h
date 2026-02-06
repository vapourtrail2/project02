#pragma once
#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QDebug>
#include <QDialog>

class QVBoxLayout;

class DocumentPage : public QWidget
{
    Q_OBJECT
public:
    explicit DocumentPage(QWidget* parent = nullptr);
    void notifySucc();
	void notifyFail(const QString& reason);

signals:
    void requestSwitchTo(const QString& page);
    void moduleClicked(const QString& info);
    void recentOpenRequested(const QString& projectName);
	void openRequested(const QString& path);
    
private:
    void buildUi();
    void buildLeftDock();
    QWidget* buildRightContent(QWidget* parent);
    void wireLeftDockSignals();
	void showOpenDialog();
    void updateStatusLabel(const QString& text, bool isError);
    void loadFilePath(const QString& path);

    QPointer<QListWidget> listNav_;
    QPointer<QPushButton> btnUndo_;
    QPointer<QPushButton> btnKeep_;
    QPointer<QPushButton> btnVisCheck_;
    QPointer<QPushButton> btnPorosity_;
    QPointer<QPushButton> btnMetrology_;
    QPointer<QPushButton> btnMaterial_;
    QPointer<QPushButton> btnDicomEntry_;
    QPointer<QTableWidget> tableRecent_;
    QPointer<QLineEdit> inputDicomDirectory_;
    QPointer<QPushButton> btnDicomBrowse_;
    QPointer<QPushButton> btnDicomLoad_;
    QPointer<QLabel> dicomStatusLabel_;
    QPointer<QDialog> docDialog_;

};

