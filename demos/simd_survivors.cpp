#define NOMINMAX
#include "vecs.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

// --- Constants ---
const int COLS = 60;
const int ROWS = 28;
const float PLAYER_SPEED = 15.0f;
const float BULLET_SPEED = 25.0f;
const float BULLET_LIFETIME = 1.2f;
const float SPAWN_INTERVAL_START = 0.5f;
const float SPAWN_INTERVAL_MIN = 0.15f;

// --- Components ---
struct CPosition { float x, y; };
struct CVelocity { float vx, vy; };
struct CHealth   { float hp, maxHp; };
struct CLifetime { float remaining; };
struct CDamage   { float amount; };
struct CWeapon   { float cooldown; float maxCooldown; float damage; };
struct CXPGem    { float value; };
struct CPlayer   {}; 
struct CEnemy    {}; 
struct CBullet   {}; 

// --- Singletons ---
struct GameState {
    int score, level, enemiesKilled;
    float xp, xpToNextLevel;
    float enemySpawnTimer, enemySpawnInterval;
    float totalTime;
    bool running;
};

// --- Globals ---
vecsWorld* g_world = nullptr;
vecsCommandBuffer* g_cmd = nullptr;
std::mt19937 g_rng(1337);
std::vector<vecsEntity> g_toKill;
bool g_testMode = false;

// --- Helpers ---
void Kill(vecsEntity e) {
    if (e != UINT64_MAX) g_toKill.push_back(e);
}

void ProcessDeaths() {
    std::sort(g_toKill.begin(), g_toKill.end());
    g_toKill.erase(std::unique(g_toKill.begin(), g_toKill.end()), g_toKill.end());
    for (auto e : g_toKill) {
        if (vecsAlive(g_world, e)) vecsDestroy(g_world, e);
    }
    g_toKill.clear();
}

void FlushCMD() {
    vecsFlush(g_cmd);
    g_cmd->createdCount = 0; // Workaround for vecs.h bug
}

#ifdef _WIN32
HANDLE g_hConsole; DWORD g_oldMode;
void InitTerminal() {
    g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hConsole != INVALID_HANDLE_VALUE) {
        GetConsoleMode(g_hConsole, &g_oldMode);
        SetConsoleMode(g_hConsole, g_oldMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    printf("\033[?25l");
}
void RestoreTerminal() {
    printf("\033[?25h\033[0m");
    if (g_hConsole != INVALID_HANDLE_VALUE) SetConsoleMode(g_hConsole, g_oldMode);
}
bool KeyAvailable() { return _kbhit() != 0; }
int GetKey() { return _getch(); }
void SleepMs(int ms) { Sleep(ms); }
#else
struct termios g_oldTerm;
void InitTerminal() {
    tcgetattr(STDIN_FILENO, &g_oldTerm);
    struct termios newTerm = g_oldTerm;
    newTerm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);
    printf("\033[?25l");
}
void RestoreTerminal() {
    printf("\033[?25h\033[0m");
    tcsetattr(STDIN_FILENO, TCSANOW, &g_oldTerm);
}
bool KeyAvailable() {
    struct timeval tv = {0, 0}; fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
int GetKey() { char ch; if (read(STDIN_FILENO, &ch, 1) == 1) return (int)ch; return 0; }
void SleepMs(int ms) { usleep(ms * 1000); }
#endif

// --- Systems ---

void SysInput() {
    GameState* gs = vecsGetSingleton<GameState>(g_world);
    float vx = 0, vy = 0;
    while (KeyAvailable()) {
        int ch = GetKey();
        if (ch == 'q' || ch == 'Q' || ch == 27) gs->running = false;
        if (ch == 'w' || ch == 'W') vy = -1;
        if (ch == 's' || ch == 'S') vy = 1;
        if (ch == 'a' || ch == 'A') vx = -1;
        if (ch == 'd' || ch == 'D') vx = 1;
    }
    vecsEach<CPlayer, CVelocity>(g_world, [&](vecsEntity, CPlayer&, CVelocity& v) {
        if (vx != 0 || vy != 0) {
            float mag = sqrtf(vx * vx + vy * vy);
            v.vx = (vx / mag) * PLAYER_SPEED;
            v.vy = (vy / mag) * PLAYER_SPEED;
        } else { v.vx = 0; v.vy = 0; }
    });
}

void SysEnemyAI() {
    CPosition pPos = {COLS / 2.0f, ROWS / 2.0f};
    vecsEach<CPlayer, CPosition>(g_world, [&](vecsEntity, CPlayer&, CPosition& p) { pPos = p; });
    GameState* gs = vecsGetSingleton<GameState>(g_world);
    float speed = 2.0f + gs->level * 0.3f;
    vecsEach<CEnemy, CPosition, CVelocity>(g_world, [&](vecsEntity, CEnemy&, CPosition& p, CVelocity& v) {
        float dx = pPos.x - p.x, dy = pPos.y - p.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.1f) { v.vx = (dx / dist) * speed; v.vy = (dy / dist) * speed; }
    });
}

