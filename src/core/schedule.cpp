#include "shared/constants.h"
#include "shared/strings.h"
#include "schedule.h"
#include <QTimeZone>
#include <QRegularExpression>
#include <cmath>
#include <stdexcept>

namespace an {
namespace {
[[noreturn]] void invalid(const QString &message) { throw std::runtime_error(message.toStdString()); }
std::set<int> field(const QString &text, int low, int high, const QString &name) {
    std::set<int> values;
    const auto source = text.trimmed().isEmpty() ? QString(strings::Wildcard) : text.trimmed();
    const QRegularExpression number(strings::ScheduleNumberPattern);
    for (const auto &raw : source.split(',')) {
        const auto part = raw.trimmed();
        if (part == strings::Wildcard) { for (int i=low; i<=high; ++i) values.insert(i); }
        else if (part.startsWith(strings::StepPrefix)) {
            const auto stepText = part.mid(2); bool ok=false; const int step=stepText.toInt(&ok);
            if (!ok || step <= 0 || !number.match(stepText).hasMatch()) invalid(name + strings::InvalidScheduleStep);
            for (int i=low; i<=high; ++i) if (i % step == 0) values.insert(i);
        } else {
            bool ok=false; const int value=part.toInt(&ok);
            if (!ok || !number.match(part).hasMatch() || value<low || value>high) invalid(name + QString(strings::InvalidScheduleField).arg(low).arg(high));
            values.insert(value);
        }
    }
    if (values.empty()) invalid(name + strings::EmptyScheduleField);
    return values;
}
}
qint64 waitMilliseconds(const Node &node) {
    const qint64 factor = node.waitUnit == strings::Days ? time::MillisecondsPerDay : node.waitUnit == strings::Hours ? time::MillisecondsPerHour : node.waitUnit == strings::Minutes ? time::MillisecondsPerMinute : node.waitUnit == strings::Seconds ? time::MillisecondsPerSecond : 0;
    if (!factor || node.waitDuration < 0 || node.waitDuration > limits::WaitDuration) invalid(QString(strings::InvalidWaitUnitOrDuration).arg(limits::WaitDuration));
    return qint64(node.waitDuration) * factor;
}
Schedule::Schedule(const Node &node)
    : months_(field(node.scheduleMonth,calendar::FirstMonth,calendar::LastMonth,strings::MonthLabel)), days_(field(node.scheduleDay,calendar::FirstDay,calendar::LastDay,strings::DayLabel)),
      hours_(field(node.scheduleHour,calendar::FirstHour,calendar::LastHour,strings::HourLabel)), minutes_(field(node.scheduleMinute,calendar::FirstMinute,calendar::LastMinute,strings::MinuteLabel)) {
    for (int day : node.scheduleWeekdays) { if (day<calendar::FirstWeekday || day>calendar::LastWeekday) invalid(strings::InvalidWeekday); weekdays_.insert(day); }
}
bool Schedule::matchesDate(const QDate &date) const {
    return date.isValid() && months_.contains(date.month()) && days_.contains(date.day()) && (weekdays_.empty() || weekdays_.contains(date.dayOfWeek()));
}
QDateTime Schedule::next(const QDateTime &after) const {
    if (!after.isValid()) invalid(strings::InvalidScheduleBase);
    const auto zone=after.timeZone(); const auto limit=after.date().addYears(limits::ScheduleSearchYears);
    for (auto date=after.date(); date.isValid() && date<=limit; date=date.addDays(1)) {
        if (!matchesDate(date)) continue;
        // Try both nearby UTC offsets so repeated wall-clock minutes at DST's
        // end are handled, and reject nonexistent minutes at DST's start.
        std::set<int> offsets;
        for(int day : {-1,0,1}) offsets.insert(zone.offsetFromUtc(QDateTime(date.addDays(day),QTime(calendar::OffsetProbeHour,calendar::FirstMinute),QTimeZone::UTC)));
        QDateTime best;
        for(int hour:hours_) for(int minute:minutes_) {
            const QTime clock(hour,minute);
            const auto wall=QDateTime(date,clock,QTimeZone::UTC).toSecsSinceEpoch();
            for(int offset:offsets) {
                const auto candidate=QDateTime::fromSecsSinceEpoch(wall-offset,zone);
                if(candidate.date()==date && candidate.time()==clock && candidate>after && (!best.isValid() || candidate<best)) best=candidate;
            }
        }
        if(best.isValid()) return best;
    }
    invalid(strings::NoMatchingScheduleDate);
}
}
