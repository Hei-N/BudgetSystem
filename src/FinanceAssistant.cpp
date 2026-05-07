#include "FinanceAssistant.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

FinanceAssistant::FinanceAssistant(BudgetManager& m) : manager(m) {}

std::string FinanceAssistant::normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ') {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else {
            out.push_back(' ');
        }
    }
    // collapse runs of spaces
    std::string clean;
    bool inSpace = false;
    for (char c : out) {
        if (c == ' ') {
            if (!inSpace && !clean.empty()) clean.push_back(' ');
            inSpace = true;
        } else {
            clean.push_back(c);
            inSpace = false;
        }
    }
    if (!clean.empty() && clean.back() == ' ') clean.pop_back();
    return clean;
}

bool FinanceAssistant::contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

FinanceAssistant::Intent FinanceAssistant::classify(const std::string& question,
                                                    std::string& detectedCategory) const {
    std::string q = normalize(question);
    detectedCategory.clear();

    if (contains(q, "help") || contains(q, "what can you do")) return Intent::Help;

    if (contains(q, "overdue")) return Intent::OverdueBills;

    if (contains(q, "next bill") || contains(q, "bill due") ||
        contains(q, "when is my next") || contains(q, "upcoming bill")) {
        return Intent::NextBill;
    }

    if (contains(q, "spend") || contains(q, "spent") || contains(q, "expense")) {
        if (contains(q, "this month") || contains(q, "month") || contains(q, "total")) {
            return Intent::SpendThisMonth;
        }
    }

    // category lookup: detect a known category mentioned anywhere.
    for (const std::string& cat : manager.getCategoryList()) {
        std::string lower = cat;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (contains(q, lower)) {
            detectedCategory = cat;
            return Intent::CategoryLookup;
        }
    }

    return Intent::Unknown;
}

std::string FinanceAssistant::ask(const std::string& question) const {
    std::string cat;
    Intent i = classify(question, cat);
    switch (i) {
        case Intent::SpendThisMonth: return handleSpendThisMonth();
        case Intent::NextBill:       return handleNextBill();
        case Intent::OverdueBills:   return handleOverdueBills();
        case Intent::CategoryLookup: return handleCategoryLookup(cat);
        case Intent::Help:           return handleHelp();
        case Intent::Unknown:
        default:
            return "I don't understand that yet. Try \"help\" to see what I can answer.";
    }
}

std::string FinanceAssistant::handleSpendThisMonth() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "You have spent $" << manager.getTotalSpent()
        << " this month out of a $" << manager.getTotalBudget() << " budget."
        << " (" << manager.getMonthlyBudgetStatus() << ")";
    return oss.str();
}

std::string FinanceAssistant::handleNextBill() const {
    Bill b;
    if (!manager.getReminders().peekNextUnpaidBill(b)) {
        return "You have no upcoming unpaid bills.";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Your next bill is " << b.getBillName()
        << " for $" << b.getAmount()
        << ", due on " << b.getDueDate().toString() << ".";
    return oss.str();
}

std::string FinanceAssistant::handleOverdueBills() const {
    auto overdue = manager.getReminders().getOverdueBills(Date::today());
    if (overdue.empty()) return "Good news: you have no overdue bills.";
    std::ostringstream oss;
    oss << "You have " << overdue.size() << " overdue bill(s):\n";
    oss << std::fixed << std::setprecision(2);
    for (const Bill& b : overdue) {
        oss << "  - " << b.getBillName() << "  $" << b.getAmount()
            << "  (due " << b.getDueDate().toString() << ")\n";
    }
    return oss.str();
}

std::string FinanceAssistant::handleCategoryLookup(const std::string& cat) const {
    double budget = 0, spent = 0, pct = 0;
    if (!manager.getCategorySnapshot(cat, budget, spent, pct)) {
        return "I couldn't find data for category '" + cat + "'.";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << cat << " budget: $" << budget
        << "  spent: $" << spent
        << "  remaining: $" << (budget - spent)
        << "  " << manager.renderProgressBar(pct, 20)
        << " " << std::fixed << std::setprecision(0) << pct << "%";
    return oss.str();
}

std::string FinanceAssistant::handleHelp() const {
    return
        "I can answer:\n"
        "  - How much did I spend this month?\n"
        "  - When is my next bill due?\n"
        "  - Do I have overdue bills?\n"
        "  - Show my food budget. (or any predefined category)\n";
}
