#include "fontconverter.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QStringRef>
#include <QStringList>
#include <QColor>
#include <QPainter>
#include <algorithm>
#include <iterator>
#include <math.h>
#include <QTextStream>
#include <QChar>
#include <QDebug>


FontConverter::FontConverter(QObject *parent) : QObject(parent)
{
    inputs = new QList<FontInput>();
    glyphOverrides = new QHash<uint32_t, GlyphSizeOverride>();
    byte_layout = ByteVertical;
}

FontConverter::~FontConverter()
{
    delete glyphOverrides;
    delete inputs;
}

void FontConverter::setByteLayout(FontConverter::ByteLayout layout)
{
    byte_layout = layout;
}

void FontConverter::clear()
{
    inputs->clear();
    glyphOverrides->clear();
}

void FontConverter::addFontInterval(const QString& fileName, uint32_t firstChar, uint32_t lastChar)
{
    inputs->append(FontInput(fileName, firstChar, lastChar));
}

void FontConverter::addGlyphSizeOverride(uint32_t char_code, const QPoint& pos, const QSize& size)
{
    glyphOverrides->insert(char_code, GlyphSizeOverride(pos, size));
}

bool FontConverter::convert(const QString& fileName, const QString& fontName) const
{
    if(inputs->empty()){
        qDebug() << tr("Nothing to convert");
        return false;
    }

    QFile file(fileName);

    if(!file.open(QIODevice::WriteOnly)){
        qDebug() << tr("Error opening output file: %1").arg(fileName);
        return false;
    }

    QList<FontData> font_data_list;

    for(auto it: *inputs){
        if(!convertInterval(it, &font_data_list)){
            qDebug() << "Error reading font:" << it.fileIn;
            file.close();
            return false;
        }
    }

    {
        QList<FontData> font_data_list_sorted;
        while(!font_data_list.isEmpty()){
            auto& fd = font_data_list.first();

            auto it = std::lower_bound(font_data_list_sorted.begin(), font_data_list_sorted.end(), fd, [](const FontData& lfd, const FontData& rfd){
                return lfd.char_from < rfd.char_from;
            });

            font_data_list_sorted.insert(it, std::move(font_data_list.takeFirst()));
        }
        font_data_list.swap(font_data_list_sorted);
    }

    if(!exportFont(file, fontName, &font_data_list)){
        qDebug() << "Error exporting font!";
        file.close();
        return false;
    }

    file.close();

    return true;
}

bool FontConverter::convertInterval(const FontConverter::FontInput& fin, QList<FontData>* font_data_list) const
{
    QFile file(fin.fileIn);

    if(!file.open(QIODevice::ReadOnly)){
        qDebug() << tr("Error open input file: %1").arg(fin.fileIn);
        return false;
    }

    FontData font_data;

    QXmlStreamReader xmlreader(&file);

    while(!xmlreader.atEnd()){
        QXmlStreamReader::TokenType tokenType = xmlreader.readNext();

        switch (tokenType) {
        case QXmlStreamReader::StartElement:
            if(xmlreader.name() == QString("FONT")){
                if(!convertFont(&xmlreader, fin, &font_data)) return false;
                if(!font_data.glyphs.empty()){
                    font_data_list->append(font_data);
                }
            }
            break;
        default:
            break;
        }
    }

    file.close();

    return true;
}
bool FontConverter::convertFont(QXmlStreamReader* xmlreader, const FontConverter::FontInput& fin, FontData* font_data) const
{
    qDebug() << tr("Begin font reading");

    while(!xmlreader->atEnd()){
        QXmlStreamReader::TokenType tokenType = xmlreader->readNext();

        if(tokenType == QXmlStreamReader::StartElement){
            QString itemName = xmlreader->name().toString();

            if(itemName == "FONTSIZE"){
                font_data->char_width = xmlreader->attributes().value("WIDTH").toUInt();
                font_data->char_height = xmlreader->attributes().value("HEIGHT").toUInt();

                qDebug() << tr("Font size: %1x%2").arg(font_data->char_width).arg(font_data->char_height);
            }else if(itemName == "RANGE"){
                font_data->char_from = xmlreader->attributes().value("FROM").toUInt();
                font_data->char_to = xmlreader->attributes().value("TO").toUInt();

                qDebug() << tr("Font range: %1x%2").arg(font_data->char_from).arg(font_data->char_to);
            }else if(itemName == "CHAR"){
                uint32_t char_code = xmlreader->attributes().value("CODE").toUInt();

                if(char_code >= fin.firstChar && char_code <= fin.lastChar){

                    qDebug() << "Importing char" << char_code;

                    QString pixelsStr = xmlreader->attributes().value("PIXELS").toString();

                    QImage char_img = pixelsStrToImage(pixelsStr, font_data->char_width, font_data->char_height);

                    font_data->glyphs.insert(char_code, GlyphData(char_img));
                }
            }

        }else if(tokenType == QXmlStreamReader::EndElement){
            if(xmlreader->name() == QString("FONT")){
                break;
            }
        }
    }

    qDebug() << tr("End font reading");

    return true;
}

