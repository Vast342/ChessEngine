#include "globals.h"
#include "datagen.h"
#include "search.h"

std::ofstream output;
uint64_t totalPositions = 0;
std::chrono::steady_clock::time_point beginTime;

// run it with *directory of the Clarity_Datagen.exe* *directory to save the file to* *number of games*
int main(int argc, char** argv) {
    initialize();
    std::string directory = std::string(argv[1]);
    int numGames = std::atoi(argv[2]);
    output.open(directory);
    beginTime = std::chrono::steady_clock::now();
    std::cout << "Beginning data generation\n";
    generateData(numGames);
    output.close();
    std::string response = "";
    std::cout << "Close thread? Y/N\n";
    std::cin >> response;
    return 0;
}

// manages the threads
void generateData(int numGames) {
    threadFunction(numGames, 1);
}

// run on each thread
void threadFunction(int numGames, int threadID) {
    for(int i = 0; i < numGames; i++) {
        std::vector<std::string> fenVector;
        double result = runGame(fenVector);
        if(result == 2) std::cout << "Error! invalid game result\n";
        dumpToArray(result, fenVector);
    }
}

// idk what to use here lol
constexpr uint8_t threadCount = 5;
constexpr int moveLimit = 1000;
// manages the games
double runGame(std::vector<std::string>& fenVector) {
    int score = 0;
    bool outOfBounds = false;
    Board board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    for(int i = 0; i <= moveLimit; i++) {
        if(i < 8) {
            // make a random move
            std::random_device rd;
            std::mt19937_64 gen(rd());

            // get moves
            std::array<Move, 256> PLmoves;
            const int totalMoves = board.getMoves(PLmoves);

            std::array<Move, 256> moves;
            int legalMoves = 0;
            // legality check
            for(int j = 0; j < totalMoves; j++) {
                if(board.makeMove(PLmoves[j])) {
                    moves[legalMoves] = PLmoves[j];
                    legalMoves++;
                    board.undoMove();
                }
            }
            // checkmate or stalemate? doesn't matter, restart
            if(legalMoves == 0) {
                board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                i = -1;
                continue;
            }
            // distribution
            std::uniform_int_distribution distribution{0, legalMoves - 1};

            const int index = distribution(gen);
            board.makeMove(moves[index]);
            // move has been made now, cool
            //std::cout << board.getFenString() << std::endl;
        } else {
            if(board.isRepeated) return 0.5;
            if(board.getFiftyMoveCount() >= 50) return 0.5;
            // checkmate check
            // get moves
            std::array<Move, 256> PLmoves;
            const int totalMoves = board.getMoves(PLmoves);

            std::array<Move, 256> moves;
            int legalMoves = 0;
            // legality check
            for(int j = 0; j < totalMoves; j++) {
                if(board.makeMove(PLmoves[j])) {
                    moves[legalMoves] = PLmoves[j];
                    legalMoves++;
                    board.undoMove();
                }
            }
            // checkmate or stalemate?
            if(legalMoves == 0) {
                int colorMultiplier = 2 * board.getColorToMove() - 1;
                if(board.isInCheck()) {
                    // checkmate! opponent wins, so if black wins it's -1000000 * -(-1)
                    score = mateScore * -colorMultiplier;
                } else {
                    score = 0;
                }
                break;
            }
            // get move from engine normally
            //std::cout << "sending board with position " << board.getFenString() << std::endl;
            const auto result = dataGenSearch(board, 5000);
            const uint64_t capturable = board.getOccupiedBitboard();
            score = (board.getColorToMove() == 1 ? result.second : -result.second);
            // i think that this score might be a problem
            if(abs(score) > 2500) {
                if(outOfBounds) break;
                outOfBounds = true;
            }
            if(((1ULL << result.first.getEndSquare()) & capturable) == 0 || result.first.getFlag() == EnPassant) {
                if(abs(score) < abs(mateScore + 256)) {
                    // non-mate, add fen string to vector
                    fenVector.push_back(board.getFenString() + " | " + std::to_string(score));
                } else {
                    // checkmate found, no more use for this
                    break;
                }
            }
            //std::cout << "score is now " << score << std::endl;
            if(!board.makeMove(result.first)) {
                std::cout << "Engine made an illegal move\n";
            }
            //std::cout << board.getFenString() << std::endl;
        }
        if(i == moveLimit) return 0.5;
    }

    // return 1 if white won, 0 if black won, and 0.5 if draw, this will be useful later
    if(score > 1) {
        return 1;
    } else if(score < -1) {
        return 0;
    } else {
        return 0.5;
    }
    // error
    return 2;
}

int games = 0;
int outputFrequency = 10;
int infoOutputFrequency = 100;
void dumpToArray(double result, std::vector<std::string>& fenVector) {
    games++;
    if((games % outputFrequency) == 0) std::cout << "Finished game " << games << std::endl;
    for(const std::string &fen : fenVector) {
        // add to file and append result \n
        output << fen << " | " << result << '\n';
        totalPositions++;
    }
    if((games % infoOutputFrequency) == 0) {
        std::cout << "Total Positions: " << totalPositions << std::endl;
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beginTime).count();
        std::cout << "Time: " << (elapsedTime / 1000) << " seconds " << std::endl;
        std::cout << "Positions per second: " << (totalPositions / (elapsedTime / 1000)) << std::endl;
    }
}