// gui_main.cpp  –  Dear ImGui front-end for the Personal Budget System.
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>

#include "BudgetManager.h"
#include "Category.h"
#include "FinanceAssistant.h"

// ─── User account ─────────────────────────────────────────────────────────────
struct UserRecord {
    std::string username;
    std::string displayName;
    std::string password;
};
static std::vector<UserRecord> g_users;
static int g_currentUser = -1;   // index into g_users, -1 = not logged in

// ─── App / screen state ───────────────────────────────────────────────────────
enum class AppState { Login, Register, Main };
static AppState g_appState = AppState::Login;

enum class Screen { Dashboard, Expenses, Incomes, Bills, Budget, Transactions, Assistant, UserInfo };
static Screen g_screen = Screen::Assistant;

// ─── Finance Assistant chat log ───────────────────────────────────────────────
static std::vector<std::string> g_chat;

// ─── Light-theme colors ───────────────────────────────────────────────────────
static const ImVec4 kAccent  {0.18f, 0.44f, 0.86f, 1.0f};
static const ImVec4 kGreen   {0.10f, 0.56f, 0.24f, 1.0f};
static const ImVec4 kYellow  {0.78f, 0.50f, 0.00f, 1.0f};
static const ImVec4 kRed     {0.82f, 0.10f, 0.10f, 1.0f};
static const ImVec4 kMuted   {0.38f, 0.38f, 0.42f, 1.0f};
static const ImVec4 kWhite   {1.00f, 1.00f, 1.00f, 1.0f};
static const ImVec4 kDark    {0.10f, 0.10f, 0.14f, 1.0f};

// ─── Persistence helpers ──────────────────────────────────────────────────────
static std::string dataRoot() {
    const char* h = getenv("HOME");
    return (h ? std::string(h) : std::string(".")) + "/.budgetsystem";
}
static std::string userDir(const std::string& uname) {
    return dataRoot() + "/" + uname;
}
static void saveUsers() {
    std::string root = dataRoot();
    mkdir(root.c_str(), 0755);
    std::ofstream f(root + "/users.csv");
    for (auto& u : g_users)
        f << u.username << "\t" << u.displayName << "\t" << u.password << "\n";
}
static void loadUsers() {
    std::ifstream f(dataRoot() + "/users.csv");
    std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string a, b, c;
        std::getline(ss, a, '\t');
        std::getline(ss, b, '\t');
        std::getline(ss, c, '\t');
        if (!a.empty()) g_users.push_back({a, b.empty() ? a : b, c});
    }
}

static ImVec4 pctColor(double pct) {
    if (pct >= 100.0) return kRed;
    if (pct >= 80.0)  return kYellow;
    return kGreen;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
static std::string fmtMoney(double v) {
    std::ostringstream ss;
    ss << "$" << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

static void StatCard(const char* id, const char* label,
                     const std::string& value, ImVec4 valCol, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWhite);
    ImGui::BeginChild(id, {width, 72}, true);
    ImGui::TextColored(kMuted, "%s", label);
    ImGui::Spacing();
    ImGui::TextColored(valCol, "%s", value.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Pill-button grid for category selection.
static void CategoryPillGrid(const std::vector<std::string>& cats, int& selected) {
    float availW = ImGui::GetContentRegionAvail().x;
    const int perRow = 5;
    float gap   = 6.0f;
    float pillW = (availW - gap * (perRow - 1)) / (float)perRow;

    for (int i = 0; i < (int)cats.size(); ++i) {
        if (i % perRow != 0) ImGui::SameLine(0.0f, gap);
        bool sel = (selected == i);
        if (sel) {
            ImGui::PushStyleColor(ImGuiCol_Button,         kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f,0.54f,0.96f,1.0f});
            ImGui::PushStyleColor(ImGuiCol_Text,           kWhite);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.88f,0.90f,0.95f,1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.78f,0.84f,0.96f,1.0f});
            ImGui::PushStyleColor(ImGuiCol_Text,           kDark);
        }
        char bid[64];
        snprintf(bid, sizeof(bid), "%s##pill%d", cats[i].c_str(), i);
        if (ImGui::Button(bid, {pillW, 28.0f})) selected = i;
        ImGui::PopStyleColor(3);
    }
}

// Full-width primary action button.
static bool PrimaryButton(const char* label, float h = 36.0f) {
    ImGui::PushStyleColor(ImGuiCol_Button,         kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f,0.54f,0.96f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.12f,0.36f,0.76f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,           kWhite);
    bool clicked = ImGui::Button(label, {-1.0f, h});
    ImGui::PopStyleColor(4);
    return clicked;
}

