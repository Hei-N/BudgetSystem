#ifndef FINANCE_ASSISTANT_H
#define FINANCE_ASSISTANT_H

#include <string>
#include "BudgetManager.h"

// NEW FEATURE 4: Smart Finance Assistant.
//
// A small rule-based natural-language layer that classifies a free-form question
// into one of a few intents and dispatches to existing BudgetManager / ReminderSystem
// methods. It does NOT call any external service — every answer is computed from
// in-memory data.
class FinanceAssistant {
public:
    enum class Intent {
        SpendThisMonth,
        NextBill,
        OverdueBills,
        CategoryLookup,
        Help,
        Unknown
    };

    explicit FinanceAssistant(BudgetManager& m);

    // Main entry point. Returns a printable answer string.
    std::string ask(const std::string& question) const;

    // Exposed for unit tests.
    Intent classify(const std::string& question, std::string& detectedCategory) const;

private:
    BudgetManager& manager;

    static std::string normalize(const std::string& s);
    static bool        contains(const std::string& haystack, const std::string& needle);

    std::string handleSpendThisMonth() const;
    std::string handleNextBill() const;
    std::string handleOverdueBills() const;
    std::string handleCategoryLookup(const std::string& cat) const;
    std::string handleHelp() const;
};

#endif
