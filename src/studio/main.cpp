#include "studio.hpp"
#include "theme.hpp"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    applyStudioTheme(app);

    MainWindow window;
    window.show();

    return app.exec();
}
