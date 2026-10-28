#pragma once
#include <QString>

namespace an {
// Searches both the inherited PATH and (on Windows) the current registry PATH.
QString resolveExecutable(const QString &program);
}
