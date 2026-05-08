#include "BudgetManager.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <vector>
#include <cmath>
#include <sys/stat.h>

BudgetManager::BudgetManager()
    : nextExpenseId(1), nextIncomeId(1), nextBillId(1)
{
    // NEW FEATURE 5: 10 predefined categories. Free-text categories are rejected.
    // Keeping this list closed makes addExpense, the progress bars, and the
    // Smart Finance Assistant deterministic — every category name is known up front.
    allowedCategories = {
        "Food",
        "Transportation",
        "Utilities",
        "Entertainment",
        "Health",
        "Education",
        "Shopping",
        "Housing",
        "Travel",
        "Other"
    };
    // Seed the hash map with each category at $0 budget.
    for (const std::string& c : allowedCategories) {
        categoryMap.set(c, CategoryBudget(0.0, 0.0));
    }
}

// ---------------- categories ----------------

void BudgetManager::addCategory(const std::string& name, double monthlyBudget) {
    setCategoryBudget(name, monthlyBudget);
}

void BudgetManager::setCategoryBudget(const std::string& name, double monthlyBudget) {
    if (!isValidCategory(name)) {
        std::cout << "  [error] '" << name << "' is not a predefined category.\n";
        return;
    }
    CategoryBudget* cb = categoryMap.get(name);
    if (cb) cb->budget = monthlyBudget;
    else    categoryMap.set(name, CategoryBudget(monthlyBudget, 0.0));
}

// ---------------- expense / income / bill ----------------

bool BudgetManager::addExpense(const Date& d, double amount, const std::string& category,
                               const std::string& description, const std::string& paymentMethod) {
    // NEW FEATURE 5: enforce predefined category.
    if (!isValidCategory(category)) {
        std::cout << "  [error] Unknown category '" << category
                  << "'. Use one of the predefined categories.\n";
        return false;
    }
    if (amount <= 0) {
        std::cout << "  [error] Expense amount must be positive.\n";
        return false;
    }

    int id = nextExpenseId++;
    Expense e(id, d, amount, category, description, paymentMethod);
    expenses.pushBack(e);
    expensesByDate.insert(d, id);

    CategoryBudget* cb = categoryMap.get(category);
    if (cb) cb->spent += amount;

    undoStack.push(UndoAction(UndoAction::Kind::AddExpense, id));

    // Threshold alert (proposal: warn at 80% / 100%).
    if (cb && cb->budget > 0) {
        double pct = (cb->spent / cb->budget) * 100.0;
        if (pct >= 100.0) {
            std::cout << "  [ALERT] " << category << " is OVER budget ("
                      << std::fixed << std::setprecision(1) << pct << "%).\n";
        } else if (pct >= 80.0) {
            std::cout << "  [warn]  " << category << " is at "
                      << std::fixed << std::setprecision(1) << pct << "% of budget.\n";
        }
    }
    return true;
}

void BudgetManager::addIncome(const Date& d, double amount, const std::string& category,
                              const std::string& description, const std::string& source) {
    int id = nextIncomeId++;
    Income i(id, d, amount, category, description, source);
    incomes.pushBack(i);
    undoStack.push(UndoAction(UndoAction::Kind::AddIncome, id));
}

void BudgetManager::addBill(const std::string& name, double amount, const Date& dueDate) {
    int id = nextBillId++;
    Bill b(id, name, amount, dueDate, false);
    bills.pushBack(b);
    reminders.addReminder(b);
    undoStack.push(UndoAction(UndoAction::Kind::AddBill, id));
}

bool BudgetManager::deleteExpense(int id) {
    Expense* found = expenses.find([&](const Expense& e){ return e.getTransactionId() == id; });
    if (!found) return false;
    // refund category total before removing
    CategoryBudget* cb = categoryMap.get(found->getCategory());
    if (cb) cb->spent -= found->getAmount();
    return expenses.removeIf([&](const Expense& e){ return e.getTransactionId() == id; });
}

bool BudgetManager::deleteIncome(int id) {
    return incomes.removeIf([&](const Income& i){ return i.getTransactionId() == id; });
}

bool BudgetManager::deleteBill(int id) {
    bool removed = bills.removeIf([&](const Bill& b){ return b.getBillId() == id; });
    if (removed) refreshReminders();
    return removed;
}

void BudgetManager::viewExpenses() {
    std::cout << "\n-- Expenses --\n";
    if (expenses.isEmpty()) { std::cout << "  (none)\n"; return; }
    expenses.forEach([](const Expense& e){ e.displayTransaction(); });
}

void BudgetManager::viewIncomes() {
    std::cout << "\n-- Incomes --\n";
    if (incomes.isEmpty()) { std::cout << "  (none)\n"; return; }
    incomes.forEach([](const Income& i){ i.displayTransaction(); });
}

