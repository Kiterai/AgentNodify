#pragma once
#include <QLockFile>

namespace an {
// Keep this object alive until shutdown, including while the window is hidden.
class InstanceLock {
public:
    explicit InstanceLock(const QString &path) : lock_(path) {
        // A long-running flow is not a stale instance. QLockFile still detects
        // and removes locks belonging to dead processes on the local machine.
        lock_.setStaleLockTime(0);
    }
    bool acquire() { return lock_.tryLock(0); }
    QLockFile::LockError error() const { return lock_.error(); }
private:
    QLockFile lock_;
};
}
