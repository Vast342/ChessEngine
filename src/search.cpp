#include "search.h"
#include "psqt.h"
#include "tt.h"

const int startDepth = 3;

Move rootBestMove = Move();

int nodes = 0;

int timeToSearch = 0;

TranspositionTable TT;

std::array<std::array<int, 64>, 64> historyTable;

std::chrono::steady_clock::time_point begin;

void resetEngine() {
    TT.clearTable();
}

void orderMoves(const Board& board, std::array<Move, 256> &moves, int numMoves, int ttMoveValue) {
    std::array<int, 256> values;
    const uint64_t occupied = board.getOccupiedBitboard();
    for(int i = 0; i < numMoves; i++) {
        values[i] = historyTable[moves[i].getStartSquare()][moves[i].getEndSquare()];
        if(moves[i].getValue() == ttMoveValue) {
            values[i] = 1000000000;
        } else if((occupied & (1ULL << moves[i].getEndSquare())) != 0) {
            // mvv lva (ciekce was here)
            const auto attacker = getType(board.pieceAtIndex(moves[i].getStartSquare()));
            const auto victim = getType(board.pieceAtIndex(moves[i].getEndSquare()));
            values[i] = eg_value[victim] - eg_value[attacker];
        }
        values[i] = -values[i];
        // incremental sort was broken, I need to come back to it at some point
        //incrementalSort(values, moves, numMoves, i);
    }
    sortMoves(values, moves, numMoves);
}

int qSearch(Board &board, int alpha, int beta) {
    if(board.isRepeated) return 0;
    // time check every 4096 nodes
    if(nodes % 4096 == 0) {
        if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() > timeToSearch) {
            return 0;
        }
    }

    // TT check
    Transposition entry = TT.getEntry(board.zobristHash);

    if(entry.zobristKey == board.zobristHash && (
        entry.flag == Exact // exact score
            || (entry.flag == BetaCutoff && entry.score >= beta) // lower bound, fail high
            || (entry.flag == FailLow && entry.score <= alpha) // upper bound, fail low
    )) {
        //std::cout << "did a TT cutoff with hash " << std::to_string(board.zobristHash) << " and score " << std::to_string(entry.score) << '\n';
        return entry.score;
    }

    // stand pat shenanigans
    int bestScore = board.getEvaluation();
    if(bestScore >= beta) return bestScore;
    if(alpha < bestScore) alpha = bestScore;
  
    // get the legal moves and sort them
    std::array<Move, 256> moves;
    int totalMoves = board.getMovesQSearch(moves);
    orderMoves(board, moves, totalMoves, entry.bestMove.getValue());

    // values useful for writing to TT later
    Move bestMove;
    int flag = FailLow;
  
    // loop though all the moves
    for(int i = 0; i < totalMoves; i++) {
        if(board.makeMove(moves[i])) {
            nodes++;
            // searches from this node
            int score = -qSearch(board, -beta, -alpha);
            board.undoMove();
            // time check
            if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() > timeToSearch) {
                return 0;
            }

            if(score > bestScore) {
                bestScore = score;
                bestMove = moves[i];

                // Improve alpha
                if(score > alpha) { 
                    flag = Exact;
                    //bestMove = moves[i];
                    alpha = score;
                }

                // Fail-high
                if(score >= beta) {
                    flag = BetaCutoff;
                    //bestMove = moves[i];
                    break;
                }
            }
        }
    }

    // push to TT
    //std::cout << "writing a qsearch entry at " << std::to_string(board.zobristHash) << " with score " << std::to_string(bestScore) << '\n';
    // just doing move ordering in q search currently
    TT.setEntry(board.zobristHash, Transposition(board.zobristHash, bestMove, flag, bestScore, 0));

    // fail low, if it's not cut off.
    return bestScore;  
}

