#include "search.h"
#include "psqt.h"
#include "tt.h"

constexpr int hardNodeCap = 400000;

constexpr int startDepth = 3;

constexpr int historyCap = 16384;

constexpr int nmpMin = 2;

constexpr int depthLimit = 100;

// The main search functions

bool dataGeneration = false;

Move rootBestMove = Move();

int nodes = 0;

int hardLimit = 0;

int seldepth = 0;

TranspositionTable TT;

std::array<std::array<std::array<int, 64>, 64>, 2> historyTable;

std::array<std::array<int, 2>, 100> killerTable;

std::chrono::steady_clock::time_point begin;

bool timesUp = false;

// resets the history, done when ucinewgame is sent, and at the start of each turn
void clearHistory() {
    for(int h = 0; h < 2; h++) {
        for(int i = 0; i < 64; i++) {
            for(int j = 0; j < 64; j++) {
                historyTable[h][i][j] = 0;
            }
        }
    }
}

// ages the history values, which could be done at the start of turns, however I am not currently doing so until I test it more
void ageHistory() {
    for(int h = 0; h < 2; h++) {
        for(int i = 0; i < 64; i++) {
            for(int j = 0; j < 64; j++) {
                // mess around with values of this, looks like /= 8 is slightly losing, as is 2
                historyTable[h][i][j] /= 2;
            }
        }
    }
}

// resizes the transposition table
void resizeTT(int newSize) {
    TT.resize(newSize);
}

// resets the engine, done when ucinewgame is sent
void resetEngine() {
    TT.clearTable();
    clearHistory(); 
}

int estimateMoveValue(const Board& board, const int end, const int flag) {
    // starting with the end square piece
    int value = eg_value[getType(board.pieceAtIndex(end))];
    // promotions! pawn--, newpiece++
    for(int i = 0; i < 4; i++) {
        if(flag == promotions[i]) {
            value = eg_value[i + 1] - eg_value[Pawn];
            return value;
        }
    }

    // Target square is empty for en passant, but you still capture a pawn
    if(flag == EnPassant) {
        value = eg_value[Pawn];
    }
    // castling can't capture and is never encoded as such so we don't care.
    return value;
}

bool see(const Board& board, Move move, int threshold) {
    // establishing stuff
    const int start = move.getStartSquare();
    const int end = move.getEndSquare();
    const int flag = move.getFlag();

    int nextVictim = getType(board.pieceAtIndex(start));
    // handle promotions
    // promotion flags are the 4 highest numbers, so this saves a loop if it's not necessary
    if(flag > DoublePawnPush) {
        for(int i = 0; i < 4; i++) {
            if(flag == promotions[i]) {
                nextVictim = i + 1;
                break;
            }
        }
    }
    int balance = estimateMoveValue(board, end, flag) - threshold;
    // best case still doesn't beat threshold, not good
    if(balance < 0) return false;
    // worst case, we lose the piece here
    balance -= eg_value[nextVictim];
    // if it's still winning in the best case scenario, we can just cut it off
    if(balance >= 0) return true;
    // make sure occupied bitboard knows we did the first move
    uint64_t occupied = board.getOccupiedBitboard();
    occupied = (occupied ^ (1ULL << start)) | (1ULL << end);
    if (flag == EnPassant) occupied ^= (1ULL << board.getEnPassantIndex());
    int color = 1 - board.getColorToMove();
    // get the pieces, for detecting revealed attackers
    const uint64_t bishops = board.getPieceBitboard(Bishop) | board.getPieceBitboard(Queen);
    const uint64_t rooks = board.getPieceBitboard(Rook) | board.getPieceBitboard(Queen);
    // generate the attackers (not including the first move)
    uint64_t attackers = board.getAttackers(end) & occupied;
    while(true) {
        // get the attackers
        uint64_t myAttackers = attackers & board.getColoredBitboard(color);
        // if no attackers, you're done
        if (myAttackers == 0ULL) break;
        // find lowest value attacker
        for (nextVictim = Pawn; nextVictim <= Queen; nextVictim++) {
            if ((myAttackers & board.getColoredPieceBitboard(color, nextVictim)) != 0) {
                break;
            }
        }
        // make the move
        occupied ^= (1ULL << std::countr_zero(myAttackers & board.getColoredPieceBitboard(color, nextVictim)));
        // diagonal moves may reveal more attackers
        if(nextVictim == Pawn || nextVictim == Bishop || nextVictim == Queen) {
            attackers |= (getBishopAttacks(end, occupied) & bishops);
        }
        // orthogonal moves may reveal more attackers
        if(nextVictim == Rook || nextVictim == Queen) {
            attackers |= (getRookAttacks(end, occupied) & rooks);
        }
        attackers &= occupied;
        color = 1 - color;
        // update balance
        balance = -balance - 1 - eg_value[nextVictim];
        // if you are ahead
        if(balance >= 0) {
            // speedrunning legality check
            if(nextVictim == King && ((attackers & board.getColoredBitboard(color)) != 0)) {
                color = 1 - color;
            }
            break;
        }
    }
    // if color is different, than you either ran out of attackers and lost the exchange or the opponent won
    return board.getColorToMove() != color;
}

