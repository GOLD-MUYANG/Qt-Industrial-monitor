#include "UiFont.h"

#include <QApplication>
#include <QFontDatabase>
#include <QStringList>

// Q_INIT_RESOURCE 生成的符号位于全局作用域，本函数也必须保持全局作用域。
static void initializeChineseFontResources()
{
    Q_INIT_RESOURCE(ChineseFonts);
}

namespace {

struct FontState
{
    bool attempted = false;
    QString uiFamily;
};

FontState &fontState()
{
    static FontState state;
    return state;
}

} // namespace

bool configureChineseUiFont()
{
    FontState &state = fontState();
    if (state.attempted) {
        return !state.uiFamily.isEmpty();
    }
    state.attempted = true;

    initializeChineseFontResources();
    const int fontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/wqy-microhei.ttc"));
    if (fontId < 0) {
        qWarning("无法加载内嵌中文 UI 字体");
        return false;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        qWarning("内嵌中文 UI 字体没有暴露字体族");
        return false;
    }

    // 字体集合同时包含普通和等宽字体，窗口控件优先使用普通字体。
    for (const QString &family : families) {
        if (!family.contains(QStringLiteral("Mono"), Qt::CaseInsensitive)) {
            state.uiFamily = family;
            break;
        }
    }
    if (state.uiFamily.isEmpty()) {
        state.uiFamily = families.constFirst();
    }

    QFont applicationFont = QApplication::font();
    applicationFont.setFamily(state.uiFamily);
    QApplication::setFont(applicationFont);
    return true;
}
