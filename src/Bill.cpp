#include "Bill.h"

#include <iostream>
#include <iomanip>

std::string billStatusToString(BillStatus s) {
    switch (s) {
        case BillStatus::Overdue:  return "Overdue";
        case BillStatus::DueToday: return "Due Today";
        case BillStatus::DueSoon:  return "Due Soon";
        case BillStatus::Upcoming: return "Upcoming";
        case BillStatus::Paid:     return "Paid";
    }
    return "Unknown";
}

Bill::Bill() : billId(0), billName(""), amount(0.0), dueDate(), isPaid(false) {}

Bill::Bill(int id, const std::string& name, double amt,
           const Date& due, bool paid)
    : billId(id), billName(name), amount(amt), dueDate(due), isPaid(paid) {}

int          Bill::getBillId()   const { return billId; }
std::string  Bill::getBillName() const { return billName; }
double       Bill::getAmount()   const { return amount; }
Date         Bill::getDueDate()  const { return dueDate; }
bool         Bill::getIsPaid()   const { return isPaid; }

void Bill::setBillName(const std::string& n) { billName = n; }
void Bill::setAmount(double a)               { amount = a; }
void Bill::setDueDate(const Date& d)         { dueDate = d; }
void Bill::markPaid()                        { isPaid = true; }

// NEW FEATURE 2: Bill Reminder Status
BillStatus Bill::getReminderStatus(const Date& today) const {
    if (isPaid) return BillStatus::Paid;
    int diff = dueDate.daysFrom(today);   // dueDate - today
    if (diff <  0) return BillStatus::Overdue;
    if (diff == 0) return BillStatus::DueToday;
    if (diff <= 3) return BillStatus::DueSoon;
    return BillStatus::Upcoming;
}

void Bill::displayBill() const {
    std::cout << "  [B" << billId << "] " << billName
              << "  $"   << std::fixed << std::setprecision(2) << amount
              << "  due " << dueDate.toString()
              << "  ["   << (isPaid ? "PAID" : "UNPAID") << "]\n";
}