QImage FontConverter::pixelsStrToImage(const QString& pixelsStr, uint32_t width, uint32_t height) const
{
    QImage imgres(width, height, QImage::Format_MonoLSB);

    auto list = pixelsStr.split(",");

    uint32_t i_w = 0;
    uint32_t i_h = 0;

    for(QString& it: list){
        uint32_t int_color = it.toUInt();

        imgres.setPixel(i_w, i_h, int_color == 0 ? 1 : 0);
        if(++ i_h >= height){ i_h = 0; i_w ++; }
    }

    return imgres;
}

bool FontConverter::exportFont(QFile& outFile, const QString& fontName, QList<FontConverter::FontData>* font_data_list) const
{
    for(FontData& it: *font_data_list){

        it.bitmap_width = 0;
        it.bitmap_height = 0;

        for(GlyphList::iterator jt = it.glyphs.begin(); jt != it.glyphs.end(); ++ jt){
            trimGlyph(jt.key(), jt.value());

            if(jt.value().data.height() > static_cast<int>(it.bitmap_height)){
                it.bitmap_height = jt.value().data.height();
            }

            it.bitmap_width += jt.value().data.width();
        }

        //qDebug() << it.glyphs.firstKey() << it.glyphs.lastKey();
        //qDebug() << it.bitmap_width << it.bitmap_height;
    }

    QTextStream ts(&outFile);

    QString upFontName = fontName.toUpper();

    ts << "#ifndef " << upFontName << "_H\n";
    ts << "#define " << upFontName << "_H\n";

    ts << "\n";

    ts << "#include <stdint.h>\n";
    ts << "#include \"graphics/graphics.h\"\n";
    ts << "#include \"graphics/font.h\"\n";

    uint32_t max_char_width = 0;//font_data_list->first().char_width;
    uint32_t max_char_height = 0;//font_data_list->first().char_height;

    std::for_each(font_data_list->begin(), font_data_list->end(), [&max_char_width, &max_char_height](FontConverter::FontData& fd){
        if(max_char_width < fd.char_width) max_char_width = fd.char_width;
        if(max_char_height < fd.char_height) max_char_height = fd.char_height;
    });

    // Export general font data info.
    ts << Qt::endl;
    ts << "#define " << upFontName << "_BITMAPS_COUNT " << font_data_list->size() << Qt::endl;
    ts << "#define " << upFontName << "_MAX_CHAR_WIDTH " << max_char_width << Qt::endl;
    ts << "#define " << upFontName << "_MAX_CHAR_HEIGHT " << max_char_height << Qt::endl;
    ts << "#define " << upFontName << "_DEF_HSPACE " << 1 << Qt::endl;
    ts << "#define " << upFontName << "_DEF_VSPACE " << 0 << Qt::endl;
    ts << "#define " << upFontName << "_DEF_CHAR " << 127 << Qt::endl;

    int part_n = 0;

    int origin_width, origin_height;

    for(FontData& it: *font_data_list){

        ts << "\n\n";

        origin_width = 0;
        origin_height = 0;

        if(byte_layout == ByteVertical){
            origin_width = it.bitmap_width;
            origin_height = getFract8(it.bitmap_height);
        }else{
            origin_width = getFract8(it.bitmap_width);
            origin_height = it.bitmap_height;
        }

        ts << "#define " << upFontName << "_PART" << part_n << "_GRAPHICS_FORMAT "
           << ((byte_layout == ByteVertical) ? "GRAPHICS_FORMAT_BW_1_V" : "GRAPHICS_FORMAT_BW_1_H") << "\n";
        ts << "#define " << upFontName << "_PART" << part_n << "_WIDTH " << origin_width << "\n";
        ts << "#define " << upFontName << "_PART" << part_n << "_HEIGHT " << origin_height << "\n";
        ts << "#define " << upFontName << "_PART" << part_n << "_FIRST_CHAR " << it.glyphs.firstKey() << "\n";
        ts << "#define " << upFontName << "_PART" << part_n << "_LAST_CHAR " << it.glyphs.lastKey() << "\n";
        ts << "#define " << upFontName << "_PART" << part_n << "_CHAR_WIDTH " << it.char_width << "\n";
        ts << "#define " << upFontName << "_PART" << part_n << "_CHAR_HEIGHT " << it.char_height << "\n";

        ts << "\n";

        QImage bitmap_img(it.bitmap_width, it.bitmap_height, QImage::Format_MonoLSB);
        bitmap_img.fill(0);
        QPainter painter(&bitmap_img);

        int cur_x = 0;

        ts << "#define " << upFontName << "_PART" << part_n << "_DESCRS_COUNT "
           << it.glyphs.size() << "\n";
        ts << "static const font_char_descr_t " << fontName << "_part" << part_n << "_descrs"
           << "[" << upFontName << "_PART" << part_n << "_DESCRS_COUNT" << "] = {\n";

        for(GlyphList::iterator jt = it.glyphs.begin(); jt != it.glyphs.end(); ++ jt){

            ts << "    " << "{" << cur_x << ", " << 0 << ", "
               << jt.value().data.width() << ", " << jt.value().data.height() << ", "
               << jt.value().offset_x << ", " << jt.value().offset_y << "},"
               << " // " << jt.key() << "\n";

            painter.drawImage(cur_x, 0, jt.value().data);
            cur_x += jt.value().data.width();
        }

        ts << "};\n";

        ts << "\n";

        ts << "#define " << upFontName << "_PART" << part_n << "_DATA_SIZE "
           << origin_width * origin_height / 8 << "\n";
        ts << "static const uint8_t " << fontName << "_part" << part_n << "_data"
           << "[" << upFontName << "_PART" << part_n << "_DATA_SIZE" << "] = {\n";

        int line_len = 0;
        for(int y = 0; y < origin_height;){
            for(int x = 0; x < origin_width;){

                if(line_len == 0) ts << "    ";

                ts << QString("0x%1").arg(static_cast<unsigned int>(getImageByte(bitmap_img, x, y, byte_layout)), 2, 16, QChar('0'));

                if(++ line_len < 16){
                    ts << ", ";
                }else{
                    ts << ",\n";
                    line_len = 0;
                }

                if(byte_layout == ByteVertical){
                    x ++;
                }else{
                    x += 8;
                }
            }
            if(byte_layout == ByteVertical){
                y += 8;
            }else{
                y ++;
            }
        }

        if(line_len != 0) ts << "\n";

        ts << "};\n";

        //bitmap_img.save(tr("out_img%1.png").arg(part_n), "PNG");

        part_n ++;
    }


    // Export font declaration.
    ts << "\n\n/*" << Qt::endl;
    ts << "#include \"" << fontName << ".h" << "\"\n\n" << Qt::endl;

    ts << "// Font bitmaps: " << fontName << Qt::endl;
    ts << "static const font_bitmap_t " << fontName << "_bitmaps[] = {" << Qt::endl;
    int cur_part_n = 0;
    std::for_each(font_data_list->begin(), font_data_list->end(), [&ts, &cur_part_n, &fontName, &upFontName](FontConverter::FontData&){
        ts << "    make_font_bitmap_descrs("
           << QString("%1_PART%3_FIRST_CHAR, %1_PART%3_LAST_CHAR, %2_part%3_data, %1_PART%3_WIDTH, %1_PART%3_HEIGHT, %1_PART%3_GRAPHICS_FORMAT, %2_part%3_descrs)").arg(upFontName).arg(fontName).arg(cur_part_n) << "," << Qt::endl;
        cur_part_n ++;
    });
    ts << "};\n" << Qt::endl;

    ts << "// Font: " << fontName << Qt::endl;
    ts << "static font_t " << fontName
       << " = make_font_defchar("
       << "" << fontName << "_bitmaps, "
       << upFontName << "_BITMAPS_COUNT, "
       << upFontName << "_MAX_CHAR_WIDTH, "
       << upFontName << "_MAX_CHAR_HEIGHT, "
       << 0 << ", "
       << upFontName << "_DEF_VSPACE, "
       << upFontName << "_DEF_CHAR);" << Qt::endl;

    ts << "*/" << Qt::endl;

    ts << "\n\n#endif\t //" << fontName.toUpper() << "_H\n";

    return true;
}