// ─── Spending line chart (current month vs previous month) ───────────────────
static void RenderSpendingChart(BudgetManager& mgr) {
    Date today = Date::today();
    int cY = today.year, cM = today.month;
    int pY = cY,         pM = cM - 1;
    if (pM == 0) { pM = 12; --pY; }

    // Daily spending buckets (index = day 1..31)
    float cur[32] = {}, prv[32] = {};
    for (auto& e : mgr.getExpensesAll()) {
        Date d = e.getDate();
        if (d.year == cY && d.month == cM && d.day >= 1 && d.day <= 31)
            cur[d.day] += (float)e.getAmount();
        else if (d.year == pY && d.month == pM && d.day >= 1 && d.day <= 31)
            prv[d.day] += (float)e.getAmount();
    }
    // Make cumulative
    for (int i = 2; i <= 31; ++i) { cur[i] += cur[i-1]; prv[i] += prv[i-1]; }

    float maxVal = 1.0f;
    for (int i = 1; i <= 31; ++i)
        maxVal = std::max(maxVal, std::max(cur[i], prv[i]));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 orig    = ImGui::GetCursorScreenPos();
    float  W       = ImGui::GetContentRegionAvail().x;

    const float H = 150.0f, pL = 8.0f, pR = 8.0f, pT = 12.0f, pB = 22.0f;
    float iW = W - pL - pR, iH = H - pT - pB;

    // Background
    dl->AddRectFilled(orig, {orig.x + W, orig.y + H}, IM_COL32(247,249,253,255), 5.0f);
    dl->AddRect      (orig, {orig.x + W, orig.y + H}, IM_COL32(208,216,232,255), 5.0f);

    auto toX = [&](int d)   { return orig.x + pL + (d - 1) / 30.0f * iW; };
    auto toY = [&](float v) { return orig.y + pT + iH * (1.0f - v / maxVal); };

    // Vertical grid lines + x-axis day labels
    const int   gd[] = {1, 8, 16, 24, 31};
    const char* gl[] = {"1", "8", "16", "24", "31"};
    for (int i = 0; i < 5; ++i) {
        float gx = toX(gd[i]);
        dl->AddLine({gx, orig.y + pT}, {gx, orig.y + pT + iH},
                    IM_COL32(208, 216, 232, 100), 0.5f);
        ImVec2 ts = ImGui::CalcTextSize(gl[i]);
        dl->AddText({gx - ts.x * 0.5f, orig.y + pT + iH + 4.0f},
                    IM_COL32(140, 150, 170, 255), gl[i]);
    }

    // Previous month line (gray)
    for (int i = 1; i <= 30; ++i)
        dl->AddLine({toX(i), toY(prv[i])}, {toX(i + 1), toY(prv[i + 1])},
                    IM_COL32(158, 163, 180, 220), 1.5f);

    // Current month line (blue, up to today)
    for (int i = 1; i < today.day && i <= 30; ++i)
        dl->AddLine({toX(i), toY(cur[i])}, {toX(i + 1), toY(cur[i + 1])},
                    IM_COL32(46, 112, 220, 235), 2.2f);

    // Dots at today's day
    if (today.day >= 1 && today.day <= 31) {
        dl->AddCircleFilled({toX(today.day), toY(cur[today.day])}, 4.5f,
                            IM_COL32(46,  112, 220, 255));
        dl->AddCircleFilled({toX(today.day), toY(prv[today.day])}, 4.5f,
                            IM_COL32(158, 163, 180, 255));
    }

    ImGui::Dummy({W, H + 2.0f});

    // Legend (two rows below chart)
    const char* mn[] = {"Jan","Feb","Mar","Apr","May","Jun",
                        "Jul","Aug","Sep","Oct","Nov","Dec"};
    char buf[64];

    // Row 1 — current month (blue dot drawn at cursor)
    {
        ImVec2 lp = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled({lp.x + 7.0f, lp.y + 7.0f}, 4.0f, IM_COL32(46, 112, 220, 255));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
        snprintf(buf, sizeof(buf), "%s %d", mn[cM - 1], today.day);
        ImGui::TextColored(kAccent, "%s", buf);
        snprintf(buf, sizeof(buf), "$%.2f", cur[today.day]);
        float aw = ImGui::CalcTextSize(buf).x;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - aw + 18.0f);
        ImGui::TextColored(kAccent, "%s", buf);
    }
    // Row 2 — previous month (gray dot)
    {
        ImVec2 lp = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled({lp.x + 7.0f, lp.y + 7.0f}, 4.0f, IM_COL32(158, 163, 180, 255));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
        snprintf(buf, sizeof(buf), "%s %d", mn[pM - 1], today.day);
        ImGui::TextColored(kMuted, "%s", buf);
        snprintf(buf, sizeof(buf), "$%.2f", prv[today.day]);
        float aw = ImGui::CalcTextSize(buf).x;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - aw + 18.0f);
        ImGui::TextColored(kMuted, "%s", buf);
    }
}

