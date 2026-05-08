#ifndef BUDGET_MANAGER_H
#define BUDGET_MANAGER_H

#include <string>
#include <vector>

#include "Category.h"
#include "LinkedList.h"
#include "Stack.h"
#include "HashMap.h"
#include "BST.h"
#include "Expense.h"
#include "Income.h"
#include "Bill.h"
#include "ReminderSystem.h"

// Each category tracked by BudgetManager has a monthly budget and a running spent total.
struct CategoryBudget {
    double budget;
    double spent;
    CategoryBudget() : budget(0.0), spent(0.0) {}
    CategoryBudget(double b, double s) : budget(b), spent(s) {}
};

// Action recorded on the undo stack so undoLastAction() can reverse it.
struct UndoAction {
    enum class Kind { AddExpense, AddIncome, AddBill, MarkPaid };
    Kind kind;
    int  recordId;
    UndoAction() : kind(Kind::AddExpense), recordId(0) {}
    UndoAction(Kind k, int id) : kind(k), recordId(id) {}
};

// BudgetManager — sole controller of the system.
//
// Storage:
//   * LinkedList<Expense>, LinkedList<Income>, LinkedList<Bill>  (design doc)
//   * Stack<UndoAction>                                          (design doc)
//   * HashMap<CategoryBudget>                                    (proposal)
//   * BST<Date, int>  -> expenseId, indexes expenses by date     (proposal)
//   * ReminderSystem (which itself uses Queue + MinHeap)
class BudgetManager {
private:
    LinkedList<Expense> expenses;
    LinkedList<Income>  incomes;
    LinkedList<Bill>    bills;
    Stack<UndoAction>   undoStack;

    HashMap<CategoryBudget> categoryMap;     // category -> {budget, spent}
    BST<Date, int>          expensesByDate;  // date     -> expenseId

    ReminderSystem reminders;

    int nextExpenseId;
    int nextIncomeId;
    int nextBillId;

    // NEW FEATURE 5: predefined category list to prevent typos.
    std::vector<std::string> allowedCategories;

    // helper: refresh the reminder system from the current bill list.
    void refreshReminders();

public:
    BudgetManager();

    // ===== Original interface =====
    // Categories
    void addCategory(const std::string& name, double monthlyBudget);
    void setCategoryBudget(const std::string& name, double monthlyBudget);

    // Expenses / Incomes / Bills
    bool addExpense(const Date& d, double amount, const std::string& category,
                    const std::string& description, const std::string& paymentMethod);
    void addIncome(const Date& d, double amount, const std::string& category,
                   const std::string& description, const std::string& source);
    void addBill(const std::string& name, double amount, const Date& dueDate);

    bool deleteExpense(int id);
    bool deleteIncome(int id);
    bool deleteBill(int id);

    void viewExpenses();
    void viewIncomes();
    void viewBills();

    void searchTransaction(int id);
    bool markBillPaid(int billId);

    // Range query over expenses, courtesy of the BST.
    void viewExpensesInRange(const Date& from, const Date& to);

    // Original monthly summary (incomes vs expenses) from the design doc.
    void showMonthlySummary();

    // Undo
    void undoLastAction();

    // Reminder access for main loop / FinanceAssistant
    ReminderSystem&       getReminders();
    const ReminderSystem& getReminders() const;

    // ===== NEW FEATURE 1: Monthly Summary Report =====
    double      getTotalBudget() const;
    double      getTotalSpent()  const;
    double      getRemainingBalance() const;
    std::string getMonthlyBudgetStatus() const;   // "Under Budget" / "Approaching Limit" / "Over Budget"
    void        showMonthlySummaryReport() const;

    // ===== NEW FEATURE 3: Budget Progress Bars =====
    double      getCategoryUsagePercent(const std::string& category) const;
    std::string renderProgressBar(double percent, int width = 20) const;
    void        showAllProgressBars() const;

    // ===== NEW FEATURE 5: Fixed Category Selection =====
    const std::vector<std::string>& getCategoryList() const;
    bool        isValidCategory(const std::string& category) const;
    std::string promptCategorySelection() const;  // interactive (uses std::cin)

    // Helper used by the assistant for "show my food budget".
    bool getCategorySnapshot(const std::string& category,
                             double& budget, double& spent, double& percent) const;

    // ===== GUI helper methods =====
    std::vector<Category> getAllCategories() const;
    std::vector<Expense>  getExpensesAll()   const;
    std::vector<Income>   getIncomesAll()    const;
    std::vector<Bill>     getBillsAll()      const;
    double getTotalIncome() const;

    // ===== Persistence =====
    void clear();
    void saveToDir(const std::string& dir) const;
    void loadFromDir(const std::string& dir);
};

#endif
