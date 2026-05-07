#include "ReminderSystem.h"

#include <iostream>
#include <iomanip>

ReminderSystem::ReminderSystem() {}

void ReminderSystem::addReminder(const Bill& b) {
    upcomingBills.enqueue(b);
    heap.push(b);
}

void ReminderSystem::checkUpcomingBills(const Date& today) {
    std::cout << "\n-- Upcoming Bill Check (" << today.toString() << ") --\n";
    upcomingBills.forEach([&](const Bill& b) {
        if (b.getIsPaid()) return;
        BillStatus s = b.getReminderStatus(today);
        if (s == BillStatus::Overdue || s == BillStatus::DueToday || s == BillStatus::DueSoon) {
            std::cout << "  REMINDER: " << b.getBillName()
                      << " ($" << std::fixed << std::setprecision(2) << b.getAmount()
                      << ") is " << billStatusToString(s)
                      << " on " << b.getDueDate().toString() << "\n";
        }
    });
}

void ReminderSystem::showReminders() const {
    std::cout << "\n-- All Queued Reminders --\n";
    if (upcomingBills.isEmpty()) {
        std::cout << "  (no reminders queued)\n";
        return;
    }
    upcomingBills.forEach([](const Bill& b) { b.displayBill(); });
}

// NEW FEATURE 2: Bill Reminder Status report
void ReminderSystem::showBillReminderStatus(const Date& today) const {
    std::cout << "\n=== Bill Reminder Status (today: " << today.toString() << ") ===\n";
    if (upcomingBills.isEmpty()) {
        std::cout << "  (no bills tracked)\n";
        return;
    }
    upcomingBills.forEach([&](const Bill& b) {
        BillStatus s = b.getReminderStatus(today);
        std::cout << "  " << std::left << std::setw(20) << b.getBillName()
                  << "  due " << b.getDueDate().toString()
                  << "  $"   << std::fixed << std::setprecision(2) << b.getAmount()
                  << "   [" << billStatusToString(s) << "]\n";
    });
}

bool ReminderSystem::peekNextUnpaidBill(Bill& out) const {
    // The heap may contain stale "paid" bills if rebuildFromBills wasn't called;
    // walk the queue defensively to find the earliest unpaid bill.
    bool found = false;
    Bill best;
    upcomingBills.forEach([&](const Bill& b) {
        if (b.getIsPaid()) return;
        if (!found || b.getDueDate() < best.getDueDate()) {
            best = b;
            found = true;
        }
    });
    if (found) out = best;
    return found;
}

std::vector<Bill> ReminderSystem::getOverdueBills(const Date& today) const {
    std::vector<Bill> result;
    upcomingBills.forEach([&](const Bill& b) {
        if (!b.getIsPaid() && b.getReminderStatus(today) == BillStatus::Overdue) {
            result.push_back(b);
        }
    });
    return result;
}

void ReminderSystem::rebuildFromBills(const std::vector<Bill>& bills) {
    // Reset both structures and re-add only unpaid bills.
    while (!upcomingBills.isEmpty()) { Bill tmp; upcomingBills.dequeue(tmp); }
    heap.clear();
    for (const Bill& b : bills) {
        if (!b.getIsPaid()) {
            upcomingBills.enqueue(b);
            heap.push(b);
        }
    }
}
