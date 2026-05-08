
#include <string>
#include <iostream>
#include <iomanip>

// ============================================================
// CategoryInfo — stored as the VALUE in the HashMap
// Maps a category name to its budget limit and total spent.
// ============================================================
struct CategoryInfo {
    std::string name;
    double      budgetLimit;  // 0.0 = no limit set
    double      totalSpent;

    CategoryInfo() : budgetLimit(0.0), totalSpent(0.0) {}
    CategoryInfo(std::string n, double limit = 0.0)
        : name(n), budgetLimit(limit), totalSpent(0.0) {}

    // Add an expense amount to this category's total
    void addExpense(double amount) { totalSpent += amount; }

    // Subtract when an expense is deleted or undone
    void removeExpense(double amount) {
        totalSpent -= amount;
        if (totalSpent < 0) totalSpent = 0;
    }

    double getRemainingBudget() const {
        if (budgetLimit <= 0) return -1.0; // no limit set
        return budgetLimit - totalSpent;
    }

    bool isOverBudget() const {
        return budgetLimit > 0 && totalSpent > budgetLimit;
    }

    double getPercentUsed() const {
        if (budgetLimit <= 0) return 0.0;
        return (totalSpent / budgetLimit) * 100.0;
    }

    void display() const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  [" << name << "]"
                  << "  Spent: $" << totalSpent;
        if (budgetLimit > 0) {
            std::cout << "  /  Limit: $" << budgetLimit
                      << "  (" << (int)getPercentUsed() << "%)";
            if (isOverBudget())
                std::cout << "  *** OVER BUDGET ***";
        } else {
            std::cout << "  (no limit set)";
        }
        std::cout << "\n";
    }
};