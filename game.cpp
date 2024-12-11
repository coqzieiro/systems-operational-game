#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cmath>

#ifdef _WIN32
    #include <conio.h> // Para Windows
#else
    #include <termios.h>
    #include <unistd.h>
#endif

using namespace std;

// Função multiplataforma para capturar tecla sem "Enter"
char getKeyPress() {
#ifdef _WIN32
    return _getch(); // Captura tecla no Windows
#else
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt); // Obtém atributos atuais
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Desativa modo canônico e eco
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    read(STDIN_FILENO, &ch, 1); // Lê uma tecla
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restaura o terminal
    return ch;
#endif
}

// Constantes do tabuleiro
const int rows = 20, cols = 20;
const int numEnemies = 10;
const int numCoins = 15;
const int minDistance = 5; // Distância mínima entre jogador e inimigos

// Tabuleiro e controle
vector<vector<char>> board(rows, vector<char>(cols, '.'));
sem_t boardSem; // Semáforo para sincronizar acesso ao tabuleiro
mutex printMutex; // Mutex para sincronizar a impressão

int playerX = rows / 2, playerY = cols / 2;
vector<pair<int, int>> enemies(numEnemies);
int score = 0; // Pontuação do jogador
int remainingCoins = numCoins; // Moedas restantes
bool gameRunning = true;
chrono::steady_clock::time_point startTime;

// Função para exibir o tabuleiro
void printBoard() {
    lock_guard<mutex> lock(printMutex);
    system("clear"); // Limpa a tela (use "cls" no Windows para Windows)
    for (const auto& row : board) {
        for (char cell : row) {
            cout << cell << ' ';
        }
        cout << '\n';
    }
    // Exibe pontuação e tempo
    auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count();
    cout << "\nPontuação: " << score << "\tTempo: " << elapsed << "s\n";
}

// Função para mover o jogador
void movePlayer() {
    while (gameRunning) {
        char input = getKeyPress(); // Captura tecla sem "Enter"
        input = toupper(input); // Normaliza para maiúsculas

        if (input == 'Q') {
            gameRunning = false;
            break; // Sai do jogo
        }

        int newX = playerX, newY = playerY;
        if (input == 'W' && playerX > 0) newX--;
        else if (input == 'S' && playerX < rows - 1) newX++;
        else if (input == 'A' && playerY > 0) newY--;
        else if (input == 'D' && playerY < cols - 1) newY++;

        sem_wait(&boardSem); // Bloqueia acesso ao tabuleiro
        if (board[newX][newY] == '$') {
            score += 10;
            remainingCoins--; // Decrementa moedas restantes
            cout << "Você coletou uma moeda! (+10 pontos)\n";

            // Verifica se todas as moedas foram coletadas
            if (remainingCoins <= 0) {
                cout << "Parabéns! Você coletou todas as moedas e venceu o jogo!\n";
                gameRunning = false;
                exit(0);
            }
        } else if (board[newX][newY] == 'E') {
            cout << "Você foi pego por um inimigo! Fim de jogo.\n";
            gameRunning = false;
            exit(0);
        }
        board[playerX][playerY] = '.'; // Limpa posição antiga
        playerX = newX;
        playerY = newY;
        board[playerX][playerY] = '@'; // Atualiza posição do jogador
        sem_post(&boardSem); // Libera acesso ao tabuleiro

        printBoard();
        this_thread::sleep_for(chrono::milliseconds(100)); // Limita a velocidade do jogador
    }
}

// Função para calcular a próxima posição do inimigo na direção do jogador
pair<int, int> getNextEnemyPosition(int enemyX, int enemyY) {
    int dx = playerX - enemyX;
    int dy = playerY - enemyY;

    // Escolhe a direção mais curta para se aproximar do jogador
    if (abs(dx) > abs(dy)) {
        return {enemyX + (dx > 0 ? 1 : -1), enemyY};
    } else {
        return {enemyX, enemyY + (dy > 0 ? 1 : -1)};
    }
}

// Função para mover inimigos
void moveEnemies(int enemyIndex) {
    srand(time(0) + enemyIndex);
    while (gameRunning) {
        this_thread::sleep_for(chrono::milliseconds(450)); // Movimento a cada 450ms

        int enemyX = enemies[enemyIndex].first;
        int enemyY = enemies[enemyIndex].second;

        auto [newX, newY] = getNextEnemyPosition(enemyX, enemyY);

        // Garante que o novo movimento seja válido
        if (newX < 0 || newX >= rows || newY < 0 || newY >= cols) continue;

        sem_wait(&boardSem); // Bloqueia acesso ao tabuleiro
        if (board[newX][newY] == '.' || board[newX][newY] == '$') {
            board[enemyX][enemyY] = '.'; // Limpa posição antiga
            enemies[enemyIndex] = {newX, newY};
            board[newX][newY] = 'E';
        } else if (board[newX][newY] == '@') {
            cout << "Um inimigo pegou você! Fim de jogo.\n";
            gameRunning = false;
            exit(0);
        }
        sem_post(&boardSem); // Libera acesso ao tabuleiro

        printBoard();
    }
}

// Função para calcular a distância entre duas posições
int calculateDistance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

int main(void){
    srand(time(0));

    // Inicializa semáforo
    sem_init(&boardSem, 0, 1);

    // Marca tempo inicial
    startTime = chrono::steady_clock::now();

    // Inicializa o tabuleiro
    board[playerX][playerY] = '@';
    for (int i = 0; i < numCoins; ++i) {
        int x = rand() % rows;
        int y = rand() % cols;
        if (board[x][y] == '.') board[x][y] = '$';
    }

    // Inicializa inimigos longe do jogador
    for (int i = 0; i < numEnemies; ++i) {
        int x, y;
        do {
            x = rand() % rows;
            y = rand() % cols;
        } while (calculateDistance(x, y, playerX, playerY) < minDistance || board[x][y] != '.');
        board[x][y] = 'E';
        enemies[i] = {x, y};
    }

    printBoard();

    // Cria threads
    thread playerThread(movePlayer);
    vector<thread> enemyThreads;
    for (int i = 0; i < numEnemies; ++i) {
        enemyThreads.emplace_back(moveEnemies, i);
    }

    playerThread.join();
    for (auto& t : enemyThreads) {
        t.join();
    }

    sem_destroy(&boardSem); // Destroi semáforo
    return 0;
}