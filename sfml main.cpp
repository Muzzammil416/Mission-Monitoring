#define _CRT_SECURE_NO_WARNINGS

#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
using namespace std;

enum class FocusedField {
    None,
    Username,
    Password,
    GoalTitle,
    GoalDescription,
    GoalDeadline
};

enum class Screen {
    Login,
    Dashboard,
    CreateGoal,
    ViewGoals,
    GoalHistory,
    PointsMatrix,
    About
};

struct Goal {
    string title;
    string description;
    string deadline;
    string dateCreated;
    bool completed = false;  // Initialize to false
    int points = 0;          // Initialize to 0
    string category;

    // Default constructor
    Goal() = default;

    // Constructor with parameters
    Goal(const string& t, const string& desc, const string& dl, const string& dc,
        bool comp = false, int pts = 0, const string& cat = "General")
        : title(t), description(desc), deadline(dl), dateCreated(dc),
        completed(comp), points(pts), category(cat) {}
};

struct TrieNode {
    std::unordered_map<char, TrieNode*> children;
    bool isEndOfWord;
    std::string fullWord;

    TrieNode() : isEndOfWord(false) {}
};

class Trie {
public:
    TrieNode* root;

    Trie() {
        root = new TrieNode();
    }

    void insert(const std::string& word) {
        TrieNode* curr = root;
        for (char c : word) {
            char lower = tolower(c);
            if (curr->children.find(lower) == curr->children.end()) {
                curr->children[lower] = new TrieNode();
            }
            curr = curr->children[lower];
        }
        curr->isEndOfWord = true;
        curr->fullWord = word;
    }

    std::vector<std::string> getSuggestions(const std::string& prefix) {
        std::vector<std::string> suggestions;
        TrieNode* curr = root;

        // Navigate to prefix
        for (char c : prefix) {
            char lower = tolower(c);
            if (curr->children.find(lower) == curr->children.end()) {
                return suggestions;
            }
            curr = curr->children[lower];
        }

        // DFS to find all words with this prefix
        dfs(curr, suggestions);
        return suggestions;
    }

private:
    void dfs(TrieNode* node, std::vector<std::string>& suggestions) {
        if (node->isEndOfWord) {
            suggestions.push_back(node->fullWord);
        }
        for (auto& child : node->children) {
            dfs(child.second, suggestions);
        }
    }
};

class GoalGraph {
public:
    std::unordered_map<int, std::vector<int>> dependencies; // goal_id -> list of dependent goals
    std::unordered_map<int, std::vector<int>> dependents;   // goal_id -> list of goals it depends on

    void addDependency(int prerequisite, int dependent) {
        dependencies[prerequisite].push_back(dependent);
        dependents[dependent].push_back(prerequisite);
    }

    std::vector<int> getUnlockedGoals(const std::vector<Goal>& goals) {
        std::vector<int> unlocked;
        for (int i = 0; i < goals.size(); i++) {
            if (canUnlock(i, goals)) {
                unlocked.push_back(i);
            }
        }
        return unlocked;
    }

    bool canUnlock(int goalIndex, const std::vector<Goal>& goals) {
        if (dependents.find(goalIndex) == dependents.end()) {
            return true; // No dependencies
        }

        for (int prereq : dependents[goalIndex]) {
            if (prereq < goals.size() && !goals[prereq].completed) {
                return false;
            }
        }
        return true;
    }
};

class SegmentTree {
public:
    std::vector<int> tree;
    int n;

    SegmentTree(int size) {
        n = size;
        tree.resize(4 * n, 0);
    }

    void update(int node, int start, int end, int idx, int val) {
        if (start == end) {
            tree[node] = val;
        }
        else {
            int mid = (start + end) / 2;
            if (idx <= mid) {
                update(2 * node, start, mid, idx, val);
            }
            else {
                update(2 * node + 1, mid + 1, end, idx, val);
            }
            tree[node] = tree[2 * node] + tree[2 * node + 1];
        }
    }

    int query(int node, int start, int end, int l, int r) {
        if (r < start || end < l) {
            return 0;
        }
        if (l <= start && end <= r) {
            return tree[node];
        }
        int mid = (start + end) / 2;
        return query(2 * node, start, mid, l, r) +
            query(2 * node + 1, mid + 1, end, l, r);
    }

    void update(int idx, int val) {
        update(1, 0, n - 1, idx, val);
    }

    int query(int l, int r) {
        return query(1, 0, n - 1, l, r);
    }
};

struct GoalPriority {
    int index = 0;       // Initialize to 0
    int priority = 0;    // Initialize to 0
    std::string deadline;

    // Default constructor
    GoalPriority() = default;

    // Constructor with parameters
    GoalPriority(int idx, int prio, const std::string& dl)
        : index(idx), priority(prio), deadline(dl) {}

    bool operator<(const GoalPriority& other) const {
        return priority < other.priority; // Max heap
    }
};

class MAQSAD {
private:
    sf::RenderWindow window;
    sf::Font font;
    map<string, string> credentials;
    vector<Goal> goals;
    string currentUser;
    Screen currentScreen;
    FocusedField focusedField;

