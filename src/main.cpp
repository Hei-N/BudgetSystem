// main.cpp -- Finance Manager
// Full ImGui UI: Login/Register screens + 5-panel main view.
// SQLite persistence via Database class; F11 fullscreen toggle.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#include "BudgetManager.h"
#include "Database.h"
#include "Expense.h"
#include "Bill.h"
#include "Date.h"
#include "CategoryInfo.h"

// ---------------------------------------------------------------------------
// App state machine
// ---------------------------------------------------------------------------
enum class AppState { LOGIN, REGISTER, MAIN };

// ---------------------------------------------------------------------------
// Load all persisted data for a user into BudgetManager
// ---------------------------------------------------------------------------
static void loadUserData(Database& db, BudgetManager& manager, int userId) {
    for (auto& e  : db.loadExpenses    (userId)) manager.addExpense(e);
    for (auto& b  : db.loadBills       (userId)) manager.addBill(b);
    for (auto& bl : db.loadBudgetLimits(userId))
        manager.setBudgetLimit(bl.category, bl.limit, bl.month, bl.year);
}

// ---------------------------------------------------------------------------
// GLFW / fullscreen
// ---------------------------------------------------------------------------
static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static bool s_isFullscreen = false;
static int  s_savedX = 100, s_savedY = 100;
static int  s_savedW = 1280, s_savedH = 720;