/* orders the moves based on the following order:
    1: TT Best move: the result of a previous search, if any
    2: MVV-LVA (soon to be aided by SEE) for captures, sorting them by the ratio of the piece capturing to the piece being captured
    3: Killer moves: moves that are proven to be good from earlier, being indexed by ply
    4: History: scores of how many times a move has caused a beta cutoff
*/
void scoreMoves(const Board& board, std::array<Move, 256> &moves, std::array<int, 256> &values, int numMoves, int ttMoveValue, int ply) {
    const uint64_t occupied = board.getOccupiedBitboard();
    for(int i = 0; i < numMoves; i++) {
        const int moveValue = moves[i].getValue();
        if(moveValue == ttMoveValue) {
            values[i] = 1000000000;
        } else if((occupied & (1ULL << moves[i].getEndSquare())) != 0) {
            // Captures!
            // see not workey yet
            if(ply == -1 || see(board, moves[i], 0)) {
                // good captures
                // mvv lva (ciekce was here)
                const auto attacker = getType(board.pieceAtIndex(moves[i].getStartSquare()));
                const auto victim = getType(board.pieceAtIndex(moves[i].getEndSquare()));
                // technically I could use 175 here and still have some wiggle room
                // with 200 that gives me a range of:
                // min: (200*112)-1187=21213
                // max: (200*1187)-112=237288
                values[i] = 200 * eg_value[victim] - eg_value[attacker];
            } else {
                // bad captures
                values[i] = 0;
            }
        } else {
            // read from history
            values[i] = historyTable[board.getColorToMove()][moves[i].getStartSquare()][moves[i].getEndSquare()];
            // if not in qsearch, killers
            if(ply != -1) {
                if(moveValue == killerTable[ply][0]) {
                    values[i] = 19000;
                } else if(moveValue == killerTable[ply][1]) {
                    values[i] = 18000;
                } 
            }
        }
        //values[i] = -values[i];
    }
    //sortMoves(values, moves, numMoves);
}

// Quiecense search, searching all the captures until there aren't any more as a slightly faster but less accurate search
int qSearch(Board &board, int alpha, int beta, int ply) {
    if(board.isRepeated) return 0;
    // time check every 4096 nodes
    if(nodes % 4096 == 0) {
        if(dataGeneration) {
            if(nodes > hardNodeCap) {
                timesUp = true;
                return 0;
            }
        } else {
            if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() > hardLimit) {
                timesUp = true;
                return 0;
            }
        }
    }
    if(ply > seldepth) seldepth = ply;
    // TT check
    Transposition entry = TT.getEntry(board.zobristHash);

    if(entry.zobristKey == board.zobristHash && (
        entry.flag == Exact // exact score
            || (entry.flag == BetaCutoff && entry.score >= beta) // lower bound, fail high
            || (entry.flag == FailLow && entry.score <= alpha) // upper bound, fail low
    )) {
        return entry.score;
    }

    // stand pat shenanigans
    int bestScore = board.getEvaluation();
    if(bestScore >= beta) return bestScore;
    if(alpha < bestScore) alpha = bestScore;

    // get the legal moves and sort them
    std::array<Move, 256> moves;
    const int totalMoves = board.getMovesQSearch(moves);
    std::array<int, 256> moveValues;
    scoreMoves(board, moves, moveValues, totalMoves, entry.bestMove.getValue(), -1);

    // values useful for writing to TT later
    Move bestMove;
    int flag = FailLow;

    // loop though all the moves
    for(int i = 0; i < totalMoves; i++) {
        for (int j = i + 1; j < totalMoves; j++) {
            if (moveValues[j] > moveValues[i]) {
                std::swap(moveValues[j], moveValues[i]);
                std::swap(moves[j], moves[i]);
            }
        }
        if(!see(board, moves[i], 0)) {
            continue;
        }
        if(board.makeMove(moves[i])) {
            nodes++;
            // searches from this node
            const int score = -qSearch(board, -beta, -alpha, ply + 1);
            board.undoMove();
            // time check
            if(timesUp) return 0;

            if(score > bestScore) {
                bestScore = score;
                bestMove = moves[i];

                // Improve alpha
                if(score > alpha) {
                    flag = Exact;
                    alpha = score;
                }

                // Fail-high
                if(score >= beta) {
                    flag = BetaCutoff;
                    break;
                }
            }
        }
    }

    // push to TT
    TT.setEntry(board.zobristHash, Transposition(board.zobristHash, bestMove, flag, bestScore, 0));

    return bestScore;  
}