void BudgetManager::viewBills() {
    std::cout << "\n-- Bills --\n";
    if (bills.isEmpty()) { std::cout << "  (none)\n"; return; }
    bills.forEach([](const Bill& b){ b.displayBill(); });
}

void BudgetManager::searchTransaction(int id) {
    Expense* e = expenses.find([&](const Expense& x){ return x.getTransactionId() == id; });
    if (e) { std::cout << "Found expense:\n"; e->displayTransaction(); return; }
    Income* i = incomes.find([&](const Income& x){ return x.getTransactionId() == id; });
    if (i) { std::cout << "Found income:\n"; i->displayTransaction(); return; }
    std::cout << "  No transaction with id " << id << "\n";
}

bool BudgetManager::markBillPaid(int billId) {
    Bill* b = bills.find([&](const Bill& x){ return x.getBillId() == billId; });
    if (!b) return false;
    b->markPaid();
    refreshReminders();
    undoStack.push(UndoAction(UndoAction::Kind::MarkPaid, billId));
    return true;
}

void BudgetManager::viewExpensesInRange(const Date& from, const Date& to) {
    auto pairs = expensesByDate.range(from, to);
    std::cout << "\n-- Expenses from " << from.toString()
              << " to " << to.toString() << " --\n";
    if (pairs.empty()) { std::cout << "  (none)\n"; return; }
    for (auto& kv : pairs) {
        int id = kv.second;
        Expense* e = expenses.find([&](const Expense& x){ return x.getTransactionId() == id; });
        if (e) e->displayTransaction();
    }
}

// ---------------- original income/expense summary ----------------

void BudgetManager::showMonthlySummary() {
    double totalIncome = 0.0;
    double totalExpense = 0.0;
    incomes.forEach([&](const Income& i){ totalIncome += i.getAmount(); });
    expenses.forEach([&](const Expense& e){ totalExpense += e.getAmount(); });

    double balance = totalIncome - totalExpense;
    std::cout << "\n-- Income vs Expense Summary --\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Total Income:   $" << totalIncome  << "\n";
    std::cout << "  Total Expense:  $" << totalExpense << "\n";
    std::cout << "  Net Balance:    $" << balance      << "\n";
}

// ---------------- undo ----------------

void BudgetManager::undoLastAction() {
    UndoAction a;
    if (!undoStack.pop(a)) {
        std::cout << "  No action to undo.\n";
        return;
    }
    switch (a.kind) {
        case UndoAction::Kind::AddExpense:
            if (deleteExpense(a.recordId))
                std::cout << "  Undone: removed expense " << a.recordId << "\n";
            break;
        case UndoAction::Kind::AddIncome:
            if (deleteIncome(a.recordId))
                std::cout << "  Undone: removed income " << a.recordId << "\n";
            break;
        case UndoAction::Kind::AddBill:
            if (deleteBill(a.recordId))
                std::cout << "  Undone: removed bill " << a.recordId << "\n";
            break;
        case UndoAction::Kind::MarkPaid: {
            Bill* b = bills.find([&](const Bill& x){ return x.getBillId() == a.recordId; });
            if (b) {
                // No real "unpay" semantically, but we restore the unpaid state.
                Bill restored(b->getBillId(), b->getBillName(), b->getAmount(),
                              b->getDueDate(), false);
                bills.removeIf([&](const Bill& x){ return x.getBillId() == a.recordId; });
                bills.pushBack(restored);
                refreshReminders();
                std::cout << "  Undone: bill " << a.recordId << " marked unpaid again.\n";
            }
            break;
        }
    }
}

ReminderSystem&       BudgetManager::getReminders()       { return reminders; }
const ReminderSystem& BudgetManager::getReminders() const { return reminders; }

void BudgetManager::refreshReminders() {
    std::vector<Bill> all;
    bills.forEach([&](const Bill& b){ all.push_back(b); });
    reminders.rebuildFromBills(all);
}

// =================================================================
// NEW FEATURE 1: Monthly Summary Report
// =================================================================

double BudgetManager::getTotalBudget() const {
    double total = 0.0;
    auto& m = const_cast<HashMap<CategoryBudget>&>(categoryMap);
    m.forEach([&](const std::string&, CategoryBudget& cb){ total += cb.budget; });
    return total;
}

double BudgetManager::getTotalSpent() const {
    double total = 0.0;
    auto& m = const_cast<HashMap<CategoryBudget>&>(categoryMap);
    m.forEach([&](const std::string&, CategoryBudget& cb){ total += cb.spent; });
    return total;
}

double BudgetManager::getRemainingBalance() const {
    return getTotalBudget() - getTotalSpent();
}

std::string BudgetManager::getMonthlyBudgetStatus() const {
    double total = getTotalBudget();
    double spent = getTotalSpent();
    if (total <= 0.0) return "No Budget Set";
    if (spent >= total)         return "Over Budget";
    if (spent >= 0.8 * total)   return "Approaching Limit";
    return "Under Budget";
}