static void key_callback(GLFWwindow* window, int key, int /*sc*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        if (!s_isFullscreen) {
            glfwGetWindowPos (window, &s_savedX, &s_savedY);
            glfwGetWindowSize(window, &s_savedW, &s_savedH);
            GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0,
                                 mode->width, mode->height, mode->refreshRate);
            s_isFullscreen = true;
        } else {
            glfwSetWindowMonitor(window, nullptr,
                                 s_savedX, s_savedY, s_savedW, s_savedH, 0);
            s_isFullscreen = false;
        }
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && s_isFullscreen) {
        glfwSetWindowMonitor(window, nullptr,
                             s_savedX, s_savedY, s_savedW, s_savedH, 0);
        s_isFullscreen = false;
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Finance Manager", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle baseStyle = ImGui::GetStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // ---- Database ----
    Database db("finance_manager.db");

    // ---- App state ----
    AppState    appState      = AppState::LOGIN;
    int         currentUserId = -1;
    std::string currentUsername;

    char loginUserBuf[64] = "";
    char loginPassBuf[64] = "";
    char regUserBuf[64]   = "";
    char regPassBuf[64]   = "";
    char regPass2Buf[64]  = "";
    std::string authError;
    std::string authSuccess;

    // ---- BudgetManager (heap-allocated so we can replace it on logout) ----
    auto manager = std::make_unique<BudgetManager>();

    // ---- Real clock ----
    time_t now_t  = std::time(nullptr);
    std::tm* lt   = std::localtime(&now_t);
    const int todayDay   = lt->tm_mday;
    const int todayMonth = lt->tm_mon + 1;
    const int todayYear  = lt->tm_year + 1900;

    // ---- Main panel state ----
    char   expCategory[64]     = "";
    char   expDescription[128] = "";
    int    expDay = todayDay, expMonth = todayMonth, expYear = todayYear;
    double expAmount = 0.0;
    std::string lastExpStatus;

    char   billName[64] = "";
    double billAmount   = 0.0;
    int    billDay = todayDay, billMonth = todayMonth, billYear = todayYear;

    char   catName[64] = "";
    double catLimit    = 0.0;
    int    catMonth = todayMonth, catYear = todayYear;

    int viewMonth = todayMonth;
    int viewYear  = todayYear;
    const int maxMonth = todayMonth, maxYear = todayYear;
    const int minMonth = todayMonth, minYear = todayYear - 2;

    static const char* kMonthNames[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };

    const ImGuiWindowFlags fixedFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    // ---- Main loop ----
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int winW, winH;
        glfwGetWindowSize(window, &winW, &winH);
        float W = static_cast<float>(winW);
        float H = static_cast<float>(winH);

        float scale = (W / 1280.0f < H / 720.0f) ? W / 1280.0f : H / 720.0f;
        if (scale < 0.5f) scale = 0.5f;
        if (scale > 4.0f) scale = 4.0f;
        io.FontGlobalScale = scale;
        ImGui::GetStyle() = baseStyle;
        ImGui::GetStyle().ScaleAllSizes(scale);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ================================================================
        // LOGIN screen
        // ================================================================
        if (appState == AppState::LOGIN) {
            float panelW = 400.0f * scale;
            float panelH = 220.0f * scale;
            ImGui::SetNextWindowPos(ImVec2((W - panelW) * 0.5f, (H - panelH) * 0.5f),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
            ImGui::Begin("##login", nullptr,
                fixedFlags | ImGuiWindowFlags_NoTitleBar);

            ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize("Finance Manager").x) * 0.5f);
            ImGui::Text("Finance Manager");
            ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize("Sign in to your account").x) * 0.5f);
            ImGui::TextDisabled("Sign in to your account");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##lu", "Username",
                                     loginUserBuf, sizeof(loginUserBuf));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##lp", "Password",
                                     loginPassBuf, sizeof(loginPassBuf),
                                     ImGuiInputTextFlags_Password);

            ImGui::Spacing();
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float btnW    = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("Login", ImVec2(btnW, 0))) {
                int id = db.loginUser(loginUserBuf, loginPassBuf);
                if (id >= 0) {
                    currentUserId   = id;
                    currentUsername = loginUserBuf;
                    loadUserData(db, *manager, id);
                    appState    = AppState::MAIN;
                    authError.clear();
                    authSuccess.clear();
                } else {
                    authError = "Invalid username or password.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Create Account", ImVec2(btnW, 0))) {
                authError.clear();
                authSuccess.clear();
                std::memset(regUserBuf,  0, sizeof(regUserBuf));
                std::memset(regPassBuf,  0, sizeof(regPassBuf));
                std::memset(regPass2Buf, 0, sizeof(regPass2Buf));
                appState = AppState::REGISTER;
            }

            if (!authError.empty())
                ImGui::TextColored({1.f,0.3f,0.3f,1.f}, "%s", authError.c_str());
            if (!authSuccess.empty())
                ImGui::TextColored({0.3f,1.f,0.3f,1.f}, "%s", authSuccess.c_str());

            ImGui::End();
        }

        // ================================================================
        // REGISTER screen
        // ================================================================
        else if (appState == AppState::REGISTER) {
            float panelW = 400.0f * scale;
            float panelH = 260.0f * scale;
            ImGui::SetNextWindowPos(ImVec2((W - panelW) * 0.5f, (H - panelH) * 0.5f),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
            ImGui::Begin("##register", nullptr,
                fixedFlags | ImGuiWindowFlags_NoTitleBar);

            ImGui::SetCursorPosX((panelW - ImGui::CalcTextSize("Create Account").x) * 0.5f);
            ImGui::Text("Create Account");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##ru", "Username",
                                     regUserBuf, sizeof(regUserBuf));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##rp", "Password",
                                     regPassBuf, sizeof(regPassBuf),
                                     ImGuiInputTextFlags_Password);

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##rp2", "Confirm Password",
                                     regPass2Buf, sizeof(regPass2Buf),
                                     ImGuiInputTextFlags_Password);

            ImGui::Spacing();
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float btnW    = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("Register", ImVec2(btnW, 0))) {
                authError.clear();
                if (regUserBuf[0] == '\0') {
                    authError = "Username cannot be empty.";
                } else if (regPassBuf[0] == '\0') {
                    authError = "Password cannot be empty.";
                } else if (std::strcmp(regPassBuf, regPass2Buf) != 0) {
                    authError = "Passwords do not match.";
                } else {
                    int id = db.registerUser(regUserBuf, regPassBuf);
                    if (id >= 0) {
                        authSuccess = "Account created! Please log in.";
                        authError.clear();
                        std::memset(loginUserBuf, 0, sizeof(loginUserBuf));
                        std::memset(loginPassBuf, 0, sizeof(loginPassBuf));
                        appState = AppState::LOGIN;
                    } else {
                        authError = "Username already taken.";
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Back to Login", ImVec2(btnW, 0))) {
                authError.clear();
                appState = AppState::LOGIN;
            }

            if (!authError.empty())
                ImGui::TextColored({1.f,0.3f,0.3f,1.f}, "%s", authError.c_str());

            ImGui::End();
        }

        // ================================================================
        // MAIN — 5-panel layout
        // ================================================================
        else {
            float c1W = W * 0.33f;
            float c2W = (W - c1W) * 0.5f;
            float c3W = W - c1W - c2W;
            float topH = H * 0.5f;
            float botH = H - topH;

            // ===== Panel 1: Budget Overview =====
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(c1W, topH), ImGuiCond_Always);
            ImGui::Begin("Budget Overview", nullptr, fixedFlags);
            {
                // User info + logout
                ImGui::Text("User: %s", currentUsername.c_str());
                ImGui::SameLine();
                float logoutX = c1W - ImGui::CalcTextSize("Logout").x - 20.0f;
                ImGui::SetCursorPosX(logoutX);
                if (ImGui::SmallButton("Logout")) {
                    BudgetManager::clearStatics();
                    manager       = std::make_unique<BudgetManager>();
                    currentUserId = -1;
                    currentUsername.clear();
                    std::memset(loginUserBuf, 0, sizeof(loginUserBuf));
                    std::memset(loginPassBuf, 0, sizeof(loginPassBuf));
                    lastExpStatus.clear();
                    appState = AppState::LOGIN;
                }
                ImGui::Separator();

                // Month navigator
                bool atMin = (viewYear < minYear) || (viewYear == minYear && viewMonth <= minMonth);
                bool atMax = (viewYear > maxYear) || (viewYear == maxYear && viewMonth >= maxMonth);

                if (atMin) ImGui::BeginDisabled();
                if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) {
                    if (--viewMonth < 1) { viewMonth = 12; viewYear--; }
                }
                if (atMin) ImGui::EndDisabled();

                ImGui::SameLine();
                ImGui::Text("%s %d", kMonthNames[viewMonth - 1], viewYear);
                ImGui::SameLine();

                if (atMax) ImGui::BeginDisabled();
                if (ImGui::ArrowButton("##next", ImGuiDir_Right)) {
                    if (++viewMonth > 12) { viewMonth = 1; viewYear++; }
                }
                if (atMax) ImGui::EndDisabled();
                ImGui::Separator();

                auto snapshot = manager->getBudgetSnapshotForMonth(viewMonth, viewYear);
                if (ImGui::BeginTable("BudgetTbl", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Limit",    ImGuiTableColumnFlags_WidthFixed, 60.f);
                    ImGui::TableSetupColumn("Spent",    ImGuiTableColumnFlags_WidthFixed, 60.f);
                    ImGui::TableSetupColumn("% Used",   ImGuiTableColumnFlags_WidthFixed, 55.f);
                    ImGui::TableSetupColumn("Status",   ImGuiTableColumnFlags_WidthFixed, 70.f);
                    ImGui::TableHeadersRow();
                    for (auto& info : snapshot) {
                        double pct = (info.budgetLimit > 0.0)
                                     ? (info.totalSpent / info.budgetLimit * 100.0) : 0.0;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", info.name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        if (info.budgetLimit == 0.0) ImGui::TextDisabled("-");
                        else                         ImGui::Text("$%.0f", info.budgetLimit);
                        ImGui::TableSetColumnIndex(2);
                        if (info.budgetLimit == 0.0) ImGui::TextDisabled("-");
                        else                         ImGui::Text("$%.0f", info.totalSpent);
                        ImGui::TableSetColumnIndex(3);
                        if (info.budgetLimit == 0.0) ImGui::TextDisabled("-");
                        else                         ImGui::Text("%.0f%%", pct);
                        ImGui::TableSetColumnIndex(4);
                        if      (info.budgetLimit == 0.0) ImGui::TextColored({0.6f,0.6f,0.6f,1.f}, "not set");
                        else if (pct > 100.0)             ImGui::TextColored({1.f,0.2f,0.2f,1.f},  "EXCEEDED");
                        else if (pct >= 80.0)             ImGui::TextColored({1.f,0.85f,0.f,1.f},  "WARNING");
                        else                              ImGui::TextColored({0.2f,0.9f,0.2f,1.f}, "OK");
                    }
                    ImGui::EndTable();
                }
                if (snapshot.empty()) ImGui::TextDisabled("No categories yet.");
            }
            ImGui::End();

            // ===== Panel 1b: (placeholder, under Budget Overview) =====
            ImGui::SetNextWindowPos(ImVec2(0, topH), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(c1W, botH), ImGuiCond_Always);
            ImGui::Begin("New Panel##c1b", nullptr, fixedFlags);
            {
            }
            ImGui::End();

            // ===== Panel 2: Set Budget =====
            ImGui::SetNextWindowPos(ImVec2(c1W, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(c2W, topH), ImGuiCond_Always);
            ImGui::Begin("Set Budget", nullptr, fixedFlags);
            {
                ImGui::InputText("Category##c",       catName, sizeof(catName));
                ImGui::InputDouble("Budget Limit##c", &catLimit, 0.0, 0.0, "$%.2f");
                ImGui::Text("Month/Year:"); ImGui::SameLine();
                ImGui::SetNextItemWidth(50); ImGui::InputInt("M##c", &catMonth, 0, 0); ImGui::SameLine();
                ImGui::SetNextItemWidth(70); ImGui::InputInt("Y##c", &catYear,  0, 0);
                if (catMonth < 1)  catMonth = 1;
                if (catMonth > 12) catMonth = 12;
                if (ImGui::Button("Set Budget") && catName[0] != '\0' && catLimit > 0.0) {
                    manager->setBudgetLimit(catName, catLimit, catMonth, catYear);
                    db.upsertBudgetLimit(currentUserId, catName, catLimit, catMonth, catYear);
                    catName[0] = '\0';
                    catLimit   = 0.0;
                }
            }
            ImGui::End();

            // ===== Panel 3: Bills =====
            ImGui::SetNextWindowPos(ImVec2(c1W, topH), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(c2W, botH), ImGuiCond_Always);
            ImGui::Begin("Bills", nullptr, fixedFlags);
            {
                ImGui::Text("Pending Bills:");
                ImGui::Separator();
                {
                    auto bills = manager->getAllBills();
                    std::string toRemove;
                    if (bills.empty()) {
                        ImGui::TextDisabled("No pending bills.");
                    } else {
                        if (ImGui::BeginTable("BillsTbl", 4,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Name");
                            ImGui::TableSetupColumn("Amount");
                            ImGui::TableSetupColumn("Due");
                            ImGui::TableSetupColumn("##action");
                            ImGui::TableHeadersRow();
                            for (auto& b : bills) {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", b.name.c_str());
                                ImGui::TableSetColumnIndex(1); ImGui::Text("$%.2f", b.amountDue);
                                ImGui::TableSetColumnIndex(2); ImGui::Text("%s", b.dueDate.toString().c_str());
                                ImGui::TableSetColumnIndex(3);
                                std::string lbl = "Mark as Paid##" + b.name;
                                if (ImGui::SmallButton(lbl.c_str())) toRemove = b.name;
                            }
                            ImGui::EndTable();
                        }
                    }
                    if (!toRemove.empty()) {
                        manager->removeBill(toRemove, Date(todayDay, todayMonth, todayYear));
                        db.deleteBill(currentUserId, toRemove);
                    }
                }
                ImGui::Separator();
                ImGui::Text("Add Bill:");
                ImGui::InputText("Name##b",     billName,   sizeof(billName));
                ImGui::InputDouble("Amount##b", &billAmount, 0.0, 0.0, "$%.2f");
                ImGui::Text("Due:"); ImGui::SameLine();
                ImGui::SetNextItemWidth(50); ImGui::InputInt("D##b", &billDay,   0, 0); ImGui::SameLine();
                ImGui::SetNextItemWidth(50); ImGui::InputInt("M##b", &billMonth, 0, 0); ImGui::SameLine();
                ImGui::SetNextItemWidth(70); ImGui::InputInt("Y##b", &billYear,  0, 0);
                if (ImGui::Button("Add Bill") && billName[0] != '\0' && billAmount > 0.0) {
                    Bill b;
                    b.name      = billName;
                    b.amountDue = billAmount;
                    b.dueDate   = Date(billDay, billMonth, billYear);
                    manager->addBill(b);
                    db.insertBill(currentUserId, b);
                    billName[0] = '\0';
                    billAmount  = 0.0;
                }
            }
            ImGui::End();

            // ===== Panel 4: Log Expense =====
            ImGui::SetNextWindowPos(ImVec2(c1W + c2W, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(c3W, topH), ImGuiCond_Always);
            ImGui::Begin("Log Expense", nullptr, fixedFlags);
            {
                ImGui::InputText("Category##e",    expCategory,    sizeof(expCategory));
                ImGui::InputText("Description##e", expDescription, sizeof(expDescription));
                ImGui::InputDouble("Amount##e", &expAmount, 0.0, 0.0, "$%.2f");
                ImGui::Text("Date:"); ImGui::SameLine();
                ImGui::SetNextItemWidth(50); ImGui::InputInt("D##e", &expDay,   0, 0); ImGui::SameLine();
                ImGui::SetNextItemWidth(50); ImGui::InputInt("M##e", &expMonth, 0, 0); ImGui::SameLine();
                ImGui::SetNextItemWidth(70); ImGui::InputInt("Y##e", &expYear,  0, 0);

                if (ImGui::Button("Log Expense") && expAmount > 0.0 && expCategory[0] != '\0') {
                    Expense e;
                    e.category    = expCategory;
                    e.description = expDescription;
                    e.amount      = expAmount;
                    e.date        = Date(expDay, expMonth, expYear);
                    manager->addExpense(e);
                    db.insertExpense(currentUserId, e);

                    lastExpStatus.clear();
                    for (auto& ci : manager->getBudgetSnapshotForMonth(e.date.month, e.date.year)) {
                        if (ci.name == e.category) {
                            if (ci.budgetLimit == 0.0) {
                                lastExpStatus = "Logged (no budget set)";
                            } else {
                                double p = ci.totalSpent / ci.budgetLimit * 100.0;
                                if      (p > 100.0) lastExpStatus = "EXCEEDED budget!";
                                else if (p >= 80.0) lastExpStatus = "WARNING: near limit";
                                else                lastExpStatus = "OK";
                            }
                            break;
                        }
                    }
                    expAmount         = 0.0;
                    expCategory[0]    = '\0';
                    expDescription[0] = '\0';
                }
                if (!lastExpStatus.empty()) {
                    if      (lastExpStatus.find("EXCEEDED") != std::string::npos)
                        ImGui::TextColored({1.f,0.2f,0.2f,1.f}, "%s", lastExpStatus.c_str());
                    else if (lastExpStatus.find("WARNING")  != std::string::npos)
                        ImGui::TextColored({1.f,0.85f,0.f,1.f}, "%s", lastExpStatus.c_str());
                    else
                        ImGui::TextColored({0.2f,0.9f,0.2f,1.f}, "%s", lastExpStatus.c_str());
                }
            }
            ImGui::End();

            // ===== Panel 5: Expense History =====
            ImGui::SetNextWindowPos(ImVec2(c1W + c2W, topH), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(c3W, botH), ImGuiCond_Always);
            ImGui::Begin("Expense History", nullptr, fixedFlags);
            {
                auto all = manager->getExpensesByRange(Date(1,1,2000), Date(31,12,2099));
                float tableH = botH - 40.f;
                if (ImGui::BeginTable("ExpTbl", 4,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                    ImVec2(0, tableH))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Date",        ImGuiTableColumnFlags_WidthFixed,   90.f);
                    ImGui::TableSetupColumn("Category",    ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Amount",      ImGuiTableColumnFlags_WidthFixed,   65.f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();
                    for (auto& e : all) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", e.date.toString().c_str());
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", e.category.c_str());
                        ImGui::TableSetColumnIndex(2); ImGui::Text("$%.2f", e.amount);
                        ImGui::TableSetColumnIndex(3); ImGui::Text("%s", e.description.c_str());
                    }
                    ImGui::EndTable();
                }
                if (all.empty()) ImGui::TextDisabled("No expenses yet.");
            }
            ImGui::End();
        } // end MAIN

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}