/*
    Clarity
    Copyright (C) 2023 Joseph Pasfield

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

int nodes = 0;

#include "search.h"
#include "tt.h"
#include <cstring>

constexpr int hardNodeCap = 400000;

constexpr int historyCap = 16384;

int MVV_value[7] = {112, 351, 361, 627, 1187, 0, 0};
int SEE_value[7] = {112, 351, 361, 627, 1187, 0, 0};

// Tunable Values
int killerScore = 54000;

int goodCaptureBonus= 500000;
// The main search functions

// resets the history, done when ucinewgame is sent, and at the start of each turn
// thanks zzzzz
void Engine::clearHistory() {
    std::memset(historyTable.data(), 0, sizeof(historyTable));
    std::memset(noisyHistoryTable.data(), 0, sizeof(noisyHistoryTable));
    std::memset(conthistTable.get(), 0, sizeof(conthistTable));
}

Move Engine::getBestMove() {
    return rootBestMove;
}

// resets the engine, done when ucinewgame is sent
void Engine::resetEngine() {
    TT->clearTable();
    clearHistory(); 
}

int Engine::estimateMoveValue(const Board& board, const int end, const int flag) {
    // starting with the end square piece
    int value = SEE_value[getType(board.pieceAtIndex(end))];
    // promotions! pawn--, newpiece++
    for(int i = 0; i < 4; i++) {
        if(flag == promotions[i]) {
            value = SEE_value[i + 1] - SEE_value[Pawn];
            return value;
        }
    }

    // Target square is empty for en passant, but you still capture a pawn
    if(flag == EnPassant) {
        value = SEE_value[Pawn];
    }
    // castling can't capture and is never encoded as such so we don't care.
    return value;
}

bool Engine::see(const Board& board, Move move, int threshold) {
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
    balance -= SEE_value[nextVictim];
    // if it's still winning in the best case scenario, we can just cut it off
    if(balance >= 0) return true;
    // make sure occupied bitboard knows we did the first move
    uint64_t occupied = board.getOccupiedBitboard();
    occupied = (occupied ^ (1ULL << start)) | (1ULL << end);
    if(flag == EnPassant) occupied ^= (1ULL << board.getEnPassantIndex());
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
        if(myAttackers == 0ULL) break;
        // find lowest value attacker
        for(nextVictim = Pawn; nextVictim <= Queen; nextVictim++) {
            if((myAttackers & board.getColoredPieceBitboard(color, nextVictim)) != 0) {
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
        balance = -balance - 1 - SEE_value[nextVictim];
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
    2: Good Captures: Sorted by the victim piece value + value from capture history
    3: Killer moves: moves that are proven to be good from earlier, being indexed by ply
    4: History: scores of how many times a move has caused a beta cutoff
    5: Bad captures: captures that result in bad exchanges.
*/
void Engine::scoreMoves(const Board& board, std::array<Move, 256> &moves, std::array<int, 256> &values, int numMoves, Move ttMove, int ply) {
    const uint64_t occupied = board.getOccupiedBitboard();
    const int colorToMove = board.getColorToMove();
    for(int i = 0; i < numMoves; i++) {
        Move move = moves[i];
        const int end = move.getEndSquare();
        const int start = move.getStartSquare();
        const int piece = getType(board.pieceAtIndex(start));
        if(move == ttMove) {
            values[i] = 1000000000;
        } else if((occupied & (1ULL << end)) != 0) {
            // Captures!
            const int victim = getType(board.pieceAtIndex(end));
            // Capthist!
            values[i] = MVV_value[victim] + noisyHistoryTable[colorToMove][piece][end][victim];
            // see!
            // if the capture results in a good exchange then we can add a big boost to the score so that it's preferred over the quiet moves.
            if(see(board, move, 0)) {
                // good captures
                values[i] += goodCaptureBonus;
            }
        } else {
            // if not in qsearch, killers
            if(move == stack[ply].killer) {
                values[i] = killerScore;
            } else {
                // read from history
                values[i] = historyTable[colorToMove][start][end]
                    + (ply > 0 ? (*stack[ply - 1].ch_entry)[colorToMove][piece][end] : 0)
                    + (ply > 1 ? (*stack[ply - 2].ch_entry)[colorToMove][piece][end] : 0);
            }
        }
    }
}

