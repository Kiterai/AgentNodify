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
    const auto source = text.trimmed().isEmpty() ? QString("*") : text.trimmed();
    const QRegularExpression number("^[0-9]+$");
    for (const auto &raw : source.split(',')) {
        const auto part = raw.trimmed();
        if (part == "*") { for (int i=low; i<=high; ++i) values.insert(i); }
        else if (part.startsWith("*/")) {
            const auto stepText = part.mid(2); bool ok=false; const int step=stepText.toInt(&ok);
            if (!ok || step <= 0 || !number.match(stepText).hasMatch()) invalid(name + ": */nのnは正の整数を指定してください");
            for (int i=low; i<=high; ++i) if (i % step == 0) values.insert(i);
        } else {
            bool ok=false; const int value=part.toInt(&ok);
            if (!ok || !number.match(part).hasMatch() || value<low || value>high) invalid(name + QString(": %1〜%2の数値、カンマ区切り、*、*/nを指定してください").arg(low).arg(high));
            values.insert(value);
        }
    }
    if (values.empty()) invalid(name + ": 一致する値がありません");
    return values;
}
}
qint64 waitMilliseconds(const Node &node) {
    const qint64 factor = node.waitUnit == "days" ? 86400000 : node.waitUnit == "hours" ? 3600000 : node.waitUnit == "minutes" ? 60000 : node.waitUnit == "seconds" ? 1000 : 0;
    if (!factor || !std::isfinite(node.waitDuration) || node.waitDuration < 0 || node.waitDuration > 1000000) invalid("待機時間は0〜1000000、単位は日・時間・分・秒を指定してください");
    return qint64(std::ceil(node.waitDuration * factor));
}
Schedule::Schedule(const Node &node)
    : months_(field(node.scheduleMonth,1,12,"月")), days_(field(node.scheduleDay,1,31,"日")),
      hours_(field(node.scheduleHour,0,23,"時")), minutes_(field(node.scheduleMinute,0,59,"分")) {
    for (int day : node.scheduleWeekdays) { if (day<1 || day>7) invalid("曜日は月曜日〜日曜日を指定してください"); weekdays_.insert(day); }
}
bool Schedule::matchesDate(const QDate &date) const {
    return date.isValid() && months_.contains(date.month()) && days_.contains(date.day()) && (weekdays_.empty() || weekdays_.contains(date.dayOfWeek()));
}
bool Schedule::matches(const QDateTime &time) const {
    return time.isValid() && matchesDate(time.date()) && hours_.contains(time.time().hour()) && minutes_.contains(time.time().minute());
}
QDateTime Schedule::next(const QDateTime &after) const {
    if (!after.isValid()) invalid("スケジュールの基準日時が不正です");
    const auto zone=after.timeZone(); const auto limit=after.date().addYears(400);
    for (auto date=after.date(); date.isValid() && date<=limit; date=date.addDays(1)) {
        if (!matchesDate(date)) continue;
        // Try both nearby UTC offsets so repeated wall-clock minutes at DST's
        // end are handled, and reject nonexistent minutes at DST's start.
        std::set<int> offsets;
        for(int day : {-1,0,1}) offsets.insert(zone.offsetFromUtc(QDateTime(date.addDays(day),QTime(12,0),QTimeZone::UTC)));
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
    invalid("スケジュール条件に一致する日時がありません（月・日・曜日の組み合わせを確認してください）");
}
}