void SysMovement(float dt) {
    vecsEach<CPosition, CVelocity>(g_world, [&](vecsEntity, CPosition& p, CVelocity& v) {
        p.x += v.vx * dt; p.y += v.vy * dt;
    });
    vecsEach<CPlayer, CPosition>(g_world, [&](vecsEntity, CPlayer&, CPosition& p) {
        p.x = std::max(1.0f, std::min((float)COLS - 2.0f, p.x));
        p.y = std::max(1.0f, std::min((float)ROWS - 2.0f, p.y));
    });
}

void SysWeapon(float dt) {
    vecsEach<CPlayer, CPosition, CWeapon>(g_world, [&](vecsEntity, CPlayer&, CPosition& p, CWeapon& w) {
        w.cooldown -= dt;
        if (w.cooldown <= 0) {
            w.cooldown = w.maxCooldown;
            float dv[4][2] = {{0,1},{0,-1},{1,0},{-1,0}};
            for (int i = 0; i < 4; i++) {
                uint32_t b = vecsCmdCreate(g_cmd);
                vecsCmdSetCreated<CPosition>(g_cmd, b, {p.x, p.y});
                vecsCmdSetCreated<CVelocity>(g_cmd, b, {dv[i][0]*BULLET_SPEED, dv[i][1]*BULLET_SPEED});
                vecsCmdSetCreated<CBullet>(g_cmd, b, {});
                vecsCmdSetCreated<CDamage>(g_cmd, b, {w.damage});
                vecsCmdSetCreated<CLifetime>(g_cmd, b, {BULLET_LIFETIME});
            }
        }
    });
    FlushCMD();
}

void SysCollision() {
    GameState* gs = vecsGetSingleton<GameState>(g_world);
    struct BInfo { vecsEntity e; float x, y, dmg; bool dead; };
    std::vector<BInfo> activeBullets;
    vecsEach<CBullet, CPosition, CDamage>(g_world, [&](vecsEntity e, CBullet&, CPosition& p, CDamage& d) {
        activeBullets.push_back({e, p.x, p.y, d.amount, false});
    });

    vecsEach<CEnemy, CPosition, CHealth>(g_world, [&](vecsEntity e, CEnemy&, CPosition& p, CHealth& h) {
        for (auto& b : activeBullets) {
            if (b.dead) continue;
            float dx = b.x - p.x, dy = b.y - p.y;
            if (sqrtf(dx * dx + dy * dy) < 1.5f) {
                h.hp -= b.dmg; b.dead = true; Kill(b.e);
                if (h.hp <= 0) {
                    Kill(e); gs->score += 10; gs->enemiesKilled++;
                    uint32_t gIdx = vecsCmdCreate(g_cmd);
                    vecsCmdSetCreated<CPosition>(g_cmd, gIdx, {p.x, p.y});
                    vecsCmdSetCreated<CXPGem>(g_cmd, gIdx, {10.0f + gs->level*2});
                    break;
                }
            }
        }
    });
    ProcessDeaths(); 
    FlushCMD();

    vecsEntity playerEnt = UINT64_MAX; CPosition pPos;
    vecsEach<CPlayer, CPosition>(g_world, [&](vecsEntity e, CPlayer&, CPosition& p) { playerEnt = e; pPos = p; });
    if (playerEnt != UINT64_MAX) {
        CHealth* pHp = vecsGet<CHealth>(g_world, playerEnt);
        vecsEach<CEnemy, CPosition, CDamage>(g_world, [&](vecsEntity, CEnemy&, CPosition& p, CDamage& d) {
            float dx = pPos.x - p.x, dy = pPos.y - p.y;
            if (sqrtf(dx * dx + dy * dy) < 1.2f) {
                pHp->hp -= d.amount * 0.1f;
                if (pHp->hp <= 0) gs->running = false;
            }
        });
        vecsEach<CXPGem, CPosition>(g_world, [&](vecsEntity e, CXPGem& gem, CPosition& p) {
            float dx = pPos.x - p.x, dy = pPos.y - p.y;
            if (sqrtf(dx * dx + dy * dy) < 2.0f) { gs->xp += gem.value; Kill(e); }
        });
        ProcessDeaths();
    }
}

