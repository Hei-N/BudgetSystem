#include "Expense.h"

#include <iostream>
#include <iomanip>

Expense::Expense() : Transaction(), paymentMethod("Cash") {}

Expense::Expense(int id, const Date& d, double amt,
                 const std::string& cat, const std::string& desc,
                 const std::string& method)
    : Transaction(id, d, amt, cat, desc), paymentMethod(method) {}

std::string Expense::getPaymentMethod() const { return paymentMethod; }
void        Expense::setPaymentMethod(const std::string& m) { paymentMethod = m; }

void Expense::displayTransaction() const {
    std::cout << "  [E" << transactionId << "] "
              << date.toString() << "  -$"
              << std::fixed << std::setprecision(2) << amount
              << "  " << category
              << "  via " << paymentMethod;
    if (!description.empty()) std::cout << "  - " << description;
    std::cout << "\n";
}
