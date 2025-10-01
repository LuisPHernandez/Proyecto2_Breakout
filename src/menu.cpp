#include "game.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <ctime>
#include <cstdlib>

// Estados
enum class Screen { MAIN_MENU, INSTRUCTIONS, HIGHSCORES, GAMEPLAY, EXIT };

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
        int L = (int)title.size(); // CORREGIDO: Faltaba el punto y coma
        int tx = left + ((w - L) / 2) + 1;
        mvprintw(top, tx, "%s", title.c_str());
    }
}

void centerPrint(int row, const std::string& s) {
    int cols, rows; // CORREGIDO: Separado en dos declaraciones
    getmaxyx(stdscr, rows, cols);
    int x = (cols - (int)s.size())/2;
    mvprintw(row, x, "%s", s.c_str());
}

// Declaraciones de pantallas
Screen showMainMenu();
void showInstructions();
void showHighscores();
void runGameplay();

// Programa principal
int main() {
    // Se crea la ventana principal stdscr con ncurses, se pasa a modo curses
    initscr();
    // Activa modo cbreak (sin line buffering)
    cbreak();
    // Desactiva el echo de las teclas, no se imprime todo lo que el usuario escribe en pantalla
    noecho();
    // Se activa keypad para interpretar teclas especiales de manera intuitiva
    keypad(stdscr, TRUE);
    // Oculta el cursor
    curs_set(0);

    // Declara el estado actual de la aplicación (Menu principal)
    Screen screen = Screen::MAIN_MENU;

    // Bucle principal
    while (screen != Screen::EXIT) {
        switch (screen) {
            case Screen::MAIN_MENU: screen = showMainMenu(); break;                            // Pantalla de menú principal                        
            case Screen::INSTRUCTIONS: showInstructions(); screen = Screen::MAIN_MENU; break;  // Pantalla de instrucciones
            case Screen::HIGHSCORES: showHighscores();  screen = Screen::MAIN_MENU; break;     // Pantalla de highscores
            case Screen::GAMEPLAY: clear();                                                    // Pantalla de juego
                refresh();
                runGameplay();
                clear();
                refresh();     
                screen = Screen::MAIN_MENU; break;       
            case Screen::EXIT: break;
        }
    }

    // Se desactiva modo curses
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
        // Se limpia el buffer de dibujo de stdscr
        clear();

        // Se obtiene el tamaño de la terminal en filas y columnas
        int rows, cols; getmaxyx(stdscr, rows, cols);

        // Se calcula un rectangulo centrado para dibujar el marco de la aplicación
        int top = rows/2 - 8, left = cols/2 - 30, bottom = rows/2 + 8, right = cols/2 + 30;

        // Se dibuja el marco y el título
        drawFrame(top, left, bottom, right, " BREAKOUT ");
        centerPrint(top + 2, "MENU PRINCIPAL");

        // Se loopea por todos los elementos del menú
        for (int i = 0; i < (int)items.size(); ++i) {
            std::string line = (i == selected ? ">> " : "") + items[i] + (i == selected ? " <<" : "");
            int y = top + 4 + i*2;

            // ncurses activa el atributo de brillo/colores invertidos para resaltar la opción seleccionada
            if (i == selected) attron(A_REVERSE);
            // Se imprime la línea centrada
            centerPrint(y, line);
            if (i == selected) attroff(A_REVERSE);
        }
        centerPrint(bottom - 2, "Usa Flechas W/S)  Enter = Seleccionar  Esc = Salir");

        // Refresca la pantalla (Se dibuja en la terminal todo lo acumulado en el buffer)
        refresh();

        // Se lee una tecla y se hace la acción asociada
        int ch = getch();
        if (ch == KEY_UP || ch == 'w' || ch == 'W') {
            selected = (selected - 1 + (int)items.size()) % (int)items.size();
        } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
            selected = (selected + 1) % (int)items.size();
        } else if (ch == 27) {  // Esc
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
    // Limpia el buffer de dibujo de stdscr
    clear();

    // Se obtiene el tamaño de la terminal en filas y columnas
    int rows, cols; getmaxyx(stdscr, rows, cols);

    // Se calcula un rectangulo centrado para dibujar el marco de la aplicación
    int top = rows/2 - 10, left = cols/2 - 35, bottom = rows/2 + 10, right = cols/2 + 35;

    // Se dibuja el marco y el título
    drawFrame(top, left, bottom, right, " INSTRUCCIONES ");
    int y = top + 2;
    
    // Objetivo del juego
    centerPrint(y++, "Objetivo: Romper todos los ladrillos con la pelota,");
    centerPrint(y++, "sin dejar que esta caiga al fondo, para sumar puntos.");
    y++;

    // Controles
    centerPrint(y++, "Mover paleta: ← / →  (A / D)");
    centerPrint(y++, "Lanzar pelota: Espacio");
    centerPrint(y++, "Pausa: P");
    centerPrint(y++, "Reiniciar nivel: R");
    centerPrint(y++, "Salir: Esc / Q");
    y++;

    // Elementos visuales
    centerPrint(y++, "Elementos del juego:");
    centerPrint(y++, "Paleta (=) controlada por el jugador");
    centerPrint(y++, "Pelota (o) que rebota y destruye ladrillos");
    centerPrint(y++, "Ladrillos (#, %, @) con diferentes resistencias");
    centerPrint(y++, "Bordes (| - +) que delimitan el area de juego");
    y++;
    
    centerPrint(y++, "[ Enter / Esc para volver ]");
    refresh();

    // Espera breve a tecla
    while (true) {
        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == KEY_ENTER) break;
    }
}

// Pantalla de highscores
void showHighscores() {
    // Limpia el buffer de dibujo de stdscr
    clear();

    // Se obtiene el tamaño de la terminal en filas y columnas
    int rows, cols; getmaxyx(stdscr, rows, cols);

    // Se calcula un rectangulo centrado para dibujar el marco de la aplicación
    int top = rows/2 - 8, left = cols/2 - 35, bottom = rows/2 + 8, right = cols/2 + 35;

    // Se dibuja el marco y el título
    drawFrame(top, left, bottom, right, " HIGHSCORES ");

    // Se imprimen los encabezados
    centerPrint(top + 2, "#  SCORE     FECHA        NOMBRE");

    // Ejemplo de datos (cambiar por lectura de archivo en otra fase)
    std::vector<std::string> lines = {
        "1  012340   2025-09-08   LUISPEDRO",
        "2  009900   2025-09-07   MIGUEL",
        "3  007500   2025-09-07   JOEL"
    };

    // Se imprimen los puntajes altos
    int y = top + 4;
    for (const auto& s : lines) centerPrint(y++, s);
    centerPrint(bottom - 2, "[ Enter / Esc para volver ]");
    refresh();

    while (true) {
        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == KEY_ENTER) break;
    }
}