
#include <iostream>
#include <SFML/Graphics.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <cstdlib>

using namespace std;
using namespace sf;

const int ROWS = 25;
const int COLS = 40;
const int TILE_SIZE_PIXELS = 18;
const int HUD_PANEL_WIDTH = 200;

int tileGrid[ROWS][COLS] = { 0 };

const int LOGIN_SCREEN = 0;
const int REGISTER_SCREEN = 1;
const int MAIN_MENU = 2;
const int START_MENU = 3;
const int SELECT_LEVEL = 4;
const int LEADERBOARD = 5;
const int PLAYING = 6;
const int END_MENU = 7;
const int MODE_SELECT = 8;
const int PROFILE = 9;
const int MATCHMAKING_QUEUE = 10;
const int GAME_ROOM = 11;
const int MULTIPLAYER = 12;

struct Level
{
    int id;
    string name;
    int difficulty;
    int initialEnemies;
    float enemySpeed;
};

struct LeaderboardEntry
{
    string username;
    int score;
    int level;
};

struct QueuePlayer
{
    string username;
    int score;
    int id;
};

struct Match
{
    int score;
    int level;
    bool won;
};

struct PlayerProfile
{
    string username;
    int totalPoints = 0;
    int wins = 0;
    int losses = 0;
    string friends[50];
    int friendCount = 0;
    Match matchHistory[100];
    int matchCount = 0;

    void addFriend(const string& friendName)
    {
        if (friendCount < 50)
        {
            for (int i = 0; i < friendCount; i++)
                if (friends[i] == friendName)
                    return; // already friends
            friends[friendCount++] = friendName;
        }
    }

    void addMatch(int score, int level, bool won)
    {
        if (matchCount < 100)
        {
            matchHistory[matchCount].score = score;
            matchHistory[matchCount].level = level;
            matchHistory[matchCount].won = won;
            matchCount++;
        }
    }
};

class MinHeap
{
private:
    static const int MAX_SIZE = 10;
    LeaderboardEntry heap[MAX_SIZE];
    int size;

    int parent(int i) { return (i - 1) / 2; }
    int left(int i) { return 2 * i + 1; }
    int right(int i) { return 2 * i + 2; }

    void heapifyUp(int i)
    {
        while (i > 0)
        {
            int p = parent(i);
            bool shouldSwap = heap[p].score > heap[i].score ||
                (heap[p].score == heap[i].score && heap[p].level < heap[i].level);
            if (!shouldSwap) break;
            swap(heap[i], heap[p]);
            i = p;
        }
    }

    void heapifyDown(int i)
    {
        int smallest = i;
        int l = left(i);
        int r = right(i);

        if (l < size)
        {
            bool lSmaller = heap[l].score < heap[smallest].score ||
                (heap[l].score == heap[smallest].score && heap[l].level > heap[smallest].level);
            if (lSmaller) smallest = l;
        }
        if (r < size)
        {
            bool rSmaller = heap[r].score < heap[smallest].score ||
                (heap[r].score == heap[smallest].score && heap[r].level > heap[smallest].level);
            if (rSmaller) smallest = r;
        }

        if (smallest != i)
        {
            swap(heap[i], heap[smallest]);
            heapifyDown(smallest);
        }
    }

public:
    MinHeap() : size(0) {}

    bool isFull() { return size == MAX_SIZE; }
    int getSize() { return size; }
    LeaderboardEntry* getHeap() { return heap; }

    int getMinScore()
    {
        return (size > 0) ? heap[0].score : 0;
    }

    void insert(const LeaderboardEntry& entry)
    {
        if (size < MAX_SIZE)
        {
            heap[size] = entry;
            heapifyUp(size);
            size++;
        }
        else if (entry.score > heap[0].score)
        {
            heap[0] = entry;
            heapifyDown(0);
        }
    }

    LeaderboardEntry* getSortedArray()
    {
        LeaderboardEntry* sorted = new LeaderboardEntry[size];
        for (int i = 0; i < size; i++)
            sorted[i] = heap[i];

        for (int i = 0; i < size - 1; i++)
            for (int j = 0; j < size - i - 1; j++)
                if (sorted[j].score < sorted[j + 1].score)
                    swap(sorted[j], sorted[j + 1]);

        return sorted;
    }

    void freeSortedArray(LeaderboardEntry* arr)
    {
        delete[] arr;
    }
};
// Helper function to set text position at integer coordinates for crisp rendering
void setTextPosition(Text& text, float x, float y)
{
    text.setPosition(roundf(x), roundf(y));
}

struct Player
{
    int id;
    string username, password;
    int topScore = 0;
    int totalScore = 0;
};

// ============================================================================
// Button class
// ============================================================================

class Button
{
public:
    RectangleShape shape;
    Text label;
    bool hovered = false;
    Color normal = Color(100, 100, 100);
    Color hover = Color(140, 140, 140);

    void init(float cx, float y, float w, float h, const string& textStr, Font& font)
    {
        float x = cx - w / 2;
        shape.setSize({ w, h });
        shape.setPosition(x, y);
        shape.setFillColor(normal);
        shape.setOutlineColor(Color::White);
        shape.setOutlineThickness(2);

        label.setFont(font);
        label.setString(textStr);
        label.setCharacterSize(20);
        label.setFillColor(Color::White);

        FloatRect b = label.getLocalBounds();
        label.setOrigin(roundf(b.width / 2), roundf(b.height / 2));
        setTextPosition(label, cx, y + h / 2);
    }

    void update(Vector2i mouse)
    {
        hovered = shape.getGlobalBounds().contains((float)mouse.x, (float)mouse.y);
        shape.setFillColor(hovered ? hover : normal);
    }

    bool isClicked(Vector2i mouse, Event& e)
    {
        return hovered &&
            e.type == Event::MouseButtonPressed &&
            e.mouseButton.button == Mouse::Left;
    }

    void draw(RenderWindow& w)
    {
        w.draw(shape);
        w.draw(label);
    }
};

// ============================================================================
// InputField class
// ============================================================================

class InputField
{
public:
    RectangleShape box;
    Text display, label;
    string content;
    bool active = false;
    bool password = false;
    Clock cursorClock;
    bool showCursor = true;

    void init(float cx, float y, float w, float h,
        const string& lbl, Font& font, bool hide = false)
    {
        password = hide;
        float x = cx - w / 2;
        box.setSize({ w, h });
        box.setPosition(x, y);
        box.setFillColor(Color::White);
        box.setOutlineColor(Color(150, 150, 150));
        box.setOutlineThickness(2);

        label.setFont(font);
        label.setCharacterSize(16);
        label.setFillColor(Color::White);
        label.setString(lbl);
        FloatRect lb = label.getLocalBounds();
        label.setOrigin(roundf(lb.width / 2), roundf(lb.height));
        setTextPosition(label, cx, y - 10);

        display.setFont(font);
        display.setCharacterSize(18);
        display.setFillColor(Color::Black);
        setTextPosition(display, x + 10, y + 10);
    }

    void update(Vector2i mouse, Event& e)
    {
        if (e.type == Event::MouseButtonPressed)
            active = box.getGlobalBounds().contains((float)mouse.x, (float)mouse.y);

        box.setOutlineColor(active ? Color(70, 130, 180) : Color(150, 150, 150));

        if (active && e.type == Event::TextEntered)
        {
            if (e.text.unicode == 8 && !content.empty())
                content.pop_back();
            else if (e.text.unicode >= 32 && e.text.unicode < 127 &&
                content.length() < 30)
                content += char(e.text.unicode);
        }

        if (cursorClock.getElapsedTime().asSeconds() > 0.5f)
        {
            showCursor = !showCursor;
            cursorClock.restart();
        }

        string shown = password ? string(content.size(), '*') : content;
        if (active && showCursor)
            shown += "|";
        display.setString(shown);
    }

    void draw(RenderWindow& w)
    {
        w.draw(label);
        w.draw(box);
        w.draw(display);
    }

    string getText() { return content; }
    void clear()
    {
        content.clear();
        display.setString("");
    }
};
class LeaderboardManager
{
private:
    MinHeap heap;
    const char* file = "leaderboard.txt";

public:
    LeaderboardManager() { load(); }

    void load()
    {
        ifstream f(file);
        if (!f.is_open())
            return;
        string line;
        while (getline(f, line))
        {
            LeaderboardEntry entry;
            istringstream iss(line);
            char delim;
            iss >> entry.score >> delim;
            getline(iss, entry.username, ',');
            iss >> entry.level;
            heap.insert(entry);
        }
    }

    void save()
    {
        ofstream f(file);
        LeaderboardEntry* sorted = heap.getSortedArray();
        for (int i = 0; i < heap.getSize(); i++)
            f << sorted[i].score << "," << sorted[i].username << ","
            << sorted[i].level << endl;
        heap.freeSortedArray(sorted);
    }

    bool isHighScore(int score)
    {
        if (!heap.isFull())
            return true;
        return score > heap.getMinScore();
    }

    void addScore(const string& username, int score, int level)
    {
        LeaderboardEntry entry;
        entry.username = username;
        entry.score = score;
        entry.level = level;
        heap.insert(entry);
        save();
    }

    LeaderboardEntry* getLeaderboard()
    {
        return heap.getSortedArray();
    }

    int getCount()
    {
        return heap.getSize();
    }

    void freeLeaderboard(LeaderboardEntry* arr)
    {
        heap.freeSortedArray(arr);
    }
};

// ============================================================================
// Priority Queue for Matchmaking
// ============================================================================

class PriorityQueue
{
private:
    QueuePlayer queue[100];
    int size;

    void heapifyUp(int index)
    {
        while (index > 0)
        {
            int parent = (index - 1) / 2;
            if (queue[parent].score >= queue[index].score)
                break;
            QueuePlayer temp = queue[parent];
            queue[parent] = queue[index];
            queue[index] = temp;
            index = parent;
        }
    }

    void heapifyDown(int index)
    {
        while (true)
        {
            int left = 2 * index + 1;
            int right = 2 * index + 2;
            int largest = index;

            if (left < size && queue[left].score > queue[largest].score)
                largest = left;
            if (right < size && queue[right].score > queue[largest].score)
                largest = right;

            if (largest == index)
                break;

            QueuePlayer temp = queue[index];
            queue[index] = queue[largest];
            queue[largest] = temp;
            index = largest;
        }
    }

public:
    PriorityQueue() : size(0) {}

    void push(const string& username, int score, int id)
    {
        if (size >= 100)
            return;
        queue[size].username = username;
        queue[size].score = score;
        queue[size].id = id;
        heapifyUp(size);
        size++;
    }