// ─── Dashboard ───────────────────────────────────────────────────────────────
static void RenderDashboard(BudgetManager& mgr) {
    double budget    = mgr.getTotalBudget();
    double spent     = mgr.getTotalSpent();
    double remaining = mgr.getRemainingBalance();
    double income    = mgr.getTotalIncome();

    float availW = ImGui::GetContentRegionAvail().x;
    float gap    = ImGui::GetStyle().ItemSpacing.x;
    float cardW  = (availW - gap * 3) / 4.0f;

    StatCard("##c1", "Total Budget",  fmtMoney(budget),    kAccent,                       cardW);
    ImGui::SameLine();
    StatCard("##c2", "Total Spent",   fmtMoney(spent),     kRed,                          cardW);
    ImGui::SameLine();
    StatCard("##c3", "Remaining",     fmtMoney(remaining), remaining >= 0 ? kGreen : kRed, cardW);
    ImGui::SameLine();
    StatCard("##c4", "Total Income",  fmtMoney(income),    kGreen,                        cardW);

    ImGui::Spacing();
    std::string status = mgr.getMonthlyBudgetStatus();
    ImVec4 stCol = (status == "Over Budget")       ? kRed    :
                   (status == "Approaching Limit") ? kYellow : kGreen;
    ImGui::Text("Status:"); ImGui::SameLine();
    ImGui::TextColored(stCol, "%s", status.c_str());

    ImGui::Separator();
    ImGui::Text("Monthly Spending Trend");
    ImGui::Spacing();
    RenderSpendingChart(mgr);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Category Progress");
    ImGui::Spacing();

    for (auto& c : mgr.getAllCategories()) {
        if (c.budgetLimit <= 0.0 && c.totalSpent <= 0.0) continue;
        double pct  = c.getPercentUsed();
        float  fPct = (pct > 100.0) ? 1.0f : (float)(pct / 100.0);
        char   ov[72];
        snprintf(ov, sizeof(ov), "%-13s  %s / %s  (%.0f%%)",
                 c.name.c_str(), fmtMoney(c.totalSpent).c_str(),
                 fmtMoney(c.budgetLimit).c_str(), pct);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, pctColor(pct));
        ImGui::ProgressBar(fPct, {-1, 20}, ov);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

// ─── Expenses ────────────────────────────────────────────────────────────────
static void RenderExpenses(BudgetManager& mgr) {
    static char  descBuf[128] = "";
    static char  dateBuf[16]  = "";
    static float amount       = 0.0f;
    static int   catIdx       = 0;
    static int   methIdx      = 0;
    static int   deleteId     = 0;
    const auto&  cats         = mgr.getCategoryList();
    const char*  methods[]    = {"Cash", "Card", "Bank Transfer"};

    if (ImGui::CollapsingHeader("Add Expense", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Category pills
        ImGui::TextColored(kMuted, "Category");
        CategoryPillGrid(cats, catIdx);
        ImGui::Spacing();

        // Row: amount | date | method  — label above each field
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Amount ($)");
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("##amount_e", &amount, 0.0f, 0.0f, "%.2f");
        ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Date (YYYY-MM-DD)");
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##date_e", dateBuf, sizeof(dateBuf));
        ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Payment Method");
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo("##method_e", methods[methIdx])) {
            for (int i = 0; i < 3; ++i) {
                if (ImGui::Selectable(methods[i], methIdx == i)) methIdx = i;
                if (methIdx == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndGroup();

        // Description — label above, full width
        ImGui::TextColored(kMuted, "Description");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##desc_e", descBuf, sizeof(descBuf));

        ImGui::Spacing();
        if (PrimaryButton("+ Add Expense")) {
            if (amount > 0.0f && catIdx < (int)cats.size()) {
                Date d;
                std::string ds(dateBuf);
                if (ds.empty() || !Date::tryParse(ds, d)) d = Date::today();
                mgr.addExpense(d, (double)amount, cats[catIdx], descBuf, methods[methIdx]);
                amount = 0.0f; descBuf[0] = '\0';
            }
        }
    }

    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kMuted, "Delete ID:");
    ImGui::SameLine(0, 6);
    ImGui::SetNextItemWidth(70);
    ImGui::InputInt("##del_e", &deleteId, 0, 0);
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button,        kRed);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f,0.20f,0.20f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,          kWhite);
    if (ImGui::Button("Delete##e") && deleteId > 0) { mgr.deleteExpense(deleteId); deleteId = 0; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Undo##e")) mgr.undoLastAction();
    ImGui::Separator();

    if (ImGui::BeginTable("##exp_tbl", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID",          ImGuiTableColumnFlags_WidthFixed,   38);
        ImGui::TableSetupColumn("Date",        ImGuiTableColumnFlags_WidthFixed,   90);
        ImGui::TableSetupColumn("Category",    ImGuiTableColumnFlags_WidthFixed,  110);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Amount",      ImGuiTableColumnFlags_WidthFixed,   80);
        ImGui::TableSetupColumn("Method",      ImGuiTableColumnFlags_WidthFixed,   95);
        ImGui::TableHeadersRow();
        for (auto& e : mgr.getExpensesAll()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d",  e.getTransactionId());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s",  e.getDate().toString().c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s",  e.getCategory().c_str());
            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e.getDescription().c_str());
            ImGui::TableSetColumnIndex(4); ImGui::TextColored(kRed,   "%s", fmtMoney(e.getAmount()).c_str());
            ImGui::TableSetColumnIndex(5); ImGui::Text("%s",  e.getPaymentMethod().c_str());
        }
        ImGui::EndTable();
    }
}

// ─── Incomes ─────────────────────────────────────────────────────────────────
static void RenderIncomes(BudgetManager& mgr) {
    static char  descBuf[128] = "";
    static char  srcBuf[64]   = "";
    static char  dateBuf[16]  = "";
    static float amount       = 0.0f;
    static int   deleteId     = 0;

    if (ImGui::CollapsingHeader("Add Income", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Amount ($)");
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("##amount_i", &amount, 0.0f, 0.0f, "%.2f");
        ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Date (YYYY-MM-DD)");
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##date_i", dateBuf, sizeof(dateBuf));
        ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Source");
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##src_i", srcBuf, sizeof(srcBuf));
        ImGui::EndGroup();

        ImGui::TextColored(kMuted, "Description");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##desc_i", descBuf, sizeof(descBuf));
        ImGui::Spacing();
        if (PrimaryButton("+ Add Income")) {
            if (amount > 0.0f) {
                Date d;
                std::string ds(dateBuf);
                if (ds.empty() || !Date::tryParse(ds, d)) d = Date::today();
                std::string src(srcBuf); if (src.empty()) src = "Other";
                mgr.addIncome(d, (double)amount, "Other", descBuf, src);
                amount = 0.0f; descBuf[0] = '\0';
            }
        }
    }

    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kMuted, "Delete ID:");
    ImGui::SameLine(0, 6);
    ImGui::SetNextItemWidth(70);
    ImGui::InputInt("##del_i", &deleteId, 0, 0);
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button,        kRed);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f,0.20f,0.20f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,          kWhite);
    if (ImGui::Button("Delete##i") && deleteId > 0) { mgr.deleteIncome(deleteId); deleteId = 0; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Undo##i")) mgr.undoLastAction();
    ImGui::Separator();

    if (ImGui::BeginTable("##inc_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID",          ImGuiTableColumnFlags_WidthFixed,   38);
        ImGui::TableSetupColumn("Date",        ImGuiTableColumnFlags_WidthFixed,   90);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Amount",      ImGuiTableColumnFlags_WidthFixed,   80);
        ImGui::TableSetupColumn("Source",      ImGuiTableColumnFlags_WidthFixed,  110);
        ImGui::TableHeadersRow();
        for (auto& i : mgr.getIncomesAll()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d",  i.getTransactionId());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s",  i.getDate().toString().c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(i.getDescription().c_str());
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(kGreen, "%s", fmtMoney(i.getAmount()).c_str());
            ImGui::TableSetColumnIndex(4); ImGui::Text("%s",  i.getSource().c_str());
        }
        ImGui::EndTable();
    }
}

// ─── Bills ───────────────────────────────────────────────────────────────────
static void RenderBills(BudgetManager& mgr) {
    static char  nameBuf[64]  = "";
    static char  dateBuf[16]  = "";
    static float amount       = 0.0f;
    static int   markPaidId   = 0;
    static int   deleteId     = 0;

    if (ImGui::CollapsingHeader("Add Bill", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Bill Name");
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##name_b", nameBuf, sizeof(nameBuf));
        ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Amount ($)");
        ImGui::SetNextItemWidth(130);
        ImGui::InputFloat("##amount_b", &amount, 0.0f, 0.0f, "%.2f");
        ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "Due Date (YYYY-MM-DD)");
        ImGui::SetNextItemWidth(170);
        ImGui::InputText("##date_b", dateBuf, sizeof(dateBuf));
        ImGui::EndGroup();
        ImGui::Spacing();
        if (PrimaryButton("+ Add Bill")) {
            if (amount > 0.0f && nameBuf[0] != '\0') {
                Date d;
                std::string ds(dateBuf);
                if (ds.empty() || !Date::tryParse(ds, d)) d = Date::today();
                mgr.addBill(nameBuf, (double)amount, d);
                amount = 0.0f; nameBuf[0] = '\0';
            }
        }
    }

    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kMuted, "Mark Paid ID:");
    ImGui::SameLine(0, 6);
    ImGui::SetNextItemWidth(70);
    ImGui::InputInt("##paid_b", &markPaidId, 0, 0);
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Mark Paid") && markPaidId > 0) {
        if (!mgr.markBillPaid(markPaidId)) ImGui::OpenPopup("bill_nf");
        markPaidId = 0;
    }
    ImGui::SameLine(0, 20);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kMuted, "Delete ID:");
    ImGui::SameLine(0, 6);
    ImGui::SetNextItemWidth(70);
    ImGui::InputInt("##del_b", &deleteId, 0, 0);
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button,        kRed);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f,0.20f,0.20f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,          kWhite);
    if (ImGui::Button("Delete##b") && deleteId > 0) { mgr.deleteBill(deleteId); deleteId = 0; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Undo##b")) mgr.undoLastAction();

    if (ImGui::BeginPopupModal("bill_nf", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Bill ID not found.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::Separator();

    if (ImGui::BeginTable("##bill_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID",     ImGuiTableColumnFlags_WidthFixed,  38);
        ImGui::TableSetupColumn("Name",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed,  80);
        ImGui::TableSetupColumn("Due",    ImGuiTableColumnFlags_WidthFixed,  90);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,  80);
        ImGui::TableHeadersRow();
        Date today = Date::today();
        for (auto& b : mgr.getBillsAll()) {
            BillStatus st = b.getReminderStatus(today);
            ImVec4 sc = (st == BillStatus::Overdue)  ? kRed    :
                        (st == BillStatus::DueToday) ? kYellow :
                        (st == BillStatus::DueSoon)  ? kYellow :
                        (st == BillStatus::Paid)     ? kMuted  : kGreen;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", b.getBillId());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(b.getBillName().c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", fmtMoney(b.getAmount()).c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%s", b.getDueDate().toString().c_str());
            ImGui::TableSetColumnIndex(4); ImGui::TextColored(sc, "%s", billStatusToString(st).c_str());
        }
        ImGui::EndTable();
    }
}

// ─── Budget ──────────────────────────────────────────────────────────────────
static void RenderBudget(BudgetManager& mgr) {
    static int   catIdx = 0;
    static float budget = 0.0f;
    const auto&  cats   = mgr.getCategoryList();

    ImGui::Text("Select category and set its monthly budget:");
    ImGui::Spacing();
    CategoryPillGrid(cats, catIdx);
    ImGui::Spacing();
    ImGui::BeginGroup();
    ImGui::TextColored(kMuted, "Monthly Budget ($)");
    ImGui::SetNextItemWidth(200);
    ImGui::InputFloat("##budget_bd", &budget, 0.0f, 0.0f, "%.2f");
    ImGui::EndGroup();
    ImGui::SameLine(0, 12);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                         ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y);
    if (ImGui::Button("Set Budget", {110, 0}) && catIdx < (int)cats.size() && budget >= 0.0f)
        mgr.setCategoryBudget(cats[catIdx], (double)budget);
    ImGui::Separator();

    if (ImGui::BeginTable("##bud_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Budget",   ImGuiTableColumnFlags_WidthFixed,  80);
        ImGui::TableSetupColumn("Spent",    ImGuiTableColumnFlags_WidthFixed,  80);
        ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupColumn("Status",   ImGuiTableColumnFlags_WidthFixed,  72);
        ImGui::TableHeadersRow();
        for (auto& c : mgr.getAllCategories()) {
            double pct  = c.getPercentUsed();
            float  fPct = (pct > 100.0) ? 1.0f : (float)(pct / 100.0);
            char   pb[16]; snprintf(pb, sizeof(pb), "%.0f%%", pct);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(c.name.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", fmtMoney(c.budgetLimit).c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", fmtMoney(c.totalSpent).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, pctColor(pct));
            ImGui::ProgressBar(fPct, {-1, 16}, pb);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(pctColor(pct), "%s", c.statusLabel());
        }
        ImGui::EndTable();
    }
}

