// --- XONIX GAME WITH LOGIN, SCORE, POWER-UPS, AND CLEAN CENTERED UI ---
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

// ============================================================================
// ALI - Game states (original)
// ============================================================================
const int LOGIN_SCREEN = 0;
const int REGISTER_SCREEN = 1;
const int MAIN_MENU = 2;
const int PLAYING = 3;

// ============================================================================
// AAYAN - Additional game states
// ============================================================================
const int MATCHMAKING_QUEUE = 4;
const int GAME_ROOM = 5;
const int MULTIPLAYER = 6;

// ============================================================================
// AAYAN
// ============================================================================

// -------------------------------------------------------------
// PLAYER AUTH
// -------------------------------------------------------------
struct Player
{
    int id;
    string username, password;
    int totalScore = 0;
};

// -------------------------------------------------------------
// TEXT POSITIONING HELPER
// -------------------------------------------------------------
// Helper function to set text position at integer coordinates for crisp rendering
void setTextPosition(Text& text, float x, float y)
{
    text.setPosition(roundf(x), roundf(y));
}

// -------------------------------------------------------------
// BUTTON
// -------------------------------------------------------------
class Button
{
public:
    RectangleShape shape;
    Text label;
    bool hovered = false;
    Color normal = Color(70, 130, 180);
    Color hover = Color(100, 160, 210);