int negamax(Board &board, int depth, int alpha, int beta, int ply) {
    if(ply > 0 && board.isRepeated) return 0;
    // time check every 4096 nodes
    if(nodes % 4096 == 0) {
        if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() > timeToSearch) {
            return 0 ;
        }
    }
    // activate q search if at the end of a branch
    if(depth <= 0) return qSearch(board, alpha, beta);

    // TT check
    Transposition entry = TT.getEntry(board.zobristHash);

    if(ply > 0 && entry.zobristKey == board.zobristHash && entry.depth >= depth && (
            entry.flag == Exact // exact score
                || (entry.flag == BetaCutoff && entry.score >= beta) // lower bound, fail high
                || (entry.flag == FailLow && entry.score <= alpha) // upper bound, fail low
        )) {
        //std::cout << "did a TT cutoff with hash " << std::to_string(board.zobristHash) << " and score " << std::to_string(entry.score) << '\n';
        return entry.score;
    }


    // get the moves
    std::array<Move, 256> moves;
    int totalMoves = board.getMoves(moves);
    orderMoves(board, moves, totalMoves, entry.bestMove.getValue());

    // values useful for writing to TT later
    int bestScore = -10000000;
    Move bestMove;
    int flag = FailLow;

    int extensions = -1;
    if(board.isInCheck()) extensions++;

    // loop through the moves
    int legalMoves = 0;
    for(int i = 0; i < totalMoves; i++) {
        if(board.makeMove(moves[i])) {
            legalMoves++;
            nodes++;
            // Late Move Reductions (LMR)
            if(extensions == -1 && depth > 4) {
                // formula here from Fruit Reloaded
                extensions -= reductions[depth][i];
            }
            // searches from this node, at a lower depth.
            int score = -negamax(board, depth + extensions, -beta, -alpha, ply + 1);
            board.undoMove();

            // backup time check
            if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() > timeToSearch) return 0;

            if(score > bestScore) {
                bestScore = score;
                bestMove = moves[i];
                if(ply == 0) rootBestMove = moves[i];

                // Improve alpha
                if(score > alpha) {
                    flag = Exact; 
                    //bestMove = moves[i];
                    //if(ply == 0) rootBestMove = moves[i];
                    alpha = score;
                }

                // Fail-high
                if(score >= beta) {
                    flag = BetaCutoff;
                    historyTable[moves[i].getStartSquare()][moves[i].getEndSquare()] += depth * depth;
                    //bestMove = moves[i];
                    //if(ply == 0) rootBestMove = moves[i];
                    break;
                }
            }
        }
    }

    // checkmate / stalemate detection
    if(legalMoves == 0) {
        if(board.isInCheck()) {
            return -10000000 + ply;
        }
        return 0;
    }

    // push to TT
    //std::cout << "writing a regular entry at " << std::to_string(board.zobristHash) << " with score " << std::to_string(bestScore) << '\n';
    TT.setEntry(board.zobristHash, Transposition(board.zobristHash, bestMove, flag, bestScore, depth));

    // if no beta cutoff, fail low
    return bestScore;
}

Move think(Board board, int timeLeft) {
    for(int i = 0; i < 64; i++) {
        for(int j = 0; j < 64; j++) {
            historyTable[i][j] = 0;
        }
    }
    nodes = 0;
    timeToSearch = timeLeft / 30;

    begin = std::chrono::steady_clock::now();

    rootBestMove = Move();

    for(int depth = 1; depth < 100; depth++) {
        Move previousBest = rootBestMove;
        int result = negamax(board, depth, -10000000, 10000000, 0);
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        if(elapsedTime > timeToSearch) {
            rootBestMove = previousBest;
            break;
        } else {
            std::cout << "info depth " << std::to_string(depth) << " nodes " << std::to_string(nodes) << " time " << std::to_string(elapsedTime) << " score cp " << std::to_string(result) << std::endl;
        }
    }

    std::random_device rd;
    std::mt19937_64 gen(rd());

    if(rootBestMove.getValue() != 0) {
        return rootBestMove;
    } else {
        std::cout << "had to make a random move\n";
        // random mover as a backup
        std::array<Move, 256> moves;
        int totalMoves = board.getMoves(moves);

        std::uniform_int_distribution distribution{0, totalMoves - 1};

        while(true) {
            int index = distribution(gen);
            if(board.makeMove(moves[index])) {
                board.undoMove();
                return moves[index];
            }
        }
    }
}
