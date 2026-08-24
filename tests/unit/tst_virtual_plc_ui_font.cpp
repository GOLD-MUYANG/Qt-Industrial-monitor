#include <QtTest>

#include <QRawFont>

#include "UiFont.h"
#include "VirtualPlcWindow.h"

class VirtualPlcUiFontTest final : public QObject
{
    Q_OBJECT

private slots:
    void windowFontSupportsChineseGlyphs();
};

void VirtualPlcUiFontTest::windowFontSupportsChineseGlyphs()
{
    QVERIFY(configureChineseUiFont());
    VirtualPlcWindow window;
    const QRawFont rawFont = QRawFont::fromFont(window.font());

    QVERIFY(rawFont.isValid());
    QVERIFY2(rawFont.supportsCharacter(QChar(u'测')),
             qPrintable(QStringLiteral("字体 %1 不包含中文字形")
                            .arg(window.font().family())));
}

QTEST_MAIN(VirtualPlcUiFontTest)

#include "tst_virtual_plc_ui_font.moc"