void BudgetManager::showMonthlySummaryReport() const {
    double total = getTotalBudget();
    double spent = getTotalSpent();
    double remaining = total - spent;
    std::cout << "\n========== Monthly Summary Report ==========\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Total Budget:       $" << total     << "\n";
    std::cout << "  Total Spent:        $" << spent     << "\n";
    std::cout << "  Remaining Balance:  $" << remaining << "\n";
    std::cout << "  Status:             " << getMonthlyBudgetStatus() << "\n";
    std::cout << "============================================\n";
}

// =================================================================
// NEW FEATURE 3: Budget Progress Bars
// =================================================================

double BudgetManager::getCategoryUsagePercent(const std::string& category) const {
    const CategoryBudget* cb = categoryMap.get(category);
    if (!cb || cb->budget <= 0.0) return 0.0;
    return (cb->spent / cb->budget) * 100.0;
}

std::string BudgetManager::renderProgressBar(double percent, int width) const {
    int displayed = static_cast<int>(std::round(percent / 100.0 * width));
    if (displayed < 0)     displayed = 0;
    if (displayed > width) displayed = width;
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < displayed;       ++i) oss << "#";
    for (int i = 0; i < width - displayed; ++i) oss << "-";
    oss << "]";
    return oss.str();
}

void BudgetManager::showAllProgressBars() const {
    std::cout << "\n-- Budget Progress --\n";
    auto& m = const_cast<HashMap<CategoryBudget>&>(categoryMap);
    m.forEach([&](const std::string& name, CategoryBudget& cb){
        if (cb.budget <= 0.0 && cb.spent <= 0.0) return; // skip untouched
        double pct = (cb.budget > 0.0) ? (cb.spent / cb.budget * 100.0) : 0.0;
        std::cout << "  " << std::left << std::setw(14) << name
                  << " " << renderProgressBar(pct, 20)
                  << " " << std::fixed << std::setprecision(0) << pct << "%"
                  << "  ($" << std::fixed << std::setprecision(2) << cb.spent
                  << "/$"   << cb.budget << ")";
        if (pct >= 100.0)      std::cout << "  OVER";
        else if (pct >= 80.0)  std::cout << "  WARN";
        std::cout << "\n";
    });
}

// =================================================================
// NEW FEATURE 5: Fixed Category Selection
// =================================================================

const std::vector<std::string>& BudgetManager::getCategoryList() const {
    return allowedCategories;
}

bool BudgetManager::isValidCategory(const std::string& category) const {
    for (const std::string& c : allowedCategories) {
        if (c == category) return true;
    }
    return false;
}

std::string BudgetManager::promptCategorySelection() const {
    std::cout << "Pick a category:\n";
    for (size_t i = 0; i < allowedCategories.size(); ++i) {
        std::cout << "  " << (i + 1) << ") " << allowedCategories[i] << "\n";
    }
    std::cout << "Enter number: ";
    int choice = 0;
    if (!(std::cin >> choice)) {
        std::cin.clear();
        std::string junk; std::getline(std::cin, junk);
        return "Other";
    }
    std::string junk; std::getline(std::cin, junk);
    if (choice < 1 || choice > static_cast<int>(allowedCategories.size())) {
        std::cout << "  [warn] invalid selection, defaulting to Other\n";
        return "Other";
    }
    return allowedCategories[choice - 1];
}

bool BudgetManager::getCategorySnapshot(const std::string& category,
                                        double& budget, double& spent, double& percent) const {
    const CategoryBudget* cb = categoryMap.get(category);
    if (!cb) return false;
    budget  = cb->budget;
    spent   = cb->spent;
    percent = (cb->budget > 0.0) ? (cb->spent / cb->budget * 100.0) : 0.0;
    return true;
}

// =================================================================
// GUI helper methods
// =================================================================

std::vector<Category> BudgetManager::getAllCategories() const {
    std::vector<Category> result;
    result.reserve(allowedCategories.size());
    for (const std::string& name : allowedCategories) {
        const CategoryBudget* cb = categoryMap.get(name);
        if (cb) result.emplace_back(name, cb->budget, cb->spent);
        else    result.emplace_back(name, 0.0, 0.0);
    }
    return result;
}

std::vector<Expense> BudgetManager::getExpensesAll() const {
    std::vector<Expense> result;
    expenses.forEach([&](const Expense& e){ result.push_back(e); });
    return result;
}

std::vector<Income> BudgetManager::getIncomesAll() const {
    std::vector<Income> result;
    incomes.forEach([&](const Income& i){ result.push_back(i); });
    return result;
}

std::vector<Bill> BudgetManager::getBillsAll() const {
    std::vector<Bill> result;
    bills.forEach([&](const Bill& b){ result.push_back(b); });
    return result;
}

