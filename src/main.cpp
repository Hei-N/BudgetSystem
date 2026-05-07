#include <iostream>
#include <string>
#include <limits>

#include "BudgetManager.h"
#include "FinanceAssistant.h"

// ---- helpers ----
static int readInt(const std::string& prompt) {
    int x;
    while (true) {
        std::cout << prompt;
        if (std::cin >> x) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return x;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "  Please enter an integer.\n";
    }
}

static double readDouble(const std::string& prompt) {
    double x;
    while (true) {
        std::cout << prompt;
        if (std::cin >> x) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return x;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "  Please enter a number.\n";
    }
}

static std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

static Date readDate(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt + " (YYYY-MM-DD, blank=today): ");
        if (s.empty()) return Date::today();
        Date d;
        if (Date::tryParse(s, d)) return d;
        std::cout << "  Invalid date format. Try again.\n";
    }
}

// ---- menu ----
static void printMenu() {
    std::cout <<
        "\n=========== Personal Budget System ===========\n"
        "  1)  Set / update category budget\n"
        "  2)  Add expense\n"
        "  3)  Add income\n"
        "  4)  Add bill\n"
        "  5)  View expenses\n"
        "  6)  View incomes\n"
        "  7)  View bills\n"
        "  8)  View expenses in date range\n"
        "  9)  Income vs expense summary\n"
        "  10) Mark bill as paid\n"
        "  11) Check upcoming reminders\n"
        "  12) Search transaction by ID\n"
        "  13) Undo last action\n"
        "  -- New Finance Manager Features --\n"
        "  14) Monthly Summary Report\n"
        "  15) Bill Reminder Status\n"
        "  16) Budget Progress Bars\n"
        "  17) Smart Finance Assistant\n"
        "  18) Show predefined category list\n"
        "  0)  Exit\n"
        "----------------------------------------------\n";
}

int main() {
    BudgetManager   manager;
    FinanceAssistant assistant(manager);

    std::cout << "Personal Budget, Expense, Bill Payment & Due Date Management System\n";
    std::cout << "(C++17, OOP, with HashMap / MinHeap / BST / LinkedList / Stack / Queue)\n";

    while (true) {
        printMenu();
        int choice = readInt("Choose: ");
        if (choice == 0) {
            std::cout << "Goodbye.\n";
            break;
        }
        switch (choice) {
            case 1: {
                std::string cat = manager.promptCategorySelection();
                double b = readDouble("Monthly budget for " + cat + ": $");
                manager.setCategoryBudget(cat, b);
                break;
            }
            case 2: {
                std::string cat = manager.promptCategorySelection();
                double amt   = readDouble("Amount: $");
                Date   d     = readDate("Date");
                std::string desc   = readLine("Description: ");
                std::string method = readLine("Payment method (Cash/Card/Bank): ");
                if (method.empty()) method = "Cash";
                manager.addExpense(d, amt, cat, desc, method);
                break;
            }
            case 3: {
                std::string cat = manager.promptCategorySelection();
                double amt   = readDouble("Amount: $");
                Date   d     = readDate("Date");
                std::string desc   = readLine("Description: ");
                std::string source = readLine("Source: ");
                if (source.empty()) source = "Other";
                manager.addIncome(d, amt, cat, desc, source);
                break;
            }
            case 4: {
                std::string name = readLine("Bill name: ");
                double amt = readDouble("Amount: $");
                Date   d   = readDate("Due date");
                manager.addBill(name, amt, d);
                break;
            }
            case 5: manager.viewExpenses(); break;
            case 6: manager.viewIncomes();  break;
            case 7: manager.viewBills();    break;
            case 8: {
                Date from = readDate("From date");
                Date to   = readDate("To date");
                manager.viewExpensesInRange(from, to);
                break;
            }
            case 9:  manager.showMonthlySummary(); break;
            case 10: {
                int id = readInt("Bill id to mark paid: ");
                if (manager.markBillPaid(id)) std::cout << "  Marked paid.\n";
                else                          std::cout << "  Bill not found.\n";
                break;
            }
            case 11: manager.getReminders().checkUpcomingBills(Date::today()); break;
            case 12: manager.searchTransaction(readInt("Transaction id: ")); break;
            case 13: manager.undoLastAction(); break;
            case 14: manager.showMonthlySummaryReport(); break;
            case 15: manager.getReminders().showBillReminderStatus(Date::today()); break;
            case 16: manager.showAllProgressBars(); break;
            case 17: {
                std::cout << "Ask the assistant (blank line to exit):\n";
                while (true) {
                    std::string q = readLine("> ");
                    if (q.empty()) break;
                    std::cout << assistant.ask(q) << "\n";
                }
                break;
            }
            case 18: {
                std::cout << "Predefined categories:\n";
                for (const std::string& c : manager.getCategoryList()) {
                    std::cout << "  - " << c << "\n";
                }
                break;
            }
            default:
                std::cout << "  Invalid choice.\n";
        }
    }
    return 0;
}
