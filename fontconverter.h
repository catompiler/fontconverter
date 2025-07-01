#ifndef FONTCONVERTER_H
#define FONTCONVERTER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QIODevice>
#include <stdint.h>
#include <QImage>
#include <QMap>
#include <QHash>
#include <QSize>
#include <QPoint>


class QFile;
class QXmlStreamReader;


class FontConverter : public QObject
{
    Q_OBJECT
public:

    /**
     * @brief Перечисление расположения байта.
     */
    enum ByteLayout { ByteVertical, ByteHorizontal };

    explicit FontConverter(QObject *parent = 0);
    ~FontConverter();

    /**
     * @brief Устанавливает расположение байта.
     * @param layout Расположение байта.
     */
    void setByteLayout(ByteLayout layout);

    /**
     * @brief Очищает все добавленные данные.
     */
    void clear();

    /**
     * @brief Добавляет файл шрифта для преобразования.
     * @param fileName Имя файла шрифта.
     * @param firstChar Начальный символ.
     * @param lastChar Конечный символ.
     */
    void addFontInterval(const QString& fileName, uint32_t firstChar, uint32_t lastChar);

    /**
     * @brief Добавляет переопределение размера символа.
     * @param char_code Код символа.
     * @param pos Позиция символа.
     * @param size Размер символа.
     */
    void addGlyphSizeOverride(uint32_t char_code, const QPoint& pos, const QSize& size);

    /**
     * @brief Преобразует шрифт.
     * @param fileName Имя выходного файла.
     * @param fontName Имя шрифта.
     * @return Флаг успеха.
     */
    bool convert(const QString& fileName, const QString& fontName) const;

signals:

public slots:

private:

    /**
     * @brief Структура исходных данных для преобразования.
     */
    struct FontInput{

        FontInput(){
            fileIn = QString();
            firstChar = 0;
            lastChar = 0;
        }

        FontInput(const QString& file_in, uint32_t first_char, uint32_t last_char){
            fileIn = file_in;
            firstChar = first_char;
            lastChar = last_char;
        }

        FontInput(const FontInput& fi){
            fileIn = fi.fileIn;
            firstChar = fi.firstChar;
            lastChar = fi.lastChar;
        }

        ~FontInput(){}

        //! Имя файла шрифта.
        QString fileIn;
        //! Начальный символ.
        uint32_t firstChar;
        //! Конечный символ.
        uint32_t lastChar;
    };

    //! Входные данные.
    QList<FontInput>* inputs;

    /**
     * @brief Структура данных глифа шрифта.
     */
    struct GlyphData {

        GlyphData(const QImage& img){
            offset_x = 0;
            offset_y = 0;
            data = img;
        }

        GlyphData(){
            offset_x = 0;
            offset_y = 0;
            data = QImage();
        }

        GlyphData(const GlyphData& gd){
            offset_x = gd.offset_x;
            offset_y = gd.offset_y;
            data = gd.data;
        }

        ~GlyphData() {}

        //! Смещение для рисования по оси X.
        uint32_t offset_x;
        //! Смещение для рисования по оси Y.
        uint32_t offset_y;
        //! Изображение глифа.
        QImage data;
    };

    //! Тип списка глифов.
    typedef QMap<uint32_t, GlyphData> GlyphList;

    /**
     * @brief Данные шрифта для преобразования.
     */
    struct FontData {

        FontData(){
            char_width = 0;
            char_height = 0;
            bitmap_width = 0;
            bitmap_height = 0;
            glyphs = GlyphList();
        }

        FontData(const FontData& fd){
            char_width = fd.char_width;
            char_height = fd.char_height;
            bitmap_width = fd.bitmap_width;
            bitmap_height = fd.bitmap_height;
            glyphs = fd.glyphs;
        }

        ~FontData() {}

        //! Начальный символ.
        uint32_t char_from;
        //! Конечный символ.
        uint32_t char_to;
        //! Ширина символа.
        uint32_t char_width;
        //! Высота символа.
        uint32_t char_height;
        //! Ширина битовой карты.
        uint32_t bitmap_width;
        //! Высота битовой карты.
        uint32_t bitmap_height;
        //! Список глифов.
        GlyphList glyphs;
    };

    /**
     * @brief Структура данных переопределения размера символа.
     */
    struct GlyphSizeOverride {

        GlyphSizeOverride(){
            pos = QPoint();
            size = QSize();
        }

        GlyphSizeOverride(const QPoint& p, const QSize& s){
            pos = p;
            size = s;
        }

        GlyphSizeOverride(const GlyphSizeOverride& gso){
            pos = gso.pos;
            size = gso.size;
        }

        ~GlyphSizeOverride(){}

        //! Позиция символа.
        QPoint pos;
        //! Размер символа.
        QSize size;
    };

    //! Словарь переопределённых размеров символов.
    QHash<uint32_t, GlyphSizeOverride>* glyphOverrides;

    //! Расположение байт.
    ByteLayout byte_layout;

    bool convertInterval(const FontInput& fin, QList<FontData>* font_data_list) const;
    bool convertFont(QXmlStreamReader* xmlreader, const FontConverter::FontInput& fin, FontData* font_data) const;
    QImage pixelsStrToImage(const QString& pixelsStr, uint32_t width, uint32_t height) const;

    bool exportFont(QFile& outFile, const QString& fontName, QList<FontData>* font_data_list) const;
    uint32_t getPow2(uint32_t n) const;
    uint32_t getFract8(uint32_t n) const;
    uint8_t getImagePixel(const QImage& img, int x, int y) const;
    uint8_t getImageByte(const QImage& img, int x, int y, ByteLayout layout) const;
    void trimGlyph(uint32_t char_code, GlyphData& gd) const;
};

#endif // FONTCONVERTER_H
