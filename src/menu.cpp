#include "game.h"
#include "highscores.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cctype>

// Estados
enum class Screen { MAIN_MENU, INSTRUCTIONS, HIGHSCORES, GAMEPLAY, EXIT };

// Manager global de highscores
HighscoreManager g_highscores;

// Utilidades de dibujo
void drawFrame(int top, int left, int bottom, int right, const std::string& title = "") { 
    // Marco rectangular
    for (int x = left; x <= right; ++x) {
        mvaddch(top, x, '=');
        mvaddch(bottom, x, '=');
    }
    for (int y = top; y <= bottom; ++y) {
        mvaddch(y, left, '|');
        mvaddch(y, right, '|');
    }
    mvaddch(top, left, '+'); mvaddch(top, right, '+');
    mvaddch(bottom, left, '+'); mvaddch(bottom, right, '+');

    if (!title.empty()) {
        int w = right - left - 1;
        int L = (int)title.size();
        int tx = left + ((w - L) / 2) + 1;
        mvprintw(top, tx, "%s", title.c_str());
    }
}

void centerPrint(int row, const std::string& s) {
    int cols, rows;
    getmaxyx(stdscr, rows, cols);
    int x = (cols - (int)s.size())/2;
    mvprintw(row, x, "%s", s.c_str());
}

// Función para pedir nombre del jugador
std::string inputPlayerName(int finalScore) {
    clear();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    int top = rows/2 - 6, left = cols/2 - 30, bottom = rows/2 + 6, right = cols/2 + 30;
    
    drawFrame(top, left, bottom, right, " NUEVO HIGHSCORE ");
    
    centerPrint(top + 2, "¡FELICIDADES!");
    centerPrint(top + 3, "Tu puntuación: " + std::to_string(finalScore));
    centerPrint(top + 5, "Ingresa tu nombre (max 10 letras):");
    centerPrint(top + 6, "(Solo letras, Enter para confirmar)");
    
    std::string name;
    int inputY = top + 8;
    int inputX = cols/2 - 5;
    
    // Modo de entrada
    echo();
    curs_set(1);
    
    while (true) {
        mvprintw(inputY, inputX, "          "); // Limpiar línea
        mvprintw(inputY, inputX, "%s_", name.c_str());
        refresh();
        
        int ch = getch();
        
        if (ch == '\n' || ch == KEY_ENTER) {
            if (!name.empty()) break;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!name.empty()) name.pop_back();
        } else if (std::isalpha(ch) && name.size() < 10) {
            name += std::toupper(ch);
        }
    }
    
    noecho();
    curs_set(0);
    
    return name;
}

// Declaraciones de pantallas
Screen showMainMenu();
void showInstructions();
void showHighscores();
void runGameplay();
int getGameScore(); // Declaración para obtener score del juego

// Programa principal
int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    Screen screen = Screen::MAIN_MENU;

    while (screen != Screen::EXIT) {
        switch (screen) {
            case Screen::MAIN_MENU: 
                screen = showMainMenu(); 
                break;
                
            case Screen::INSTRUCTIONS: 
                showInstructions(); 
                screen = Screen::MAIN_MENU; 
                break;
                
            case Screen::HIGHSCORES: 
                showHighscores();  
                screen = Screen::MAIN_MENU; 
                break;
                
            case Screen::GAMEPLAY:
                {
                    clear();
                    refresh();
                    runGameplay();
                    
                    // Obtener score final del juego
                    int finalScore = getGameScore();
                    
                    // Verificar si es highscore
                    if (finalScore > 0 && g_highscores.isHighscore(finalScore)) {
                        std::string playerName = inputPlayerName(finalScore);
                        g_highscores.addScore(finalScore, playerName);
                    }
                    
                    clear();
                    refresh();     
                    screen = Screen::MAIN_MENU; 
                }
                break;
                
            case Screen::EXIT: 
                break;
        }
    }

    endwin();
    return 0;
}

// Implementación de pantallas