void Engine::scoreMovesQS(const Board& board, std::array<Move, 256> &moves, std::array<int, 256> &values, int numMoves, Move ttMove) {
    const int colorToMove = board.getColorToMove();
    for(int i = 0; i < numMoves; i++) {
        Move move = moves[i];
        const int end = move.getEndSquare();
        const int start = move.getStartSquare();
        const int piece = getType(board.pieceAtIndex(start));
        if(move == ttMove) {
            values[i] = 1000000000;
        } else {
            // Captures!
            const int victim = getType(board.pieceAtIndex(end));
            // Capthist!
            values[i] = MVV_value[victim] + qsHistoryTable[colorToMove][piece][end][victim];
            // see!
            // if the capture results in a good exchange then we can add a big boost to the score so that it's preferred over the quiet moves.
            if(see(board, move, 0)) {
                // good captures
                values[i] += goodCaptureBonus;
            }
        }
    }
}

// ice4 deltas
//constexpr int deltas[] = {814, 139, 344, 403, 649, 867, 0};

// Quiecense search, searching all the captures until there aren't anymore so that you can get an accurate eval
int Engine::qSearch(Board &board, int alpha, int beta, int ply) {
    //if(board.isRepeatedPosition()) return 0;
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
    const uint64_t hash = board.getZobristHash();
    // TT check
    Transposition* entry = TT->getEntry(hash);

    if(entry->zobristKey == hash && (
        entry->flag == Exact // exact score
            || (entry->flag == BetaCutoff && entry->score >= beta) // lower bound, fail high
            || (entry->flag == FailLow && entry->score <= alpha) // upper bound, fail low
    )) {
        return entry->score;
    }

    // stand pat shenanigans
    int staticEval = board.getEvaluation();
    int bestScore = staticEval;
    if(bestScore >= beta) return bestScore;
    if(alpha < bestScore) alpha = bestScore;

    // get the legal moves and sort them
    std::array<Move, 256> moves;
    std::array<Move, 256> testedMoves;
    const int totalMoves = board.getMovesQSearch(moves);
    std::array<int, 256> moveValues;
    scoreMovesQS(board, moves, moveValues, totalMoves, entry->bestMove);

    // values useful for writing to TT later
    Move bestMove;
    int flag = FailLow;
    
    int legalMoves = 0;
    // loop though all the moves
    for(int i = 0; i < totalMoves; i++) {
        for(int j = i + 1; j < totalMoves; j++) {
            if(moveValues[j] > moveValues[i]) {
                std::swap(moveValues[j], moveValues[i]);
                std::swap(moves[j], moves[i]);
            }
        }
        Move move = moves[i];
        // this detects bad captures
        if(moveValues[i] < MVV_value[Queen] + historyCap) {
            continue;
        }
        // History Pruning
        //if(moveValues[i] < qhpDepthMultiplier.value * qDepth) break;
        if(!board.makeMove(move)) {
            continue;
        }
        testedMoves[legalMoves] = move;
        legalMoves++;
        nodes++;
        // searches from this node
        const int score = -qSearch(board, -beta, -alpha, ply + 1);
        board.undoMove();
        // time check
        if(timesUp) return 0;

        if(score > bestScore) {
            bestScore = score;

            // Improve alpha
            if(score > alpha) {
                flag = Exact;
                alpha = score;
                bestMove = move;
            }

            // Fail-high
            if(score >= beta) {
                flag = BetaCutoff;
                bestMove = move;
                // approximating depth using the distance from seldepth
                int qDepth = seldepth - ply;
                int bonus = std::min(qhsMaxBonus.value, qhsMultiplier.value * qDepth * qDepth + qhsAdder.value * qDepth - qhsSubtractor.value);
                // if it's a capture or queen promotion
                if(move.getFlag() < promotions[0] || move.getFlag() == promotions[3]) {
                    const int end = move.getEndSquare();
                    const int piece = getType(board.pieceAtIndex(move.getStartSquare()));
                    const int victim = getType(board.pieceAtIndex(end));
                    updateQSHistory(board.getColorToMove(), piece, end, victim, bonus);
                }
                bonus = -bonus;
                // malus!
                for(int moveNo = 0; moveNo < legalMoves - 1; moveNo++) {
                    Move maluMove = testedMoves[moveNo];
                    if(maluMove.getFlag() < promotions[0] || maluMove.getFlag() == promotions[3]) {
                        const int maluEnd = maluMove.getEndSquare();
                        const int maluPiece = getType(board.pieceAtIndex(maluMove.getStartSquare()));
                        const int maluVictim = getType(board.pieceAtIndex(maluEnd));
                        updateQSHistory(board.getColorToMove(), maluPiece, maluEnd, maluVictim, bonus);
                    }
                }
                break;
            }
        }
    }

    // push to TT
    TT->setEntry(hash, Transposition(hash, bestMove, flag, bestScore, 0));

    return bestScore;
}