void SysSpawnEnemies(float dt) {
    GameState* gs = vecsGetSingleton<GameState>(g_world);
    gs->enemySpawnTimer -= dt;
    if (gs->enemySpawnTimer <= 0) {
        gs->enemySpawnTimer = gs->enemySpawnInterval;
        int count = std::uniform_int_distribution<int>(3, 8)(g_rng);
        for (int i = 0; i < count; i++) {
            float ex, ey; int side = std::uniform_int_distribution<int>(0, 3)(g_rng);
            if (side == 0) { ex = (float)(rand()%COLS); ey = 0; }
            else if (side == 1) { ex = (float)(rand()%COLS); ey = ROWS-1; }
            else if (side == 2) { ex = 0; ey = (float)(rand()%ROWS); }
            else { ex = COLS-1; ey = (float)(rand()%ROWS); }
            uint32_t e = vecsCmdCreate(g_cmd);
            vecsCmdSetCreated<CPosition>(g_cmd, e, {ex, ey});
            vecsCmdSetCreated<CVelocity>(g_cmd, e, {0, 0});
            vecsCmdSetCreated<CEnemy>(g_cmd, e, {});
            float hp = 20.0f + gs->level * 5.0f;
            vecsCmdSetCreated<CHealth>(g_cmd, e, {hp, hp});
            vecsCmdSetCreated<CDamage>(g_cmd, e, {10.0f});
        }
    }
    FlushCMD();
}

void SysRender() {
    char grid[ROWS][COLS + 1];
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) grid[y][x] = (y==0||y==ROWS-1||x==0||x==COLS-1)?'#':' ';
        grid[y][COLS] = '\0';
    }
    vecsEach<CXPGem, CPosition>(g_world, [&](vecsEntity, CXPGem&, CPosition& p) {
        int x = (int)p.x, y = (int)p.y; if (x>0&&x<COLS-1&&y>0&&y<ROWS-1) grid[y][x] = '*';
    });
    vecsEach<CBullet, CPosition>(g_world, [&](vecsEntity, CBullet&, CPosition& p) {
        int x = (int)p.x, y = (int)p.y; if (x>0&&x<COLS-1&&y>0&&y<ROWS-1) grid[y][x] = '.';
    });
    vecsEach<CEnemy, CPosition, CHealth>(g_world, [&](vecsEntity, CEnemy&, CPosition& p, CHealth& h) {
        int x = (int)p.x, y = (int)p.y; if (x>0&&x<COLS-1&&y>0&&y<ROWS-1) grid[y][x] = (h.hp<h.maxHp*0.5f)?'z':'Z';
    });
    vecsEach<CPlayer, CPosition>(g_world, [&](vecsEntity, CPlayer&, CPosition& p) {
        int x = (int)p.x, y = (int)p.y; if (x>0&&x<COLS-1&&y>0&&y<ROWS-1) grid[y][x] = '@';
    });
    GameState* gs = vecsGetSingleton<GameState>(g_world);
    printf("\033[H\033[37mSIMD SURVIVORS | Score: %d | Lvl: %d | Time: %.0fs\033[0m\n", gs->score, gs->level, gs->totalTime);
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            char c = grid[y][x];
            if (c=='@') printf("\033[32m@\033[0m");
            else if (c=='Z'||c=='z') printf("\033[31m%c\033[0m", c);
            else if (c=='.') printf("\033[33m.\033[0m");
            else if (c=='*') printf("\033[36m*\033[0m");
            else printf("%c", c);
        }
        printf("\n");
    }
    vecsEach<CPlayer, CHealth>(g_world, [&](vecsEntity, CPlayer&, CHealth& h) {
        printf("HP: ["); int bars = (int)(h.hp/h.maxHp*20);
        for (int i=0; i<20; i++) printf(i<bars?"=":" ");
        printf("] %.0f/%.0f  [WASD] Move [Q] Quit\n", h.hp, h.maxHp);
    });
}

