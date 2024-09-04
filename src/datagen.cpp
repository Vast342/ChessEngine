/*
    Clarity
    Copyright (C) 2024 Joseph Pasfield

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "globals.h"
#include "datagen.h"
#include "uci.h"
#include "search.h"
#include <thread>

void stopOtherThreads() {
    // hehehe
}

uint64_t getTotalNodes() {
    // hehehe
    return 0;
}

std::string directory;
uint64_t totalPositions = 0;
std::chrono::steady_clock::time_point beginTime;
std::vector<uint64_t> gamesPerThread;

// run it with *directory of the Clarity_Datagen.exe* *directory to save the file to* *number of games* *number of threads*
int main([[maybe_unused]]int argc, char** argv) {
    initialize();
    int numGames;
    int numThreads;
    if(argc == 1) {
        std::cout << "directory: ";
        std::cin >> directory;
        std::cout << "number of games: ";
        std::cin >> numGames;
        std::cout << "number of threads: ";
        std::cin >> numThreads;
    } else {
        directory = std::string(argv[1]);
        numGames = std::atoi(argv[2]);
        numThreads = std::atoi(argv[3]);
    }
    beginTime = std::chrono::steady_clock::now();
    std::cout << "Beginning data generation\n";
    generateData(numGames, numThreads);
    std::string response = "";
    std::cout << "Close thread? Y/N\n";
    std::cin >> response;
    return 0;
}

// manages the threads
void generateData(int numGames, int numThreads) {
    //std::cout << "Made it through the generateData Call\n";
    std::vector<std::jthread> threads;
    threads.reserve(numThreads);
    gamesPerThread.resize(numThreads);
    for(int i = 1; i <= numThreads; i++) {
        //std::cout << "Made Thread " << i << '\n';
        // lambda AAAAAAAAAAAAAAAAAAA RUN IN FeAR
        threads.emplace_back([numGames, i] {
                threadFunction(numGames, i);
        });
    }
    for(auto &thread : threads) {
        //std::cout << "Joining Thread\n";
        if(thread.joinable()) thread.join();
    }
}

// run on each thread
void threadFunction(int numGames, int threadID) {
    //std::cout << "Thread Function called on thread " << std::to_string(threadID) << '\n';
    Board board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    TranspositionTable TT;
    std::unique_ptr<Engine> engine = std::make_unique<Engine>(&TT);
    std::ofstream output;
    output.open(directory + "thread" + std::to_string(threadID) + ".txt");
    for(int i = 0; i < numGames; i++) {
        std::vector<std::string> fenVector;
        double result = runGame(*engine, fenVector, board);
        if(result == 2) {
            fenVector.clear();
            i--;
            continue;
        }
        //std::cout << "Finished Game " << i << "on thread" << threadID << '\n';
        dumpToArray(output, result, fenVector);
        gamesPerThread[threadID - 1]++;
    }
}
constexpr int moveLimit = 1000;
// manages the games
double runGame(Engine &engine, std::vector<std::string>& fenVector, Board board) {
    int score = 0;
    bool outOfBounds = false;
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
                if(board.isLegal(PLmoves[j])) {
                    moves[legalMoves] = PLmoves[j];
                    legalMoves++;
                    board.undoMove<true>();
                }
            }
            // checkmate or stalemate? doesn't matter, restart
            if(legalMoves == 0) {
                return 2;
            }
            // distribution
            std::uniform_int_distribution distribution{0, legalMoves - 1};

            const int index = distribution(gen);
            board.makeMove<false>(moves[index]);
            // move has been made now, cool
            //std::cout << board.getFenString() << '\n';
        } else {
            if(board.isRepeatedPosition()) return 0.5;
            if(board.getFiftyMoveCount() >= 50) return 0.5;
            // checkmate check
            // get moves
            std::array<Move, 256> PLmoves;
            const int totalMoves = board.getMoves(PLmoves);

            std::array<Move, 256> moves;
            int legalMoves = 0;
            // legality check
            for(int j = 0; j < totalMoves; j++) {
                if(board.isLegal(PLmoves[j])) {
                    moves[legalMoves] = PLmoves[j];
                    legalMoves++;
                }
            }
            // checkmate or stalemate?
            if(legalMoves == 0) {
                int colorMultiplier = 2 * board.getColorToMove() - 1;
                if(board.isInCheck()) {
                    // checkmate! opponent wins, so if black wins it's -1000000 * -(-1)
                    score = matedScore * -colorMultiplier;
                } else {
                    score = 0;
                }
                break;
            }
            // i think that this score might be a problem
            if(abs(score) > 2500) {
                if(outOfBounds) break;
                outOfBounds = true;
            } else {
                outOfBounds = false;
            }
            // get move from engine normally
            //std::cout << "sending board with position " << board.getFenString() << '\n';
            const auto result = engine.dataGenSearch(board, 5000);
            score = (board.getColorToMove() == 1 ? result.second : -result.second);
            if(((1ULL << result.first.getEndSquare()) & board.getOccupiedBitboard()) == 0 && result.first.getFlag() != EnPassant) {
                if(abs(score) < abs(matedScore + 256)) {
                    if(!board.isInCheck()) {
                        // non-mate, not in check, add fen string to vector
                        fenVector.push_back(board.getFenString() + " | " + std::to_string(score));
                    }
                } else {
                    // checkmate found, no more use for this
                    break;
                }
            }
            //std::cout << "score is now " << score << '\n';
            if(!board.isLegal(result.first)) {
                std::cout << "Engine made an illegal move\n";
            }
            //std::cout << board.getFenString() << '\n';
        }
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
int outputFrequency = 100;
int infoOutputFrequency = 1000;
void dumpToArray(std::ofstream &output, double result, std::vector<std::string>& fenVector) {
    games++;
    if((games % outputFrequency) == 0) std::cout << "Finished game " << games << '\n';
    for(const std::string &fen : fenVector) {
        // add to file and append result \n
        output << fen << " | " << result << '\n';
        totalPositions++;
    }
    if((games % infoOutputFrequency) == 0) {
        std::cout << "Total Positions: " << totalPositions << '\n';
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beginTime).count();
        std::cout << "Time: " << (elapsedTime / 1000) << " seconds " << '\n';
        std::cout << "Positions per second: " << (totalPositions / (elapsedTime / 1000)) << '\n';
        std::cout << "Positions per game: " << (totalPositions / games) << '\n';
        std::cout << "Games Per Thread: [";
        for(const uint64_t gameCount : gamesPerThread) {
            std::cout << gameCount << ", ";
        }
        std::cout << "]" << std::endl;
    }
}
