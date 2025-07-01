#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fontconverter.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    font_converter = new FontConverter(this);
}

MainWindow::~MainWindow()
{
    delete font_converter;
    delete ui;
}

/*
//! Битмапы шрифта 15x16.
static const font_bitmap_t font_arial_15x16_bitmaps[] = {
    make_font_bitmap_descrs(FONT_ARIAL_15X16_PART0_FIRST_CHAR, FONT_ARIAL_15X16_PART0_LAST_CHAR, font_arial_15x16_part0_data, FONT_ARIAL_15X16_PART0_WIDTH, FONT_ARIAL_15X16_PART0_HEIGHT, FONT_ARIAL_15X16_PART0_GRAPHICS_FORMAT, font_arial_15x16_part0_descrs),
    make_font_bitmap_descrs(FONT_ARIAL_15X16_PART1_FIRST_CHAR, FONT_ARIAL_15X16_PART1_LAST_CHAR, font_arial_15x16_part1_data, FONT_ARIAL_15X16_PART1_WIDTH, FONT_ARIAL_15X16_PART1_HEIGHT, FONT_ARIAL_15X16_PART1_GRAPHICS_FORMAT, font_arial_15x16_part1_descrs)
};
//! Шрифт 15x16.
static font_t font_arial_15x16 = make_font_defchar(font_arial_15x16_bitmaps, 2, 15, 16, 1, 1, 127);
*/
void MainWindow::on_pbConvert_clicked()
{
    QStringList fontsFiles = QFileDialog::getOpenFileNames(this, tr("Open font files..."), QDir::currentPath(), "LCD Fonts (*.lcd);;All files (*.*)");
    if(fontsFiles.isEmpty()) return;
    QString resFile = QFileDialog::getSaveFileName(this, tr("Save C header..."), QDir::currentPath(), "C Header (*.h);;All files (*.*)");
    if(resFile.isEmpty()) return;

    ui->pbConvert->setEnabled(false);

    statusBar()->showMessage(tr("Converting..."), 5000);

    // Очистка с прошлого раза.
    font_converter->clear();
    // Файлы с интервалами символов.
    for(const auto &ff: qAsConst(fontsFiles)){
        font_converter->addFontInterval(ff, 0, UINT32_MAX);
    }
    /*font_converter->addFontInterval("droid_sans_32_127_33_37.lcd", 32, 127);
    font_converter->addFontInterval("droid_sans_176_186_33_37.lcd", 32, 127);
    font_converter->addFontInterval("droid_sans_1040_1105_33_37.lcd", 1040, 1105);*/

    // Пробел.
    //font_converter->addGlyphSizeOverride(32, QPoint(1, 2), QSize(5, 12));
    font_converter->addGlyphSizeOverride(32, QPoint(), QSize());

    // Байты горизонтально.
    font_converter->setByteLayout(FontConverter::ByteHorizontal);

    // Сделать немного магии.
    //font_converter->convert("font_droid_sans_33x37.h", "font_droid_sans_33x37");
    font_converter->convert(resFile, "font_droid_sans_33x37");

    ui->pbConvert->setEnabled(true);

    QMessageBox::information(this, tr("Conversion"), tr("Done!"));
}
