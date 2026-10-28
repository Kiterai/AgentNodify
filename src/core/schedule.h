#pragma once
#include "flow.h"
#include <QDateTime>
#include <set>

namespace an {
qint64 waitMilliseconds(const Node &node);
// The requested cron subset uses multiples for */n, and ANDs all conditions.
class Schedule {
public:
    explicit Schedule(const Node &node);
    QDateTime next(const QDateTime &after) const; // Strictly future minute boundary.
private:
    std::set<int> months_, days_, hours_, minutes_, weekdays_;
    bool matchesDate(const QDate &date) const;
};
}