// ─── Transactions ─────────────────────────────────────────────────────────────
static void RenderTransactions(BudgetManager& mgr) {
    ImGui::TextColored(kMuted, "All money-out records — expenses and bills in one place.");
    ImGui::Spacing();

    struct TxRow {
        bool        isBill;
        std::string name;
        std::string category;
        double      amount;
        std::string date;
        std::string detail;  // payment method (expense) or bill status
    };

    std::vector<TxRow> rows;
    for (auto& e : mgr.getExpensesAll()) {
        std::string desc = e.getDescription().empty() ? "—" : e.getDescription();
        rows.push_back({false, desc, e.getCategory(), e.getAmount(),
                        e.getDate().toString(), e.getPaymentMethod()});
    }
    Date today = Date::today();
    for (auto& b : mgr.getBillsAll()) {
        BillStatus st = b.getReminderStatus(today);
        rows.push_back({true, b.getBillName(), "Bill", b.getAmount(),
                        b.getDueDate().toString(), billStatusToString(st)});
    }

    if (rows.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kMuted, "No transactions yet. Add expenses or bills to see them here.");
        return;
    }

    if (ImGui::BeginTable("##tx_tbl", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Type",        ImGuiTableColumnFlags_WidthFixed,   72);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Category",    ImGuiTableColumnFlags_WidthFixed,  108);
        ImGui::TableSetupColumn("Amount",      ImGuiTableColumnFlags_WidthFixed,   82);
        ImGui::TableSetupColumn("Date",        ImGuiTableColumnFlags_WidthFixed,   92);
        ImGui::TableSetupColumn("Method/Status", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableHeadersRow();
        for (auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (r.isBill)
                ImGui::TextColored(kYellow, "Bill");
            else
                ImGui::TextColored(kAccent, "Expense");
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.name.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.category.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(kRed, "%s", fmtMoney(r.amount).c_str());
            ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(r.date.c_str());
            ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(r.detail.c_str());
        }
        ImGui::EndTable();
    }
}