    // Input fields
    string username, password;
    string goalTitle, goalDescription, goalDeadline;
    int selectedGoalIndex;
    Trie goalTrie;
    GoalGraph goalGraph;
    SegmentTree* pointsTree;
    std::priority_queue<GoalPriority> urgentGoals;
    std::vector<std::string> suggestions;
    bool showSuggestions;
    int selectedSuggestion;

    // For binary search functionality
    std::vector<std::pair<std::string, int>> sortedGoalsByDate;
    std::vector<std::pair<int, int>> sortedGoalsByPoints;

    // UI elements
    sf::Text header, subHeader, welcomeText, menuText;
    sf::RectangleShape usernameBox, passwordBox, loginBtn, registerBtn;
    sf::Text usernameText, passwordText, loginLabel, registerLabel;

    // Goal creation elements
    sf::RectangleShape titleBox, descBox, deadlineBox, createBtn, backBtn;
    sf::Text titleText, descText, deadlineText, createLabel, backLabel;

    // Status message
    sf::Text statusMessage;
    sf::Clock statusClock;

public:
    MAQSAD() : window(sf::VideoMode(800, 600), "MAQSAD - Goal Management System"),
        currentScreen(Screen::Login), focusedField(FocusedField::None),
        selectedGoalIndex(-1), pointsTree(nullptr), showSuggestions(false),
        selectedSuggestion(0) {
        window.setFramerateLimit(60);

        if (!font.loadFromFile("ARIAL.ttf")) {
            cout << "Could not load font. Using default font.\n";
        }

        credentials = loadCredentials("credentials.txt");
        goals = loadGoals("goals.txt");
        initializeUI();
        initializeDataStructures();

        // Example: Add some goal dependencies
        if (goals.size() > 2) {
            goalGraph.addDependency(0, 1); // Goal 0 must be completed before Goal 1
            goalGraph.addDependency(1, 2); // Goal 1 must be completed before Goal 2
        }
    }

    ~MAQSAD() {
        delete pointsTree;
    }

    void initializeDataStructures() {
        // Initialize segment tree for points tracking
        pointsTree = new SegmentTree(1000); // Assuming max 1000 goals
        showSuggestions = false;
        selectedSuggestion = 0;

        // Populate trie with existing goal titles
        for (const auto& goal : goals) {
            goalTrie.insert(goal.title);
        }

        // Update points tree
        updatePointsTree();

        // Update priority queue
        updateUrgentGoals();

        // Update sorted arrays for binary search
        updateSortedArrays();
    }

    void updatePointsTree() {
        for (int i = 0; i < goals.size(); i++) {
            pointsTree->update(i, goals[i].completed ? goals[i].points : 0);
        }
    }

    void updateUrgentGoals() {
        // Clear and rebuild priority queue
        urgentGoals = std::priority_queue<GoalPriority>();

        for (int i = 0; i < goals.size(); i++) {
            if (!goals[i].completed) {
                GoalPriority gp(i, calculateUrgency(goals[i]), goals[i].deadline);
                urgentGoals.push(gp);
            }
        }
    }

    int calculateUrgency(const Goal& goal) {
        // Simple urgency calculation based on deadline proximity
        // Higher number = more urgent
        std::string today = getCurrentDate();
        if (goal.deadline < today) return 100; // Overdue
        if (goal.deadline == today) return 90;  // Due today

        // Calculate days until deadline (simplified)
        return 50; // Default urgency
    }

    void updateSortedArrays() {
        sortedGoalsByDate.clear();
        sortedGoalsByPoints.clear();

        for (int i = 0; i < goals.size(); i++) {
            sortedGoalsByDate.push_back({ goals[i].deadline, i });
            sortedGoalsByPoints.push_back({ goals[i].points, i });
        }

        std::sort(sortedGoalsByDate.begin(), sortedGoalsByDate.end());
        std::sort(sortedGoalsByPoints.begin(), sortedGoalsByPoints.end());
    }