    bool pop(QueuePlayer& player)
    {
        if (size == 0)
            return false;
        player = queue[0];
        queue[0] = queue[size - 1];
        size--;
        if (size > 0)
            heapifyDown(0);
        return true;
    }

    bool peek(QueuePlayer& player)
    {
        if (size == 0)
            return false;
        player = queue[0];
        return true;
    }

    int getSize() { return size; }

    bool isEmpty() { return size == 0; }

    void removePlayer(const string& username)
    {
        for (int i = 0; i < size; i++)
        {
            if (queue[i].username == username)
            {
                queue[i] = queue[size - 1];
                size--;
                if (i < size)
                {
                    heapifyUp(i);
                    heapifyDown(i);
                }
                break;
            }
        }
    }

    int getPlayerPosition(const string& username)
    {
        QueuePlayer temp[100];
        int tempSize = 0;

        for (int i = 0; i < size; i++)
            temp[tempSize++] = queue[i];

        for (int i = 0; i < tempSize - 1; i++)
        {
            for (int j = 0; j < tempSize - i - 1; j++)
            {
                if (temp[j].score < temp[j + 1].score)
                {
                    QueuePlayer swap = temp[j];
                    temp[j] = temp[j + 1];
                    temp[j + 1] = swap;
                }
            }
        }

        for (int i = 0; i < tempSize; i++)
        {
            if (temp[i].username == username)
                return i + 1;
        }
        return -1;
    }

    int getPlayersAbove(const string& username)
    {
        int playerScore = -1;
        for (int i = 0; i < size; i++)
        {
            if (queue[i].username == username)
            {
                playerScore = queue[i].score;
                break;
            }
        }
        if (playerScore == -1)
            return -1;

        int count = 0;
        for (int i = 0; i < size; i++)
        {
            if (queue[i].score > playerScore)
                count++;
        }
        return count;
    }
};

// ============================================================================
// MATCHMAKING SYSTEM
// ============================================================================

class MatchmakingSystem
{
private:
    PriorityQueue queue;
    int nextMatchID;

public:
    MatchmakingSystem() : nextMatchID(1) {}

    void addPlayer(const string& username, int score, int playerID)
    {
        queue.push(username, score, playerID);
    }

    bool canMatch()
    {
        return queue.getSize() >= 2;
    }

    bool createMatch(QueuePlayer& player1, QueuePlayer& player2)
    {
        if (!canMatch())
            return false;
        queue.pop(player1);
        queue.pop(player2);
        return true;
    }

    int getQueueSize() { return queue.getSize(); }

    bool getTopPlayer(QueuePlayer& player) { return queue.peek(player); }

    void removePlayer(const string& username) { queue.removePlayer(username); }

    int getPlayerPosition(const string& username) { return queue.getPlayerPosition(username); }

    int getPlayersAbove(const string& username) { return queue.getPlayersAbove(username); }
};

// ============================================================================
// END MATCHMAKING SYSTEM
// ============================================================================

class AuthManager
{
private:
    Player players[100];
    int count = 0;
    int nextID = 1;
    const char* file = "players.txt";

public:
    AuthManager() { load(); }

    void load()
    {
        ifstream f(file);
        if (!f.is_open())
            return;
        string line;
        while (getline(f, line) && count < 100)
        {
            Player p;
            istringstream iss(line);
            char delim;
            iss >> p.id >> delim;
            getline(iss, p.username, ',');
            getline(iss, p.password, ',');
            iss >> p.topScore;
            players[count++] = p;
            nextID = max(nextID, p.id + 1);
        }
    }

    void save()
    {
        ofstream f(file);
        for (int i = 0; i < count; i++)
            f << players[i].id << "," << players[i].username << "," << players[i].password << "," << players[i].topScore << endl;
    }

    bool registerPlayer(const string& u, const string& p, string& msg)
    {
        if (u.length() < 3)
        {
            msg = "Username too short";
            return false;
        }
        if (p.length() < 6)
        {
            msg = "Password too short";
            return false;
        }
        if (count >= 100)
        {
            msg = "User limit reached";
            return false;
        }
        for (int i = 0; i < count; i++)
            if (players[i].username == u)
            {
                msg = "Username exists";
                return false;
            }
        Player pl;
        pl.id = nextID++;
        pl.username = u;
        pl.password = p;
        players[count++] = pl;
        save();
        return true;
    }

    bool loginPlayer(const string& u, const string& p, string& msg)
    {
        for (int i = 0; i < count; i++)
            if (players[i].username == u)
            {
                if (players[i].password == p)
                    return true;
                msg = "Wrong password";
                return false;
            }
        msg = "User not found";
        return false;
    }

    int getPlayerTopScore(const string& username)
    {
        for (int i = 0; i < count; i++)
            if (players[i].username == username)
                return players[i].topScore;
        return 0;
    }

    void updatePlayerTopScore(const string& username, int newScore)
    {
        for (int i = 0; i < count; i++)
        {
            if (players[i].username == username)
            {
                if (newScore > players[i].topScore)
                    players[i].topScore = newScore;
                save();
                return;
            }
        }
    }

    int getPlayerScore(const string& username)
    {
        for (int i = 0; i < count; i++)
            if (players[i].username == username)
                return players[i].totalScore;
        return 0;
    }

    void updatePlayerScore(const string& username, int newScore)
    {
        for (int i = 0; i < count; i++)
            if (players[i].username == username)
            {
                if (newScore > players[i].totalScore)
                {
                    players[i].totalScore = newScore;
                    save();
                }
                return;
            }
    }
};

class ProfileManager
{
private:
    PlayerProfile profiles[100];
    int count = 0;
    const char* file = "profiles.txt";

public:
    ProfileManager() { load(); }

    void load()
    {
        ifstream f(file);
        if (!f.is_open())
            return;
        string line;
        while (getline(f, line) && count < 100)
        {
            PlayerProfile p;
            istringstream iss(line);
            char delim;
            getline(iss, p.username, ',');
            iss >> p.totalPoints >> delim >> p.wins >> delim >> p.losses;
            profiles[count++] = p;
        }
    }

    void save()
    {
        ofstream f(file);
        for (int i = 0; i < count; i++)
            f << profiles[i].username << "," << profiles[i].totalPoints << ","
            << profiles[i].wins << "," << profiles[i].losses << endl;
    }

    PlayerProfile* getProfile(const string& username)
    {
        for (int i = 0; i < count; i++)
            if (profiles[i].username == username)
                return &profiles[i];
        return nullptr;
    }

    void createProfile(const string& username)
    {
        if (count < 100)
        {
            profiles[count].username = username;
            profiles[count].totalPoints = 0;
            profiles[count].wins = 0;
            profiles[count].losses = 0;
            profiles[count].friendCount = 0;
            profiles[count].matchCount = 0;
            count++;
            save();
        }
    }

    void updateProfile(const string& username, int points, bool won)
    {
        for (int i = 0; i < count; i++)
        {
            if (profiles[i].username == username)
            {
                profiles[i].totalPoints += points;
                if (won)
                    profiles[i].wins++;
                else
                    profiles[i].losses++;
                save();
                return;
            }
        }
    }

    void addFriend(const string& username, const string& friendName)
    {
        for (int i = 0; i < count; i++)
        {
            if (profiles[i].username == username)
            {
                profiles[i].addFriend(friendName);
                save();
                return;
            }
        }
    }

    void addMatch(const string& username, int score, int level, bool won)
    {
        for (int i = 0; i < count; i++)
        {
            if (profiles[i].username == username)
            {
                profiles[i].addMatch(score, level, won);
                save();
                return;
            }
        }
    }
};

struct Enemy
{
    int posX, posY;
    int velX, velY;
    Enemy()
    {
        posX = posY = 300;
        velX = 4 - rand() % 8;
        velY = 4 - rand() % 8;
    }
    void move()
    {
        posX += velX;
        if (tileGrid[posY / TILE_SIZE_PIXELS][posX / TILE_SIZE_PIXELS] == 1)
        {
            velX = -velX;
            posX += velX;
        }
        posY += velY;
        if (tileGrid[posY / TILE_SIZE_PIXELS][posX / TILE_SIZE_PIXELS] == 1)
        {
            velY = -velY;
            posY += velY;
        }
    }
};

void floodFillMark(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return;
    if (tileGrid[row][col] == 0)
    {
        tileGrid[row][col] = -1;
        if (row > 0 && tileGrid[row - 1][col] == 0)
            floodFillMark(row - 1, col);
        if (row < ROWS - 1 && tileGrid[row + 1][col] == 0)
            floodFillMark(row + 1, col);
        if (col > 0 && tileGrid[row][col - 1] == 0)
            floodFillMark(row, col - 1);
        if (col < COLS - 1 && tileGrid[row][col + 1] == 0)
            floodFillMark(row, col + 1);
    }
}

