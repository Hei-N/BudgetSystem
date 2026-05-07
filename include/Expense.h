#ifndef EXPENSE_H
#define EXPENSE_H

#include "Transaction.h"

// Inherits from Transaction. Adds the payment method (Cash, Card, Bank Transfer, ...).
class Expense : public Transaction {
private:
    std::string paymentMethod;

public:
    Expense();
    Expense(int id, const Date& d, double amt,
            const std::string& cat, const std::string& desc,
            const std::string& method);

    std::string getPaymentMethod() const;
    void        setPaymentMethod(const std::string& m);

    void displayTransaction() const override;
};

#endif