uint32_t FontConverter::getPow2(uint32_t n) const
{
    return pow(2.0, ceil(log(n) / log(2.0)));
}

uint32_t FontConverter::getFract8(uint32_t n) const
{
    if((n & 0x7) == 0) return n;
    return (n & ~0x7) + 0x8;
}

uint8_t FontConverter::getImagePixel(const QImage& img, int x, int y) const
{
    if(x <  0) return 0;
    if(y <  0) return 0;
    if(x >= img.width())  return 0;
    if(y >= img.height()) return 0;

    return (img.pixelColor(x, y).value() != 0) ? 1 : 0;
}

uint8_t FontConverter::getImageByte(const QImage& img, int x, int y, ByteLayout layout) const
{
    uint8_t res = 0;
    uint8_t bit = 0;
    for(int i = 0; i < 8; i ++){
        bit = getImagePixel(img, x, y);
        res |= bit << i;

        if(layout == ByteVertical){
            y ++;
        }else{
            x ++;
        }
    }

    return res;
}

void FontConverter::trimGlyph(uint32_t char_code, FontConverter::GlyphData& gd) const
{
    int first_x = gd.data.width();
    int last_x = 0;
    int first_y = gd.data.height();
    int last_y = 0;

    if(glyphOverrides->contains(char_code)){
        GlyphSizeOverride& override = (*glyphOverrides)[char_code];

        if(override.size.isValid()){
            first_x = override.pos.x();
            first_y = override.pos.y();

            last_x = first_x + override.size.width() - 1;
            last_y = first_y + override.size.height() - 1;
        }
    }else{
        for(int x = 0; x < gd.data.width(); x ++){
            for(int y = 0; y < gd.data.height(); y ++){
                if(gd.data.pixelColor(x, y).value() != 0){
                    if(x < first_x) first_x = x;
                    if(x > last_x) last_x = x;
                    if(y < first_y) first_y = y;
                    if(y > last_y) last_y = y;
                }
            }
        }
    }

    gd.offset_x = first_x;
    gd.offset_y = first_y;
    gd.data = gd.data.copy(first_x, first_y, last_x - first_x + 1, last_y - first_y + 1);
}
