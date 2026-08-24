#include "VirtualPlcWindow.h"
#include "UiFont.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("VirtualPLC"));
    QApplication::setOrganizationName(QStringLiteral("Muyang"));
    configureChineseUiFont();

    VirtualPlcWindow window;
    window.show();
    window.startDefaultServer();

    return application.exec();
}