// adds to the history of a particular move
void Engine::updateHistory(const int colorToMove, const int start, const int end, const int piece, const int bonus, const int ply) {
    int thingToAdd = bonus - historyTable[colorToMove][start][end] * std::abs(bonus) / historyCap;
    historyTable[colorToMove][start][end] += thingToAdd;
    if(ply > 0) {
        thingToAdd = bonus - (*stack[ply - 1].ch_entry)[colorToMove][piece][end] * std::abs(bonus) / historyCap;
        (*stack[ply - 1].ch_entry)[colorToMove][piece][end] += thingToAdd;
    }

    if(ply > 1) {
        thingToAdd = bonus - (*stack[ply - 2].ch_entry)[colorToMove][piece][end] * std::abs(bonus) / historyCap;
        (*stack[ply - 2].ch_entry)[colorToMove][piece][end] += thingToAdd;
    }
}

void Engine::updateNoisyHistory(const int colorToMove, const int piece, const int end, const int victim, const int bonus) {
    const int thingToAdd = bonus - noisyHistoryTable[colorToMove][piece][end][victim] * std::abs(bonus) / historyCap;
    noisyHistoryTable[colorToMove][piece][end][victim] += thingToAdd;
}

void Engine::updateQSHistory(const int colorToMove, const int piece, const int end, const int victim, const int bonus) {
    const int thingToAdd = bonus - qsHistoryTable[colorToMove][piece][end][victim] * std::abs(bonus) / historyCap;
    qsHistoryTable[colorToMove][piece][end][victim] += thingToAdd;
}

