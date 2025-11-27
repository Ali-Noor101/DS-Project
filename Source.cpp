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

int tileGrid[ROWS][COLS] = { 0 };

// Game states
const int LOGIN_SCREEN = 0;
const int REGISTER_SCREEN = 1;
const int MAIN_MENU = 2;
const int PLAYING = 3;

// -------------------------------------------------------------
// PLAYER AUTH
// -------------------------------------------------------------
struct Player
{
    int id;
    string username, password;
};

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
        label.setOrigin(b.width / 2, b.height / 2);
        label.setPosition(cx, y + h / 2);
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
        label.setOrigin(lb.width / 2, lb.height);
        label.setPosition(cx, y - 10);

        display.setFont(font);
        display.setCharacterSize(18);
        display.setFillColor(Color::Black);
        display.setPosition(x + 10, y + 10);
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
            getline(iss, p.password);
            players[count++] = p;
            nextID = max(nextID, p.id + 1);
        }
    }

    void save()
    {
        ofstream f(file);
        for (int i = 0; i < count; i++)
            f << players[i].id << "," << players[i].username << "," << players[i].password << endl;
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
    if (tileGrid[row][col] == 0)
        tileGrid[row][col] = -1;
    if (tileGrid[row - 1][col] == 0)
        floodFillMark(row - 1, col);
    if (tileGrid[row + 1][col] == 0)
        floodFillMark(row + 1, col);
    if (tileGrid[row][col - 1] == 0)
        floodFillMark(row, col - 1);
    if (tileGrid[row][col + 1] == 0)
        floodFillMark(row, col + 1);
}

// -------------------------------------------------------------
// MAIN
// -------------------------------------------------------------
int main()
{
    srand(time(0));
    RenderWindow window(VideoMode(COLS * TILE_SIZE_PIXELS, ROWS * TILE_SIZE_PIXELS), "XONIX");
    window.setFramerateLimit(60);

    Font font;
    font.loadFromFile("C:/Windows/Fonts/arial.ttf");

    Texture tiles, enemyTex, gameoverTex;
    tiles.loadFromFile("images/tiles.png");
    enemyTex.loadFromFile("images/enemy.png");
    gameoverTex.loadFromFile("images/gameover.png");

    Sprite sTile(tiles), sEnemy(enemyTex), sGameover(gameoverTex);
    sEnemy.setOrigin(20, 20);
    sGameover.setPosition(100, 100);

    AuthManager auth;
    int state = LOGIN_SCREEN;
    float centerX = COLS * TILE_SIZE_PIXELS / 2;

    // UI
    InputField usernameInputLogin, passwordInputLogin, usernameInputRegister, passwordInputRegister;
    usernameInputLogin.init(centerX, 150, 300, 40, "Username", font);
    passwordInputLogin.init(centerX, 230, 300, 40, "Password", font, true);
    usernameInputRegister.init(centerX, 170, 300, 40, "New Username", font);
    passwordInputRegister.init(centerX, 250, 300, 40, "New Password", font, true);

    Button loginButton, registerScreenButton, createAccountButton, backButton, playGameButton;
    loginButton.init(centerX, 310, 200, 50, "Login", font);
    registerScreenButton.init(centerX, 380, 200, 50, "Register", font);
    createAccountButton.init(centerX, 330, 200, 50, "Create Account", font);
    backButton.init(centerX, 400, 200, 50, "Back", font);
    playGameButton.init(centerX, ROWS * TILE_SIZE_PIXELS / 2 - 25, 200, 50, "Play Game", font);

    // Game
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

            // MAIN MENU
            if (state == MAIN_MENU && playGameButton.isClicked(mouse, e))
            {
                state = PLAYING;
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
            }

            // ESC RESET
            if (state == PLAYING && e.type == Event::KeyPressed && e.key.code == Keyboard::Escape)
            {
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
            if (Keyboard::isKeyPressed(Keyboard::Space) && availablePowerUps > 0 && !isFreezeActive)
            {
                isFreezeActive = true;
                freezeClock.restart();
                availablePowerUps--;
            }
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

            // Capture logic & score
            if (tileGrid[playerRow][playerCol] == 1)
            {
                playerDirCol = playerDirRow = 0;
                int tilesCaptured = 0;
                for (int i = 0; i < enemyCount; i++)
                    floodFillMark(enemies[i].posY / TILE_SIZE_PIXELS, enemies[i].posX / TILE_SIZE_PIXELS);
                for (int i = 0; i < ROWS; i++)
                    for (int j = 0; j < COLS; j++)
                    {
                        if (tileGrid[i][j] == 2)
                            tilesCaptured++;
                        tileGrid[i][j] = (tileGrid[i][j] == -1 ? 0 : 1);
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

            for (int i = 0; i < enemyCount; i++)
                if (tileGrid[enemies[i].posY / TILE_SIZE_PIXELS][enemies[i].posX / TILE_SIZE_PIXELS] == 2)
                    running = false;
        }

        // -------------------------------------------------------------
        // DRAW
        // -------------------------------------------------------------
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
                Text e(errorMessage, font, 18);
                e.setFillColor(Color::Red);
                e.setPosition(centerX - 100, 280);
                window.draw(e);
            }
        }
        else if (state == REGISTER_SCREEN)
        {
            Text t("CREATE ACCOUNT", font, 40);
            FloatRect b = t.getLocalBounds();
            t.setOrigin(b.width / 2, b.height / 2);
            t.setPosition(centerX, 80);
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
                e.setPosition(centerX - 100, 320);
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
        else if (state == PLAYING)
        {
            for (int i = 0; i < ROWS; i++)
                for (int j = 0; j < COLS; j++)
                {
                    if (tileGrid[i][j] == 0)
                        continue;
                    sTile.setTextureRect(IntRect(tileGrid[i][j] == 1 ? 0 : 54, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
                    sTile.setPosition(j * TILE_SIZE_PIXELS, i * TILE_SIZE_PIXELS);
                    window.draw(sTile);
                }
            sTile.setTextureRect(IntRect(36, 0, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS));
            sTile.setPosition(playerCol * TILE_SIZE_PIXELS, playerRow * TILE_SIZE_PIXELS);
            window.draw(sTile);
            for (int i = 0; i < enemyCount; i++)
            {
                sEnemy.setPosition(enemies[i].posX, enemies[i].posY);
                if (running)
                    sEnemy.rotate(4);
                window.draw(sEnemy);
            }

            Text scoreText("Score: " + to_string(score), font, 18);
            Text powerText("PowerUps: " + to_string(availablePowerUps), font, 18);
            scoreText.setFillColor(Color::White);
            powerText.setFillColor(Color::White);
            scoreText.setPosition(10, 10);
            powerText.setPosition(10, 30);
            window.draw(scoreText);
            window.draw(powerText);

            if (!running)
                window.draw(sGameover);
        }

        window.display();
    }

    return 0;
}
