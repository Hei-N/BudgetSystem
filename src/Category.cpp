#include "Category.h"

Category::Category() : budgetLimit(0.0), totalSpent(0.0) {}

Category::Category(const std::string& n, double limit, double spent)
    : name(n), budgetLimit(limit), totalSpent(spent) {}

double Category::getPercentUsed() const {
    if (budgetLimit <= 0.0) return 0.0;
    return (totalSpent / budgetLimit) * 100.0;
}

double Category::getRemaining() const {
    return budgetLimit - totalSpent;
}

bool Category::isOverBudget() const {
    return budgetLimit > 0.0 && totalSpent > budgetLimit;
}

const char* Category::statusLabel() const {
    double pct = getPercentUsed();
    if (pct >= 100.0)       return "OVER";
    if (pct >= 80.0)        return "WARN";
    if (budgetLimit <= 0.0) return "NO LIMIT";
    return "OK";
}