// The main search function
int Engine::negamax(Board &board, int depth, int alpha, int beta, int ply, bool nmpAllowed) {
    // if it's a repeated position, it's a draw
    if(ply > 0 && board.isRepeatedPosition()) return 0;
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
    const bool inSingularSearch = stack[ply].excluded != Move();
    const bool isPV = beta > alpha + 1;
    const bool inCheck = board.isInCheck();
    stack[ply].inCheck = inCheck;
    const uint64_t hash = board.getZobristHash();

    // TT check
    Transposition* entry = nullptr;
    
    if(!inSingularSearch) entry = TT->getEntry(hash);

    // if it meets these criteria, it's done the search exactly the same way before, if not more throuroughly in the past and you can skip it
    // it would make sense to add !isPV here, however from my testing that makes it about 80 elo worse
    // turns out that score above was complete bs lol, my isPV was broken
    if(!inSingularSearch && !isPV && ply > 0 && entry->zobristKey == hash && entry->depth >= depth && (
            entry->flag == Exact // exact score
                || (entry->flag == BetaCutoff && entry->score >= beta) // lower bound, fail high
                || (entry->flag == FailLow && entry->score <= alpha) // upper bound, fail low
        )) {
        return entry->score; 
    }

    // Internal Iterative Reduction (IIR)
    // Things to test: alternative depth
    if(!inSingularSearch && (entry->zobristKey != hash || entry->bestMove == Move()) && depth > iirDepthCondition.value) depth--;

    int staticEval = board.getEvaluation();
    if(ply > depthLimit - 1) return staticEval;
    stack[ply].staticEval = staticEval;
    const bool improving = (ply > 1 && !inCheck && staticEval > stack[ply - 2].staticEval && !stack[ply - 2].inCheck);

    // adjust staticEval to TT score if it's good enough
    if(!inCheck && !inSingularSearch && hash == entry->zobristKey && (
        entry->flag == Exact ||
        (entry->flag == BetaCutoff && entry->score >= staticEval) ||
        (entry->flag == FailLow && entry->score <= staticEval)
    )) {
        staticEval = entry->score;
    }

    // Razoring
    if(!inSingularSearch && !isPV && staticEval < alpha - razDepthMultiplier.value * depth) {
        int score = qSearch(board, alpha, beta, ply);
        if(score < alpha) {
            return score;
        }
    }

    // Reverse Futility Pruning
    if(!inSingularSearch && staticEval - rfpMultiplier.value * (depth - improving) >= beta && !inCheck && depth < rfpDepthCondition.value && !isPV) return staticEval;

    // Null Move Pruning (NMP)
    // Things to test: !isPV, alternate formulas, etc
    // "I could probably detect zugzwang here but ehhhhh" -Me, a few months ago
    if(!inSingularSearch && nmpAllowed && depth >= nmpDepthCondition.value && !inCheck && staticEval >= beta) {
        stack[ply].ch_entry = &(*conthistTable)[0][0][0];
        board.changeColor();
        const int score = -negamax(board, depth - 3 - depth / 3 - std::min((staticEval - beta) / int(nmpDivisor.value), int(nmpSubtractor.value)), 0-beta, 1-beta, ply + 1, false);
        board.undoChangeColor();
        if(score >= beta) {
            return score;
        }
    }

    // get the moves
    std::array<Move, 256> moves;
    std::array<Move, 256> testedMoves;
    int quietCount = 0;
    const int totalMoves = board.getMoves(moves);
    std::array<int, 256> moveValues;
    scoreMoves(board, moves, moveValues, totalMoves, inSingularSearch ? Move() : entry->bestMove, ply);

    // values useful for writing to TT later
    int bestScore = mateScore;
    Move bestMove;
    int flag = FailLow;

    // extensions, currently only extending if you are in check
    depth += inCheck;

    // Mate Distance Pruning (I will test it at some point I swear)
    if(!isPV) {
        // my mateScore is a large negative number and that is what I return, people seem to get confused by that when I talk with other devs.
        const auto mdAlpha = std::max(alpha, mateScore + ply);
        const auto mdBeta = std::min(beta, -mateScore - ply - 1);
        if(mdAlpha >= mdBeta) {
            return mdAlpha;
        }
    }
    // capturable squares to determine if a move is a capture.
    const uint64_t capturable = board.getOccupiedBitboard();
    // loop through the moves
    int legalMoves = 0;
    for(int i = 0; i < totalMoves; i++) {
        // gets the best move (according to move ordering) from the list, incrementally sorting it.
        for(int j = i + 1; j < totalMoves; j++) {
            if(moveValues[j] > moveValues[i]) {
                std::swap(moveValues[j], moveValues[i]);
                std::swap(moves[j], moves[i]);
            }
        }

        // information gathering
        Move move = moves[i];
        if(move == stack[ply].excluded) continue;
        int moveStartSquare = move.getStartSquare();
        int moveEndSquare = move.getEndSquare();
        int moveFlag = move.getFlag();
        bool isCapture = ((capturable & (1ULL << moveEndSquare)) != 0) || moveFlag == EnPassant;
        bool isQuiet = (!isCapture && (moveFlag <= DoublePawnPush));
        bool isQuietOrBadCapture = (moveValues[i] <= historyCap * 3);

        // move loop prunings:
        // futility pruning
        if(bestScore > mateScore && !inCheck && depth <= fpDepthCondition.value && staticEval + fpBase.value + depth * fpMultiplier.value <= alpha) break;
        // Late Move Pruning
        if(!isPV && isQuiet && bestScore > mateScore + 256 && quietCount > lmpBase.value + depth * depth / (2 - improving)) continue;
        // see pruning
        if(depth <= sprDepthCondition.value && isQuietOrBadCapture && bestScore > mateScore + 256 && !see(board, move, depth * (isCapture ? sprCaptureThreshold.value : sprQuietThreshold.value))) continue;
        // History Pruning
        if(ply > 0 && !isPV && isQuiet && depth <= hipDepthCondition.value && moveValues[i] < hipDepthMultiplier.value * depth) break;


        int TTExtensions = 0;
        // determine whether or not to extend TT move (Singular Extensions)
        if(!inSingularSearch && entry->bestMove == move && depth >= sinDepthCondition.value && entry->depth >= depth - sinDepthMargin.value && entry->flag != FailLow) {
            const auto sBeta = std::max(mateScore, entry->score - depth * int(sinDepthScale.value) / 16);
            const auto sDepth = (depth - 1) / 2;

            stack[ply].excluded = entry->bestMove;
            const auto score = negamax(board, sDepth, sBeta - 1, sBeta, ply, true);
            stack[ply].excluded = Move();
            if(score < sBeta) {
                if (!isPV && score < sBeta - dexMargin.value) {
                    TTExtensions = 2;
                } else {
                    TTExtensions = 1;
                }
            } else if(sBeta >= beta) {
                // multicut!
                return sBeta;
            } else if (entry->score >= beta) {
                // negative extensions!
                TTExtensions = -2;
            }
        }

        if(!board.makeMove(move)) {
            continue;
        }

        stack[ply].ch_entry = &(*conthistTable)[board.getColorToMove()][getType(board.pieceAtIndex(moveEndSquare))][moveEndSquare];\
        testedMoves[legalMoves] = move;
        legalMoves++;
        nodes++;
        if(isQuiet) quietCount++;
        int score = 0;
        // Principal Variation Search
        int presearchNodeCount = nodes;
        if(legalMoves == 1) {
            // searches the first move at full depth
            score = -negamax(board, depth - 1 + TTExtensions, -beta, -alpha, ply + 1, true);
        } else {
            // Late Move Reductions (LMR)
            int depthReduction = 0;
            if(!inCheck && depth > 1 && isQuietOrBadCapture) {
                depthReduction = reductions[depth][legalMoves];
                depthReduction -= isPV;
                if(isQuiet) {
                    depthReduction -= moveValues[i] / int(hmrDivisor.value);
                } else {
                    depthReduction -= moveValues[i] / int(cmrDivisor.value);
                }

                depthReduction = std::clamp(depthReduction, 0, depth - 2);
            }
            // this is more PVS stuff, searching with a reduced margin
            score = -negamax(board, depth - depthReduction - 1, -alpha - 1, -alpha, ply + 1, true);
            // and then if it fails high or low we search again with the original bounds
            if(score > alpha && (score < beta || depthReduction > 0)) {
                score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, true);
            }
        }
        board.undoMove();

        if(ply == 0) nodeTMTable[moveStartSquare][moveEndSquare] += nodes - presearchNodeCount;

        // backup time check
        if(timesUp) return 0;

        if(score > bestScore) {
            bestScore = score;

            // Improve alpha
            if(score > alpha) {
                flag = Exact; 
                alpha = score;
                bestMove = move;
                if(ply == 0) rootBestMove = move;
            }

            // Fail-high
            if(score >= beta) {
                flag = BetaCutoff;
                bestMove = move;
                if(ply == 0) rootBestMove = move;
                const int colorToMove = board.getColorToMove();
                // testing berserk history bonus
                int bonus = std::min(hstMaxBonus.value, hstMultiplier.value * depth * depth + hstAdder.value * depth - hstSubtractor.value);
                if(isQuiet) {
                    // adds to the move's history and adjusts the killer move accordingly
                    int start = moveStartSquare;
                    int end = moveEndSquare;
                    int piece = getType(board.pieceAtIndex(start));
                    updateHistory(colorToMove, start, end, piece, bonus, ply);
                    stack[ply].killer = move;
                } else if (move.getFlag() < promotions[0] || move.getFlag() == promotions[3]) {
                    const int end = move.getEndSquare();
                    const int piece = getType(board.pieceAtIndex(move.getStartSquare()));
                    const int victim = getType(board.pieceAtIndex(end));
                    updateNoisyHistory(board.getColorToMove(), piece, end, victim, bonus);
                }
                bonus = -bonus;
                // malus!
                for(int moveNo = 0; moveNo < legalMoves - 1; moveNo++) {
                    Move maluMove = testedMoves[moveNo];
                    const int start = maluMove.getStartSquare();
                    const int end = maluMove.getEndSquare();
                    const int flag = maluMove.getFlag();
                    const int piece = getType(board.pieceAtIndex(start));
                    bool maluIsCapture = ((capturable & (1ULL << end)) != 0) || flag == EnPassant;
                    bool maluIsQuiet = (!maluIsCapture && (flag <= DoublePawnPush));
                    if(maluIsQuiet) {
                        updateHistory(colorToMove, start, end, piece, bonus, ply);
                    } else if(maluMove.getFlag() < promotions[0] || maluMove.getFlag() == promotions[3]) {
                        const int victim = getType(board.pieceAtIndex(end));
                        updateNoisyHistory(colorToMove, piece, end, victim, bonus);
                    }
                }
                break;
            }
        }
    }

    // checkmate / stalemate detection, if I did legal move generation instead of pseudolegal I could probably do this first
    if(legalMoves == 0) {
        if(stack[ply].excluded != Move()) {
            return alpha;
        }
        if(inCheck) {
            return mateScore + ply;
        }
        return 0;
    }

    // push to TT
    if(!inSingularSearch) {
        if(entry->zobristKey == hash && entry->bestMove != Move() && bestMove == Move()) bestMove = entry->bestMove;
        TT->setEntry(hash, Transposition(hash, bestMove, flag, bestScore, depth));
    }

    return bestScore;
}