// ─── Finance Assistant ────────────────────────────────────────────────────────
static void RenderAssistant(FinanceAssistant& fa) {
    static char   inputBuf[256] = "";
    static size_t lastSize      = 0;

    // Header bar — same dark style as reminder block
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.13f, 0.15f, 0.21f, 1.0f});
    ImGui::BeginChild("##fa_hdr", {-1.0f, 58.0f}, false);
    ImGui::SetCursorPos({14.0f, 10.0f});
    ImGui::TextColored({0.60f, 0.68f, 0.82f, 1.0f}, "SMILE");
    ImGui::SetCursorPos({14.0f, 32.0f});
    ImGui::TextColored({0.45f, 0.52f, 0.68f, 1.0f},
                       "Your personal finance assistant — always here to help");
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Chat log
    const float inputH = 46.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWhite);
    ImGui::BeginChild("##chat", {-1.0f, ImGui::GetContentRegionAvail().y - inputH - 6.0f}, true);
    ImGui::Spacing();

    for (auto& line : g_chat) {
        bool isUser = (line.rfind("You: ", 0) == 0);
        if (isUser) {
            std::string msg = line.substr(5);
            float tw    = ImGui::CalcTextSize(msg.c_str()).x + 4.0f;
            float avail = ImGui::GetContentRegionAvail().x - 8.0f;
            if (tw < avail) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - tw));
            ImGui::TextColored(kAccent, "%s", msg.c_str());
        } else {
            std::string msg = line;
            if (msg.size() >= 2 && msg[0] == ' ' && msg[1] == ' ') msg = msg.substr(2);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
            ImGui::TextWrapped("%s", msg.c_str());
        }
        ImGui::Spacing();
    }

    if (g_chat.size() != lastSize) { ImGui::SetScrollHereY(1.0f); lastSize = g_chat.size(); }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Input row
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-96.0f);
    bool submit = ImGui::InputText("##qa", inputBuf, sizeof(inputBuf),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0, 8);
    if (PrimaryButton("Send") || submit) {
        if (inputBuf[0] != '\0') {
            g_chat.push_back("You: " + std::string(inputBuf));
            g_chat.push_back("  " + fa.ask(inputBuf));
            inputBuf[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
}

// ─── User Info ────────────────────────────────────────────────────────────────
static void RenderUserInfo(BudgetManager& mgr) {
    if (g_currentUser < 0 || g_currentUser >= (int)g_users.size()) return;
    auto& u = g_users[g_currentUser];

    ImGui::TextColored(kAccent, "User Information");
    ImGui::Separator();
    ImGui::Spacing();

    // Profile card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWhite);
    ImGui::BeginChild("##profile_card", {360.0f, 130.0f}, true);
    ImGui::Spacing();
    ImGui::Text("Username");     ImGui::SameLine(130); ImGui::TextColored(kDark, "%s", u.username.c_str());
    ImGui::Spacing();
    ImGui::Text("Display Name"); ImGui::SameLine(130); ImGui::TextColored(kDark, "%s", u.displayName.c_str());
    ImGui::Spacing();
    ImGui::Text("Budget Status"); ImGui::SameLine(130);
    {
        std::string st = mgr.getMonthlyBudgetStatus();
        ImVec4 sc = (st == "Over Budget") ? kRed : (st == "Approaching Limit") ? kYellow : kGreen;
        ImGui::TextColored(sc, "%s", st.c_str());
    }
    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("Change Display Name");
    ImGui::Spacing();
    static char newName[64] = "";
    ImGui::SetNextItemWidth(240);
    ImGui::InputText("##nn", newName, sizeof(newName));
    ImGui::SameLine();
    if (ImGui::Button("Save") && newName[0] != '\0') {
        u.displayName = newName;
        newName[0] = '\0';
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Logout
    ImGui::PushStyleColor(ImGuiCol_Button,         kRed);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f,0.20f,0.20f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,           kWhite);
    if (ImGui::Button("Log Out", {130.0f, 36.0f})) {
        mgr.saveToDir(userDir(u.username));
        mgr.clear();
        g_currentUser = -1;
        g_appState    = AppState::Login;
        g_screen      = Screen::Assistant;
        g_chat.clear();
        g_chat.push_back("  Hi, I am Smile. Your personal finance assistant.");
        g_chat.push_back("  I can help you track your spending, check how your budget is holding up,");
        g_chat.push_back("  spot upcoming bills before they surprise you, and give you a clear");
        g_chat.push_back("  picture of where your money is going. Just ask me anything!");
    }
    ImGui::PopStyleColor(3);
}

// ─── Main app window ──────────────────────────────────────────────────────────
static void RenderMainWindow(BudgetManager& mgr, FinanceAssistant& fa) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    // ── Top bar ──
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.13f, 0.15f, 0.21f, 1.0f});
    ImGui::BeginChild("##topbar", {-1.0f, 48.0f}, false);
    ImGui::SetCursorPos({14.0f, 13.0f});
    ImGui::TextColored({0.60f, 0.68f, 0.82f, 1.0f}, "Personal Budget System");

    // Show logged-in user name
    if (g_currentUser >= 0 && g_currentUser < (int)g_users.size()) {
        ImGui::SameLine();
        ImGui::TextColored(kWhite, " |  %s", g_users[g_currentUser].displayName.c_str());
    }

    // Smile button — right-aligned, same dark color as reminder block
    float btnW = 110.0f;
    ImGui::SameLine(io.DisplaySize.x - btnW - 28.0f);
    ImGui::SetCursorPosY(10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        (g_screen == Screen::Assistant)
            ? ImVec4{0.22f, 0.28f, 0.42f, 1.0f}
            : ImVec4{0.13f, 0.15f, 0.21f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.28f, 0.42f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.28f, 0.36f, 0.54f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,           kWhite);
    if (ImGui::Button("  SMILE  ", {btnW, 28.0f}))
        g_screen = Screen::Assistant;
    ImGui::PopStyleColor(4);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Body ──
    const float sideW = 162.0f;
    const float bodyH = ImGui::GetContentRegionAvail().y;

    // Sidebar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.90f, 0.92f, 0.96f, 1.0f});
    ImGui::BeginChild("##side", {sideW, bodyH}, false);
    ImGui::Spacing();

    struct Nav { const char* label; Screen sc; };
    constexpr Nav navItems[] = {
        { "  Dashboard",    Screen::Dashboard    },
        { "  Incomes",      Screen::Incomes      },
        { "  Budget",       Screen::Budget       },
        { "  Expenses",     Screen::Expenses     },
        { "  Bills",        Screen::Bills        },
        { "  Transactions", Screen::Transactions },
    };
    for (auto& n : navItems) {
        bool active = (g_screen == n.sc);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,        kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f,0.54f,0.96f,1.0f});
            ImGui::PushStyleColor(ImGuiCol_Text,          kWhite);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f,0.44f,0.86f,0.15f});
            ImGui::PushStyleColor(ImGuiCol_Text,          kDark);
        }
        if (ImGui::Button(n.label, {sideW - 10.0f, 36.0f})) g_screen = n.sc;
        ImGui::PopStyleColor(3);
        ImGui::Spacing();
    }

    // ── Reminder block (dark, always visible) ──
    const float remH   = 148.0f;
    const float uiBarH =  54.0f;   // User Info button + small padding
    ImGui::SetCursorPosY(bodyH - uiBarH - remH - 4.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.13f, 0.15f, 0.21f, 1.0f});
    ImGui::BeginChild("##rem_block", {sideW - 10.0f, remH}, false);

    ImGui::SetCursorPos({8.0f, 8.0f});
    ImGui::TextColored({0.60f, 0.68f, 0.82f, 1.0f}, "REMINDERS");

    // thin divider
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine({p.x, p.y}, {p.x + sideW - 10.0f, p.y},
                IM_COL32(60, 70, 100, 255), 1.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

    Date today = Date::today();
    auto overdue = mgr.getReminders().getOverdueBills(today);
    Bill nextBill;
    bool hasNext = mgr.getReminders().peekNextUnpaidBill(nextBill);

    // Overdue badge
    if (!overdue.empty()) {
        ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored({1.0f, 0.38f, 0.38f, 1.0f},
                           "! %d overdue", (int)overdue.size());
    }

    ImGui::SetCursorPosX(8.0f);
    ImGui::TextColored({0.65f, 0.72f, 0.88f, 1.0f}, "Next Due:");

    if (hasNext) {
        // Truncate bill name to fit sidebar
        std::string name = nextBill.getBillName();
        if (name.size() > 15) name = name.substr(0, 14) + "..";
        ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored(kWhite, "%s", name.c_str());

        char amtBuf[24];
        snprintf(amtBuf, sizeof(amtBuf), "$%.2f", nextBill.getAmount());
        ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored({0.35f, 0.88f, 0.55f, 1.0f}, "%s", amtBuf);

        // Days until due
        int days = nextBill.getDueDate().daysFrom(today);
        ImGui::SetCursorPosX(8.0f);
        if (days < 0)
            ImGui::TextColored({1.0f,0.38f,0.38f,1.0f}, "%s (overdue)",
                               nextBill.getDueDate().toString().c_str());
        else if (days == 0)
            ImGui::TextColored({1.0f,0.80f,0.20f,1.0f}, "%s (today)",
                               nextBill.getDueDate().toString().c_str());
        else
            ImGui::TextColored({0.65f,0.72f,0.88f,1.0f}, "%s  (%d days)",
                               nextBill.getDueDate().toString().c_str(), days);
    } else {
        ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored({0.55f, 0.62f, 0.75f, 1.0f}, "No upcoming bills");
        ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored(kWhite, "$0.00");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── User Info at very bottom ──
    ImGui::SetCursorPosY(bodyH - uiBarH + 4.0f);
    ImGui::Separator();
    ImGui::Spacing();
    bool uiActive = (g_screen == Screen::UserInfo);
    if (uiActive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f,0.54f,0.96f,1.0f});
        ImGui::PushStyleColor(ImGuiCol_Text,          kWhite);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f,0.44f,0.86f,0.15f});
        ImGui::PushStyleColor(ImGuiCol_Text,          kDark);
    }
    if (ImGui::Button("  User Info", {sideW - 10.0f, 36.0f})) g_screen = Screen::UserInfo;
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Content area
    ImGui::BeginChild("##content", {0.0f, bodyH}, false);
    ImGui::Spacing();
    switch (g_screen) {
        case Screen::Dashboard:    RenderDashboard(mgr);    break;
        case Screen::Expenses:     RenderExpenses(mgr);     break;
        case Screen::Incomes:      RenderIncomes(mgr);      break;
        case Screen::Bills:        RenderBills(mgr);        break;
        case Screen::Budget:       RenderBudget(mgr);       break;
        case Screen::Transactions: RenderTransactions(mgr); break;
        case Screen::Assistant:    RenderAssistant(fa);     break;
        case Screen::UserInfo:     RenderUserInfo(mgr);     break;
    }
    ImGui::EndChild();
    ImGui::End();
}

