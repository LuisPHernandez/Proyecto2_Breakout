# ProyectoMicroprocesadores
Para poder compilar este programa se necesita compilar con  el sigueinte command"g++ -o breakout menu.cpp -lncurses" para luego ejecturar asi mismo en consola con "./breakout pero eso necesario primero instalar ncurses con el siguiente command en terminal "sudo apt update
sudo apt install libncurses5-dev libncursesw5-dev"

Ahora bien para poder compilar el el game entonces es 
"g++ -o breakout game.cpp -lncurses -lpthread-lm " y ejecutar es "./breakout"