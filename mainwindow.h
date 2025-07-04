#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class FontConverter;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pbConvert_clicked();

private:
    Ui::MainWindow *ui;
    FontConverter* font_converter;
};

#endif // MAINWINDOW_H