int main(int argc, char** argv) {
    for (int i=1; i<argc; i++) if (strcmp(argv[i], "--test")==0) g_testMode = true;
    g_world = vecsCreateWorld(16384); g_cmd = vecsCreateCommandBuffer(g_world);
    vecsEntity waste = vecsCreate(g_world); vecsDestroy(g_world, waste);
    vecsSetSingleton<GameState>(g_world, {0, 1, 0, 0.0f, 100.0f, 0.0f, SPAWN_INTERVAL_START, 0.0f, true});
    vecsEntity p = vecsCreate(g_world);
    vecsSet<CPlayer>(g_world, p, {}); vecsSet<CPosition>(g_world, p, {30, 14});
    vecsSet<CVelocity>(g_world, p, {0, 0}); vecsSet<CHealth>(g_world, p, {500, 500});
    vecsSet<CWeapon>(g_world, p, {0, 0.4f, 15});
    InitTerminal();
    if (!g_testMode) printf("\033[2J");
    auto last = std::chrono::steady_clock::now();
    int frames = 0;
    while (vecsGetSingleton<GameState>(g_world)->running) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        if (dt > 0.1f) dt = 0.1f; last = now;
        GameState* gs = vecsGetSingleton<GameState>(g_world); gs->totalTime += dt;
        if (g_testMode) {
            vecsEach<CPlayer, CVelocity>(g_world, [&](vecsEntity, CPlayer&, CVelocity& v) {
                v.vx = cosf(gs->totalTime)*PLAYER_SPEED; v.vy = sinf(gs->totalTime*2)*PLAYER_SPEED;
            });
            for (int i=0; i<10; i++) {
                uint32_t e = vecsCmdCreate(g_cmd);
                vecsCmdSetCreated<CPosition>(g_cmd, e, {(float)(rand()%COLS), (float)(rand()%ROWS)});
                vecsCmdSetCreated<CEnemy>(g_cmd, e, {}); vecsCmdSetCreated<CHealth>(g_cmd, e, {10,10});
                vecsCmdSetCreated<CDamage>(g_cmd, e, {5});
            }
            FlushCMD();
            if (++frames > 1000) gs->running = false;
        } else SysInput();
        SysEnemyAI(); SysMovement(dt); SysWeapon(dt); SysCollision();
        if (gs->xp >= gs->xpToNextLevel) {
            gs->level++; gs->xp -= gs->xpToNextLevel; gs->xpToNextLevel *= 1.6f;
            gs->enemySpawnInterval = std::max(SPAWN_INTERVAL_MIN, gs->enemySpawnInterval - 0.05f);
            vecsEach<CPlayer, CWeapon>(g_world, [](vecsEntity, CPlayer&, CWeapon& w) {
                w.maxCooldown = std::max(0.2f, w.maxCooldown - 0.07f); w.damage += 2;
            });
        }
        if (!g_testMode) SysSpawnEnemies(dt);
        vecsEach<CLifetime>(g_world, [&](vecsEntity e, CLifetime& l) {
            l.remaining -= dt; if (l.remaining <= 0) Kill(e);
        });
        vecsEach<CPosition>(g_world, [&](vecsEntity e, CPosition& p) {
            if (p.x < -15 || p.x > COLS + 15 || p.y < -15 || p.y > ROWS + 15) Kill(e);
        });
        ProcessDeaths();
        if (!g_testMode) SysRender();
        if (!g_testMode) SleepMs(50);
    }
    RestoreTerminal();
    if (vecsGetSingleton<GameState>(g_world)->score > 0) {
        printf("\n--- GAME OVER ---\n");
        printf("Final Score: %d\n", vecsGetSingleton<GameState>(g_world)->score);
        printf("Level: %d\n", vecsGetSingleton<GameState>(g_world)->level);
    }
    printf("Test finished after %d frames.\n", frames);
    return 0;
}