// adds to the history of a particular move
void addHistory(const int start, const int end, const int depth, const int colorToMove) {
    const int bonus = depth * depth;
    const int thingToAdd = bonus - historyTable[colorToMove][start][end] * std::abs(bonus) / historyCap;
    historyTable[colorToMove][start][end] += thingToAdd;
}

// The main search function
int negamax(Board &board, int depth, int alpha, int beta, int ply, bool nmpAllowed) {
    // if it's a repeated position, it's a draw
    if(ply > 0 && board.isRepeated) return 0;
    // time check every 4096 nodes
    if(nodes % 4096 == 0) {
        if(dataGeneration) {
            if(nodes > hardNodeCap) {
                timesUp = true;
                return 0;
            }
        } else {
            if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() > hardLimit) {
                timesUp = true;
                return 0;
            }
        }
    }

    // activate q search if at the end of a branch
    if(depth <= 0) return qSearch(board, alpha, beta, ply);
    const bool isPV = beta == alpha + 1;
    const bool inCheck = board.isInCheck();

    // TT check
    Transposition entry = TT.getEntry(board.zobristHash);

    // if it meets these criteria, it's done the search exactly the same way before, if not more throuroughly in the past and you can skip it
    if(ply > 0 && entry.zobristKey == board.zobristHash && entry.depth >= depth && (
            entry.flag == Exact // exact score
                || (entry.flag == BetaCutoff && entry.score >= beta) // lower bound, fail high
                || (entry.flag == FailLow && entry.score <= alpha) // upper bound, fail low
        )) {
        return entry.score;
    }

    // Reverse Futility Pruning
    const int eval = board.getEvaluation();
    if(eval - 80 * depth >= beta && !inCheck && depth < 9 && !isPV) return eval - 80 * depth;

    // nmp, "I could probably detect zugzwang here but ehhhhh" -Me, a few months ago
    // potential conditions to add: staticEval >= beta and !isPV, however they seem to be roughly equal after I tested them in the past. I could test it again soon but ehhh I'm a bit busy
    if(nmpAllowed && depth >= nmpMin && !inCheck && board.getEvaluation() >= beta) {
        board.changeColor();
        const int score = -negamax(board, depth - (depth+1)/3 - 2, 0-beta, 1-beta, ply + 1, false);
        board.undoChangeColor();
        if(score >= beta) {
            return score;
        }
    }

    // get the moves
    std::array<Move, 256> moves;
    const int totalMoves = board.getMoves(moves);
    std::array<int, 256> moveValues;
    scoreMoves(board, moves, moveValues, totalMoves, entry.bestMove.getValue(), ply);

    // values useful for writing to TT later
    int bestScore = mateScore;
    Move bestMove;
    int flag = FailLow;

    // extensions, currently only extending if you are in check
    int extensions = 0;
    if(inCheck) {
        extensions++;
    }

    // capturable squares to determine if a move is a capture.
    const uint64_t capturable = board.getOccupiedBitboard();
    // loop through the moves
    int legalMoves = 0;
    for(int i = 0; i < totalMoves; i++) {
        for (int j = i + 1; j < totalMoves; j++) {
            if (moveValues[j] > moveValues[i]) {
                std::swap(moveValues[j], moveValues[i]);
                std::swap(moves[j], moves[i]);
            }
        }
        bool isCapture = ((capturable & (1ULL << moves[i].getEndSquare())) != 0) || moves[i].getFlag() == EnPassant;
        // see pruning
        //                     This detects either quiet moves or bad captures
        if (!isPV && depth <= 8 && (moveValues[i] <= historyCap) && bestScore > mateScore + 256 && !see(board, moves[i], depth * (!isCapture ? -50 : -90))) continue;
        if(board.makeMove(moves[i])) {
            legalMoves++;
            nodes++;
            int score = 0;
            // Principal Variation Search
            if(legalMoves == 1) {
                // searches TT move at full depth, no reductions or anything, given first by the move ordering step.
                score = -negamax(board, depth + extensions - 1, -beta, -alpha, ply + 1, true);
            } else {
                // Late Move Reductions (LMR)
                int depthReduction = 0;
                if(extensions == 0 && depth > 1 && !isCapture) {
                    depthReduction = reductions[depth][legalMoves];
                }
                // this is more PVS stuff, searching with a reduced margin
                score = -negamax(board, depth + extensions - depthReduction - 1, -alpha - 1, -alpha, ply + 1, true);
                // and then if it fails high or low we search again with the original bounds
                if(score > alpha && (score < beta || depthReduction > 0)) {
                    score = -negamax(board, depth + extensions - 1, -beta, -alpha, ply + 1, true);
                }
            }
            board.undoMove();

            // backup time check
            if(timesUp) return 0;

            if(score > bestScore) {
                bestScore = score;
                bestMove = moves[i];
                if(ply == 0) rootBestMove = moves[i];

                // Improve alpha
                if(score > alpha) {
                    flag = Exact; 
                    alpha = score;
                }

                // Fail-high
                if(score >= beta) {
                    flag = BetaCutoff;
                    if(!isCapture) {
                        // adds to the move's history and adjusts the killer table accordingly
                        addHistory(moves[i].getStartSquare(), moves[i].getEndSquare(), depth, board.getColorToMove());
                        killerTable[ply][1] = killerTable[ply][0];
                        killerTable[ply][0] = moves[i].getValue();
                    }
                    break;
                }
            }
            // Late Move Pruning (not working, needs more testing)
            if(depth < 7 && !isPV && bestScore > mateScore + 256 && legalMoves > (3 + 2 * depth * depth)) break;
        }
    }

    // checkmate / stalemate detection, if I did legal move generation instead of pseudolegal I could probably do this first and it would be faster
    if(legalMoves == 0) {
        if(inCheck) {
            return mateScore + ply;
        }
        return 0;
    }

    // push to TT
    TT.setEntry(board.zobristHash, Transposition(board.zobristHash, bestMove, flag, bestScore, depth));

    return bestScore;
}

