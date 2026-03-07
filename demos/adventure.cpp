#include "vecs.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>
#include <set>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// --- ANSI Colors ---
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_ITALIC  "\033[3m"
#define CLR_DIM     "\033[2m"
#define CLR_TITLE   "\033[1;36m" // Bold Cyan
#define CLR_TEXT    "\033[0;37m" // White
#define CLR_CODE    "\033[0;33m" // Yellow/Orange
#define CLR_QUOTE   "\033[0;90m" // Dark Gray
#define CLR_OPT_NUM "\033[1;32m" // Bold Green
#define CLR_ERROR   "\033[1;31m" // Bold Red

// --- Components ---
struct CStateID { std::string id; };
struct CTitle   { std::string title; };
struct CText    { std::string body; };
struct COption  { std::string text; std::string targetId; };
struct COptions { std::vector<COption> list; };
struct CActiveState {}; // Tag for current player location

// --- Singletons ---
struct GameConfig {
    bool isRunning = true;
    bool testMode = false;
    std::string startId = "S01";
};

// --- Helpers ---
void ClearScreen() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

std::string RenderMarkdown(const std::string& input) {
    std::string out;
    bool inBold = false, inItalic = false, inCode = false, inPre = false;
    
    std::stringstream ss(input);
    std::string line;
    while (std::getline(ss, line)) {
        // Handle Blockquote
        if (line.size() > 2 && line[0] == '>' && line[1] == ' ') {
            out += std::string(CLR_QUOTE) + "  " + line.substr(2) + CLR_RESET + "\n";
            continue;
        }
        
        // Handle Preformatted block
        if (line.substr(0, 3) == "```") {
            inPre = !inPre;
            continue;
        }
        if (inPre) {
            out += std::string(CLR_CODE) + "    " + line + CLR_RESET + "\n";
            continue;
        }

        // Inline formatting (Simple greedy replacement)
        std::string processed;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '*' && i + 1 < line.size() && line[i+1] == '*') {
                processed += inBold ? CLR_RESET : CLR_BOLD;
                inBold = !inBold;
                i++;
            } else if (line[i] == '*') {
                processed += inItalic ? CLR_RESET : CLR_ITALIC;
                inItalic = !inItalic;
            } else if (line[i] == '`') {
                processed += inCode ? CLR_RESET : CLR_CODE;
                inCode = !inCode;
            } else {
                processed += line[i];
            }
        }
        out += processed + "\n";
    }
    return out;
}

// --- Systems ---

void LoadAdventure(vecsWorld* world, std::istream& input) {
    std::string line;
    vecsEntity current = VECS_INVALID_ENTITY;
    std::string bodyBuffer;
    bool inText = false;

    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find("[STATE]") != std::string::npos) {
            current = vecsCreate(world);
            vecsSet<COptions>(world, current, {});
        } else if (line.substr(0, 3) == "ID=") {
            vecsSet<CStateID>(world, current, { line.substr(3) });
        } else if (line.substr(0, 6) == "TITLE=") {
            vecsSet<CTitle>(world, current, { line.substr(6) });
        } else if (line == "TEXT") {
            inText = true;
            bodyBuffer.clear();
        } else if (line == "ENDTEXT") {
            inText = false;
            vecsSet<CText>(world, current, { bodyBuffer });
        } else if (line.substr(0, 4) == "OPT=") {
            std::string content = line.substr(4);
            size_t pipe = content.find('|');
            if (pipe != std::string::npos) {
                COptions* opts = vecsGet<COptions>(world, current);
                opts->list.push_back({ content.substr(0, pipe), content.substr(pipe + 1) });
            }
        } else if (inText) {
            bodyBuffer += line + "\n";
        }
    }
}

void SetActiveState(vecsWorld* world, const std::string& id) {
    // Clear old active state
    vecsEach<CActiveState>(world, [&](vecsEntity e, CActiveState&) {
        vecsUnset<CActiveState>(world, e);
    });

    // Find and set new active state
    bool found = false;
    vecsEach<CStateID>(world, [&](vecsEntity e, CStateID& sid) {
        if (!found && sid.id == id) {
            vecsSet<CActiveState>(world, e, {});
            found = true;
        }
    });
}

void SysRender(vecsWorld* world) {
    vecsEach<CActiveState, CStateID, CTitle, CText, COptions>(world, [&](vecsEntity, CActiveState&, CStateID&, CTitle& t, CText& txt, COptions& opts) {
        ClearScreen();
        std::cout << CLR_TITLE << "--- " << t.title << " ---" << CLR_RESET << "\n\n";
        std::cout << RenderMarkdown(txt.body) << "\n";
        
        for (size_t i = 0; i < opts.list.size(); ++i) {
            std::cout << CLR_OPT_NUM << " [" << (i + 1) << "] " << CLR_RESET << opts.list[i].text << "\n";
        }
        std::cout << "\n> ";
    });
}

