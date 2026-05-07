# Personal Budget, Expense, Bill Payment & Due Date Management System

CSC 331H Honors Project — Spring 2026
C++17 implementation of the design described in the proposal and detailed design
documents, plus the five new Finance Manager features.

## What's inside

Six original components from the design doc:

- `Transaction` (base class)
- `Expense` (inherits from `Transaction`, adds `paymentMethod`)
- `Income` (inherits from `Transaction`, adds `source`)
- `Bill` (id, name, amount, due date, paid flag, **reminder status**)
- `BudgetManager` (sole controller — owns all data)
- `ReminderSystem` (FIFO queue + min-heap of upcoming bills)

Plus the data structures called out in the proposal and design:

- `LinkedList<T>` — Expense / Income / Bill storage
- `Stack<UndoAction>` — undo support
- `Queue<Bill>` — reminder workflow
- `HashMap<CategoryBudget>` — O(1) per-category budget + spent tracking
- `MinHeap<Bill, Cmp>` — next-due bill in O(log n)
- `BST<Date, int>` — date-range queries on expenses

## New Finance Manager features (this revision)

1. **Monthly Summary Report** — `BudgetManager::showMonthlySummaryReport()`
   shows total budget, total spent, remaining, and status
   (`Under Budget` / `Approaching Limit` / `Over Budget`).
2. **Bill Reminder Status** — `Bill::getReminderStatus(today)` returns
   `Overdue` / `Due Today` / `Due Soon` / `Upcoming` / `Paid`. Use
   `ReminderSystem::showBillReminderStatus(today)` to print all.
3. **Budget Progress Bars** — `BudgetManager::showAllProgressBars()` renders
   a text bar for every category, e.g. `Food         [#############-------] 65%`.
4. **Smart Finance Assistant** — `FinanceAssistant::ask("How much did I spend this month?")`
   answers a small set of natural questions (spend total, next bill, overdue bills,
   `Show my <Category> budget`).
5. **Fixed Category Selection** — categories are picked from a predefined list
   (`Food`, `Transport`, `Utilities`, `Entertainment`, `Health`, `Education`,
   `Shopping`, `Other`). `addExpense()` rejects free-text categories.

## Build

Requires a C++17 compiler (g++ 7+ or clang++ 5+).

    make
    ./budget

Or `make run`.

## Project layout

    BudgetSystem/
      include/   header files
      src/       implementation files (.cpp) + main.cpp
      Makefile
      README.md

## Menu (interactive)

    1) Set / update category budget
    2) Add expense
    3) Add income
    4) Add bill
    5) View expenses
    6) View incomes
    7) View bills
    8) View expenses in date range          (BST range query)
    9) Income vs expense summary
    10) Mark bill as paid
    11) Check upcoming reminders
    12) Search transaction by ID
    13) Undo last action
    14) Monthly Summary Report               (NEW)
    15) Bill Reminder Status                 (NEW)
    16) Budget Progress Bars                 (NEW)
    17) Smart Finance Assistant              (NEW)
    18) Show predefined category list        (NEW)

## Notes on design decisions

- `BudgetManager` keeps its existing public interface from the design doc.
  All five new features are additive — nothing breaks for existing callers.
- The `ReminderSystem` keeps its `Queue<Bill>` workflow but also maintains a
  `MinHeap` so the assistant can answer "when is my next bill?" without scanning.
- Expenses are stored both in a `LinkedList<Expense>` (design doc) and indexed
  by date in a `BST<Date, int>` (proposal) so date-range queries are O(log n + k).
- Data is in-memory only (consistent with the proposal's stated limitation);
  file persistence is left as a future enhancement.
