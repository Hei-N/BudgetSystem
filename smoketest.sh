#!/usr/bin/env bash
# Drives the menu non-interactively to exercise every feature.
set -e
cd "$(dirname "$0")"

# Build today + a few future / past dates relative to today so reminder statuses are exercised.
TODAY=$(date +%F)
PAST=$(date -d "$TODAY - 5 days" +%F 2>/dev/null || date -j -v-5d +%F)
DUE_TODAY=$TODAY
SOON=$(date -d "$TODAY + 2 days" +%F 2>/dev/null  || date -j -v+2d +%F)
LATER=$(date -d "$TODAY + 10 days" +%F 2>/dev/null || date -j -v+10d +%F)

./budget <<EOF
1
1
500
1
2
500
1
3
200
1
4
150
2
1
80
$TODAY
Groceries
Card
2
1
245
$TODAY
Restaurant
Card
2
2
40
$TODAY
Bus pass
Cash
2
4
160
$TODAY
Concert
Card
3
8
1500
$TODAY
Paycheck
Salary
4
Electricity
75
$PAST
4
Internet
60
$DUE_TODAY
4
Phone
40
$SOON
4
Water
30
$LATER
14
15
16
17
How much did I spend this month?
When is my next bill due?
Do I have overdue bills?
Show my food budget.

5
6
7
8
$PAST
$LATER
9
13
0
EOF
