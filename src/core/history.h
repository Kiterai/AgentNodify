#pragma once
#include "runner.h"

namespace an {
void saveNodeExecution(const QString &directory, const NodeExecution &execution);
// A missing directory is an empty history. Damaged records are reported and skipped.
// Persisted running records are restored as interrupted, never as still running.
std::vector<NodeExecution> loadNodeExecutions(const QString &directory, QStringList *errors = nullptr);
}