void outputInfo(const Board& board, int score, int depth, int elapsedTime) {
    std::string scoreString = " score ";
    if(abs(score) < abs(mateScore + 256)) {
        scoreString += "cp ";
        scoreString += std::to_string(score);
    } else {
        // score is checkmate in score - mateScore ply
        // position fen rn1q2rk/pp3p1p/2p4Q/3p4/7P/2NP2R1/PPP3P1/4RK2 w - - 0 1
        // ^^ mate in 3 test position
        scoreString += "mate ";
        scoreString += std::to_string(abs(score + mateScore) / 2 + board.getColorToMove());
    }
    std::cout << "info depth " << std::to_string(depth) << " seldepth " << std::to_string(seldepth) << " nodes " << std::to_string(nodes) << " time " << std::to_string(elapsedTime) << scoreString << " pv " << toLongAlgebraic(rootBestMove) << std::endl;
}

// yes cloning the board is intentional here.
// this crashes, which is why you only see me outputting the currently believed best move for the position
std::string getPV(Board board) {
    std::string pv = "";
    if(TT.matchZobrist(board.zobristHash)) {
        Move bestMove = TT.getBestMove(board.zobristHash);
        if(board.isLegalMove(bestMove) && board.makeMove(bestMove)) {
            std::string restOfPV = getPV(board);
            pv = toLongAlgebraic(bestMove) + " " + restOfPV;
        }
    }
    return pv;
}

