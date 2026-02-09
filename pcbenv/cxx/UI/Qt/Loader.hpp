#include "UserSettings.hpp"
#include <QtUiTools>

inline QWidget *LoadUIFile(QWidget *target, const char *name)
{
    auto filePath = UserSettings::get().Paths.QTUI + name;
    QFile file(filePath.c_str());
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    return QUiLoader().load(&file, target);
}
