#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include "Date.h"

// Base class for any financial record (Expense / Income).
// Stores ID, date, amount, category, description.
class Transaction {
protected:
    int         transactionId;
    Date        date;
    double      amount;
    std::string category;
    std::string description;

public:
    Transaction();
    Transaction(int id, const Date& d, double amt,
                const std::string& cat, const std::string& desc);
    virtual ~Transaction();

    int                getTransactionId() const;
    Date               getDate()          const;
    double             getAmount()        const;
    std::string        getCategory()      const;
    std::string        getDescription()   const;

    void setDate(const Date& d);
    void setAmount(double a);
    void setCategory(const std::string& c);
    void setDescription(const std::string& d);

    virtual void displayTransaction() const;
};

#endif