// Font loading helper
bool loadFont(Font& font)
{
    if (font.loadFromFile("fonts/Minecraft.ttf"))
        return true;

    if (font.loadFromFile("../fonts/Minecraft.ttf"))
        return true;

    return false;
}
int main()
{
    srand(time(0));

    ContextSettings settings;
    settings.antialiasingLevel = 8;

    RenderWindow window(VideoMode(COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH, ROWS * TILE_SIZE_PIXELS + 60), "XONIX", Style::Default, settings);
    window.setFramerateLimit(60);

    Font font;
    if (!loadFont(font))
    {
    }
    else
    {
        font.setSmooth(false);
    }

    Texture tiles, enemyTex, gameoverTex;
    tiles.loadFromFile("images/tiles.png");
    enemyTex.loadFromFile("images/enemy.png");
    gameoverTex.loadFromFile("images/gameover.png");

    Sprite sTile(tiles), sEnemy(enemyTex), sGameover(gameoverTex);
    sEnemy.setOrigin(20, 20);
    FloatRect gameoverBounds = sGameover.getLocalBounds();
    sGameover.setOrigin(gameoverBounds.width / 2, gameoverBounds.height / 2);
    float gameAreaCenterX = HUD_PANEL_WIDTH + (COLS * TILE_SIZE_PIXELS) / 2;
    float gameAreaCenterY = (ROWS * TILE_SIZE_PIXELS) / 2;
    sGameover.setPosition(gameAreaCenterX, gameAreaCenterY);

    AuthManager auth;
    LeaderboardManager leaderboardManager;
    ProfileManager profileManager;
    int state = LOGIN_SCREEN;
    float centerX = (COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH) / 2;

    Level levels[3] = {
        {1, "Easy", 1, 3, 2.0f},
        {2, "Medium", 2, 5, 3.0f},
        {3, "Hard", 3, 7, 4.5f}
    };
    int selectedLevel = 0;

    InputField usernameInputLogin, passwordInputLogin, usernameInputRegister, passwordInputRegister;
    usernameInputLogin.init(centerX, 150, 300, 40, "Username", font);
    passwordInputLogin.init(centerX, 230, 300, 40, "Password", font, true);
    usernameInputRegister.init(centerX, 210, 300, 40, "New Username", font);
    passwordInputRegister.init(centerX, 280, 300, 40, "New Password", font, true);

    Button loginButton, registerScreenButton, createAccountButton, backButton, playGameButton;
    Button startGameButton, selectLevelButton, leaderboardButton, logoutButton;
    Button singlePlayerButton, multiPlayerButton;
    Button easyButton, mediumButton, hardButton, levelBackButton;
    Button restartButton, mainMenuButton, exitGameButton;
    Button profileButton, profileBackButton;

    loginButton.init(centerX, 310, 200, 50, "Login", font);
    registerScreenButton.init(centerX, 380, 200, 50, "Register", font);
    createAccountButton.init(centerX, 350, 200, 50, "Create Account", font);
    backButton.init(centerX, 420, 200, 50, "Back", font);
    playGameButton.init(centerX, ROWS * TILE_SIZE_PIXELS / 2 - 25, 200, 50, "Play Game", font);

    // Matchmaking buttons
    Button matchmakingButton, addPlayer2Button, startMultiplayerButton, leaveQueueButton;
    matchmakingButton.init(centerX, ROWS * TILE_SIZE_PIXELS / 2 + 50, 200, 50, "Matchmaking", font);
    addPlayer2Button.init(centerX, 250, 200, 50, "Add Player 2", font);
    startMultiplayerButton.init(centerX, 360, 200, 50, "Start Game", font);
    leaveQueueButton.init(centerX, 320, 200, 50, "Leave Queue", font);

    MatchmakingSystem matchmaking;
    QueuePlayer player1Queue, player2Queue;
    string player2Username = "";
    bool player2LoggedIn = false;
    int player1ID = 1, player2ID = 2;
    int player1QueuePosition = -1, player2QueuePosition = -1;
    int player1PlayersAbove = -1, player2PlayersAbove = -1;

    InputField player2UsernameInput, player2PasswordInput;
    player2UsernameInput.init(centerX, 200, 300, 40, "Player 2 Username", font);
    player2PasswordInput.init(centerX, 280, 300, 40, "Player 2 Password", font, true);

    startGameButton.init(centerX, 140, 200, 50, "Start Game", font);
    profileButton.init(centerX, 200, 200, 50, "Profile", font);
    selectLevelButton.init(centerX, 260, 200, 50, "Select Level", font);
    leaderboardButton.init(centerX, 320, 200, 50, "Leaderboard", font);
    logoutButton.init(centerX, 380, 200, 50, "Logout", font);

    singlePlayerButton.init(centerX - 140, 220, 240, 50, "Single Player", font);
    multiPlayerButton.init(centerX + 140, 220, 240, 50, "Multiplayer", font);


    // Select Level buttons
    easyButton.init(centerX - 150, 250, 150, 60, "Easy", font);
    mediumButton.init(centerX, 250, 150, 60, "Medium", font);
    hardButton.init(centerX + 150, 250, 150, 60, "Hard", font);
    levelBackButton.init(centerX, 350, 200, 50, "Back", font);

    // End Menu buttons
    restartButton.init(centerX - 140, 340, 150, 50, "Restart", font);
    mainMenuButton.init(centerX, 340, 150, 50, "Main Menu", font);
    exitGameButton.init(centerX + 140, 340, 150, 50, "Exit Game", font);
    profileBackButton.init(centerX, 450, 200, 50, "Back", font);

    // Game
    int playerCol = 10, playerRow = 0, playerDirCol = 0, playerDirRow = 0;
    bool running = true;
    float timer = 0, delay = 0.07;
    Clock clock;
    Enemy enemies[10];
    int enemyCount = 4;
    int currentLevelId = 1;
    int gameMode = 1; // 1 = Single, 2 = Multiplayer

    // Score / Power-ups
    int score = 0, rewardComboCount = 0, availablePowerUps = 0, lastPowerUpAwardScore = 0;
    bool isFreezeActive = false;
    Clock freezeClock;
    string currentUser, errorMessage;
    Clock errorClock;
    bool isNewHighScore = false; // tracks if current game is a high score

    // Multiplayer game variables
    int player2Col = 15, player2Row = 0, player2DirCol = 0, player2DirRow = 0;
    bool player2Running = true;
    float player2Timer = 0;
    int player2Score = 0, player2RewardComboCount = 0, player2AvailablePowerUps = 0, player2LastPowerUpAwardScore = 0;
    bool player2FreezeActive = false;
    Clock player2FreezeClock;
    // Track key states to prevent multiple triggers when held
    bool spaceWasPressed = false;
    bool enterWasPressed = false;

    // Initialize game grid: surround with blue border tiles (safe zone)
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (i == 0 || j == 0 || i == ROWS - 1 || j == COLS - 1)
                tileGrid[i][j] = 1;  // 1 = blue tile (safe)

    // ============================================================================
    // MAIN GAME LOOP - Runs 60 times per second (60 FPS)
    // ============================================================================
    // This loop:
    // 1. Processes user input (mouse clicks, keyboard)
    // 2. Updates game logic (movement, collisions, scoring)
    // 3. Renders everything to screen

    while (window.isOpen())
    {
        Vector2i mouse = Mouse::getPosition(window);  // Get current mouse position
        Event e;

        // Process all pending events (clicks, key presses, window close, etc.)
        while (window.pollEvent(e))
        {
            if (e.type == Event::Closed)
                window.close();

            if (state == LOGIN_SCREEN)
            {
                usernameInputLogin.update(mouse, e);
                passwordInputLogin.update(mouse, e);
            }
            else if (state == REGISTER_SCREEN)
            {
                usernameInputRegister.update(mouse, e);
                passwordInputRegister.update(mouse, e);
            }

            // LOGIN
            if (state == LOGIN_SCREEN)
            {
                if (loginButton.isClicked(mouse, e))
                {
                    if (auth.loginPlayer(usernameInputLogin.getText(), passwordInputLogin.getText(), errorMessage))
                    {
                        currentUser = usernameInputLogin.getText();
                        state = START_MENU;
                        continue;
                    }
                    else
                        errorClock.restart();
                }
                if (registerScreenButton.isClicked(mouse, e))
                {
                    // prepare register screen and consume the click
                    usernameInputRegister.clear();
                    passwordInputRegister.clear();
                    usernameInputRegister.active = false;
                    passwordInputRegister.active = false;
                    state = REGISTER_SCREEN;
                    continue;
                }
            }

            // REGISTER
            if (state == REGISTER_SCREEN)
            {
                if (createAccountButton.isClicked(mouse, e))
                {
                    if (auth.registerPlayer(usernameInputRegister.getText(), passwordInputRegister.getText(), errorMessage))
                    {
                        errorMessage = "Registration successful!";
                        errorClock.restart();
                        state = LOGIN_SCREEN;
                        usernameInputRegister.clear();
                        passwordInputRegister.clear();
                    }
                    else
                        errorClock.restart();
                }
                if (backButton.isClicked(mouse, e))
                    state = LOGIN_SCREEN;
            }

            // MAIN MENU to START MENU
            if (state == MAIN_MENU && playGameButton.isClicked(mouse, e))
            {
                state = START_MENU;
                continue;
            }

            // ============================================================================
            // MATCHMAKING QUEUE (handler moved to START_MENU block)
            // ============================================================================

            if (state == MATCHMAKING_QUEUE)
            {
                if (leaveQueueButton.isClicked(mouse, e))
                {
                    matchmaking.removePlayer(currentUser);
                    leaveQueueButton.hovered = false;
                    matchmakingButton.hovered = false;
                    state = START_MENU;
                    continue;
                }
                if (addPlayer2Button.isClicked(mouse, e))
                {
                    state = GAME_ROOM;
                    continue;
                }
            }

            // ============================================================================
            // GAME ROOM (Player 2 Login)
            // ============================================================================
            if (state == GAME_ROOM)
            {
                player2UsernameInput.update(mouse, e);
                player2PasswordInput.update(mouse, e);

                if (backButton.isClicked(mouse, e))
                {
                    if (!player2Username.empty())
                    {
                        matchmaking.removePlayer(player2Username);
                    }
                    backButton.hovered = false;
                    leaveQueueButton.hovered = false;
                    addPlayer2Button.hovered = false;
                    state = MATCHMAKING_QUEUE;
                    player2UsernameInput.clear();
                    player2PasswordInput.clear();
                    player2LoggedIn = false;
                    player2Username = "";
                    player2UsernameInput.active = false;
                    player2PasswordInput.active = false;
                    errorMessage = "";
                    continue;
                }

                Button player2LoginButton;
                player2LoginButton.init(centerX, 350, 200, 50, "Player 2 Login", font);
                player2LoginButton.update(mouse);

                if (player2LoginButton.isClicked(mouse, e))
                {
                    string p2User = player2UsernameInput.getText();
                    string p2Pass = player2PasswordInput.getText();
                    if (p2User.empty())
                    {
                        errorMessage = "Username cannot be empty";
                    }
                    else if (auth.loginPlayer(p2User, p2Pass, errorMessage))
                    {
                        player2Username = p2User;
                        player2LoggedIn = true;
                        int player2Score = auth.getPlayerScore(player2Username);
                        player2Queue.username = player2Username;
                        player2Queue.score = player2Score;
                        player2Queue.id = player2ID;
                        matchmaking.addPlayer(player2Username, player2Score, player2ID);
                        player2QueuePosition = matchmaking.getPlayerPosition(player2Username);
                        player2PlayersAbove = matchmaking.getPlayersAbove(player2Username);
                        matchmaking.removePlayer(player2Username);
                    }
                    else
                    {
                        errorClock.restart();
                    }
                }

                if (player2LoggedIn && startMultiplayerButton.isClicked(mouse, e))
                {
                    state = MULTIPLAYER;
                    currentLevelId = levels[selectedLevel].id;
                    enemyCount = levels[selectedLevel].initialEnemies;
                    for (int i = 1; i < ROWS - 1; i++)
                        for (int j = 1; j < COLS - 1; j++)
                            tileGrid[i][j] = 0;
                    playerCol = 10;
                    playerRow = 0;
                    playerDirCol = playerDirRow = 0;
                    running = true;
                    score = 0;
                    rewardComboCount = 0;
                    availablePowerUps = 0;
                    lastPowerUpAwardScore = 0;
                    player2Col = 15;
                    player2Row = 0;
                    player2DirCol = player2DirRow = 0;
                    player2Running = true;
                    player2Score = 0;
                    player2RewardComboCount = 0;
                    player2AvailablePowerUps = 0;
                    player2LastPowerUpAwardScore = 0;
                    for (int i = 0; i < enemyCount; i++)
                        enemies[i] = Enemy();
                    continue;
                }
            }

            // ============================================================================
            // ESC to return to menu
            // ============================================================================
            if ((state == PLAYING || state == MULTIPLAYER) && e.type == Event::KeyPressed && e.key.code == Keyboard::Escape)
            {
                if (state == PLAYING)
                {
                    matchmaking.removePlayer(currentUser);
                    state = START_MENU;
                }
                else if (state == MULTIPLAYER)
                {
                    state = GAME_ROOM;
                    player1Queue.score = auth.getPlayerScore(currentUser);
                    player2Queue.score = auth.getPlayerScore(player2Username);
                    for (int i = 1; i < ROWS - 1; i++)
                        for (int j = 1; j < COLS - 1; j++)
                            tileGrid[i][j] = 0;
                    playerCol = 10;
                    playerRow = 0;
                    playerDirCol = playerDirRow = 0;
                    running = true;
                    score = 0;
                    rewardComboCount = 0;
                    availablePowerUps = 0;
                    lastPowerUpAwardScore = 0;
                    player2Col = 15;
                    player2Row = 0;
                    player2DirCol = player2DirRow = 0;
                    player2Running = true;
                    player2Score = 0;
                    player2RewardComboCount = 0;
                    player2AvailablePowerUps = 0;
                    player2LastPowerUpAwardScore = 0;
                    for (int i = 0; i < enemyCount; i++)
                    {
                        enemies[i] = Enemy();
                    }
                }
            }

            // MODE SELECT REMOVED - matchmaking handles all modes

            // START MENU
            if (state == START_MENU)
            {
                if (startGameButton.isClicked(mouse, e))
                {
                    state = MODE_SELECT;
                    continue;
                }
                if (selectLevelButton.isClicked(mouse, e))
                {
                    state = SELECT_LEVEL;
                    continue;
                }
                if (profileButton.isClicked(mouse, e))
                {
                    state = PROFILE;
                    continue;
                }
                if (leaderboardButton.isClicked(mouse, e))
                {
                    state = LEADERBOARD;
                    continue;
                }
                if (logoutButton.isClicked(mouse, e))
                {
                    state = LOGIN_SCREEN;
                    usernameInputLogin.clear();
                    passwordInputLogin.clear();
                    usernameInputRegister.clear();
                    passwordInputRegister.clear();
                    usernameInputRegister.active = false;
                    passwordInputRegister.active = false;
                    currentUser = "";
                    errorMessage = "";
                    continue;
                }
            }

            // MODE SELECT - Choose Single or Multiplayer
            if (state == MODE_SELECT)
            {
                if (singlePlayerButton.isClicked(mouse, e))
                {
                    gameMode = 1; // single player
                    state = PLAYING;
                    currentLevelId = levels[selectedLevel].id;
                    enemyCount = levels[selectedLevel].initialEnemies;
                    // Reset
                    for (int i = 1; i < ROWS - 1; i++)
                        for (int j = 1; j < COLS - 1; j++)
                            tileGrid[i][j] = 0;
                    playerCol = 10;
                    playerRow = 0;
                    playerDirCol = playerDirRow = 0;
                    running = true;
                    score = 0;
                    rewardComboCount = 0;
                    availablePowerUps = 0;
                    lastPowerUpAwardScore = 0;
                    for (int i = 0; i < enemyCount; i++)
                        enemies[i] = Enemy();
                    continue;
                }
                if (multiPlayerButton.isClicked(mouse, e))
                {
                    gameMode = 2; // multiplayer
                    matchmaking.removePlayer(currentUser);
                    multiPlayerButton.hovered = false;
                    int playerScore = auth.getPlayerScore(currentUser);
                    matchmaking.addPlayer(currentUser, playerScore, player1ID);
                    player1Queue.username = currentUser;
                    player1Queue.score = playerScore;
                    player1Queue.id = player1ID;
                    player1QueuePosition = matchmaking.getPlayerPosition(currentUser);
                    player1PlayersAbove = matchmaking.getPlayersAbove(currentUser);
                    state = MATCHMAKING_QUEUE;
                    continue;
                }
            }

            // SELECT LEVEL
            if (state == SELECT_LEVEL)
            {
                if (easyButton.isClicked(mouse, e))
                {
                    selectedLevel = 0;
                    state = START_MENU;
                    continue;
                }
                if (mediumButton.isClicked(mouse, e))
                {
                    selectedLevel = 1;
                    state = START_MENU;
                    continue;
                }
                if (hardButton.isClicked(mouse, e))
                {
                    selectedLevel = 2;
                    state = START_MENU;
                    continue;
                }
                if (levelBackButton.isClicked(mouse, e))
                {
                    state = START_MENU;
                    continue;
                }
            }
            if (state == LEADERBOARD)
            {
                if (e.type == Event::MouseButtonPressed &&
                    e.mouseButton.button == Mouse::Left &&
                    mouse.x >= 10 && mouse.x <= 60 &&
                    mouse.y >= 20 && mouse.y <= 70)
                {
                    state = START_MENU;
                    errorMessage = "";
                    continue;
                }
            }

            // PROFILE
            if (state == PROFILE)
            {
                if (e.type == Event::MouseButtonPressed &&
                    e.mouseButton.button == Mouse::Left &&
                    mouse.x >= 10 && mouse.x <= 60 &&
                    mouse.y >= 20 && mouse.y <= 70)
                {
                    state = START_MENU;
                    errorMessage = "";
                    continue;
                }
            }

            // END MENU
            if (state == END_MENU)
            {
                if (restartButton.isClicked(mouse, e))
                {
                    if (gameMode == 2) // multiplayer
                    {
                        state = MULTIPLAYER;
                        currentLevelId = levels[selectedLevel].id;
                        enemyCount = levels[selectedLevel].initialEnemies;
                        for (int i = 1; i < ROWS - 1; i++)
                            for (int j = 1; j < COLS - 1; j++)
                                tileGrid[i][j] = 0;
                        playerCol = 10;
                        playerRow = 0;
                        playerDirCol = playerDirRow = 0;
                        running = true;
                        score = 0;
                        rewardComboCount = 0;
                        availablePowerUps = 0;
                        lastPowerUpAwardScore = 0;
                        player2Col = 30;
                        player2Row = 24;
                        player2DirCol = player2DirRow = 0;
                        player2Running = true;
                        player2Score = 0;
                        player2RewardComboCount = 0;
                        player2AvailablePowerUps = 0;
                        player2LastPowerUpAwardScore = 0;
                        isFreezeActive = false;
                        player2FreezeActive = false;
                        freezeClock.restart();
                        player2FreezeClock.restart();
                        for (int i = 0; i < enemyCount; i++)
                            enemies[i] = Enemy();
                    }
                    else // single player
                    {
                        state = PLAYING;
                        currentLevelId = levels[selectedLevel].id;
                        enemyCount = levels[selectedLevel].initialEnemies;
                        for (int i = 1; i < ROWS - 1; i++)
                            for (int j = 1; j < COLS - 1; j++)
                                tileGrid[i][j] = 0;
                        playerCol = 10;
                        playerRow = 0;
                        playerDirCol = playerDirRow = 0;
                        running = true;
                        score = 0;
                        rewardComboCount = 0;
                        availablePowerUps = 0;
                        lastPowerUpAwardScore = 0;
                        for (int i = 0; i < enemyCount; i++)
                            enemies[i] = Enemy();
                    }
                    continue;
                }
                if (mainMenuButton.isClicked(mouse, e))
                {
                    state = START_MENU;
                    gameMode = 0;
                    continue;
                }
                if (exitGameButton.isClicked(mouse, e))
                {
                    window.close();
                    continue;
                }
            }
        }
        if (state == PLAYING && running)
        {
            // Player movement
            if (Keyboard::isKeyPressed(Keyboard::Left))
            {
                playerDirCol = -1;
                playerDirRow = 0;
            }
            if (Keyboard::isKeyPressed(Keyboard::Right))
            {
                playerDirCol = 1;
                playerDirRow = 0;
            }
            if (Keyboard::isKeyPressed(Keyboard::Up))
            {
                playerDirCol = 0;
                playerDirRow = -1;
            }
            if (Keyboard::isKeyPressed(Keyboard::Down))
            {
                playerDirCol = 0;
                playerDirRow = 1;
            }

            // Power-up
            // Power-up: only trigger on initial press, not when held
            bool spacePressed = Keyboard::isKeyPressed(Keyboard::Space);
            if (spacePressed && !spaceWasPressed && availablePowerUps > 0 && !isFreezeActive)
            {
                isFreezeActive = true;
                freezeClock.restart();
                availablePowerUps--;
            }
            spaceWasPressed = spacePressed;

            if (isFreezeActive && freezeClock.getElapsedTime().asSeconds() > 3.0f)
                isFreezeActive = false;

            timer += clock.restart().asSeconds();
            if (timer > delay)
            {
                playerCol += playerDirCol;
                playerRow += playerDirRow;
                playerCol = max(0, min(COLS - 1, playerCol));
                playerRow = max(0, min(ROWS - 1, playerRow));
                if (tileGrid[playerRow][playerCol] == 2)
                    running = false;
                if (tileGrid[playerRow][playerCol] == 0)
                    tileGrid[playerRow][playerCol] = 2;
                timer = 0;
            }

            if (!isFreezeActive)
                for (int i = 0; i < enemyCount; i++)
                    enemies[i].move();

            if (tileGrid[playerRow][playerCol] == 1)
            {
                playerDirCol = playerDirRow = 0;
                int tilesCaptured = 0;

                // Mark all areas with enemies (those are NOT capturable)
                for (int i = 0; i < enemyCount; i++)
                    floodFillMark(enemies[i].posY / TILE_SIZE_PIXELS, enemies[i].posX / TILE_SIZE_PIXELS);

                // Convert all red tiles to blue (captured), empty spaces stay empty
                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                    {
                        if (tileGrid[i][j] == 2)
                            tilesCaptured++;
                        tileGrid[i][j] = (tileGrid[i][j] == -1 ? 0 : 1);  // Mark -1 as empty, else blue
                    }

                int multiplier = 1;
                if (tilesCaptured > 10)
                    multiplier = 2;
                if (rewardComboCount >= 3 && tilesCaptured > 5)
                    multiplier = 2;
                if (rewardComboCount >= 5 && tilesCaptured > 5)
                    multiplier = 4;
                score += tilesCaptured * multiplier;

                if (multiplier > 1)
                    rewardComboCount++;  // Increase combo counter for bonus tiles

                // Award power-up every 50 points
                if (score - lastPowerUpAwardScore >= 50)
                {
                    availablePowerUps++;
                    lastPowerUpAwardScore = score;
                }
            }

            // Check if enemy stepped on player's constructing tiles (game over condition)
            for (int i = 0; i < enemyCount; i++)
                if (tileGrid[enemies[i].posY / TILE_SIZE_PIXELS][enemies[i].posX / TILE_SIZE_PIXELS] == 2)
                    running = false;

            // When game ends (player dies or grid filled)
            if (!running)
            {
                // Get player's previous best score
                int playerTopScore = auth.getPlayerTopScore(currentUser);

                // Check if current score is a new personal best
                isNewHighScore = (score > playerTopScore);

                // Update player's top score if needed
                auth.updatePlayerTopScore(currentUser, score);

                // Add score to leaderboard (MinHeap keeps top 10)
                leaderboardManager.addScore(currentUser, score, currentLevelId);

                // Update player profile with match result
                PlayerProfile* profile = profileManager.getProfile(currentUser);
                if (profile == nullptr)
                    profileManager.createProfile(currentUser);

                // Win condition: score >= 450 points
                bool playerWon = (score >= 450);
                profileManager.updateProfile(currentUser, score, playerWon);
                profileManager.addMatch(currentUser, score, currentLevelId, playerWon);

                state = END_MENU;
            }
        }

        // ============================================================================
        // MULTIPLAYER GAME LOGIC
        // ============================================================================
        if (state == MULTIPLAYER && (running || player2Running))
        {
            if (running)
            {
                if (Keyboard::isKeyPressed(Keyboard::Left))
                {
                    playerDirCol = -1;
                    playerDirRow = 0;
                }
                if (Keyboard::isKeyPressed(Keyboard::Right))
                {
                    playerDirCol = 1;
                    playerDirRow = 0;
                }
                if (Keyboard::isKeyPressed(Keyboard::Up))
                {
                    playerDirCol = 0;
                    playerDirRow = -1;
                }
                if (Keyboard::isKeyPressed(Keyboard::Down))
                {
                    playerDirCol = 0;
                    playerDirRow = 1;
                }

                bool enterPressed = Keyboard::isKeyPressed(Keyboard::Enter);
                if (enterPressed && !enterWasPressed && availablePowerUps > 0 && !isFreezeActive)
                {
                    player2FreezeActive = true;
                    player2FreezeClock.restart();
                    availablePowerUps--;
                }
                enterWasPressed = enterPressed;
            }

            if (player2Running && running)
            {
                if (Keyboard::isKeyPressed(Keyboard::A))
                {
                    player2DirCol = -1;
                    player2DirRow = 0;
                }
                if (Keyboard::isKeyPressed(Keyboard::D))
                {
                    player2DirCol = 1;
                    player2DirRow = 0;
                }
                if (Keyboard::isKeyPressed(Keyboard::W))
                {
                    player2DirCol = 0;
                    player2DirRow = -1;
                }
                if (Keyboard::isKeyPressed(Keyboard::S))
                {
                    player2DirCol = 0;
                    player2DirRow = 1;
                }

                bool spacePressed = Keyboard::isKeyPressed(Keyboard::Space);
                if (spacePressed && !spaceWasPressed && player2AvailablePowerUps > 0 && !player2FreezeActive)
                {
                    isFreezeActive = true;
                    freezeClock.restart();
                    player2AvailablePowerUps--;
                }
                spaceWasPressed = spacePressed;
            }

            if (isFreezeActive && freezeClock.getElapsedTime().asSeconds() > 3.0f)
                isFreezeActive = false;
            if (player2FreezeActive && player2FreezeClock.getElapsedTime().asSeconds() > 3.0f)
                player2FreezeActive = false;

            timer += clock.restart().asSeconds();
            if (timer > delay)
            {
                bool p1Frozen = isFreezeActive;
                bool p2Frozen = player2FreezeActive;
                if (running && !p1Frozen)
                {
                    playerCol += playerDirCol;
                    playerRow += playerDirRow;
                    playerCol = max(0, min(COLS - 1, playerCol));
                    playerRow = max(0, min(ROWS - 1, playerRow));

                    int prevTile = tileGrid[playerRow][playerCol];
                    if (playerRow == player2Row && playerCol == player2Col)
                    {
                        bool p1Constructing = (playerDirCol != 0 || playerDirRow != 0);
                        bool p2Constructing = (player2DirCol != 0 || player2DirRow != 0);
                        bool p1OnBlue = (prevTile == 1);

                        if (p1OnBlue && !p1Constructing && !p2Constructing)
                        {
                        }
                        else if (p1Constructing && p2Constructing)
                        {
                            running = false;
                            player2Running = false;
                        }
                        else if (p1Constructing && !p2Constructing)
                        {
                            running = false;
                        }
                        else if (!p1Constructing && p2Constructing)
                        {
                            player2Running = false;
                        }
                    }

                    if (tileGrid[playerRow][playerCol] == 2)
                    {
                        running = false;
                    }
                    if (tileGrid[playerRow][playerCol] == 0)
                        tileGrid[playerRow][playerCol] = 2;
                }

                if (player2Running && running && !p2Frozen)
                {
                    player2Col += player2DirCol;
                    player2Row += player2DirRow;
                    player2Col = max(0, min(COLS - 1, player2Col));
                    player2Row = max(0, min(ROWS - 1, player2Row));

                    if (playerRow == player2Row && playerCol == player2Col)
                    {
                        bool p1Constructing = (playerDirCol != 0 || playerDirRow != 0);
                        bool p2Constructing = (player2DirCol != 0 || player2DirRow != 0);
                        bool p2OnBlue = (tileGrid[player2Row][player2Col] == 1);

                        if (p2OnBlue && !p1Constructing && !p2Constructing)
                        {
                        }
                        else if (p1Constructing && p2Constructing)
                        {
                            running = false;
                            player2Running = false;
                        }
                        else if (p1Constructing && !p2Constructing)
                        {
                            running = false;
                        }
                        else if (!p1Constructing && p2Constructing)
                        {
                            player2Running = false;
                        }
                    }

                    if (tileGrid[player2Row][player2Col] == 2 || tileGrid[player2Row][player2Col] == 3)
                    {
                        player2Running = false;
                    }
                    if (tileGrid[player2Row][player2Col] == 0)
                        tileGrid[player2Row][player2Col] = 3;
                }

                timer = 0;
            }

            if (!isFreezeActive && !player2FreezeActive && running && player2Running)
                for (int i = 0; i < enemyCount; i++)
                    enemies[i].move();

            if (running && tileGrid[playerRow][playerCol] == 1)
            {
                playerDirCol = playerDirRow = 0;
                int pathTiles = 0;

                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                        if (tileGrid[i][j] == 2)
                            pathTiles++;

                if (pathTiles > 0)
                {
                    for (int i = 0; i < enemyCount; i++)
                    {
                        int enemyRow = enemies[i].posY / TILE_SIZE_PIXELS;
                        int enemyCol = enemies[i].posX / TILE_SIZE_PIXELS;
                        floodFillMark(enemyRow, enemyCol);
                    }

                    int tilesCaptured = 0;
                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == 2)
                                tilesCaptured++;
                            else if (tileGrid[i][j] == 0 && i > 0 && i < ROWS - 1 && j > 0 && j < COLS - 1)
                                tilesCaptured++;
                        }

                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == -1)
                                tileGrid[i][j] = 0;
                            else if (tileGrid[i][j] == 2 || tileGrid[i][j] == 0)
                                tileGrid[i][j] = 1;
                        }

                    int multiplier = 1;
                    if (tilesCaptured > 10)
                        multiplier = 2;
                    if (rewardComboCount >= 3 && tilesCaptured > 5)
                        multiplier = 2;
                    if (rewardComboCount >= 5 && tilesCaptured > 5)
                        multiplier = 4;
                    score += tilesCaptured * multiplier;
                    if (multiplier > 1)
                        rewardComboCount++;

                    if (score - lastPowerUpAwardScore >= 50)
                    {
                        availablePowerUps++;
                        lastPowerUpAwardScore = score;
                    }
                }
            }

            if (player2Running && running && tileGrid[player2Row][player2Col] == 1)
            {
                player2DirCol = player2DirRow = 0;
                int pathTiles = 0;

                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                        if (tileGrid[i][j] == 3)
                            pathTiles++;

                if (pathTiles > 0)
                {
                    for (int i = 0; i < enemyCount; i++)
                    {
                        int enemyRow = enemies[i].posY / TILE_SIZE_PIXELS;
                        int enemyCol = enemies[i].posX / TILE_SIZE_PIXELS;
                        floodFillMark(enemyRow, enemyCol);
                    }

                    int tilesCaptured = 0;
                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == 3)
                                tilesCaptured++;
                            else if (tileGrid[i][j] == 0 && i > 0 && i < ROWS - 1 && j > 0 && j < COLS - 1)
                                tilesCaptured++;
                        }

                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == -1)
                                tileGrid[i][j] = 0;
                            else if (tileGrid[i][j] == 3 || tileGrid[i][j] == 0)
                                tileGrid[i][j] = 1;
                        }

                    int multiplier = 1;
                    if (tilesCaptured > 10)
                        multiplier = 2;
                    if (player2RewardComboCount >= 3 && tilesCaptured > 5)
                        multiplier = 2;
                    if (player2RewardComboCount >= 5 && tilesCaptured > 5)
                        multiplier = 4;
                    player2Score += tilesCaptured * multiplier;
                    if (multiplier > 1)
                        player2RewardComboCount++;

                    if (player2Score - player2LastPowerUpAwardScore >= 50)
                    {
                        player2AvailablePowerUps++;
                        player2LastPowerUpAwardScore = player2Score;
                    }
                }

                int emptyTiles = 0;
                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                        if (tileGrid[i][j] == 0)
                            emptyTiles++;
                if (emptyTiles == 0)
                {
                    running = false;
                    player2Running = false;
                }
            }

            for (int i = 0; i < enemyCount; i++)
            {
                int enemyRow = enemies[i].posY / TILE_SIZE_PIXELS;
                int enemyCol = enemies[i].posX / TILE_SIZE_PIXELS;
                if (enemyCol >= 0 && enemyCol < COLS && enemyRow >= 0 && enemyRow < ROWS)
                {
                    if (tileGrid[enemyRow][enemyCol] == 2)
                    {
                        running = false;
                    }
                    if (tileGrid[enemyRow][enemyCol] == 3)
                    {
                        player2Running = false;
                    }
                }
            }

            if (!running && player2Running)
            {
                auth.updatePlayerScore(currentUser, score);
                auth.updatePlayerScore(player2Username, player2Score);
                leaderboardManager.addScore(currentUser, score, currentLevelId);
                leaderboardManager.addScore(player2Username, player2Score, currentLevelId);
                state = END_MENU;
            }
            if (running && !player2Running)
            {
                auth.updatePlayerScore(currentUser, score);
                auth.updatePlayerScore(player2Username, player2Score);
                leaderboardManager.addScore(currentUser, score, currentLevelId);
                leaderboardManager.addScore(player2Username, player2Score, currentLevelId);
                state = END_MENU;
            }
            if (!running && !player2Running)
            {
                auth.updatePlayerScore(currentUser, score);
                auth.updatePlayerScore(player2Username, player2Score);
                leaderboardManager.addScore(currentUser, score, currentLevelId);
                leaderboardManager.addScore(player2Username, player2Score, currentLevelId);
                state = END_MENU;
            }
        }

        // ============================================================================
        // DRAW
        // ============================================================================
        window.clear();

        if (state == LOGIN_SCREEN)
        {
            Text t("XONIX GAME", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 80);
            window.draw(t);
            usernameInputLogin.draw(window);
            passwordInputLogin.draw(window);
            loginButton.update(mouse);
            loginButton.draw(window);
            registerScreenButton.update(mouse);
            registerScreenButton.draw(window);
            if (!errorMessage.empty() && errorClock.getElapsedTime().asSeconds() < 3)
            {
                Text e(errorMessage, font, 16);
                e.setFillColor(Color::Red);
                FloatRect eb = e.getLocalBounds();
                e.setOrigin(eb.width / 2, eb.height / 2);
                e.setPosition(centerX, 280);
                window.draw(e);
            }
        }
        else if (state == REGISTER_SCREEN)
        {
            Text t("CREATE ACCOUNT", font, 30);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 150);
            window.draw(t);
            usernameInputRegister.draw(window);
            passwordInputRegister.draw(window);
            createAccountButton.update(mouse);
            createAccountButton.draw(window);
            backButton.update(mouse);
            backButton.draw(window);
            if (!errorMessage.empty() && errorClock.getElapsedTime().asSeconds() < 3)
            {
                Text e(errorMessage, font, 16);
                e.setFillColor(Color::Red);
                FloatRect eb = e.getLocalBounds();
                e.setOrigin(eb.width / 2, eb.height / 2);
                e.setPosition(centerX, 330);
                window.draw(e);
            }
        }
        else if (state == MAIN_MENU)
        {
            Text t("WELCOME " + currentUser + "!", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, ROWS * TILE_SIZE_PIXELS / 2 - 100);
            window.draw(t);
            playGameButton.update(mouse);
            playGameButton.draw(window);
        }
        else if (state == START_MENU)
        {
            Text t("WELCOME " + currentUser + "!", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 80);
            window.draw(t);

            Text sub("Main Menu", font, 18);
            FloatRect sb = sub.getLocalBounds();
            sub.setOrigin(sb.width / 2, sb.height / 2);
            sub.setPosition(centerX, 120);
            sub.setFillColor(Color::White);
            window.draw(sub);

            startGameButton.update(mouse);
            startGameButton.draw(window);
            profileButton.update(mouse);
            profileButton.draw(window);
            selectLevelButton.update(mouse);
            selectLevelButton.draw(window);
            leaderboardButton.update(mouse);
            leaderboardButton.draw(window);
            logoutButton.update(mouse);
            logoutButton.draw(window);
        }
        // MODE_SELECT - Choose Single or Multiplayer
        else if (state == MODE_SELECT)
        {
            Text t("SELECT MODE", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 80);
            window.draw(t);

            Text info("Choose Single Player or Multiplayer", font, 18);
            FloatRect ib = info.getLocalBounds();
            info.setOrigin(ib.width / 2, ib.height / 2);
            info.setPosition(centerX, 130);
            info.setFillColor(Color::White);
            window.draw(info);

            singlePlayerButton.update(mouse);
            singlePlayerButton.draw(window);
            multiPlayerButton.update(mouse);
            multiPlayerButton.draw(window);
        }
        // ============================================================================
        // MATCHMAKING QUEUE SCREEN
        // ============================================================================
        else if (state == MATCHMAKING_QUEUE)
        {
            Text title("MATCHMAKING QUEUE", font, 40);
            FloatRect b = title.getLocalBounds();
            title.setOrigin(roundf(b.width / 2), roundf(b.height / 2));
            setTextPosition(title, centerX, 80);
            window.draw(title);

            string statsText = "Your Score: " + to_string(player1Queue.score);
            Text stats(statsText, font, 24);
            FloatRect sb = stats.getLocalBounds();
            stats.setOrigin(roundf(sb.width / 2), roundf(sb.height / 2));
            setTextPosition(stats, centerX, 150);
            window.draw(stats);

            string queueText = "Players in Queue: " + to_string(matchmaking.getQueueSize());
            Text queue(queueText, font, 20);
            FloatRect qb = queue.getLocalBounds();
            queue.setOrigin(roundf(qb.width / 2), roundf(qb.height / 2));
            setTextPosition(queue, centerX, 200);
            window.draw(queue);

            addPlayer2Button.update(mouse);
            addPlayer2Button.draw(window);
            leaveQueueButton.update(mouse);
            leaveQueueButton.draw(window);
        }
        // ============================================================================
        // GAME ROOM SCREEN
        // ============================================================================
        else if (state == GAME_ROOM)
        {
            Text title("GAME ROOM", font, 40);
            FloatRect b = title.getLocalBounds();
            title.setOrigin(roundf(b.width / 2), roundf(b.height / 2));
            setTextPosition(title, centerX, 60);
            window.draw(title);

            string p1Text = "Player 1: " + currentUser + " ( Score: " + to_string(player1Queue.score) + " )";
            Text p1(p1Text, font, 20);
            FloatRect p1b = p1.getLocalBounds();
            p1.setOrigin(roundf(p1b.width / 2), roundf(p1b.height / 2));
            setTextPosition(p1, centerX, 110);
            window.draw(p1);

            if (player2LoggedIn)
            {
                string p2Text = "Player 2: " + player2Username + " ( Score: " + to_string(player2Queue.score) + " )";
                Text p2(p2Text, font, 20);
                FloatRect p2b = p2.getLocalBounds();
                p2.setOrigin(roundf(p2b.width / 2), roundf(p2b.height / 2));
                setTextPosition(p2, centerX, 140);
                window.draw(p2);

                Text statsTitle("--- MATCHMAKING STATS ---", font, 18);
                statsTitle.setFillColor(Color::Yellow);
                FloatRect stb = statsTitle.getLocalBounds();
                statsTitle.setOrigin(roundf(stb.width / 2), roundf(stb.height / 2));
                setTextPosition(statsTitle, centerX, 180);
                window.draw(statsTitle);

                string p1QueueStats = currentUser + ": Queue Position #" + to_string(player1QueuePosition);
                if (player1PlayersAbove > 0)
                    p1QueueStats += " (" + to_string(player1PlayersAbove) + " players ahead)";
                else if (player1PlayersAbove == 0)
                    p1QueueStats += " (Top Priority!)";
                Text p1Stats(p1QueueStats, font, 16);
                p1Stats.setFillColor(Color::Cyan);
                FloatRect p1sb = p1Stats.getLocalBounds();
                p1Stats.setOrigin(roundf(p1sb.width / 2), roundf(p1sb.height / 2));
                setTextPosition(p1Stats, centerX, 210);
                window.draw(p1Stats);

                string p2QueueStats = player2Username + ": Queue Position #" + to_string(player2QueuePosition);
                if (player2PlayersAbove > 0)
                    p2QueueStats += " (" + to_string(player2PlayersAbove) + " players ahead)";
                else if (player2PlayersAbove == 0)
                    p2QueueStats += " (Top Priority!)";
                Text p2Stats(p2QueueStats, font, 16);
                p2Stats.setFillColor(Color(255, 200, 100));
                FloatRect p2sb = p2Stats.getLocalBounds();
                p2Stats.setOrigin(roundf(p2sb.width / 2), roundf(p2sb.height / 2));
                setTextPosition(p2Stats, centerX, 235);
                window.draw(p2Stats);

                string priorityText;
                Color priorityColor;
                if (player1Queue.score > player2Queue.score)
                {
                    priorityText = currentUser + " has HIGHER PRIORITY (Score: " + to_string(player1Queue.score) + " > " + to_string(player2Queue.score) + ")";
                    priorityColor = Color::Cyan;
                }
                else if (player2Queue.score > player1Queue.score)
                {
                    priorityText = player2Username + " has HIGHER PRIORITY (Score: " + to_string(player2Queue.score) + " > " + to_string(player1Queue.score) + ")";
                    priorityColor = Color(255, 200, 100);
                }
                else
                {
                    priorityText = "EQUAL PRIORITY (Both have score: " + to_string(player1Queue.score) + ")";
                    priorityColor = Color::Green;
                }
                Text priority(priorityText, font, 16);
                priority.setFillColor(priorityColor);
                FloatRect prb = priority.getLocalBounds();
                priority.setOrigin(roundf(prb.width / 2), roundf(prb.height / 2));
                setTextPosition(priority, centerX, 265);
                window.draw(priority);

                string firstInQueue;
                if (player1QueuePosition < player2QueuePosition)
                    firstInQueue = currentUser + " joined queue first (Position #" + to_string(player1QueuePosition) + ")";
                else if (player2QueuePosition < player1QueuePosition)
                    firstInQueue = player2Username + " joined queue first (Position #" + to_string(player2QueuePosition) + ")";
                else
                    firstInQueue = "Both had same queue position";
                Text firstQ(firstInQueue, font, 14);
                firstQ.setFillColor(Color(180, 180, 180));
                FloatRect fqb = firstQ.getLocalBounds();
                firstQ.setOrigin(roundf(fqb.width / 2), roundf(fqb.height / 2));
                setTextPosition(firstQ, centerX, 295);
                window.draw(firstQ);

                int scoreDiff = abs(player1Queue.score - player2Queue.score);
                string diffText = "Score Difference: " + to_string(scoreDiff) + " points";
                Text diff(diffText, font, 14);
                diff.setFillColor(Color(150, 150, 150));
                FloatRect db = diff.getLocalBounds();
                diff.setOrigin(roundf(db.width / 2), roundf(db.height / 2));
                setTextPosition(diff, centerX, 320);
                window.draw(diff);

                startMultiplayerButton.update(mouse);
                startMultiplayerButton.draw(window);
            }
            else
            {
                player2UsernameInput.draw(window);
                player2PasswordInput.draw(window);
                Button player2LoginButton;
                player2LoginButton.init(centerX, 350, 200, 50, "Player 2 Login", font);
                player2LoginButton.update(mouse);
                player2LoginButton.draw(window);
            }

            backButton.update(mouse);
            backButton.draw(window);

            if (!errorMessage.empty() && errorClock.getElapsedTime().asSeconds() < 3)
            {
                Text e(errorMessage, font, 16);
                e.setFillColor(Color::Red);
                FloatRect eb = e.getLocalBounds();
                e.setOrigin(eb.width / 2, eb.height / 2);
                e.setPosition(centerX, 420);
                window.draw(e);
            }
        }
        else if (state == SELECT_LEVEL)
        {
            Text t("SELECT LEVEL", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 80);
            window.draw(t);

            easyButton.update(mouse);
            easyButton.draw(window);
            mediumButton.update(mouse);
            mediumButton.draw(window);
            hardButton.update(mouse);
            hardButton.draw(window);
            levelBackButton.update(mouse);
            levelBackButton.draw(window);
        }
        else if (state == LEADERBOARD)
        {
            bool backArrowHovered = (mouse.x >= 10 && mouse.x <= 60 &&
                mouse.y >= 20 && mouse.y <= 70);

            Text backArrow("<", font, 50);
            backArrow.setFillColor(backArrowHovered ? Color(128, 128, 128) : Color::White);
            backArrow.setPosition(20, 30);
            window.draw(backArrow);

            RectangleShape backButtonArea(Vector2f(50, 50));
            backButtonArea.setPosition(10, 20);
            backButtonArea.setFillColor(Color::Transparent);

            Text t("LEADERBOARD", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 50);
            window.draw(t);

            LeaderboardEntry* sorted = leaderboardManager.getLeaderboard();
            int count = leaderboardManager.getCount();

            Text rankHeader("RANK", font, 14);
            rankHeader.setFillColor(Color::Cyan);
            rankHeader.setPosition(centerX - 240, 120);
            window.draw(rankHeader);

            Text playerHeader("PLAYER", font, 14);
            playerHeader.setFillColor(Color::Cyan);
            playerHeader.setPosition(centerX - 100, 120);
            window.draw(playerHeader);

            Text scoreHeader("SCORE", font, 14);
            scoreHeader.setFillColor(Color::Cyan);
            scoreHeader.setPosition(centerX + 200, 120);
            window.draw(scoreHeader);

            for (int i = 0; i < count && i < 10; i++)
            {
                char rankStr[10];
                snprintf(rankStr, sizeof(rankStr), "%d", i + 1);
                Text rankText(rankStr, font, 14);
                rankText.setFillColor(i < 3 ? Color::Cyan : Color::White);
                rankText.setPosition(centerX - 240, 160 + i * 28);
                window.draw(rankText);

                Text playerText(sorted[i].username, font, 14);
                playerText.setFillColor(i < 3 ? Color::Cyan : Color::White);
                playerText.setPosition(centerX - 100, 160 + i * 28);
                window.draw(playerText);

                char scoreStr[20];
                snprintf(scoreStr, sizeof(scoreStr), "%d", sorted[i].score);
                Text scoreText(scoreStr, font, 14);
                scoreText.setFillColor(i < 3 ? Color::Cyan : Color::White);
                scoreText.setPosition(centerX + 200, 160 + i * 28);
                window.draw(scoreText);
            }

            if (count == 0)
            {
                Text empty("No scores yet!", font, 18);
                empty.setFillColor(Color::White);
                empty.setPosition(centerX - 100, 200);
                window.draw(empty);
            }

            leaderboardManager.freeLeaderboard(sorted);
        }
        else if (state == PROFILE)
        {
            bool backArrowHovered = (mouse.x >= 10 && mouse.x <= 60 &&
                mouse.y >= 20 && mouse.y <= 70);

            Text backArrow("<", font, 50);
            backArrow.setFillColor(backArrowHovered ? Color(128, 128, 128) : Color::White);
            backArrow.setPosition(20, 30);
            window.draw(backArrow);

            Text t("PLAYER PROFILE", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 50);
            window.draw(t);

            PlayerProfile* profile = profileManager.getProfile(currentUser);
            if (profile != nullptr)
            {
                RectangleShape profileBox(Vector2f(500, 330));
                profileBox.setPosition(centerX - 250, 110);
                profileBox.setFillColor(Color(40, 40, 40));
                profileBox.setOutlineColor(Color::White);
                profileBox.setOutlineThickness(2);
                window.draw(profileBox);

                // Username
                Text usernameLabel("Username: " + profile->username, font, 18);
                usernameLabel.setFillColor(Color::White);
                usernameLabel.setPosition(centerX - 230, 125);
                window.draw(usernameLabel);

                RectangleShape separator(Vector2f(460, 2));
                separator.setPosition(centerX - 230, 155);
                separator.setFillColor(Color::White);
                window.draw(separator);

                Text pointsLabel("Total Points: " + to_string(profile->totalPoints), font, 16);
                pointsLabel.setFillColor(Color::White);
                pointsLabel.setPosition(centerX - 230, 168);
                window.draw(pointsLabel);

                Text statsLabel("Wins: " + to_string(profile->wins) + "  |  Losses: " + to_string(profile->losses), font, 16);
                statsLabel.setFillColor(Color::Green);
                statsLabel.setPosition(centerX - 230, 195);
                window.draw(statsLabel);

                Text friendsTitle("Friends", font, 15);
                friendsTitle.setFillColor(Color::White);
                friendsTitle.setPosition(centerX - 230, 225);
                window.draw(friendsTitle);

                for (int i = 0; i < profile->friendCount && i < 2; i++)
                {
                    Text friendText("  � " + profile->friends[i], font, 13);
                    friendText.setFillColor(Color::White);
                    friendText.setPosition(centerX - 220, 243 + i * 16);
                    window.draw(friendText);
                }

                if (profile->friendCount == 0)
                {
                    Text noFriends("  (No friends)", font, 13);
                    noFriends.setFillColor(Color(150, 150, 150));
                    noFriends.setPosition(centerX - 220, 243);
                    window.draw(noFriends);
                }
            }
        }
        else if (state == END_MENU)
        {
            if (gameMode == 2) // Multiplayer END MENU
            {
                Text resultTitle("GAME OVER", font, 40);
                FloatRect rtitle = resultTitle.getLocalBounds();
                resultTitle.setOrigin(rtitle.width / 2, rtitle.height / 2);
                resultTitle.setPosition(centerX, 120);
                resultTitle.setFillColor(Color::White);
                window.draw(resultTitle);

                // Determine winner
                string winner = "";
                Color winnerColor = Color::White;
                if (score > player2Score)
                {
                    winner = currentUser + " WINS!";
                    winnerColor = Color::Cyan;
                }
                else if (player2Score > score)
                {
                    winner = player2Username + " WINS!";
                    winnerColor = Color(255, 255, 0); // Yellow
                }
                else
                {
                    winner = "IT'S A TIE!";
                    winnerColor = Color::Green;
                }

                Text winnerText(winner, font, 36);
                FloatRect wt = winnerText.getLocalBounds();
                winnerText.setOrigin(wt.width / 2, wt.height / 2);
                winnerText.setPosition(centerX, 180);
                winnerText.setFillColor(winnerColor);
                window.draw(winnerText);

                // Player 1 Score
                Text p1Label(currentUser + "'s Score:", font, 20);
                p1Label.setFillColor(Color::Cyan);
                p1Label.setPosition(centerX - 200, 250);
                window.draw(p1Label);

                Text p1Score(to_string(score), font, 32);
                p1Score.setFillColor(Color::Cyan);
                FloatRect p1s = p1Score.getLocalBounds();
                p1Score.setOrigin(p1s.width / 2, 0);
                p1Score.setPosition(centerX - 200, 280);
                window.draw(p1Score);

                // Player 2 Score
                Text p2Label(player2Username + "'s Score:", font, 20);
                p2Label.setFillColor(Color(255, 255, 0)); // Yellow
                p2Label.setPosition(centerX + 50, 250);
                window.draw(p2Label);

                Text p2Score(to_string(player2Score), font, 32);
                p2Score.setFillColor(Color(255, 255, 0)); // Yellow
                FloatRect p2s = p2Score.getLocalBounds();
                p2Score.setOrigin(p2s.width / 2, 0);
                p2Score.setPosition(centerX + 50, 280);
                window.draw(p2Score);
            }
            else // Single Player END MENU
            {
                Text resultTitle("GAME OVER", font, 40);
                FloatRect rtitle = resultTitle.getLocalBounds();
                resultTitle.setOrigin(rtitle.width / 2, rtitle.height / 2);
                resultTitle.setPosition(centerX, 120);
                resultTitle.setFillColor(Color::White);
                window.draw(resultTitle);

                Text finalScoreLabel("FINAL SCORE", font, 20);
                finalScoreLabel.setFillColor(Color::White);
                FloatRect fsl = finalScoreLabel.getLocalBounds();
                finalScoreLabel.setOrigin(fsl.width / 2, fsl.height / 2);
                finalScoreLabel.setPosition(centerX, 200);
                window.draw(finalScoreLabel);

                Text finalScore(to_string(score), font, 48);
                finalScore.setFillColor(isNewHighScore ? Color::Cyan : Color::White);
                FloatRect fs = finalScore.getLocalBounds();
                finalScore.setOrigin(fs.width / 2, fs.height / 2);
                finalScore.setPosition(centerX, 240);
                window.draw(finalScore);

                if (isNewHighScore)
                {
                    Text newHighScore("NEW HIGH SCORE!", font, 28);
                    newHighScore.setFillColor(Color::Cyan);
                    FloatRect nh = newHighScore.getLocalBounds();
                    newHighScore.setOrigin(nh.width / 2, nh.height / 2);
                    newHighScore.setPosition(centerX, 300);
                    window.draw(newHighScore);
                }
            }

            restartButton.update(mouse);
            restartButton.draw(window);
            mainMenuButton.update(mouse);
            mainMenuButton.draw(window);
            exitGameButton.update(mouse);
            exitGameButton.draw(window);
        }
        // ============================================================================
        // MULTIPLAYER DRAWING
        // ============================================================================
        else if (state == MULTIPLAYER)
        {
            RectangleShape leftPanel, rightPanel, bottomPanel;
            leftPanel.setSize({ (float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS });
            leftPanel.setPosition(0, 0);
            leftPanel.setFillColor(Color(30, 30, 30));
            window.draw(leftPanel);

            rightPanel.setSize({ (float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS });
            rightPanel.setPosition(COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH, 0);
            rightPanel.setFillColor(Color(30, 30, 30));
            window.draw(rightPanel);

            bottomPanel.setSize({ (float)(COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH), 60.0f });
            bottomPanel.setPosition(0, ROWS * TILE_SIZE_PIXELS);
            bottomPanel.setFillColor(Color(30, 30, 30));
            window.draw(bottomPanel);

            for (int i = 0; i < ROWS; i++)
                for (int j = 0; j < COLS; j++)
                {
                    if (tileGrid[i][j] == 0)
                        continue;
                    int tileType = 0;
                    if (tileGrid[i][j] == 1)
                        tileType = 0;
                    else if (tileGrid[i][j] == 2)
                        tileType = 54;
                    else if (tileGrid[i][j] == 3)
                        tileType = 72;
                    sTile.setTextureRect(IntRect(tileType, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
                    sTile.setPosition(HUD_PANEL_WIDTH + j * TILE_SIZE_PIXELS, i * TILE_SIZE_PIXELS);
                    window.draw(sTile);
                }

            sTile.setTextureRect(IntRect(36, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
            sTile.setPosition(HUD_PANEL_WIDTH + playerCol * TILE_SIZE_PIXELS, playerRow * TILE_SIZE_PIXELS);
            window.draw(sTile);

            sTile.setTextureRect(IntRect(18, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
            sTile.setPosition(HUD_PANEL_WIDTH + player2Col * TILE_SIZE_PIXELS, player2Row * TILE_SIZE_PIXELS);
            window.draw(sTile);

            for (int i = 0; i < enemyCount; i++)
            {
                sEnemy.setPosition(HUD_PANEL_WIDTH + enemies[i].posX, enemies[i].posY);
                if (running && player2Running)
                    sEnemy.rotate(4);
                else
                    sEnemy.setRotation(0);
                window.draw(sEnemy);
            }

            Text p1Title("PLAYER 1", font, 20);
            p1Title.setFillColor(Color::White);
            FloatRect p1tb = p1Title.getLocalBounds();
            p1Title.setOrigin(roundf(p1tb.width / 2), 0);
            setTextPosition(p1Title, HUD_PANEL_WIDTH / 2, 20);
            window.draw(p1Title);

            Text p1Name(currentUser, font, 16);
            p1Name.setFillColor(Color::Cyan);
            FloatRect p1nb = p1Name.getLocalBounds();
            p1Name.setOrigin(roundf(p1nb.width / 2), 0);
            setTextPosition(p1Name, HUD_PANEL_WIDTH / 2, 50);
            window.draw(p1Name);

            Text scoreText("Score: " + to_string(score), font, 18);
            scoreText.setFillColor(Color::White);
            FloatRect stb = scoreText.getLocalBounds();
            scoreText.setOrigin(roundf(stb.width / 2), 0);
            setTextPosition(scoreText, HUD_PANEL_WIDTH / 2, 80);
            window.draw(scoreText);

            Text powerText("PowerUps: " + to_string(availablePowerUps), font, 18);
            powerText.setFillColor(Color::White);
            FloatRect ptb = powerText.getLocalBounds();
            powerText.setOrigin(roundf(ptb.width / 2), 0);
            setTextPosition(powerText, HUD_PANEL_WIDTH / 2, 110);
            window.draw(powerText);

            if (!running)
            {
                Text status("ELIMINATED", font, 18);
                status.setFillColor(Color::Red);
                FloatRect sb = status.getLocalBounds();
                status.setOrigin(roundf(sb.width / 2), 0);
                setTextPosition(status, HUD_PANEL_WIDTH / 2, 140);
                window.draw(status);
            }

            Text p2Title("PLAYER 2", font, 20);
            p2Title.setFillColor(Color::White);
            FloatRect p2tb = p2Title.getLocalBounds();
            p2Title.setOrigin(roundf(p2tb.width / 2), 0);
            setTextPosition(p2Title, COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH + HUD_PANEL_WIDTH / 2, 20);
            window.draw(p2Title);

            Text p2Name(player2Username, font, 16);
            p2Name.setFillColor(Color::Yellow);
            FloatRect p2nb = p2Name.getLocalBounds();
            p2Name.setOrigin(roundf(p2nb.width / 2), 0);
            setTextPosition(p2Name, COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH + HUD_PANEL_WIDTH / 2, 50);
            window.draw(p2Name);

            Text p2ScoreText("Score: " + to_string(player2Score), font, 18);
            p2ScoreText.setFillColor(Color::White);
            FloatRect p2stb = p2ScoreText.getLocalBounds();
            p2ScoreText.setOrigin(roundf(p2stb.width / 2), 0);
            setTextPosition(p2ScoreText, COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH + HUD_PANEL_WIDTH / 2, 80);
            window.draw(p2ScoreText);

            Text p2PowerText("PowerUps: " + to_string(player2AvailablePowerUps), font, 18);
            p2PowerText.setFillColor(Color::White);
            FloatRect p2ptb = p2PowerText.getLocalBounds();
            p2PowerText.setOrigin(roundf(p2ptb.width / 2), 0);
            setTextPosition(p2PowerText, COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH + HUD_PANEL_WIDTH / 2, 110);
            window.draw(p2PowerText);

            if (!player2Running)
            {
                Text status("ELIMINATED", font, 18);
                status.setFillColor(Color::Red);
                FloatRect sb = status.getLocalBounds();
                status.setOrigin(roundf(sb.width / 2), 0);
                setTextPosition(status, COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH + HUD_PANEL_WIDTH / 2, 140);
                window.draw(status);
            }

            if (!running && player2Running)
            {
                Text winner(player2Username + " WINS!", font, 40);
                winner.setFillColor(Color::Yellow);
                FloatRect wb = winner.getLocalBounds();
                winner.setOrigin(roundf(wb.width / 2), roundf(wb.height / 2));
                setTextPosition(winner, centerX, ROWS * TILE_SIZE_PIXELS / 2);
                window.draw(winner);
            }
            else if (running && !player2Running)
            {
                Text winner(currentUser + " WINS!", font, 40);
                winner.setFillColor(Color::Yellow);
                FloatRect wb = winner.getLocalBounds();
                winner.setOrigin(roundf(wb.width / 2), roundf(wb.height / 2));
                setTextPosition(winner, centerX, ROWS * TILE_SIZE_PIXELS / 2);
                window.draw(winner);
            }
            else if (!running && !player2Running)
            {
                string winnerText;
                if (score > player2Score)
                    winnerText = currentUser + " WINS!";
                else if (player2Score > score)
                    winnerText = player2Username + " WINS!";
                else
                    winnerText = "TIE!";

                Text winner(winnerText, font, 40);
                winner.setFillColor(Color::Yellow);
                FloatRect wb = winner.getLocalBounds();
                winner.setOrigin(roundf(wb.width / 2), roundf(wb.height / 2));
                setTextPosition(winner, centerX, ROWS * TILE_SIZE_PIXELS / 2);
                window.draw(winner);
            }

            Text controls("P1: Arrow Keys - Move | Enter - Power-Up | P2: W/A/S/D - Move | Space - Power-Up | ESC - Exit", font, 14);
            controls.setFillColor(Color(150, 150, 150));
            FloatRect cb = controls.getLocalBounds();
            controls.setOrigin(roundf(cb.width / 2), 0);
            setTextPosition(controls, centerX, ROWS * TILE_SIZE_PIXELS + 10);
            window.draw(controls);
        }
        else if (state == PLAYING)
        {
            RectangleShape leftPanel, rightPanel, bottomPanel;
            leftPanel.setSize({ (float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS });
            leftPanel.setPosition(0, 0);
            leftPanel.setFillColor(Color(30, 30, 30));
            window.draw(leftPanel);

            rightPanel.setSize({ (float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS });
            rightPanel.setPosition(COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH, 0);
            rightPanel.setFillColor(Color(30, 30, 30));
            window.draw(rightPanel);

            bottomPanel.setSize({ (float)(COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH), 60.0f });
            bottomPanel.setPosition(0, ROWS * TILE_SIZE_PIXELS);
            bottomPanel.setFillColor(Color(30, 30, 30));
            window.draw(bottomPanel);

            for (int i = 0; i < ROWS; i++)
                for (int j = 0; j < COLS; j++)
                {
                    if (tileGrid[i][j] == 0)
                        continue;
                    sTile.setTextureRect(IntRect(tileGrid[i][j] == 1 ? 0 : 54, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
                    sTile.setPosition(HUD_PANEL_WIDTH + j * TILE_SIZE_PIXELS, i * TILE_SIZE_PIXELS);
                    window.draw(sTile);
                }
            sTile.setTextureRect(IntRect(36, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
            sTile.setPosition(HUD_PANEL_WIDTH + playerCol * TILE_SIZE_PIXELS, playerRow * TILE_SIZE_PIXELS);
            window.draw(sTile);
            for (int i = 0; i < enemyCount; i++)
            {
                sEnemy.setPosition(HUD_PANEL_WIDTH + enemies[i].posX, enemies[i].posY);
                if (running)
                    sEnemy.rotate(4);
                window.draw(sEnemy);
            }

            Text title("PLAYER INFO", font, 20);
            title.setFillColor(Color::White);
            FloatRect tb = title.getLocalBounds();
            title.setOrigin(roundf(tb.width / 2), 0);
            setTextPosition(title, HUD_PANEL_WIDTH / 2, 20);
            window.draw(title);

            Text name(currentUser, font, 16);
            name.setFillColor(Color::Cyan);
            FloatRect nb = name.getLocalBounds();
            name.setOrigin(roundf(nb.width / 2), 0);
            setTextPosition(name, HUD_PANEL_WIDTH / 2, 50);
            window.draw(name);

            Text scoreText("Score: " + to_string(score), font, 18);
            scoreText.setFillColor(Color::White);
            FloatRect stb = scoreText.getLocalBounds();
            scoreText.setOrigin(roundf(stb.width / 2), 0);
            setTextPosition(scoreText, HUD_PANEL_WIDTH / 2, 80);
            window.draw(scoreText);

            Text powerText("PowerUps: " + to_string(availablePowerUps), font, 18);
            powerText.setFillColor(Color::White);
            FloatRect ptb = powerText.getLocalBounds();
            powerText.setOrigin(roundf(ptb.width / 2), 0);
            setTextPosition(powerText, HUD_PANEL_WIDTH / 2, 110);
            window.draw(powerText);

            Text controls("Controls: Arrow Keys - Move | Enter - Power-Up | ESC - Exit", font, 14);
            controls.setFillColor(Color(150, 150, 150));
            FloatRect cb = controls.getLocalBounds();
            controls.setOrigin(roundf(cb.width / 2), 0);
            setTextPosition(controls, centerX, ROWS * TILE_SIZE_PIXELS + 10);
            window.draw(controls);

            if (!running)
                window.draw(sGameover);
        }

        window.display();
    }
    return 0;
}