// ─── Login screen ─────────────────────────────────────────────────────────────
static void RenderLogin(BudgetManager& mgr) {
    static char   userBuf[64] = "";
    static char   passBuf[64] = "";
    static std::string errMsg;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##login_bg", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    const float cardW = 380.0f, cardH = 330.0f;
    ImGui::SetCursorPos({(io.DisplaySize.x - cardW) * 0.5f,
                         (io.DisplaySize.y - cardH) * 0.5f});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWhite);
    ImGui::BeginChild("##lcard", {cardW, cardH}, true);
    ImGui::Spacing(); ImGui::Spacing();

    // Header
    auto centerText = [&](const char* t) {
        ImGui::SetCursorPosX((cardW - ImGui::CalcTextSize(t).x) * 0.5f);
    };
    centerText("Personal Budget System");
    ImGui::TextColored(kAccent, "Personal Budget System");
    ImGui::Spacing();
    centerText("Sign in to your account");
    ImGui::TextColored(kMuted, "Sign in to your account");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    float fieldW = cardW - 24.0f;
    ImGui::Text("Username");
    ImGui::SetNextItemWidth(fieldW);
    ImGui::InputText("##lu", userBuf, sizeof(userBuf));
    ImGui::Spacing();
    ImGui::Text("Password");
    ImGui::SetNextItemWidth(fieldW);
    bool enter = ImGui::InputText("##lp", passBuf, sizeof(passBuf),
                                  ImGuiInputTextFlags_Password |
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Spacing();

    if (!errMsg.empty()) { ImGui::TextColored(kRed, "%s", errMsg.c_str()); ImGui::Spacing(); }

    // Login button
    ImGui::PushStyleColor(ImGuiCol_Button,         kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f,0.54f,0.96f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,           kWhite);
    bool doLogin = ImGui::Button("Log In", {fieldW, 36.0f}) || enter;
    ImGui::PopStyleColor(3);

    if (doLogin) {
        errMsg.clear();
        bool ok = false;
        for (int i = 0; i < (int)g_users.size(); ++i) {
            if (g_users[i].username == userBuf && g_users[i].password == passBuf) {
                g_currentUser = i;
                g_appState    = AppState::Main;
                userBuf[0] = passBuf[0] = '\0';
                ok = true; break;
            }
        }
        if (ok) mgr.loadFromDir(userDir(g_users[g_currentUser].username));
        else    errMsg = "Incorrect username or password.";
    }

    // Switch to Register
    ImGui::Spacing();
    const char* regLine = "Don't have an account?";
    ImGui::SetCursorPosX((cardW - ImGui::CalcTextSize(regLine).x - 58.0f) * 0.5f);
    ImGui::TextColored(kMuted, "%s", regLine);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text,   kAccent);
    ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
    if (ImGui::SmallButton("Register")) { g_appState = AppState::Register; errMsg.clear(); }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}

