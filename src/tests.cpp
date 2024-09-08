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
#include "testessentials.h"
#include "testsuites.h"

// runs a single perft test
int perft(Board &board, int depth) {
    if(depth == 0) return 1;
    std::array<Move, 256> moves;
    int numMoves = board.getMoves(moves);
    int result = 0;
    for(int i = 0; i < numMoves; i++) {
        if(board.isLegal(moves[i])) {
            board.makeMove<false>(moves[i]);
            result += perft(board, depth-1);
            board.undoMove<false>();
        }
    }
    return result;
}

// runs an entire suite of perft tests
void runPerftSuite(int number) {
    if(number == 0) {
        int i = 0;
        int passed = 0;
        int failed = 0;
        double total = 0;
        clock_t start = clock();
        for(PerftTest test : etherealSuite) {
            i++;
            Board board(test.fen);
            int result = perft(board, test.depth);
            total += result;
            if(result == test.expectedOutput) {
                std::cout << "Test " << std::to_string(i) << " Passed\n";
                passed++;
            } else {
                std::cout << "Test " << std::to_string(i) << " Failed, outputted " << std::to_string(result) << " With fen string "  << test.fen << " and depth " << std::to_string(test.depth) << '\n';
                failed++;
            }
        }
        clock_t end = clock();
        std::cout << "Passed " << std::to_string(passed) << ", Failed " << std::to_string(failed) << '\n';
        std::cout << "Tests took: " << std::to_string((end-start)/static_cast<double>(1000)) << '\n';
        std::cout << "Total nodes: " << std::to_string(static_cast<int>(total)) << '\n';
        std::cout << "NPS: " << std::to_string(total / ((end-start)/static_cast<double>(1000))) << '\n';
    }
}

// runs perft split by what the first move that is done is
void splitPerft(Board board, int depth) {
    std::array<Move, 256> moves;
    int numMoves = board.getMoves(moves);
    int total = 0;
    clock_t start = clock();
    for(int i = 0; i < numMoves; i++) {
        if(board.isLegal(moves[i])) {
            board.makeMove<false>(moves[i]);
            int result = perft(board, depth - 1);
            board.undoMove<false>();
            total += result;
            std::cout << toLongAlgebraic(moves[i]) << ": " << std::to_string(result) << '\n';
        }
    }
    clock_t end = clock();
    std::cout << "Total: " << std::to_string(total) << '\n';
    std::cout << "Time: " << std::to_string((end-start)/static_cast<double>(1000)) << '\n';
    std::cout << "NPS: " << std::to_string(total / ((end-start)/static_cast<double>(1000))) << '\n';
}

// runs an individual perft test, and outputs the results
void individualPerft(Board board, int depth) {
    clock_t start = clock();
    int result = perft(board, depth);
    clock_t end = clock();
    std::cout << "Result: " << std::to_string(result) << '\n';
    std::cout << "Time: " << std::to_string((end-start)/static_cast<double>(1000)) << '\n';
    std::cout << "NPS: " << std::to_string(result / ((end-start)/static_cast<double>(1000))) << '\n';
}
