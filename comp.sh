g++ -c panfield.austere.cpp -o panfield.o -std=c++14 -O3 #-g
[ ! -f panfield.o ] || g++ panfield.o -o panfield -lsfml-graphics -lsfml-window -lsfml-system #-static-libstdc++
[ ! -f panfield.o ] || rm panfield.o