// the usual think function, where you give it the amount of time it has left, and it will think in increasing depth steps until it runs out of time
Move think(Board board, int softBound, int hardBound, bool info) {
    //ageHistory();
    clearHistory();
    nodes = 0;
    hardLimit = hardBound;
    seldepth = 0;
    timesUp = false;

    begin = std::chrono::steady_clock::now();

    rootBestMove = Move();
    int score = 0;

    // Iterative Deepening, searches to increasing depths, which sounds like it would slow things down but it makes it much better
    for(int depth = 1; depth < 100; depth++) {
        // Aspiration Windows, searches with reduced bounds until it doesn't fail high or low
        seldepth = depth;
        timesUp = false;
        int delta = 25;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        const Move previousBest = rootBestMove;
        if(depth > 3) {
            while (true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                if(timesUp) {
                    return previousBest;
                }
                if (score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if (score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;

                delta *= 1.5;
            }
        } else {
            score = negamax(board, depth, mateScore, -mateScore, 0, true);
        }
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        if(timesUp) {
            return previousBest;
        }
        // outputs info which is picked up by the user
        if(info) outputInfo(board, score, depth, elapsedTime);
        // soft time bounds check
        if(elapsedTime > softBound) break;
    }

    return rootBestMove;
}

// searches done for bench, returns the number of nodes searched.
int benchSearch(Board board, int depthToSearch) {
    clearHistory();
    nodes = 0;
    hardLimit = 1215752192;
    seldepth = 0;
    timesUp = false;

    begin = std::chrono::steady_clock::now();

    rootBestMove = Move();
    int score = 0;
    // Iterative Deepening, searches to increasing depths, which sounds like it would slow things down but it makes it much better
    for(int depth = 1; depth <= depthToSearch; depth++) {
        // Aspiration Windows, searches with reduced bounds until it doesn't fail high or low
        seldepth = depth;
        int delta = 25;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        if(depth > 3) {
            while (true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                
                if (score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if (score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;

                delta *= 1.5;
            }
        } else {
            score = negamax(board, depth, mateScore, -mateScore, 0, true);
        }
        //const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        // outputs info which is picked up by the user
        //outputInfo(board, score, depth, elapsedTime);
    }
    return nodes;
}

// searches to a fixed depth when the user says go depth x
Move fixedDepthSearch(Board board, int depthToSearch, bool info) {
    //ageHistory();
    clearHistory();
    nodes = 0;
    seldepth = 0;
    hardLimit = 1215752192;
    begin = std::chrono::steady_clock::now();
    
    int score = 0;

    for(int depth = 1; depth <= depthToSearch; depth++) {
        seldepth = 0;
        timesUp = false;
        int delta = 25;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        if(depth > 3) {
            while (true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                
                if (score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if (score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;

                delta *= 1.5;
            }
        } else {
            score = negamax(board, depth, mateScore, -mateScore, 0, true);
        }
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        if(info) outputInfo(board, score, depth, elapsedTime);
    }
    return rootBestMove;
}

std::pair<Move, int> dataGenSearch(Board board, int nodeCap) {
    //std::cout << "recieved board with position " << board.getFenString() << std::endl;
    clearHistory();
    dataGeneration = true;
    nodes = 0;
    hardLimit = 1215752192;
    seldepth = 0;
    timesUp = false;

    begin = std::chrono::steady_clock::now();

    rootBestMove = Move();
    int score = 0;
    // Iterative Deepening, searches to increasing depths, which sounds like it would slow things down but it makes it much better
    for(int depth = 1; depth <= 100; depth++) {
        // Aspiration Windows, searches with reduced bounds until it doesn't fail high or low
        seldepth = depth;
        int delta = 25;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        if(depth > 3) {
            while (true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                
                if (score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if (score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;
                if(nodes > nodeCap) break;
                delta *= 1.5;
            }
        } else {
            score = negamax(board, depth, mateScore, -mateScore, 0, true);
        }
        //const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        // outputs info which is picked up by the user
        //outputInfo(board, score, depth, elapsedTime);
        if(nodes > nodeCap) break;
    }
    return std::pair<Move, int>(rootBestMove, score);
}
