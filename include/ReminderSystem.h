#ifndef REMINDER_SYSTEM_H
#define REMINDER_SYSTEM_H

#include <vector>
#include "Bill.h"
#include "Queue.h"
#include "MinHeap.h"

// Comparator: bill with the smaller dueDate "comes first" in the heap.
struct BillEarlierDue {
    bool operator()(const Bill& a, const Bill& b) const {
        return a.getDueDate() < b.getDueDate();
    }
};

// ReminderSystem holds bills in two structures:
//   * Queue<Bill>  for the original FIFO reminder workflow described in the design doc
//   * MinHeap<Bill> so the next-due bill can be retrieved in O(log n)
// Both are updated together so the original interface (queue) keeps working
// while the new features (Bill Reminder Status, Smart Assistant "next bill")
// can be answered efficiently from the heap.
class ReminderSystem {
private:
    Queue<Bill>                  upcomingBills;
    MinHeap<Bill, BillEarlierDue> heap;

public:
    ReminderSystem();

    // Original interface from the design doc.
    void addReminder(const Bill& b);
    void checkUpcomingBills(const Date& today);
    void showReminders() const;

    // NEW FEATURE 2: Bill Reminder Status report.
    void showBillReminderStatus(const Date& today) const;

    // Used by the Smart Finance Assistant.
    bool peekNextUnpaidBill(Bill& out) const;
    std::vector<Bill> getOverdueBills(const Date& today) const;

    // Re-sync the heap when an underlying bill state changed
    // (e.g. user marked a bill as paid through BudgetManager).
    void rebuildFromBills(const std::vector<Bill>& bills);
};

#endif