double BudgetManager::getTotalIncome() const {
    double total = 0.0;
    incomes.forEach([&](const Income& i){ total += i.getAmount(); });
    return total;
}

// =================================================================
// Persistence
// =================================================================

void BudgetManager::clear() {
    expenses.clear();
    incomes.clear();
    bills.clear();
    undoStack.clear();
    expensesByDate.clear();
    for (const std::string& c : allowedCategories)
        categoryMap.set(c, CategoryBudget(0.0, 0.0));
    nextExpenseId = nextIncomeId = nextBillId = 1;
    refreshReminders();
}

void BudgetManager::saveToDir(const std::string& dir) const {
    mkdir(dir.c_str(), 0755);

    // expenses
    {
        std::ofstream f(dir + "/expenses.csv");
        f << std::fixed << std::setprecision(2);
        expenses.forEach([&](const Expense& e) {
            f << e.getTransactionId() << "\t"
              << e.getDate().toString()  << "\t"
              << e.getAmount()           << "\t"
              << e.getCategory()         << "\t"
              << e.getDescription()      << "\t"
              << e.getPaymentMethod()    << "\n";
        });
    }
    // incomes
    {
        std::ofstream f(dir + "/incomes.csv");
        f << std::fixed << std::setprecision(2);
        incomes.forEach([&](const Income& i) {
            f << i.getTransactionId() << "\t"
              << i.getDate().toString()  << "\t"
              << i.getAmount()           << "\t"
              << i.getCategory()         << "\t"
              << i.getDescription()      << "\t"
              << i.getSource()           << "\n";
        });
    }
    // bills
    {
        std::ofstream f(dir + "/bills.csv");
        f << std::fixed << std::setprecision(2);
        bills.forEach([&](const Bill& b) {
            f << b.getBillId()            << "\t"
              << b.getBillName()          << "\t"
              << b.getAmount()            << "\t"
              << b.getDueDate().toString()<< "\t"
              << (b.getIsPaid() ? 1 : 0) << "\n";
        });
    }
    // category budgets
    {
        std::ofstream f(dir + "/budgets.csv");
        f << std::fixed << std::setprecision(2);
        auto& m = const_cast<HashMap<CategoryBudget>&>(categoryMap);
        m.forEach([&](const std::string& name, CategoryBudget& cb) {
            f << name << "\t" << cb.budget << "\n";
        });
    }
}

void BudgetManager::loadFromDir(const std::string& dir) {
    clear();

    auto splitLine = [](const std::string& line, std::vector<std::string>& out) {
        out.clear();
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) out.push_back(tok);
    };
    std::vector<std::string> f;

    // budgets first so spent values accumulate correctly
    {
        std::ifstream in(dir + "/budgets.csv");
        std::string line;
        while (std::getline(in, line)) {
            splitLine(line, f);
            if (f.size() < 2) continue;
            if (isValidCategory(f[0])) setCategoryBudget(f[0], std::stod(f[1]));
        }
    }
    // expenses
    {
        std::ifstream in(dir + "/expenses.csv");
        std::string line;
        while (std::getline(in, line)) {
            splitLine(line, f);
            if (f.size() < 6) continue;
            int id = std::stoi(f[0]);
            Date d; if (!Date::tryParse(f[1], d)) continue;
            double amt = std::stod(f[2]);
            Expense e(id, d, amt, f[3], f[4], f[5]);
            expenses.pushBack(e);
            expensesByDate.insert(d, id);
            CategoryBudget* cb = categoryMap.get(f[3]);
            if (cb) cb->spent += amt;
            if (id >= nextExpenseId) nextExpenseId = id + 1;
        }
    }
    // incomes
    {
        std::ifstream in(dir + "/incomes.csv");
        std::string line;
        while (std::getline(in, line)) {
            splitLine(line, f);
            if (f.size() < 6) continue;
            int id = std::stoi(f[0]);
            Date d; if (!Date::tryParse(f[1], d)) continue;
            double amt = std::stod(f[2]);
            Income i(id, d, amt, f[3], f[4], f[5]);
            incomes.pushBack(i);
            if (id >= nextIncomeId) nextIncomeId = id + 1;
        }
    }
    // bills
    {
        std::ifstream in(dir + "/bills.csv");
        std::string line;
        while (std::getline(in, line)) {
            splitLine(line, f);
            if (f.size() < 5) continue;
            int id = std::stoi(f[0]);
            Date d; if (!Date::tryParse(f[3], d)) continue;
            double amt = std::stod(f[2]);
            bool paid = (f[4] == "1");
            Bill b(id, f[1], amt, d, paid);
            bills.pushBack(b);
            if (id >= nextBillId) nextBillId = id + 1;
        }
    }
    refreshReminders();
}