    int binarySearchByDate(const std::string& date) {
        int left = 0, right = sortedGoalsByDate.size() - 1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (sortedGoalsByDate[mid].first == date) {
                return sortedGoalsByDate[mid].second;
            }
            else if (sortedGoalsByDate[mid].first < date) {
                left = mid + 1;
            }
            else {
                right = mid - 1;
            }
        }
        return -1;
    }

    int binarySearchByPoints(int points) {
        int left = 0, right = sortedGoalsByPoints.size() - 1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (sortedGoalsByPoints[mid].first == points) {
                return sortedGoalsByPoints[mid].second;
            }
            else if (sortedGoalsByPoints[mid].first < points) {
                left = mid + 1;
            }
            else {
                right = mid - 1;
            }
        }
        return -1;
    }

    int getRangePointsSum(int start, int end) {
        if (start > end || start < 0 || end >= goals.size()) return 0;
        return pointsTree->query(start, end);
    }

    void initializeUI() {
        // Header
        header.setFont(font);
        header.setString("MAQSAD - Goal Management System");
        header.setCharacterSize(28);
        header.setFillColor(sf::Color::Black);
        header.setPosition(150, 20);

        // Sub header
        subHeader.setFont(font);
        subHeader.setString("Login / Register");
        subHeader.setCharacterSize(20);
        subHeader.setFillColor(sf::Color(100, 100, 255));
        subHeader.setPosition(320, 60);

        // Username box
        usernameBox.setSize(sf::Vector2f(400, 40));
        usernameBox.setPosition(200, 120);
        usernameBox.setFillColor(sf::Color::White);
        usernameBox.setOutlineThickness(2);
        usernameBox.setOutlineColor(sf::Color::Black);

        usernameText.setFont(font);
        usernameText.setCharacterSize(20);
        usernameText.setFillColor(sf::Color::Black);
        usernameText.setPosition(210, 125);

        // Password box
        passwordBox.setSize(sf::Vector2f(400, 40));
        passwordBox.setPosition(200, 180);
        passwordBox.setFillColor(sf::Color::White);
        passwordBox.setOutlineThickness(2);
        passwordBox.setOutlineColor(sf::Color::Black);

        passwordText.setFont(font);
        passwordText.setCharacterSize(20);
        passwordText.setFillColor(sf::Color::Black);
        passwordText.setPosition(210, 185);

        // Login button
        loginBtn.setSize(sf::Vector2f(180, 50));
        loginBtn.setPosition(200, 260);
        loginBtn.setFillColor(sf::Color(100, 200, 100));

        loginLabel.setFont(font);
        loginLabel.setString("Login");
        loginLabel.setCharacterSize(22);
        loginLabel.setPosition(250, 270);
        loginLabel.setFillColor(sf::Color::White);

        // Register button
        registerBtn.setSize(sf::Vector2f(180, 50));
        registerBtn.setPosition(420, 260);
        registerBtn.setFillColor(sf::Color(100, 100, 250));

        registerLabel.setFont(font);
        registerLabel.setString("Register");
        registerLabel.setCharacterSize(22);
        registerLabel.setPosition(460, 270);
        registerLabel.setFillColor(sf::Color::White);

        // Welcome text
        welcomeText.setFont(font);
        welcomeText.setCharacterSize(32);
        welcomeText.setFillColor(sf::Color::Black);
        welcomeText.setPosition(120, 100);

        // Menu text
        menuText.setFont(font);
        menuText.setCharacterSize(20);
        menuText.setFillColor(sf::Color::Black);
        menuText.setPosition(120, 180);

        // Goal creation elements
        titleBox.setSize(sf::Vector2f(500, 40));
        titleBox.setPosition(150, 150);
        titleBox.setFillColor(sf::Color::White);
        titleBox.setOutlineThickness(2);
        titleBox.setOutlineColor(sf::Color::Black);
        titleText.setFont(font);
        titleText.setCharacterSize(16);
        titleText.setFillColor(sf::Color::Black);
        titleText.setPosition(160, 155);

        descBox.setSize(sf::Vector2f(500, 100));
        descBox.setPosition(150, 220);
        descBox.setFillColor(sf::Color::White);
        descBox.setOutlineThickness(2);
        descBox.setOutlineColor(sf::Color::Black);
        descText.setFont(font);
        descText.setCharacterSize(16);
        descText.setFillColor(sf::Color::Black);
        descText.setPosition(160, 225);

        deadlineBox.setSize(sf::Vector2f(500, 40));
        deadlineBox.setPosition(150, 350);
        deadlineBox.setFillColor(sf::Color::White);
        deadlineBox.setOutlineThickness(2);
        deadlineBox.setOutlineColor(sf::Color::Black);
        deadlineText.setFont(font);
        deadlineText.setCharacterSize(16);
        deadlineText.setFillColor(sf::Color::Black);
        deadlineText.setPosition(160, 355);

        createBtn.setSize(sf::Vector2f(150, 50));
        createBtn.setPosition(150, 420);
        createBtn.setFillColor(sf::Color(100, 200, 100));
        createLabel.setFont(font);
        createLabel.setString("Create");
        createLabel.setCharacterSize(18);
        createLabel.setFillColor(sf::Color::White);
        createLabel.setPosition(195, 435);

        backBtn.setSize(sf::Vector2f(150, 50));
        backBtn.setPosition(320, 420);
        backBtn.setFillColor(sf::Color(200, 100, 100));
        backLabel.setFont(font);
        backLabel.setString("Back");
        backLabel.setCharacterSize(18);
        backLabel.setFillColor(sf::Color::White);
        backLabel.setPosition(375, 435);

        // Status message
        statusMessage.setFont(font);
        statusMessage.setCharacterSize(18);
        statusMessage.setFillColor(sf::Color::Red);
        statusMessage.setPosition(150, 320);
    }

    map<string, string> loadCredentials(const string& filename) {
        map<string, string> creds;
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                size_t pos = line.find(":");
                if (pos != string::npos) {
                    string user = line.substr(0, pos);
                    string pass = line.substr(pos + 1);
                    creds[user] = pass;
                }
            }
            file.close();
        }
        return creds;
    }

    vector<Goal> loadGoals(const string& filename) {
        vector<Goal> loadedGoals;
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                stringstream ss(line);
                string item;
                Goal goal;
                int field = 0;

                while (getline(ss, item, '|')) {
                    switch (field) {
                    case 0: goal.title = item; break;
                    case 1: goal.description = item; break;
                    case 2: goal.deadline = item; break;
                    case 3: goal.dateCreated = item; break;
                    case 4: goal.completed = (item == "1"); break;
                    case 5: goal.points = stoi(item); break;
                    case 6: goal.category = item; break;
                    }
                    field++;
                }

                if (field >= 7) {
                    loadedGoals.push_back(goal);
                }
            }
            file.close();
        }
        return loadedGoals;
    }

    void saveGoals(const string& filename) {
        ofstream file(filename);
        if (file.is_open()) {
            for (const auto& goal : goals) {
                file << goal.title << "|" << goal.description << "|"
                    << goal.deadline << "|" << goal.dateCreated << "|"
                    << (goal.completed ? "1" : "0") << "|" << goal.points << "|"
                    << goal.category << "\n";
            }
            file.close();
        }
    }

    string getCurrentDate() {
        time_t now = time(0);
        tm timeinfo;
        localtime_s(&timeinfo, &now);  // Use the safer version
        char buffer[80];
        strftime(buffer, 80, "%Y-%m-%d", &timeinfo);
        return string(buffer);
    }

    void showStatusMessage(const string& message, sf::Color color = sf::Color::Red) {
        statusMessage.setString(message);
        statusMessage.setFillColor(color);
        statusClock.restart();
    }

    void handleLoginScreen(sf::Event& event) {
        if (event.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            if (usernameBox.getGlobalBounds().contains(mousePos)) {
                focusedField = FocusedField::Username;
            }
            else if (passwordBox.getGlobalBounds().contains(mousePos)) {
                focusedField = FocusedField::Password;
            }
            else {
                focusedField = FocusedField::None;
            }

            // Login
            if (loginBtn.getGlobalBounds().contains(mousePos)) {
                if (credentials.find(username) != credentials.end() &&
                    credentials[username] == password) {
                    currentUser = username;
                    currentScreen = Screen::Dashboard;
                    showStatusMessage("Login successful!", sf::Color::Green);
                }
                else {
                    showStatusMessage("Invalid login credentials!");
                }
            }

            // Register
            if (registerBtn.getGlobalBounds().contains(mousePos)) {
                if (!username.empty() && !password.empty()) {
                    credentials[username] = password;
                    ofstream file("credentials.txt");
                    for (const auto& cred : credentials) {
                        file << cred.first << ":" << cred.second << "\n";
                    }
                    file.close();
                    showStatusMessage("Registration successful!", sf::Color::Green);
                }
                else {
                    showStatusMessage("Please enter username and password!");
                }
            }
        }

        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char typedChar = static_cast<char>(event.text.unicode);
                if (focusedField == FocusedField::Username) {
                    if (typedChar == '\b' && !username.empty())
                        username.pop_back();
                    else if (typedChar >= 32 && username.length() < 20)
                        username += typedChar;
                }
                else if (focusedField == FocusedField::Password) {
                    if (typedChar == '\b' && !password.empty())
                        password.pop_back();
                    else if (typedChar >= 32 && password.length() < 20)
                        password += typedChar;
                }
            }
        }
    }

    void handleDashboard(sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            switch (event.key.code) {
            case sf::Keyboard::Num1:
                currentScreen = Screen::CreateGoal;
                clearInputFields();
                break;
            case sf::Keyboard::Num2:
                currentScreen = Screen::ViewGoals;
                break;
            case sf::Keyboard::Num3:
                currentScreen = Screen::GoalHistory;
                break;
            case sf::Keyboard::Num4:
                currentScreen = Screen::PointsMatrix;
                break;
            case sf::Keyboard::Num5:
                currentScreen = Screen::About;
                break;
            case sf::Keyboard::Num6:
            case sf::Keyboard::Escape:
                window.close();
                break;
            }
        }
    }

    void handleCreateGoal(sf::Event& event) {
        if (event.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            if (titleBox.getGlobalBounds().contains(mousePos)) {
                focusedField = FocusedField::GoalTitle;
            }
            else if (descBox.getGlobalBounds().contains(mousePos)) {
                focusedField = FocusedField::GoalDescription;
                showSuggestions = false;
            }
            else if (deadlineBox.getGlobalBounds().contains(mousePos)) {
                focusedField = FocusedField::GoalDeadline;
                showSuggestions = false;
            }
            else {
                focusedField = FocusedField::None;
                showSuggestions = false;
            }

            // Handle suggestion selection
            if (showSuggestions && selectedSuggestion < suggestions.size()) {
                float suggestionY = 195 + selectedSuggestion * 25;
                sf::FloatRect suggestionArea(160, suggestionY, 400, 25);
                if (suggestionArea.contains(mousePos)) {
                    goalTitle = suggestions[selectedSuggestion];
                    showSuggestions = false;
                }
            }

            // Create button
            if (createBtn.getGlobalBounds().contains(mousePos)) {
                if (!goalTitle.empty() && !goalDescription.empty() && !goalDeadline.empty()) {
                    Goal newGoal(goalTitle, goalDescription, goalDeadline, getCurrentDate(),
                        false, 10, "General");

                    goals.push_back(newGoal);
                    goalTrie.insert(goalTitle); // Add to trie
                    saveGoals("goals.txt");

                    // Update data structures
                    updatePointsTree();
                    updateUrgentGoals();
                    updateSortedArrays();

                    showStatusMessage("Goal created successfully!", sf::Color::Green);
                    clearInputFields();
                }
                else {
                    showStatusMessage("Please fill all fields!");
                }
            }

            // Back button
            if (backBtn.getGlobalBounds().contains(mousePos)) {
                currentScreen = Screen::Dashboard;
                clearInputFields();
            }
        }

        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char typedChar = static_cast<char>(event.text.unicode);
                if (focusedField == FocusedField::GoalTitle) {
                    if (typedChar == '\b' && !goalTitle.empty()) {
                        goalTitle.pop_back();
                    }
                    else if (typedChar >= 32 && goalTitle.length() < 50) {
                        goalTitle += typedChar;
                    }

                    // Update autocomplete suggestions
                    if (goalTitle.length() > 0) {
                        suggestions = goalTrie.getSuggestions(goalTitle);
                        showSuggestions = !suggestions.empty();
                        selectedSuggestion = 0;
                    }
                    else {
                        showSuggestions = false;
                    }
                }
                else if (focusedField == FocusedField::GoalDescription) {
                    if (typedChar == '\b' && !goalDescription.empty())
                        goalDescription.pop_back();
                    else if (typedChar >= 32 && goalDescription.length() < 200)
                        goalDescription += typedChar;
                }
                else if (focusedField == FocusedField::GoalDeadline) {
                    if (typedChar == '\b' && !goalDeadline.empty())
                        goalDeadline.pop_back();
                    else if (typedChar >= 32 && goalDeadline.length() < 20)
                        goalDeadline += typedChar;
                }
            }
        }

        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) {
                currentScreen = Screen::Dashboard;
                clearInputFields();
            }
            else if (showSuggestions && focusedField == FocusedField::GoalTitle) {
                if (event.key.code == sf::Keyboard::Up && selectedSuggestion > 0) {
                    selectedSuggestion--;
                }
                else if (event.key.code == sf::Keyboard::Down && selectedSuggestion < suggestions.size() - 1) {
                    selectedSuggestion++;
                }
                else if (event.key.code == sf::Keyboard::Return && selectedSuggestion < suggestions.size()) {
                    goalTitle = suggestions[selectedSuggestion];
                    showSuggestions = false;
                }
            }
        }
    }

    void handleViewGoals(sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) {
                currentScreen = Screen::Dashboard;
            }
            else if (event.key.code >= sf::Keyboard::Num1 &&
                event.key.code <= sf::Keyboard::Num9) {
                int index = event.key.code - sf::Keyboard::Num1;
                if (index < static_cast<int>(goals.size())) {
                    // Check if goal can be completed (dependencies satisfied)
                    std::vector<int> unlocked = goalGraph.getUnlockedGoals(goals);
                    bool canComplete = std::find(unlocked.begin(), unlocked.end(), index) != unlocked.end();

                    if (canComplete || goals[index].completed) {
                        goals[index].completed = !goals[index].completed;
                        saveGoals("goals.txt");

                        // Update all data structures
                        updatePointsTree();
                        updateUrgentGoals();
                        updateSortedArrays();

                        showStatusMessage(goals[index].completed ? "Goal completed!" : "Goal reopened!",
                            sf::Color::Green);
                    }
                    else {
                        showStatusMessage("Goal is locked! Complete prerequisites first.", sf::Color::Red);
                    }
                }
            }
        }
    }

    void clearInputFields() {
        goalTitle.clear();
        goalDescription.clear();
        goalDeadline.clear();
        focusedField = FocusedField::None;
    }

    void drawLoginScreen() {
        window.clear(sf::Color(240, 240, 240));

        window.draw(header);
        window.draw(subHeader);

        // Draw input boxes
        usernameBox.setOutlineColor(focusedField == FocusedField::Username ?
            sf::Color::Blue : sf::Color::Black);
        passwordBox.setOutlineColor(focusedField == FocusedField::Password ?
            sf::Color::Blue : sf::Color::Black);

        window.draw(usernameBox);
        window.draw(passwordBox);

        // Draw text
        usernameText.setString(username);
        passwordText.setString(string(password.length(), '*'));

        window.draw(usernameText);
        window.draw(passwordText);

        // Draw buttons
        window.draw(loginBtn);
        window.draw(loginLabel);
        window.draw(registerBtn);
        window.draw(registerLabel);

        // Draw labels
        // Draw labels
        sf::Text userLabel("Username:", font, 16);
        userLabel.setPosition(200, 100);
        userLabel.setFillColor(sf::Color::Black);
        window.draw(userLabel);

        sf::Text passLabel("Password:", font, 16);
        passLabel.setPosition(200, 160);
        passLabel.setFillColor(sf::Color::Black);
        window.draw(passLabel);

        // Draw status message if recent
        if (statusClock.getElapsedTime().asSeconds() < 3.0f) {
            window.draw(statusMessage);
        }

        window.display();
    }

    void drawDashboard() {
        window.clear(sf::Color(240, 240, 240));

        welcomeText.setString("Welcome, " + currentUser + "!");
        window.draw(welcomeText);

        string menu = "1. Create Goal\n2. View Goals\n3. Goal History\n4. Points Matrix\n5. About\n6. Exit";
        menuText.setString(menu);
        window.draw(menuText);

        // Show total points
        int totalPoints = getRangePointsSum(0, goals.size() - 1);
        sf::Text pointsText("Total Points: " + to_string(totalPoints), font, 24);
        pointsText.setPosition(120, 400);
        pointsText.setFillColor(sf::Color(0, 150, 0));
        window.draw(pointsText);

        // Show urgent goals
        sf::Text urgentText("Urgent Goals:", font, 20);
        urgentText.setPosition(450, 180);
        urgentText.setFillColor(sf::Color::Red);
        window.draw(urgentText);

        int y = 210;
        auto tempQueue = urgentGoals;
        for (int i = 0; i < 3 && !tempQueue.empty(); i++) {
            auto urgent = tempQueue.top();
            tempQueue.pop();
            if (urgent.index < goals.size()) {
                sf::Text urgentGoal(goals[urgent.index].title, font, 16);
                urgentGoal.setPosition(450, y);
                urgentGoal.setFillColor(sf::Color::Red);
                window.draw(urgentGoal);
                y += 25;
            }
        }

        // Draw status message if recent
        if (statusClock.getElapsedTime().asSeconds() < 3.0f) {
            window.draw(statusMessage);
        }

        window.display();
    }

    void drawCreateGoal() {
        window.clear(sf::Color(240, 240, 240));

        sf::Text createHeader("Create New Goal", font, 24);
        createHeader.setPosition(300, 80);
        createHeader.setFillColor(sf::Color::Black);
        window.draw(createHeader);

        // Draw input boxes with focus highlighting
        titleBox.setOutlineColor(focusedField == FocusedField::GoalTitle ?
            sf::Color::Blue : sf::Color::Black);
        descBox.setOutlineColor(focusedField == FocusedField::GoalDescription ?
            sf::Color::Blue : sf::Color::Black);
        deadlineBox.setOutlineColor(focusedField == FocusedField::GoalDeadline ?
            sf::Color::Blue : sf::Color::Black);

        window.draw(titleBox);
        window.draw(descBox);
        window.draw(deadlineBox);

        // Draw labels
        sf::Text titleLabel("Title:", font, 16);
        titleLabel.setPosition(150, 130);
        titleLabel.setFillColor(sf::Color::Black);
        window.draw(titleLabel);

        sf::Text descLabel("Description:", font, 16);
        descLabel.setPosition(150, 200);
        descLabel.setFillColor(sf::Color::Black);
        window.draw(descLabel);

        sf::Text deadlineLabel("Deadline (YYYY-MM-DD):", font, 16);
        deadlineLabel.setPosition(150, 330);
        deadlineLabel.setFillColor(sf::Color::Black);
        window.draw(deadlineLabel);

        // Draw input text
        titleText.setString(goalTitle);
        titleText.setPosition(160, 155);
        titleText.setCharacterSize(16);
        titleText.setFillColor(sf::Color::Black);
        window.draw(titleText);

        descText.setString(goalDescription);
        descText.setPosition(160, 225);
        descText.setCharacterSize(16);
        descText.setFillColor(sf::Color::Black);
        window.draw(descText);

        deadlineText.setString(goalDeadline);
        deadlineText.setPosition(160, 355);
        deadlineText.setCharacterSize(16);
        deadlineText.setFillColor(sf::Color::Black);
        window.draw(deadlineText);

        // Draw autocomplete suggestions
        if (showSuggestions && focusedField == FocusedField::GoalTitle) {
            for (int i = 0; i < suggestions.size() && i < 5; i++) {
                sf::RectangleShape suggestionBox(sf::Vector2f(400, 25));
                suggestionBox.setPosition(160, 195 + i * 25);
                suggestionBox.setFillColor(i == selectedSuggestion ?
                    sf::Color(200, 200, 255) : sf::Color(240, 240, 240));
                suggestionBox.setOutlineThickness(1);
                suggestionBox.setOutlineColor(sf::Color::Blue);
                window.draw(suggestionBox);

                sf::Text suggestionText(suggestions[i], font, 14);
                suggestionText.setPosition(165, 200 + i * 25);
                suggestionText.setFillColor(sf::Color::Black);
                window.draw(suggestionText);
            }
        }

        // Draw buttons
        window.draw(createBtn);
        window.draw(backBtn);

        createLabel.setString("Create");
        createLabel.setPosition(195, 435);
        createLabel.setFillColor(sf::Color::White);
        window.draw(createLabel);

        backLabel.setString("Back");
        backLabel.setPosition(375, 435);
        backLabel.setFillColor(sf::Color::White);
        window.draw(backLabel);

        // Draw status message if recent
        if (statusClock.getElapsedTime().asSeconds() < 3.0f) {
            window.draw(statusMessage);
        }

        window.display();
    }

    void drawViewGoals() {
        window.clear(sf::Color(240, 240, 240));

        sf::Text viewHeader("View Goals", font, 24);
        viewHeader.setPosition(320, 50);
        viewHeader.setFillColor(sf::Color::Black);
        window.draw(viewHeader);

        sf::Text instructions("Press 1-9 to toggle goal completion, ESC to go back", font, 14);
        instructions.setPosition(150, 80);
        instructions.setFillColor(sf::Color::Blue);
        window.draw(instructions);

        // Get unlocked goals
        std::vector<int> unlocked = goalGraph.getUnlockedGoals(goals);
        std::unordered_set<int> unlockedSet(unlocked.begin(), unlocked.end());

        int y = 120;
        for (int i = 0; i < goals.size() && i < 9; i++) {
            sf::Color textColor = sf::Color::Black;
            string prefix = to_string(i + 1) + ". ";
            string status = goals[i].completed ? " [COMPLETED]" : " [PENDING]";

            // Check if goal is locked
            if (!unlockedSet.count(i) && !goals[i].completed) {
                textColor = sf::Color(150, 150, 150); // Gray for locked goals
                status += " [LOCKED]";
            }

            string goalText = prefix + goals[i].title + status;
            goalText += " (Points: " + to_string(goals[i].points) + ")";
            goalText += " (Deadline: " + goals[i].deadline + ")";

            sf::Text text(goalText, font, 16);
            text.setPosition(120, y);
            text.setFillColor(textColor);
            window.draw(text);

            y += 35;
        }

        // Show dependencies info
        sf::Text depHeader("Goal Dependencies:", font, 18);
        depHeader.setPosition(120, y + 20);
        depHeader.setFillColor(sf::Color::Blue);
        window.draw(depHeader);

        y += 50;
        for (const auto& dep : goalGraph.dependents) {
            if (dep.first < goals.size()) {
                string depText = goals[dep.first].title + " requires: ";
                for (int i = 0; i < dep.second.size(); i++) {
                    if (dep.second[i] < goals.size()) {
                        depText += goals[dep.second[i]].title;
                        if (i < dep.second.size() - 1) depText += ", ";
                    }
                }

                sf::Text text(depText, font, 14);
                text.setPosition(120, y);
                text.setFillColor(sf::Color::Black);
                window.draw(text);
                y += 25;
            }
        }

        // Draw status message if recent
        if (statusClock.getElapsedTime().asSeconds() < 3.0f) {
            window.draw(statusMessage);
        }

        window.display();
    }

    void drawGoalHistory() {
        window.clear(sf::Color(240, 240, 240));

        sf::Text historyHeader("Goal History", font, 24);
        historyHeader.setPosition(300, 50);
        historyHeader.setFillColor(sf::Color::Black);
        window.draw(historyHeader);

        sf::Text instructions("Press ESC to go back", font, 14);
        instructions.setPosition(320, 80);
        instructions.setFillColor(sf::Color::Blue);
        window.draw(instructions);

        // Show completed goals
        sf::Text completedHeader("Completed Goals:", font, 20);
        completedHeader.setPosition(120, 120);
        completedHeader.setFillColor(sf::Color::Red);
        window.draw(completedHeader);

        int y = 150;
        int completedCount = 0;
        for (const auto& goal : goals) {
            if (goal.completed) {
                string goalText = "✓ " + goal.title + " (Completed on: " + goal.deadline + ")";
                goalText += " [+" + to_string(goal.points) + " points]";

                sf::Text text(goalText, font, 16);
                text.setPosition(120, y);
                text.setFillColor(sf::Color::Green);
                window.draw(text);

                y += 25;
                completedCount++;
            }
        }

        if (completedCount == 0) {
            sf::Text noCompleted("No completed goals yet.", font, 16);
            noCompleted.setPosition(120, 150);
            noCompleted.setFillColor(sf::Color::Blue);
            window.draw(noCompleted);
        }

        // Show pending goals
        sf::Text pendingHeader("Pending Goals:", font, 20);
        pendingHeader.setPosition(120, y + 30);
        pendingHeader.setFillColor(sf::Color::Red);
        window.draw(pendingHeader);

        y += 60;
        int pendingCount = 0;
        for (const auto& goal : goals) {
            if (!goal.completed) {
                string goalText = "○ " + goal.title + " (Due: " + goal.deadline + ")";

                sf::Text text(goalText, font, 16);
                text.setPosition(120, y);
                text.setFillColor(sf::Color::Red);
                window.draw(text);

                y += 25;
                pendingCount++;
            }
        }

        if (pendingCount == 0) {
            sf::Text noPending("All goals completed!", font, 16);
            noPending.setPosition(120, y);
            noPending.setFillColor(sf::Color::Red);
            window.draw(noPending);
        }

        window.display();
    }

    void drawPointsMatrix() {
        window.clear(sf::Color(240, 240, 240));

        sf::Text matrixHeader("Points Matrix", font, 24);
        matrixHeader.setPosition(300, 50);
        matrixHeader.setFillColor(sf::Color::Black);
        window.draw(matrixHeader);

        sf::Text instructions("Press ESC to go back", font, 14);
        instructions.setPosition(320, 80);
        instructions.setFillColor(sf::Color::Blue);
        window.draw(instructions);

        // Show total points
        int totalPoints = getRangePointsSum(0, goals.size() - 1);
        sf::Text totalText("Total Points Earned: " + to_string(totalPoints), font, 20);
        totalText.setPosition(120, 120);
        totalText.setFillColor(sf::Color::Red);
        window.draw(totalText);

        // Show points breakdown
        sf::Text breakdownHeader("Points Breakdown:", font, 18);
        breakdownHeader.setPosition(120, 160);
        breakdownHeader.setFillColor(sf::Color::Black);
        window.draw(breakdownHeader);

        int y = 190;
        for (int i = 0; i < goals.size(); i++) {
            string pointsText = goals[i].title + ": " + to_string(goals[i].points) + " points";
            if (goals[i].completed) {
                pointsText += " ✓";
            }
            else {
                pointsText += " (pending)";
            }

            sf::Text text(pointsText, font, 16);
            text.setPosition(120, y);
            text.setFillColor(goals[i].completed ? sf::Color::Red : sf::Color::Blue);
            window.draw(text);

            y += 25;
        }

        // Show range queries demonstration
        sf::Text rangeHeader("Range Queries:", font, 18);
        rangeHeader.setPosition(120, y + 20);
        rangeHeader.setFillColor(sf::Color::Blue);
        window.draw(rangeHeader);

        y += 50;
        if (goals.size() > 0) {
            int halfSize = goals.size() / 2;
            int firstHalfPoints = getRangePointsSum(0, halfSize);
            int secondHalfPoints = getRangePointsSum(halfSize + 1, goals.size() - 1);

            sf::Text firstHalf("First half goals: " + to_string(firstHalfPoints) + " points", font, 14);
            firstHalf.setPosition(120, y);
            firstHalf.setFillColor(sf::Color::Black);
            window.draw(firstHalf);

            sf::Text secondHalf("Second half goals: " + to_string(secondHalfPoints) + " points", font, 14);
            secondHalf.setPosition(120, y + 25);
            secondHalf.setFillColor(sf::Color::Black);
            window.draw(secondHalf);
        }

        window.display();
    }

    void drawAbout() {
        window.clear(sf::Color(240, 240, 240));

        sf::Text aboutHeader("About MAQSAD", font, 24);
        aboutHeader.setPosition(300, 50);
        aboutHeader.setFillColor(sf::Color::Black);
        window.draw(aboutHeader);

        sf::Text instructions("Press ESC to go back", font, 14);
        instructions.setPosition(320, 80);
        instructions.setFillColor(sf::Color::Blue);
        window.draw(instructions);

        string aboutText = "MAQSAD - Goal Management System\n\n";
        aboutText += "Features:\n";
        aboutText += "• Create and manage personal goals\n";
        aboutText += "• Track goal completion and points\n";
        aboutText += "• Goal dependencies and prerequisites\n";
        aboutText += "• Autocomplete suggestions using Trie\n";
        aboutText += "• Efficient points tracking with Segment Tree\n";
        aboutText += "• Priority queue for urgent goals\n";
        aboutText += "• Binary search for goal lookup\n";
        aboutText += "• Persistent data storage\n\n";
        aboutText += "Data Structures Used:\n";
        aboutText += "• Trie for autocomplete\n";
        aboutText += "• Graph for goal dependencies\n";
        aboutText += "• Segment Tree for range queries\n";
        aboutText += "• Priority Queue for urgent goals\n";
        aboutText += "• Binary Search for efficient lookup\n\n";
        aboutText += "Version: 1.0\n";
        aboutText += "Developed with SFML Graphics Library\n\n";
        aboutText += "Finally Created By Bilal Ahmad";

        sf::Text about(aboutText, font, 16);
        about.setPosition(120, 120);
        about.setFillColor(sf::Color::Black);
        window.draw(about);

        window.display();
    }

    void run() {
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                }

                switch (currentScreen) {
                case Screen::Login:
                    handleLoginScreen(event);
                    break;
                case Screen::Dashboard:
                    handleDashboard(event);
                    break;
                case Screen::CreateGoal:
                    handleCreateGoal(event);
                    break;
                case Screen::ViewGoals:
                    handleViewGoals(event);
                    break;
                case Screen::GoalHistory:
                case Screen::PointsMatrix:
                case Screen::About:
                    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                        currentScreen = Screen::Dashboard;
                    }
                    break;
                }
            }

            // Render based on current screen
            switch (currentScreen) {
            case Screen::Login:
                drawLoginScreen();
                break;
            case Screen::Dashboard:
                drawDashboard();
                break;
            case Screen::CreateGoal:
                drawCreateGoal();
                break;
            case Screen::ViewGoals:
                drawViewGoals();
                break;
            case Screen::GoalHistory:
                drawGoalHistory();
                break;
            case Screen::PointsMatrix:
                drawPointsMatrix();
                break;
            case Screen::About:
                drawAbout();
                break;
            }
        }
    }
};

int main() {
    MAQSAD app;
    app.run();
    return 0;
}