// gets the PV from the TT, has some inconsistencies or illegal moves, and will be replaced with a triangular PV table eventually
std::string Engine::getPV(Board board, std::vector<uint64_t> &hashVector, int numEntries) {
    std::string pv;
    const uint64_t hash = board.getZobristHash();
    for(int i = hashVector.size()-1; i > -1; i--) {
        if(hashVector[i] == hash) {
            // repitition, GET THAT OUTTA HERE
            return pv;
        }
    }
    hashVector.push_back(hash);
    numEntries++;
    Transposition* entry = TT->getEntry(hash);
    if(entry->zobristKey == hash && entry->flag == Exact) {
        Move bestMove = entry->bestMove;
        if(bestMove != Move() && board.makeMove(bestMove)) {
            std::string restOfPV = getPV(board, hashVector, numEntries);
            pv = toLongAlgebraic(bestMove) + " " + restOfPV;
        }
    }
    return pv;
}

void Engine::outputInfo(const Board& board, int score, int depth, int elapsedTime) {
    std::string scoreString = " score ";
    if(abs(score) < abs(mateScore + 256)) {
        scoreString += "cp ";
        scoreString += std::to_string(score);
    } else {
        // score is checkmate in score - mateScore ply
        // position fen rn1q2rk/pp3p1p/2p4Q/3p4/7P/2NP2R1/PPP3P1/4RK2 w - - 0 1
        // ^^ mate in 3 test position
        int colorMultiplier = score > 0 ? 1 : -1;
        scoreString += "mate ";
        scoreString += std::to_string((abs(abs(score) + mateScore) / 2 + board.getColorToMove()) * colorMultiplier);
    }
    if(depth > 6) {
        std::vector<uint64_t> hashVector;
        std::cout << "info depth " << std::to_string(depth) << " seldepth " << std::to_string(seldepth) << " nodes " << std::to_string(nodes) << " time " << std::to_string(elapsedTime) << " nps " << std::to_string(int(double(nodes) / (elapsedTime == 0 ? 1 : elapsedTime) * 1000)) << scoreString << " pv " << getPV(board, hashVector, 0) << std::endl;
    } else {
        std::cout << "info depth " << std::to_string(depth) << " seldepth " << std::to_string(seldepth) << " nodes " << std::to_string(nodes) << " time " << std::to_string(elapsedTime) << " nps " << std::to_string(int(double(nodes) / (elapsedTime == 0 ? 1 : elapsedTime) * 1000)) << scoreString << " pv " << toLongAlgebraic(rootBestMove) << std::endl;
    }
}

