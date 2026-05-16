# Xit-M-ximo

## Compilar o exemplo C++

No Windows:
- Visual Studio / MSVC:
  `cl /EHsc main.cpp`
- MinGW:
  `g++ -o main.exe main.cpp -std=c++17`

No Linux/macOS (modo simulado):
- `g++ -o main main.cpp -std=c++17`

No Windows, o programa pedirá o PID do processo e tentará aplicar os ângulos de mira.
Em Linux/macOS, ele roda em modo simulado para testar a lógica de cálculo e escrita de ângulos.
