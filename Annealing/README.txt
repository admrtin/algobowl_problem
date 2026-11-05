Files:
  main.cpp
  algorithm.h
  algorithm.cpp

Compile from Annealing directory (Linux / WSL):
  g++ -std=c++17 -O2 -Wall -Wextra -pedantic -pthread main.cpp algorithm.cpp -o algorithm

Compile verifier once, if needed:
  g++ -std=c++17 -O2 -Wall -Wextra ../Verifier/verifier.cpp -o ../Verifier/verifier

Run:
  ./algorithm