// ─── Register screen ──────────────────────────────────────────────────────────
static void RenderRegister(BudgetManager& mgr) { (void)mgr;
    static char   userBuf[64]    = "";
    static char   nameBuf[64]    = "";
    static char   passBuf[64]    = "";
    static char   confBuf[64]    = "";
    static std::string errMsg;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##reg_bg", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    const float cardW = 380.0f, cardH = 430.0f;
    ImGui::SetCursorPos({(io.DisplaySize.x - cardW) * 0.5f,
                         (io.DisplaySize.y - cardH) * 0.5f});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWhite);
    ImGui::BeginChild("##rcard", {cardW, cardH}, true);
    ImGui::Spacing(); ImGui::Spacing();

    auto centerText = [&](const char* t) {
        ImGui::SetCursorPosX((cardW - ImGui::CalcTextSize(t).x) * 0.5f);
    };
    centerText("Create Account");
    ImGui::TextColored(kAccent, "Create Account");
    ImGui::Spacing();
    centerText("Start managing your budget");
    ImGui::TextColored(kMuted, "Start managing your budget");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    float fieldW = cardW - 24.0f;
    ImGui::Text("Username");
    ImGui::SetNextItemWidth(fieldW);
    ImGui::InputText("##ru", userBuf, sizeof(userBuf));
    ImGui::Spacing();
    ImGui::Text("Display Name  (optional)");
    ImGui::SetNextItemWidth(fieldW);
    ImGui::InputText("##rn", nameBuf, sizeof(nameBuf));
    ImGui::Spacing();
    ImGui::Text("Password");
    ImGui::SetNextItemWidth(fieldW);
    ImGui::InputText("##rp", passBuf, sizeof(passBuf), ImGuiInputTextFlags_Password);
    ImGui::Spacing();
    ImGui::Text("Confirm Password");
    ImGui::SetNextItemWidth(fieldW);
    bool enter = ImGui::InputText("##rc", confBuf, sizeof(confBuf),
                                  ImGuiInputTextFlags_Password |
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Spacing();

    if (!errMsg.empty()) { ImGui::TextColored(kRed, "%s", errMsg.c_str()); ImGui::Spacing(); }

    ImGui::PushStyleColor(ImGuiCol_Button,         kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f,0.54f,0.96f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,           kWhite);
    bool doReg = ImGui::Button("Create Account", {fieldW, 36.0f}) || enter;
    ImGui::PopStyleColor(3);

    if (doReg) {
        errMsg.clear();
        if (userBuf[0] == '\0')               errMsg = "Username cannot be empty.";
        else if (passBuf[0] == '\0')           errMsg = "Password cannot be empty.";
        else if (std::string(passBuf) != confBuf) errMsg = "Passwords do not match.";
        else {
            for (auto& u : g_users)
                if (u.username == userBuf) { errMsg = "Username already taken."; break; }
            if (errMsg.empty()) {
                std::string dname = (nameBuf[0] != '\0') ? nameBuf : userBuf;
                g_users.push_back({userBuf, dname, passBuf});
                g_currentUser = (int)g_users.size() - 1;
                g_appState    = AppState::Main;
                saveUsers();
                std::string ud = userDir(userBuf);
                mkdir(ud.c_str(), 0755);
                userBuf[0] = nameBuf[0] = passBuf[0] = confBuf[0] = '\0';
            }
        }
    }

    ImGui::Spacing();
    const char* loginLine = "Already have an account?";
    ImGui::SetCursorPosX((cardW - ImGui::CalcTextSize(loginLine).x - 50.0f) * 0.5f);
    ImGui::TextColored(kMuted, "%s", loginLine);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text,   kAccent);
    ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
    if (ImGui::SmallButton("Log In")) { g_appState = AppState::Login; errMsg.clear(); }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* win = glfwCreateWindow(1280, 800, "Personal Budget System", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsLight();
    ImGuiStyle& sty = ImGui::GetStyle();
    sty.WindowRounding    = 6.0f;
    sty.ChildRounding     = 6.0f;
    sty.FrameRounding     = 5.0f;
    sty.GrabRounding      = 5.0f;
    sty.ScrollbarRounding = 5.0f;
    sty.PopupRounding     = 5.0f;
    sty.ItemSpacing       = {8.0f, 7.0f};
    sty.FramePadding      = {8.0f, 5.0f};
    sty.WindowPadding     = {12.0f, 12.0f};
    sty.Colors[ImGuiCol_WindowBg]         = {0.95f, 0.96f, 0.98f, 1.0f};
    sty.Colors[ImGuiCol_ChildBg]          = {0.97f, 0.97f, 0.99f, 1.0f};
    sty.Colors[ImGuiCol_FrameBg]          = {0.88f, 0.90f, 0.95f, 1.0f};
    sty.Colors[ImGuiCol_FrameBgHovered]   = {0.80f, 0.85f, 0.96f, 1.0f};
    sty.Colors[ImGuiCol_FrameBgActive]    = {0.74f, 0.80f, 0.96f, 1.0f};
    sty.Colors[ImGuiCol_Button]           = {0.18f, 0.44f, 0.86f, 0.80f};
    sty.Colors[ImGuiCol_ButtonHovered]    = {0.26f, 0.54f, 0.96f, 1.0f};
    sty.Colors[ImGuiCol_ButtonActive]     = {0.12f, 0.36f, 0.76f, 1.0f};
    sty.Colors[ImGuiCol_Header]           = {0.18f, 0.44f, 0.86f, 0.20f};
    sty.Colors[ImGuiCol_HeaderHovered]    = {0.18f, 0.44f, 0.86f, 0.35f};
    sty.Colors[ImGuiCol_Separator]        = {0.70f, 0.76f, 0.86f, 1.0f};
    sty.Colors[ImGuiCol_TableBorderLight] = {0.70f, 0.76f, 0.86f, 1.0f};
    sty.Colors[ImGuiCol_TableBorderStrong]= {0.50f, 0.58f, 0.76f, 1.0f};
    sty.Colors[ImGuiCol_TableRowBg]       = {0.98f, 0.98f, 1.00f, 1.0f};
    sty.Colors[ImGuiCol_TableRowBgAlt]    = {0.92f, 0.94f, 0.98f, 1.0f};
    sty.Colors[ImGuiCol_Text]             = {0.10f, 0.10f, 0.14f, 1.0f};
    sty.Colors[ImGuiCol_PopupBg]          = {0.98f, 0.98f, 1.00f, 1.0f};

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    BudgetManager    mgr;
    FinanceAssistant fa(mgr);

    loadUsers();
    g_chat.push_back("  Hi, I am Smile. Your personal finance assistant.");
    g_chat.push_back("  I can help you track your spending, check how your budget is holding up,");
    g_chat.push_back("  spot upcoming bills before they surprise you, and give you a clear");
    g_chat.push_back("  picture of where your money is going. Just ask me anything!");

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if      (g_appState == AppState::Login)    RenderLogin(mgr);
        else if (g_appState == AppState::Register) RenderRegister(mgr);
        else                                        RenderMainWindow(mgr, fa);

        ImGui::Render();
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.95f, 0.96f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    // Save current user's data when the window is closed
    if (g_currentUser >= 0 && g_currentUser < (int)g_users.size())
        mgr.saveToDir(userDir(g_users[g_currentUser].username));

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