// the usual search function, where you give it the amount of time it has left, and it will search in increasing depth steps until it runs out of time
Move Engine::think(Board board, int softBound, int hardBound, bool info) {
    //ageHistory();
    //clearHistory();
    std::memset(nodeTMTable.data(), 0, sizeof(nodeTMTable));
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
        int delta = aspBaseDelta.value;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        const Move previousBest = rootBestMove;
        if(depth > aspDepthCondition.value) {
            while(true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                if(timesUp) {
                    return previousBest;
                } 
                if(score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if(score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;

                delta *= aspDeltaMultiplier.value;
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
        double frac = nodeTMTable[rootBestMove.getStartSquare()][rootBestMove.getEndSquare()] / static_cast<double>(nodes);
        if(elapsedTime >= softBound * (depth > ntmDepthCondition.value ? (ntmSubtractor.value - frac) * ntmMultiplier.value : ntmDefault.value)) break;
        //if(elapsedTime > softBound) break;
    }

    return rootBestMove;
}

// searches done for bench, returns the number of nodes searched.
int Engine::benchSearch(Board board, int depthToSearch) {
    //clearHistory();
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
        int delta = aspBaseDelta.value;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        if(depth > aspDepthCondition.value) {
            while(true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                
                if(score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if(score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;

                delta *= aspDeltaMultiplier.value;
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
Move Engine::fixedDepthSearch(Board board, int depthToSearch, bool info) {
    //ageHistory();
    //clearHistory();
    nodes = 0;
    seldepth = 0;
    hardLimit = 1215752192;
    begin = std::chrono::steady_clock::now();

    int score = 0;

    for(int depth = 1; depth <= depthToSearch; depth++) {
        seldepth = 0;
        timesUp = false;
        int delta = aspBaseDelta.value;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        if(depth > aspDepthCondition.value) {
            while(true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                
                if(score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if(score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;

                delta *= aspDeltaMultiplier.value;
            }
        } else {
            score = negamax(board, depth, mateScore, -mateScore, 0, true);
        }
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        if(info) outputInfo(board, score, depth, elapsedTime);
    }
    return rootBestMove;
}

std::pair<Move, int> Engine::dataGenSearch(Board board, int nodeCap) {
    //clearHistory();
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
        int delta = aspBaseDelta.value;
        int alpha = std::max(mateScore, score - delta);
        int beta = std::min(-mateScore, score + delta);
        if(depth > aspDepthCondition.value) {
            while(true) {
                score = negamax(board, depth, alpha, beta, 0, true);
                
                if(score >= beta) {
                    beta = std::min(beta + delta, -mateScore);
                } else if(score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, mateScore);
                } else break;
                if(nodes > nodeCap) break;
                delta *= aspDeltaMultiplier.value;
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