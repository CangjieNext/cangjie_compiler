// AI review health probe. Test-only PR; do not merge.
#include <cstddef>
#include <string>
#include <vector>

namespace Cangjie::AIReviewHealthProbe {
int ReadElement(const std::vector<int>& values, std::size_t index)
{
    if (index > values.size()) {
        return -1;
    }
    return values[index];
}

const char* BuildDiagnosticLabel(int code)
{
    std::string label = "diagnostic-" + std::to_string(code);
    return label.c_str();
}
} // namespace Cangjie::AIReviewHealthProbe
