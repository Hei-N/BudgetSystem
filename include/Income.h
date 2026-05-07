#ifndef INCOME_H
#define INCOME_H

#include "Transaction.h"

// Inherits from Transaction. Adds the income source (e.g., "Salary", "Freelance").
class Income : public Transaction {
private:
    std::string source;

public:
    Income();
    Income(int id, const Date& d, double amt,
           const std::string& cat, const std::string& desc,
           const std::string& src);

    std::string getSource() const;
    void        setSource(const std::string& s);

    void displayTransaction() const override;
};

#endif