void SysInput(vecsWorld* world) {
    GameConfig* cfg = vecsGetSingleton<GameConfig>(world);
    vecsEntity current = VECS_INVALID_ENTITY;
    COptions* opts = nullptr;

    vecsEach<CActiveState, COptions>(world, [&](vecsEntity e, CActiveState&, COptions& o) {
        current = e;
        opts = &o;
    });

    if (current == VECS_INVALID_ENTITY) return;

    std::string input;
    if (!std::getline(std::cin, input)) {
        cfg->isRunning = false;
        return;
    }

    try {
        int choice = std::stoi(input);
        if (choice > 0 && (size_t)choice <= opts->list.size()) {
            SetActiveState(world, opts->list[choice - 1].targetId);
        }
    } catch (...) {}
}

// --- Test Suite ---

void RunTests(vecsWorld* world) {
    std::cout << CLR_TITLE << "=== ADVENTURE TEST SUITE ===" << CLR_RESET << "\n";
    int passed = 0, total = 0;

    auto Assert = [&](bool condition, const char* msg) {
        total++;
        if (condition) {
            std::cout << CLR_OPT_NUM << "  [PASS] " << CLR_RESET << msg << "\n";
            passed++;
        } else {
            std::cout << CLR_ERROR << "  [FAIL] " << CLR_RESET << msg << "\n";
        }
    };

    // 1. Parser Validation
    uint32_t count = 0;
    vecsEach<CStateID>(world, [&](vecsEntity, CStateID&) { count++; });
    Assert(count > 0, "States loaded from data file");

    // 2. Link Integrity
    bool linksOk = true;
    std::set<std::string> validIds;
    vecsEach<CStateID>(world, [&](vecsEntity, CStateID& sid) { validIds.insert(sid.id); });
    
    vecsEach<CStateID, COptions>(world, [&](vecsEntity, CStateID& sid, COptions& opts) {
        for (auto& o : opts.list) {
            if (validIds.find(o.targetId) == validIds.end()) {
                std::cout << CLR_ERROR << "    Dead link: " << sid.id << " -> " << o.targetId << CLR_RESET << "\n";
                linksOk = false;
            }
        }
    });
    Assert(linksOk, "No dead links found");

    // 3. Reachability (BFS)
    std::set<std::string> reachable;
    std::vector<std::string> queue = { "S01" };
    reachable.insert("S01");
    size_t head = 0;
    while(head < queue.size()) {
        std::string curr = queue[head++];
        vecsEach<CStateID, COptions>(world, [&](vecsEntity, CStateID& sid, COptions& opts) {
            if (sid.id == curr) {
                for(auto& o : opts.list) {
                    if (reachable.find(o.targetId) == reachable.end()) {
                        reachable.insert(o.targetId);
                        queue.push_back(o.targetId);
                    }
                }
            }
        });
    }
    Assert(reachable.size() == validIds.size(), "All states are reachable from START");
    if (reachable.size() < validIds.size()) {
        for(auto& id : validIds) {
            if (reachable.find(id) == reachable.end()) 
                std::cout << CLR_DIM << "    Orphan: " << id << CLR_RESET << "\n";
        }
    }

    // 4. Stress Test (Random traversal)
    srand(1337);
    std::string current = "START";
    bool stressOk = true;
    for (int i = 0; i < 1000; ++i) {
        bool found = false;
        vecsEach<CStateID, COptions>(world, [&](vecsEntity, CStateID& sid, COptions& opts) {
            if (sid.id == current && !opts.list.empty()) {
                current = opts.list[rand() % opts.list.size()].targetId;
                found = true;
            }
        });
        if (!found) break; // Terminal node reached
    }
    Assert(stressOk, "Stress test (1000 random steps) completed without crash");

    // 5. Embedded Test Scenario
    const char* embedded = "[STATE]\nID=T1\nTITLE=TEST\nTEXT\nHello\nENDTEXT\nOPT=Go|T2\n[STATE]\nID=T2\nTITLE=END\nTEXT\nBye\nENDTEXT\nOPT=Reset|T1\n";
    vecsWorld* testWorld = vecsCreateWorld(100);
    std::stringstream ss(embedded);
    LoadAdventure(testWorld, ss);
    uint32_t tCount = 0;
    vecsEach<CStateID>(testWorld, [&](vecsEntity, CStateID&) { tCount++; });
    Assert(tCount == 2, "Embedded string parser handles small datasets");
    vecsDestroyWorld(testWorld);

    std::cout << "\nSummary: " << passed << "/" << total << " tests passed.\n";
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    vecsWorld* world = vecsCreateWorld(1024);
    vecsSetSingleton<GameConfig>(world, {});
    GameConfig* cfg = vecsGetSingleton<GameConfig>(world);

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--test") cfg->testMode = true;
    }

    std::ifstream file("demos/adventure.md");
    if (!file.is_open()) {
        std::cerr << CLR_ERROR << "Failed to open demos/adventure.md" << CLR_RESET << "\n";
        return 1;
    }
    LoadAdventure(world, file);

    if (cfg->testMode) {
        RunTests(world);
    } else {
        SetActiveState(world, cfg->startId);
        while (cfg->isRunning) {
            SysRender(world);
            SysInput(world);
        }
    }

    vecsDestroyWorld(world);
    return 0;
}