    void init(float cx, float y, float w, float h,
        const string& textStr, Font& font)
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
        hovered = shape.getGlobalBounds().contains(mouse.x, mouse.y);
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

// -------------------------------------------------------------
// INPUT FIELD
// -------------------------------------------------------------
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
            active = box.getGlobalBounds().contains(mouse.x, mouse.y);

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

// -------------------------------------------------------------
// AUTH MANAGER
// -------------------------------------------------------------
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
            string rest;
            getline(iss, rest);
            istringstream restStream(rest);
            getline(restStream, p.password, ',');
            string scoreStr;
            if (getline(restStream, scoreStr))
            {
                p.totalScore = stoi(scoreStr);
            }
            else
            {
                p.totalScore = 0;
            }
            players[count++] = p;
            nextID = max(nextID, p.id + 1);
        }
    }

    void save()
    {
        ofstream f(file);
        for (int i = 0; i < count; i++)
            f << players[i].id << "," << players[i].username << "," << players[i].password << "," << players[i].totalScore << endl;
    }

    string toLower(const string& s)
    {
        string result = s;
        for (int i = 0; i < result.length(); i++)
            if (result[i] >= 'A' && result[i] <= 'Z')
                result[i] = result[i] - 'A' + 'a';
        return result;
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
        string uLower = toLower(u);
        for (int i = 0; i < count; i++)
            if (toLower(players[i].username) == uLower)
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
        string uLower = toLower(u);
        for (int i = 0; i < count; i++)
            if (toLower(players[i].username) == uLower)
            {
                if (players[i].password == p)
                    return true;
                msg = "Wrong password";
                return false;
            }
        msg = "User not found";
        return false;
    }

    int getPlayerScore(const string& username)
    {
        string uLower = toLower(username);
        for (int i = 0; i < count; i++)
            if (toLower(players[i].username) == uLower)
                return players[i].totalScore;
        return 0;
    }

    void updatePlayerScore(const string& username, int newScore)
    {
        string uLower = toLower(username);
        for (int i = 0; i < count; i++)
            if (toLower(players[i].username) == uLower)
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

// ============================================================================
// AAYAN - PRIORITY QUEUE IMPLEMENTATION
// ============================================================================

struct QueuePlayer
{
    string username;
    int score;
    int id;
};

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

    // Get position (rank) of a player in the queue (1-based, 1 = highest priority)
    int getPlayerPosition(const string& username)
    {
        // Create a temporary sorted array to find position
        QueuePlayer temp[100];
        int tempSize = 0;
        
        // Copy all players
        for (int i = 0; i < size; i++)
            temp[tempSize++] = queue[i];
        
        // Sort by score (descending) - simple bubble sort
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
        
        // Find position
        for (int i = 0; i < tempSize; i++)
        {
            if (temp[i].username == username)
                return i + 1; // 1-based position
        }
        return -1; // Not found
    }

    // Get how many players have higher priority (higher score)
    int getPlayersAbove(const string& username)
    {
        int playerScore = -1;
        // Find the player's score
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
        
        // Count players with higher score
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
// AAYAN - MATCHMAKING SYSTEM
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

// -------------------------------------------------------------
// ENEMY + FLOOD FILL
// -------------------------------------------------------------
struct Enemy
{
    int posX, posY;    // pixel coordinates
    int velX, velY;    // velocity in pixels per tick
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

// -------------------------------------------------------------
// FONT LOADING HELPER
// -------------------------------------------------------------
bool loadFont(Font& font)
{
    if (font.loadFromFile("fonts/Minecraft.ttf"))
        return true;
        
    if (font.loadFromFile("../fonts/Minecraft.ttf"))
        return true;
    
    return false;
}

// -------------------------------------------------------------
// MAIN
// -------------------------------------------------------------
int main()
{
    srand(time(0));
    
    // Enable antialiasing for smoother rendering
    ContextSettings settings;
    settings.antialiasingLevel = 8;
    
    RenderWindow window(VideoMode(COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH, ROWS * TILE_SIZE_PIXELS + 60), "XONIX", Style::Default, settings);
    window.setFramerateLimit(60);

    Font font;
    if (!loadFont(font))
    {
        cerr << "Warning: Could not load font. Text may not display correctly." << endl;
        // Continue anyway - SFML will use a default font
    }
    else
    {
        // Disable smoothing for crisp, sharp text rendering
        // Smoothing can cause blurriness at certain sizes
        font.setSmooth(false);
    }

    Texture tiles, enemyTex, gameoverTex;
    tiles.loadFromFile("images/tiles.png");
    enemyTex.loadFromFile("images/enemy.png");
    gameoverTex.loadFromFile("images/gameover.png");

    Sprite sTile(tiles), sEnemy(enemyTex), sGameover(gameoverTex);
    sEnemy.setOrigin(20, 20);
    // Center game over sprite in the game area (accounting for HUD panel)
    FloatRect gameoverBounds = sGameover.getLocalBounds();
    sGameover.setOrigin(gameoverBounds.width / 2, gameoverBounds.height / 2);
    float gameAreaCenterX = HUD_PANEL_WIDTH + (COLS * TILE_SIZE_PIXELS) / 2;
    float gameAreaCenterY = (ROWS * TILE_SIZE_PIXELS) / 2;
    sGameover.setPosition(gameAreaCenterX, gameAreaCenterY);

    AuthManager auth;
    int state = LOGIN_SCREEN;
    float centerX = (COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH) / 2;

    // ============================================================================
    // ALI - UI Input Fields
    // ============================================================================
    InputField usernameInputLogin, passwordInputLogin, usernameInputRegister, passwordInputRegister;
    usernameInputLogin.init(centerX, 150, 300, 40, "Username", font);
    passwordInputLogin.init(centerX, 230, 300, 40, "Password", font, true);
    usernameInputRegister.init(centerX, 170, 300, 40, "New Username", font);
    passwordInputRegister.init(centerX, 250, 300, 40, "New Password", font, true);

    // ============================================================================
    // AAYAN - Player 2 login input fields
    // ============================================================================
    InputField player2UsernameInput, player2PasswordInput;
    player2UsernameInput.init(centerX, 200, 300, 40, "Player 2 Username", font);
    player2PasswordInput.init(centerX, 280, 300, 40, "Player 2 Password", font, true);

    Button loginButton, registerScreenButton, createAccountButton, backButton, playGameButton;
    loginButton.init(centerX, 310, 200, 50, "Login", font);
    registerScreenButton.init(centerX, 380, 200, 50, "Register", font);
    createAccountButton.init(centerX, 330, 200, 50, "Create Account", font);
    backButton.init(centerX, 420, 200, 50, "Back", font);
    playGameButton.init(centerX, ROWS * TILE_SIZE_PIXELS / 2 - 25, 200, 50, "Play Game", font);

    // ============================================================================
    // AAYAN - Matchmaking buttons
    // ============================================================================
    Button matchmakingButton, addPlayer2Button, startMultiplayerButton, leaveQueueButton;
    matchmakingButton.init(centerX, ROWS * TILE_SIZE_PIXELS / 2 + 50, 200, 50, "Matchmaking", font);
    addPlayer2Button.init(centerX, 250, 200, 50, "Add Player 2", font);
    startMultiplayerButton.init(centerX, 360, 200, 50, "Start Game", font);
    leaveQueueButton.init(centerX, 320, 200, 50, "Leave Queue", font);

    // ============================================================================
    // AAYAN - Matchmaking system
    // ============================================================================
    MatchmakingSystem matchmaking;
    QueuePlayer player1Queue, player2Queue;
    string player2Username = "";
    bool player2LoggedIn = false;
    int player1ID = 1, player2ID = 2;
    int player1QueuePosition = -1, player2QueuePosition = -1;
    int player1PlayersAbove = -1, player2PlayersAbove = -1;

    // ============================================================================
    // ALI - Single player game variables
    // ============================================================================
    int playerCol = 10, playerRow = 0, playerDirCol = 0, playerDirRow = 0;
    bool running = true;
    float timer = 0, delay = 0.07;
    Clock clock;
    Enemy enemies[10];
    int enemyCount = 4;

    // Score / Power-ups
    int score = 0, rewardComboCount = 0, availablePowerUps = 0, lastPowerUpAwardScore = 0;
    bool isFreezeActive = false;
    Clock freezeClock;
    string currentUser, errorMessage;
    Clock errorClock;

    // ============================================================================
    // AAYAN - Multiplayer game variables
    // ============================================================================
    int player2Col = 15, player2Row = 0, player2DirCol = 0, player2DirRow = 0;
    bool player2Running = true;
    float player2Timer = 0;
    int player2Score = 0, player2RewardComboCount = 0, player2AvailablePowerUps = 0, player2LastPowerUpAwardScore = 0;
    bool player2FreezeActive = false;
    Clock player2FreezeClock;
    string player1DeathReason = "";
    string player2DeathReason = "";
    bool isMultiplayerMode = false;
    
    // Track key states to prevent multiple triggers when held
    bool spaceWasPressed = false;
    bool enterWasPressed = false;

    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (i == 0 || j == 0 || i == ROWS - 1 || j == COLS - 1)
                tileGrid[i][j] = 1;

    while (window.isOpen())
    {
        Vector2i mouse = Mouse::getPosition(window);
        Event e;
        while (window.pollEvent(e))
        {
            if (e.type == Event::Closed)
                window.close();

            // Input Fields
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
                        state = MAIN_MENU;
                    }
                    else
                        errorClock.restart();
                }
                if (registerScreenButton.isClicked(mouse, e))
                    state = REGISTER_SCREEN;
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

            // ============================================================================
            // ALI - MAIN MENU (Single Player)
            // ============================================================================
            if (state == MAIN_MENU && playGameButton.isClicked(mouse, e))
            {
                state = PLAYING;
                isMultiplayerMode = false;
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
                player1DeathReason = "";
                player2DeathReason = "";
                for (int i = 0; i < enemyCount; i++)
                {
                    enemies[i] = Enemy();
                }
            }

            // ============================================================================
            // AAYAN - Matchmaking Queue
            // ============================================================================
            if (state == MAIN_MENU && matchmakingButton.isClicked(mouse, e))
            {
                cerr << "Matchmaking button clicked, currentUser: " << currentUser << endl;
                matchmaking.removePlayer(currentUser);
                matchmakingButton.hovered = false;
                int playerScore = auth.getPlayerScore(currentUser);
                cerr << "Player score: " << playerScore << ", adding to queue" << endl;
                matchmaking.addPlayer(currentUser, playerScore, player1ID);
                player1Queue.username = currentUser;
                player1Queue.score = playerScore;
                player1Queue.id = player1ID;
                player1QueuePosition = matchmaking.getPlayerPosition(currentUser);
                player1PlayersAbove = matchmaking.getPlayersAbove(currentUser);
                state = MATCHMAKING_QUEUE;
                cerr << "State changed to MATCHMAKING_QUEUE" << endl;
            }

            if (state == MATCHMAKING_QUEUE)
            {
                if (leaveQueueButton.isClicked(mouse, e))
                {
                    cerr << "Leave queue button clicked, removing player: " << currentUser << endl;
                    matchmaking.removePlayer(currentUser);
                    leaveQueueButton.hovered = false;
                    matchmakingButton.hovered = false;
                    state = MAIN_MENU;
                    cerr << "State changed to MAIN_MENU from MATCHMAKING_QUEUE" << endl;
                }
                if (addPlayer2Button.isClicked(mouse, e))
                {
                    state = GAME_ROOM;
                }
            }

            // ============================================================================
            // AAYAN - Game Room (Player 2 Login)
            // ============================================================================
            if (state == GAME_ROOM)
            {
                player2UsernameInput.update(mouse, e);
                player2PasswordInput.update(mouse, e);

                if (backButton.isClicked(mouse, e))
                {
                    cerr << "Back button clicked in GAME_ROOM, returning to MATCHMAKING_QUEUE" << endl;
                    if (!player2Username.empty())
                    {
                        cerr << "Removing player2 from matchmaking: " << player2Username << endl;
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
                    cerr << "State changed to MATCHMAKING_QUEUE from GAME_ROOM" << endl;
                }

                Button player2LoginButton;
                player2LoginButton.init(centerX, 350, 200, 50, "Player 2 Login", font);
                player2LoginButton.update(mouse);

                if (player2LoginButton.isClicked(mouse, e))
                {
                    string p2User = player2UsernameInput.getText();
                    string p2Pass = player2PasswordInput.getText();
                    cerr << "Player 2 login button clicked, username: '" << p2User << "', password length: " << p2Pass.length() << endl;
                    if (p2User.empty())
                    {
                        cerr << "ERROR: Username is empty!" << endl;
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
                        // Add player 2 to matchmaking queue to get their position
                        matchmaking.addPlayer(player2Username, player2Score, player2ID);
                        player2QueuePosition = matchmaking.getPlayerPosition(player2Username);
                        player2PlayersAbove = matchmaking.getPlayersAbove(player2Username);
                        // Remove them immediately since they're joining via direct login
                        matchmaking.removePlayer(player2Username);
                        cerr << "Player 2 logged in successfully: " << player2Username << ", score: " << player2Score << endl;
                    }
                    else
                    {
                        cerr << "Player 2 login failed: " << errorMessage << endl;
                        errorClock.restart();
                    }
                }

                if (player2LoggedIn && startMultiplayerButton.isClicked(mouse, e))
                {
                    state = MULTIPLAYER;
                    isMultiplayerMode = true;
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
                    player1DeathReason = "";
                    player2DeathReason = "";
                    for (int i = 0; i < enemyCount; i++)
                        enemies[i] = Enemy();
                }
            }

            // ============================================================================
            // AAYAN - ESC to return to menu
            // ============================================================================
            if ((state == PLAYING || state == MULTIPLAYER) && e.type == Event::KeyPressed && e.key.code == Keyboard::Escape)
            {
                if (state == PLAYING)
                {
                    cerr << "ESC pressed in PLAYING, returning to MAIN_MENU" << endl;
                    matchmaking.removePlayer(currentUser);
                    state = MAIN_MENU;
                }
                else if (state == MULTIPLAYER)
                {
                    state = GAME_ROOM;
                    // Update the displayed scores with latest values from auth
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
                    cerr << "ESC pressed in MULTIPLAYER, returning to GAME_ROOM" << endl;
                }
            }
        }

        // -------------------------------------------------------------
        // GAME LOGIC
        // -------------------------------------------------------------
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
                {
                    running = false;
                    cerr << "Player eliminated: Stepped on own constructing tile" << endl;
                }
                if (tileGrid[playerRow][playerCol] == 0)
                    tileGrid[playerRow][playerCol] = 2;
                timer = 0;
            }

            if (!isFreezeActive)
                for (int i = 0; i < enemyCount; i++)
                    enemies[i].move();

            // Capture logic & score
            if (tileGrid[playerRow][playerCol] == 1)
            {
                playerDirCol = playerDirRow = 0;
                int pathTiles = 0;
                
                // First count path tiles (type 2) to check if player has a path
                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                        if (tileGrid[i][j] == 2)
                            pathTiles++;
                
                // Only process if player actually had a path
                if (pathTiles > 0)
                {
                    // Flood fill from enemies to mark their connected area
                    for (int i = 0; i < enemyCount; i++)
                        floodFillMark(enemies[i].posY / TILE_SIZE_PIXELS, enemies[i].posX / TILE_SIZE_PIXELS);
                    
                    // Count captured tiles: path (2) + enclosed empty (0) that aren't enemy-connected (-1)
                    int tilesCaptured = 0;
                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == 2)
                                tilesCaptured++;
                            else if (tileGrid[i][j] == 0 && i > 0 && i < ROWS - 1 && j > 0 && j < COLS - 1)
                                tilesCaptured++;  // Enclosed area (not on border)
                        }
                    
                    // Convert tiles: -1 back to 0, path and enclosed to 1
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
                    
                    int emptyTiles = 0;
                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                            if (tileGrid[i][j] == 0)
                                emptyTiles++;
                    if (emptyTiles == 0)
                    {
                        running = false;
                        cerr << "Game won: Grid completely filled!" << endl;
                    }
                }
            }

            for (int i = 0; i < enemyCount; i++)
                if (tileGrid[enemies[i].posY / TILE_SIZE_PIXELS][enemies[i].posX / TILE_SIZE_PIXELS] == 2)
                {
                    running = false;
                    cerr << "Player eliminated: Enemy touched constructing tile" << endl;
                }
        }

        // ============================================================================
        // AAYAN - MULTIPLAYER GAME LOGIC
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

                // Power-up: only trigger on initial press, not when held
                bool spacePressed = Keyboard::isKeyPressed(Keyboard::Space);
                if (spacePressed && !spaceWasPressed && availablePowerUps > 0 && !isFreezeActive)
                {
                    player2FreezeActive = true;
                    player2FreezeClock.restart();
                    availablePowerUps--;
                    cerr << "P1 used power-up, freezing P2 and enemies" << endl;
                }
                spaceWasPressed = spacePressed;
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

                // Power-up: only trigger on initial press, not when held
                bool enterPressed = Keyboard::isKeyPressed(Keyboard::Enter);
                if (enterPressed && !enterWasPressed && player2AvailablePowerUps > 0 && !player2FreezeActive)
                {
                    isFreezeActive = true;
                    freezeClock.restart();
                    player2AvailablePowerUps--;
                    cerr << "P2 used power-up, freezing P1 and enemies" << endl;
                }
                enterWasPressed = enterPressed;
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
                            if (player1DeathReason.empty())
                                player1DeathReason = "Collided with Player 2 while both constructing";
                            if (player2DeathReason.empty())
                                player2DeathReason = "Collided with Player 1 while both constructing";
                        }
                        else if (p1Constructing && !p2Constructing)
                        {
                            running = false;
                            if (player1DeathReason.empty())
                                player1DeathReason = "Collided with Player 2 while constructing";
                        }
                        else if (!p1Constructing && p2Constructing)
                        {
                            player2Running = false;
                            if (player2DeathReason.empty())
                                player2DeathReason = "Collided with Player 1 while constructing";
                        }
                    }

                    if (tileGrid[playerRow][playerCol] == 2)
                    {
                        running = false;
                        if (player1DeathReason.empty())
                            player1DeathReason = "Stepped on own constructing tile";
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
                            if (player1DeathReason.empty())
                                player1DeathReason = "Collided with Player 2 while both constructing";
                            if (player2DeathReason.empty())
                                player2DeathReason = "Collided with Player 1 while both constructing";
                        }
                        else if (p1Constructing && !p2Constructing)
                        {
                            running = false;
                            if (player1DeathReason.empty())
                                player1DeathReason = "Collided with Player 2 while constructing";
                        }
                        else if (!p1Constructing && p2Constructing)
                        {
                            player2Running = false;
                            if (player2DeathReason.empty())
                                player2DeathReason = "Collided with Player 1 while constructing";
                        }
                    }

                    if (tileGrid[player2Row][player2Col] == 2 || tileGrid[player2Row][player2Col] == 3)
                    {
                        player2Running = false;
                        if (player2DeathReason.empty())
                        {
                            if (tileGrid[player2Row][player2Col] == 2)
                                player2DeathReason = "Stepped on Player 1's constructing tile";
                            else
                                player2DeathReason = "Stepped on own constructing tile";
                        }
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
                
                // First count P1's path tiles (type 2) to check if they have a path
                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                        if (tileGrid[i][j] == 2)
                            pathTiles++;
                
                // Only process if P1 actually had a path
                if (pathTiles > 0)
                {
                    // Flood fill from enemies to mark their connected area
                    for (int i = 0; i < enemyCount; i++)
                    {
                        int enemyRow = enemies[i].posY / TILE_SIZE_PIXELS;
                        int enemyCol = enemies[i].posX / TILE_SIZE_PIXELS;
                        floodFillMark(enemyRow, enemyCol);
                    }
                    
                    // Count captured tiles: path (2) + enclosed empty (0) that aren't enemy-connected (-1)
                    int tilesCaptured = 0;
                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == 2)
                                tilesCaptured++;
                            else if (tileGrid[i][j] == 0 && i > 0 && i < ROWS - 1 && j > 0 && j < COLS - 1)
                                tilesCaptured++;  // Enclosed area (not on border)
                        }
                    
                    // Convert tiles: -1 back to 0, path and enclosed to 1
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
                    cerr << "P1 captured " << tilesCaptured << " tiles, score: " << score << endl;
                }
            }

            if (player2Running && running && tileGrid[player2Row][player2Col] == 1)
            {
                player2DirCol = player2DirRow = 0;
                int pathTiles = 0;
                
                // First count P2's path tiles (type 3) to check if they have a path
                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                        if (tileGrid[i][j] == 3)
                            pathTiles++;
                
                // Only process if P2 actually had a path
                if (pathTiles > 0)
                {
                    // Flood fill from enemies to mark their connected area
                    for (int i = 0; i < enemyCount; i++)
                    {
                        int enemyRow = enemies[i].posY / TILE_SIZE_PIXELS;
                        int enemyCol = enemies[i].posX / TILE_SIZE_PIXELS;
                        floodFillMark(enemyRow, enemyCol);
                    }
                    
                    // Count captured tiles: path (3) + enclosed empty (0) that aren't enemy-connected (-1)
                    int tilesCaptured = 0;
                    for (int i = 0; i < ROWS; i++)
                        for (int j = 0; j < COLS; j++)
                        {
                            if (tileGrid[i][j] == 3)
                                tilesCaptured++;
                            else if (tileGrid[i][j] == 0 && i > 0 && i < ROWS - 1 && j > 0 && j < COLS - 1)
                                tilesCaptured++;  // Enclosed area (not on border)
                        }
                    
                    // Convert tiles: -1 back to 0, path and enclosed to 1
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
                    cerr << "P2 captured " << tilesCaptured << " tiles, score: " << player2Score << endl;
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
                    cerr << "Game won: Grid completely filled! P1 score: " << score << ", P2 score: " << player2Score << endl;
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
                        if (player1DeathReason.empty())
                            player1DeathReason = "Enemy touched Player 1's constructing tile";
                    }
                    if (tileGrid[enemyRow][enemyCol] == 3)
                    {
                        player2Running = false;
                        if (player2DeathReason.empty())
                            player2DeathReason = "Enemy touched Player 2's constructing tile";
                    }
                }
            }

            if (!running && player2Running)
            {
                auth.updatePlayerScore(currentUser, score);
                auth.updatePlayerScore(player2Username, player2Score);
                cerr << "P1 eliminated, P2 wins. P1 score: " << score << ", P2 score: " << player2Score << endl;
            }
            if (running && !player2Running)
            {
                auth.updatePlayerScore(currentUser, score);
                auth.updatePlayerScore(player2Username, player2Score);
                cerr << "P2 eliminated, P1 wins. P1 score: " << score << ", P2 score: " << player2Score << endl;
            }
            if (!running && !player2Running)
            {
                auth.updatePlayerScore(currentUser, score);
                auth.updatePlayerScore(player2Username, player2Score);
                cerr << "Both eliminated. P1 score: " << score << ", P2 score: " << player2Score << endl;
            }
        }

        // -------------------------------------------------------------
        // DRAW
        // -------------------------------------------------------------
        window.clear();

        if (state == LOGIN_SCREEN)
        {
            Text t("XONIX GAME", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(roundf(b.width / 2), roundf(b.height / 2));
            setTextPosition(t, centerX, 80);
            window.draw(t);
            usernameInputLogin.draw(window);
            passwordInputLogin.draw(window);
            loginButton.update(mouse);
            loginButton.draw(window);
            registerScreenButton.update(mouse);
            registerScreenButton.draw(window);
            if (!errorMessage.empty() && errorClock.getElapsedTime().asSeconds() < 3)
            {
                Text e(errorMessage, font, 18);
                e.setFillColor(Color::Red);
                setTextPosition(e, centerX - 100, 280);
                window.draw(e);
            }
        }
        else if (state == REGISTER_SCREEN)
        {
            Text t("CREATE ACCOUNT", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(roundf(b.width / 2), roundf(b.height / 2));
            setTextPosition(t, centerX, 80);
            window.draw(t);
            usernameInputRegister.draw(window);
            passwordInputRegister.draw(window);
            createAccountButton.update(mouse);
            createAccountButton.draw(window);
            backButton.update(mouse);
            backButton.draw(window);
            if (!errorMessage.empty() && errorClock.getElapsedTime().asSeconds() < 3)
            {
                Text e(errorMessage, font, 18);
                e.setFillColor(Color::Red);
                setTextPosition(e, centerX - 100, 320);
                window.draw(e);
            }
        }
        else if (state == MAIN_MENU)
        {
            Text t("WELCOME " + currentUser + "!", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(roundf(b.width / 2), roundf(b.height / 2));
            setTextPosition(t, centerX, ROWS * TILE_SIZE_PIXELS / 2 - 100);
            window.draw(t);
            playGameButton.update(mouse);
            playGameButton.draw(window);
            matchmakingButton.update(mouse);
            matchmakingButton.draw(window);
            static int logCounter = 0;
            if (logCounter++ % 60 == 0)
            {
                cerr << "MAIN_MENU: matchmakingButton hovered=" << matchmakingButton.hovered << ", mouse=(" << mouse.x << "," << mouse.y << ")" << endl;
            }
        }
        // ============================================================================
        // AAYAN - Matchmaking Queue Screen
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
        // AAYAN - Game Room Screen
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

                // Matchmaking Stats Section
                Text statsTitle("--- MATCHMAKING STATS ---", font, 18);
                statsTitle.setFillColor(Color::Yellow);
                FloatRect stb = statsTitle.getLocalBounds();
                statsTitle.setOrigin(roundf(stb.width / 2), roundf(stb.height / 2));
                setTextPosition(statsTitle, centerX, 180);
                window.draw(statsTitle);

                // Player 1 queue stats
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

                // Player 2 queue stats
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

                // Priority comparison
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

                // Who was first in queue
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

                // Score difference
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
                Text e(errorMessage, font, 18);
                e.setFillColor(Color::Red);
                setTextPosition(e, centerX - 100, 400);
                window.draw(e);
            }
        }
        else if (state == PLAYING)
        {
            RectangleShape leftPanel, rightPanel, bottomPanel;
            leftPanel.setSize({(float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS});
            leftPanel.setPosition(0, 0);
            leftPanel.setFillColor(Color(30, 30, 30));
            window.draw(leftPanel);

            rightPanel.setSize({(float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS});
            rightPanel.setPosition(COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH, 0);
            rightPanel.setFillColor(Color(30, 30, 30));
            window.draw(rightPanel);

            bottomPanel.setSize({(float)(COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH), 60.0f});
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

            Text controls("Controls: Arrow Keys - Move | Space - Power-Up | ESC - Exit", font, 14);
            controls.setFillColor(Color(150, 150, 150));
            FloatRect cb = controls.getLocalBounds();
            controls.setOrigin(roundf(cb.width / 2), 0);
            setTextPosition(controls, centerX, ROWS * TILE_SIZE_PIXELS + 10);
            window.draw(controls);

            if (!running)
                window.draw(sGameover);
        }
        // ============================================================================
        // AAYAN - MULTIPLAYER DRAWING
        // ============================================================================
        else if (state == MULTIPLAYER)
        {
            RectangleShape leftPanel, rightPanel, bottomPanel;
            leftPanel.setSize({(float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS});
            leftPanel.setPosition(0, 0);
            leftPanel.setFillColor(Color(30, 30, 30));
            window.draw(leftPanel);

            rightPanel.setSize({(float)HUD_PANEL_WIDTH, (float)ROWS * TILE_SIZE_PIXELS});
            rightPanel.setPosition(COLS * TILE_SIZE_PIXELS + HUD_PANEL_WIDTH, 0);
            rightPanel.setFillColor(Color(30, 30, 30));
            window.draw(rightPanel);

            bottomPanel.setSize({(float)(COLS * TILE_SIZE_PIXELS + 2 * HUD_PANEL_WIDTH), 60.0f});
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

            Text controls("P1: Arrow Keys - Move | Space - Power-Up | P2: W/A/S/D - Move | Enter - Power-Up | ESC - Exit", font, 14);
            controls.setFillColor(Color(150, 150, 150));
            FloatRect cb = controls.getLocalBounds();
            controls.setOrigin(roundf(cb.width / 2), 0);
            setTextPosition(controls, centerX, ROWS * TILE_SIZE_PIXELS + 10);
            window.draw(controls);
        }

        window.display();
    }

    return 0;
}
