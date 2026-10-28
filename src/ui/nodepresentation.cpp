#include "nodepresentation.h"
#include "shared/colors.h"
#include "shared/strings.h"

namespace an {
NodePresentation::NodePresentation(Kind value)
    : kind(value), background(colors::NodeBackground), foreground(colors::NodeForeground), accent(colors::HostAccent) {
    portCaption = [](const Node &, bool input, unsigned) { return input ? QString(strings::InputLabel) : QString(strings::OutputLabel); };
}
} // namespace an
