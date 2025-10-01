# Instrucciones de Compilación - Breakout Multihilo

## Estructura de Archivos

```
proyecto/
├── game.h                      # Header principal del juego
├── highscores.h               # Header del sistema de puntajes
├── game.cpp                   # Lógica principal del juego
├── highscores.cpp             # Implementación de puntajes
├── menu.cpp                   # Menú principal e interfaz
├── threads/
│   ├── tick.cpp              # Hilo coordinador de frames
│   ├── input.cpp             # Hilo de entrada del teclado
│   ├── paddle.cpp            # Hilo de la paleta
│   ├── ball.cpp              # Hilo de la pelota
│   ├── collisionsWallsPaddle.cpp  # Colisiones paredes/paleta
│   ├── collisionsBricks.cpp  # Colisiones con ladrillos
│   ├── render.cpp            # Hilo de renderizado
│   └── state.cpp             # Hilo de estado del juego
└── highscores.txt            # Archivo de puntajes (se crea automáticamente)
```

## Compilación

### Opción 1: Comando completo
```bash
g++ -o breakout \
    menu.cpp \
    game.cpp \
    highscores.cpp \
    threads/tick.cpp \
    threads/input.cpp \
    threads/paddle.cpp \
    threads/ball.cpp \
    threads/collisionsWallsPaddle.cpp \
    threads/collisionsBricks.cpp \
    threads/render.cpp \
    threads/state.cpp \
    -lncurses -lpthread -std=c++11
```

### Opción 2: Usando Makefile (recomendado)

Crea un archivo `Makefile`:

```makefile
CXX = g++
CXXFLAGS = -std=c++11 -Wall
LIBS = -lncurses -lpthread

SRCS = menu.cpp game.cpp highscores.cpp \
       threads/tick.cpp threads/input.cpp threads/paddle.cpp \
       threads/ball.cpp threads/collisionsWallsPaddle.cpp \
       threads/collisionsBricks.cpp threads/render.cpp threads/state.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = breakout

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) highscores.txt

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
```

Luego compila con:
```bash
make
```

## Ejecución

```bash
./breakout
```

## Requisitos del Sistema

- **Compilador**: g++ con soporte para C++11 o superior
- **Bibliotecas**:
  - `ncurses` (interfaz de terminal)
  - `pthread` (hilos POSIX)
- **Sistema Operativo**: Linux, macOS, o WSL en Windows

### Instalar dependencias en Ubuntu/Debian:
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

### Instalar dependencias en macOS:
```bash
brew install ncurses
```

## Características del Sistema de Highscores

1. **Almacenamiento persistente**: Los puntajes se guardan en `highscores.txt`
2. **Top 10**: Mantiene los 10 mejores puntajes
3. **Ordenamiento automático**: Los puntajes se ordenan de mayor a menor
4. **Entrada de nombre**: Si logras un highscore, puedes ingresar tu nombre (máx. 10 letras)
5. **Fecha automática**: Registra la fecha del puntaje automáticamente
6. **Valores por defecto**: Si no existe el archivo, se crea con 5 puntajes de ejemplo

## Formato del archivo highscores.txt

```
5000 2025-01-01 CLAUDIA
4500 2025-01-01 ANTONIO
4000 2025-01-01 SOFIA
```

Formato: `SCORE FECHA NOMBRE` (separados por espacios)

## Solución de Problemas

### Error: "ncurses.h: No such file or directory"
Instala la biblioteca ncurses (ver sección de requisitos)

### Error: "undefined reference to pthread_create"
Asegúrate de incluir `-lpthread` en la compilación

### El juego no se ve bien
Asegúrate de que tu terminal tenga al menos 80x25 caracteres

### Los puntajes no se guardan
Verifica permisos de escritura en el directorio del juego