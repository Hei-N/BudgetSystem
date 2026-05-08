#ifndef CATEGORY_H
#define CATEGORY_H

#include <string>

// Represents a budget category with its limit and current spend.
// Returned by BudgetManager::getAllCategories() for GUI display.
class Category {
public:
    std::string name;
    double      budgetLimit;
    double      totalSpent;

    Category();
    Category(const std::string& n, double limit, double spent);

    double      getPercentUsed() const;
    double      getRemaining()   const;
    bool        isOverBudget()   const;
    const char* statusLabel()    const; // "OK" | "WARN" | "OVER" | "NO LIMIT"
};

#endif
