#ifndef BILL_H
#define BILL_H

#include <string>
#include "Date.h"

// Reminder status returned by Bill::getReminderStatus()
enum class BillStatus {
    Overdue,    // dueDate < today AND unpaid
    DueToday,   // dueDate == today AND unpaid
    DueSoon,    // today < dueDate <= today + 3 days AND unpaid
    Upcoming,   // dueDate > today + 3 days AND unpaid
    Paid        // already paid
};

std::string billStatusToString(BillStatus s);

class Bill {
private:
    int         billId;
    std::string billName;
    double      amount;
    Date        dueDate;
    bool        isPaid;

public:
    Bill();
    Bill(int id, const std::string& name, double amt,
         const Date& due, bool paid = false);

    int          getBillId()   const;
    std::string  getBillName() const;
    double       getAmount()   const;
    Date         getDueDate()  const;
    bool         getIsPaid()   const;

    void setBillName(const std::string& n);
    void setAmount(double a);
    void setDueDate(const Date& d);
    void markPaid();

    // NEW FEATURE: Bill Reminder Status
    BillStatus getReminderStatus(const Date& today) const;

    void displayBill() const;
};

#endif
