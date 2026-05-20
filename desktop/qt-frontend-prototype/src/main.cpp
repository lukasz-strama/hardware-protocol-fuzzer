#include <QApplication>

#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Hardware Protocol Fuzzer Desktop");
    QApplication::setOrganizationName("hardware-protocol-fuzzer");

    MainWindow window;
    window.resize(1240, 760);
    window.show();

    return app.exec();
}