// Menu principal
Screen showMainMenu() {
    std::vector<std::string> items = {
        "Iniciar partida",
        "Instrucciones",
        "Puntajes destacados",
        "Salir"
    };
    int selected = 0;

    while (true) {
        clear();
        int rows, cols; 
        getmaxyx(stdscr, rows, cols);

        int top = rows/2 - 8, left = cols/2 - 30, bottom = rows/2 + 8, right = cols/2 + 30;

        drawFrame(top, left, bottom, right, " BREAKOUT ");
        centerPrint(top + 2, "MENU PRINCIPAL");

        for (int i = 0; i < (int)items.size(); ++i) {
            std::string line = (i == selected ? ">> " : "") + items[i] + (i == selected ? " <<" : "");
            int y = top + 4 + i*2;

            if (i == selected) attron(A_REVERSE);
            centerPrint(y, line);
            if (i == selected) attroff(A_REVERSE);
        }
        centerPrint(bottom - 2, "Usa Flechas (W/S)  Enter = Seleccionar  Esc = Salir");

        refresh();

        int ch = getch();
        if (ch == KEY_UP || ch == 'w' || ch == 'W') {
            selected = (selected - 1 + (int)items.size()) % (int)items.size();
        } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
            selected = (selected + 1) % (int)items.size();
        } else if (ch == 27) {
            return Screen::EXIT;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            switch (selected) {
                case 0: return Screen::GAMEPLAY;
                case 1: return Screen::INSTRUCTIONS;
                case 2: return Screen::HIGHSCORES;
                case 3: return Screen::EXIT;
            }
        }
    }
}

// Pantalla de instrucciones
void showInstructions() {
    clear();
    int rows, cols; 
    getmaxyx(stdscr, rows, cols);

    int top = rows/2 - 10, left = cols/2 - 35, bottom = rows/2 + 10, right = cols/2 + 35;

    drawFrame(top, left, bottom, right, " INSTRUCCIONES ");
    int y = top + 2;
    
    centerPrint(y++, "Objetivo: Romper todos los ladrillos con la pelota,");
    centerPrint(y++, "sin dejar que esta caiga al fondo, para sumar puntos.");
    y++;

    centerPrint(y++, "Mover paleta: ← / →  (A / D)");
    centerPrint(y++, "Lanzar pelota: Espacio");
    centerPrint(y++, "Pausa: P");
    centerPrint(y++, "Reiniciar nivel: R");
    centerPrint(y++, "Salir: Esc / Q");
    y++;

    centerPrint(y++, "Elementos del juego:");
    centerPrint(y++, "Paleta (=) controlada por el jugador");
    centerPrint(y++, "Pelota (o) que rebota y destruye ladrillos");
    centerPrint(y++, "Ladrillos (#, %, @) con diferentes resistencias");
    centerPrint(y++, "Bordes (| - +) que delimitan el area de juego");
    y++;
    
    centerPrint(y++, "[ Enter / Esc para volver ]");
    refresh();

    while (true) {
        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == KEY_ENTER) break;
    }
}

// Pantalla de highscores
void showHighscores() {
    clear();
    int rows, cols; 
    getmaxyx(stdscr, rows, cols);

    int top = rows/2 - 8, left = cols/2 - 35, bottom = rows/2 + 8, right = cols/2 + 35;

    drawFrame(top, left, bottom, right, " HIGHSCORES ");

    centerPrint(top + 2, "#   SCORE     FECHA        NOMBRE");

    const auto& scores = g_highscores.getScores();
    
    int y = top + 4;
    for (int i = 0; i < (int)scores.size() && i < 10; ++i) {
        char line[100];
        snprintf(line, sizeof(line), "%-3d %-9d %-12s %s", 
                 i + 1, 
                 scores[i].score, 
                 scores[i].date.c_str(), 
                 scores[i].name.c_str());
        centerPrint(y++, line);
    }
    
    if (scores.empty()) {
        centerPrint(top + 5, "No hay puntajes registrados aún");
    }
    
    centerPrint(bottom - 2, "[ Enter / Esc para volver ]");
    refresh();

    while (true) {
        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == KEY_ENTER) break;
    }
}