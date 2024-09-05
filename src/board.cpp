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
#include <cstdlib>

template void Board::makeMove<false>(Move move);
template void Board::undoMove<false>();
template void Board::makeMove<true>(Move move);
template void Board::undoMove<true>();
template void Board::addPiece<false>(int square, int type);
template void Board::removePiece<false>(int square, int type);
template void Board::movePiece<false>(int square1, int type1, int square2, int type2);
template void Board::addPiece<true>(int square, int type);
template void Board::removePiece<true>(int square, int type);
template void Board::movePiece<true>(int square1, int type1, int square2, int type2);

// zobrist hashing values 
std::array<std::array<uint64_t, 14>, 64> zobTable;
// if black is to move this value is xor'ed
uint64_t zobColorToMove;

// masks for caslting rights, used to update the castling rights faster after a rook or king move
constexpr std::array<uint8_t, 64> rookRightMasks = {
0b1101,255,255,255,255,255,255,0b1110,
255,   255,255,255,255,255,255,   255,
255,   255,255,255,255,255,255,   255,
255,   255,255,255,255,255,255,   255,
255,   255,255,255,255,255,255,   255,
255,   255,255,255,255,255,255,   255,
255,   255,255,255,255,255,255,   255,
0b0111,255,255,255,255,255,255,0b1011
};

constexpr std::array<uint8_t, 2> kingRightMasks = {
	0b0011, 0b1100
};

std::array<int, 8> directionalOffsets = {8, -8, 1, -1, 7, -7, 9, -9};

void Board::toString() {
    for(int rank = 7; rank >= 0; rank--) {
        for(int file = 0; file < 8; file++) {
            int piece = pieceAtIndex(rank * 8 + file);
            bool isBlack = getColor(piece) == 0;
            int pieceType = getType(piece);
            char pieceChar = ' ';
            switch(pieceType)
            {
                case Rook:
                    pieceChar = 'R';
                    break;
                case Knight:
                    pieceChar = 'N';
                    break;
                case Bishop:
                    pieceChar = 'B';
                    break;
                case Queen:
                    pieceChar = 'Q';
                    break;
                case King:
                    pieceChar = 'K';
                    break;
                case Pawn:
                    pieceChar = 'P';
                    break;
                default:
                    pieceChar = ' ';
                    break;
            }
            std::cout << static_cast<char>(isBlack ? tolower(pieceChar) : pieceChar) << " ";
        }
        std::cout << '\n';
    }
    std::cout << "Ply count: " << std::to_string(plyCount) << '\n';
    std::cout << "Hash: " << std::to_string(stateHistory.back().zobristHash) << '\n';
    std::cout << "Castling rights: " << std::to_string(stateHistory.back().castlingRights & 1) << std::to_string((stateHistory.back().castlingRights >> 1) & 1) << std::to_string((stateHistory.back().castlingRights >> 2) & 1) << std::to_string((stateHistory.back().castlingRights >> 3) & 1) << '\n';
    if(stateHistory.back().enPassantIndex == 64) {
        std::cout << "En passant square: none\n";
    } else {
        std::cout << "En passant square: " << squareNames[stateHistory.back().enPassantIndex] << '\n';
    }
    std::cout << "Color to move: " << (colorToMove == 0 ? "black" : "white") << '\n';
    std::cout << "Evaluation: " << std::to_string(getEvaluation()) << '\n';
}

Board::Board(std::string fen) {
    stateHistory.clear();
    stateHistory.reserve(256);
    stateHistory.push_back(BoardState());
    nnueState.reset();
    stateHistory.back().zobristHash = 0;
	stateHistory.back().pawnHash = 0;
    for(int i = 0; i < 64; i++) {
        stateHistory.back().mailbox[i] = None;
    }
    for(int i = 0; i < 6; i++) {
        stateHistory.back().pieceBitboards[i] = 0ULL;
    }
    for(int i = 0; i < 2; i++) {
        stateHistory.back().coloredBitboards[i] = 0ULL;
    }
    // main board state, segment 1
    std::vector<std::string> segments = split(fen, ' ');
    std::vector<std::string> ranks = split(segments[0], '/');
    std::ranges::reverse(ranks);
    int i = 0;
    for(const auto& rank : ranks) {
        for(char c : rank) {
            switch(c) {
                case 'p':
                    addPiece<false>(i, Pawn | Black);
                    i++;
                    break;
                case 'P':
                    addPiece<false>(i, Pawn | White);
                    i++;
                    break;
                case 'n':
                    addPiece<false>(i, Knight | Black);
                    i++;
                    break;
                case 'N':
                    addPiece<false>(i, Knight | White);
                    i++;
                    break;
                case 'b':
                    addPiece<false>(i, Bishop | Black);
                    i++;
                    break;
                case 'B':
                    addPiece<false>(i, Bishop | White);
                    i++;
                    break;
                case 'r':
                    addPiece<false>(i, Rook | Black);
                    i++;
                    break;
                case 'R':
                    addPiece<false>(i, Rook | White);
                    i++;
                    break;
                case 'q':
                    addPiece<false>(i, Queen | Black);
                    i++;
                    break;
                case 'Q':
                    addPiece<false>(i, Queen | White);
                    i++;
                    break;
                case 'k':
                    addPiece<false>(i, King | Black);
                    stateHistory.back().kingSquares[0] = i;
                    i++;
                    break;
                case 'K':
                    addPiece<false>(i, King | White);
                    stateHistory.back().kingSquares[1] = i;
                    i++;
                    break;
                default:
                    i += (c - '0');
                    break;
            }
        }
    }
    // convert color to move into 0 or 1, segment 2
    colorToMove = (segments[1] == "w" ? 1 : 0);
    if(colorToMove == 1) stateHistory.back().zobristHash ^= zobColorToMove;
    // decode the castling rights into the 4 bit number, segment 3
    stateHistory.back().castlingRights = 0;
    for(char c : segments[2]) {
        if(c == 'K') {
            stateHistory.back().castlingRights |= 1;
        } else if(c == 'Q') {
            stateHistory.back().castlingRights |= 2;
        } else if(c == 'k') {
            stateHistory.back().castlingRights |= 4;
        } else if(c == 'q') {
            stateHistory.back().castlingRights |= 8;
        }
    }
    // decode the en passant index, segment 4
    if(segments[3] != "-") {
        int i = 0;
        int num = 0;
        for(char c : segments[3]) {
            if(i == 0) {
                num += (c - 'a');
            } else {
                num += ((c - '1') * 8);
            }
            i++;
        }
        stateHistory.back().enPassantIndex = num;
    } else {
        stateHistory.back().enPassantIndex = 64;
    }
    // 50 move counter, segment 5
    stateHistory.back().hundredPlyCounter = 0;
    // ply count, segment 6
    plyCount = std::stoi(segments[5]) * 2 - colorToMove;
    nnueState.refreshAccumulator(0, stateHistory.back(), stateHistory.back().kingSquares[0]);
    nnueState.refreshAccumulator(1, stateHistory.back(), stateHistory.back().kingSquares[1]);
    stateHistory.back().threats = calculateThreats();
    stateHistory.back().checkers = getAttackers(stateHistory.back().kingSquares[colorToMove]);
}

std::string Board::getFenString() {
    // essentially copy and pasted from my c# engine lol
    std::string fen = "";
    for(int rank = 7; rank >= 0; rank--) {
        int numEmptyFiles = 0;
        for(int file = 0; file < 8; file++) {
            int piece = pieceAtIndex(8*rank+file);
            if(piece != None) {
                if(numEmptyFiles != 0) {
                    fen += std::to_string(numEmptyFiles);
                    numEmptyFiles = 0;
                }
                bool isBlack = getColor(piece) == 0;
                int pieceType = getType(piece);
                char pieceChar = ' ';
                switch(pieceType)
                {
                    case Rook:
                        pieceChar = 'R';
                        break;
                    case Knight:
                        pieceChar = 'N';
                        break;
                    case Bishop:
                        pieceChar = 'B';
                        break;
                    case Queen:
                        pieceChar = 'Q';
                        break;
                    case King:
                        pieceChar = 'K';
                        break;
                    case Pawn:
                        pieceChar = 'P';
                        break;
                }
                fen += isBlack ? tolower(pieceChar) : pieceChar;
            }
            else {
                numEmptyFiles++;
            }

        }
        if(numEmptyFiles != 0) {
            fen += std::to_string(numEmptyFiles);
        }
        if(rank != 0) {
            fen += '/';
        }
    }

    // color to move
    fen += ' ';
    fen += (colorToMove == 0 ? 'b' : 'w');
    // castling rights
    fen += ' ';
    bool thingAdded = false;
    if((stateHistory.back().castlingRights & 1) != 0) {
        fen += 'K'; 
        thingAdded = true;
    }
    if((stateHistory.back().castlingRights & 2) != 0) {
        fen += 'Q'; 
        thingAdded = true;
    }
    if((stateHistory.back().castlingRights & 4) != 0) {
        fen += 'k'; 
        thingAdded = true;
    }
    if((stateHistory.back().castlingRights & 8) != 0) {
        fen += 'q'; 
        thingAdded = true;
    }
    if(thingAdded == false) fen += '-';
    // en passant square
    fen += ' ';
    if(stateHistory.back().enPassantIndex == 64) {
        fen += '-';
    } else {
        fen += squareNames[stateHistory.back().enPassantIndex];
    }
    // 50 move counter
    fen += ' ';
    fen += std::to_string(stateHistory.back().hundredPlyCounter / 2);
    // ply count
    fen += ' ';
    fen += std::to_string(plyCount / 2 + colorToMove);
    return fen;
}

template <bool UpdateNNUE> void Board::addPiece(int square, int type) {
    assert(type != None);
    assert(square < 64);
    assert(pieceAtIndex(square) == None);
    assert(square >= 0);
    const uint64_t bitboardSquare = squareToBitboard[square];
    //std::cout << "Adding piece of type " << std::to_string(type) << " at index " << std::to_string(square) << '\n';
    stateHistory.back().coloredBitboards[getColor(type)] ^= bitboardSquare;
    stateHistory.back().pieceBitboards[getType(type)] ^= bitboardSquare;
    stateHistory.back().mailbox[square] = type;
    assert(pieceAtIndex(square) == type);
    if constexpr(UpdateNNUE) nnueState.activateFeature(square, type, stateHistory.back().kingSquares[0], stateHistory.back().kingSquares[1]);
    stateHistory.back().zobristHash ^= zobTable[square][type];
    if(getType(type) == Pawn) stateHistory.back().pawnHash ^= zobTable[square][type];
}

template <bool UpdateNNUE> void Board::removePiece(int square, int type) {
    assert(type != None);
    assert(square < 64);
    assert(pieceAtIndex(square) == type);
    assert(square >= 0);
    const uint64_t bitboardSquare = squareToBitboard[square];
    stateHistory.back().coloredBitboards[getColor(type)] ^= bitboardSquare;
    stateHistory.back().pieceBitboards[getType(type)] ^= bitboardSquare;
    stateHistory.back().mailbox[square] = None;
    if constexpr(UpdateNNUE) nnueState.disableFeature(square, type, stateHistory.back().kingSquares[0], stateHistory.back().kingSquares[1]);
    stateHistory.back().zobristHash ^= zobTable[square][type];
    if(getType(type) == Pawn) stateHistory.back().pawnHash ^= zobTable[square][type];
    assert(pieceAtIndex(square) == None);
}

template <bool UpdateNNUE> void Board::movePiece(int square1, int type1, int square2, int type2) {
    assert(type1 != None);
    assert(square1 < 64);
    assert(square2 < 64);
    if(type2 != None) removePiece<UpdateNNUE>(square2, type2);
    addPiece<UpdateNNUE>(square2, type1);
    removePiece<UpdateNNUE>(square1, type1);
}

int Board::pieceAtIndex(int index) const {
    return stateHistory.back().mailbox[index];
}
int Board::colorAtIndex(int index) const {
    return stateHistory.back().mailbox[index] == None ? 2 : getColor(stateHistory.back().mailbox[index]);
}

uint64_t Board::getColoredPieceBitboard(int color, int piece) const {
    return stateHistory.back().pieceBitboards[piece] & stateHistory.back().coloredBitboards[color];
}

uint64_t Board::getOccupiedBitboard() const {
    return stateHistory.back().coloredBitboards[0] | stateHistory.back().coloredBitboards[1];
}

// fills up the array and then returns the number of moves to be looped through later.
int Board::getMoves(std::array<Move, 256> &moves) {
    uint64_t occupiedBitboard = getOccupiedBitboard();
    int totalMoves = 0;
    // castling
    if((stateHistory.back().castlingRights & kingRightMasks[1 - colorToMove]) != 0) {
        if(!isInCheck()) {
            if(colorToMove == 1) {
                if((stateHistory.back().castlingRights & 1) != 0 && (occupiedBitboard & 0x60) == 0 && !squareIsUnderAttack(5)) {
                    moves[totalMoves] = Move(4, 6, castling[0]);
                    totalMoves++;
                }
                if((stateHistory.back().castlingRights & 2) != 0 && (occupiedBitboard & 0xE) == 0 && !squareIsUnderAttack(3)) {
                    moves[totalMoves] = Move(4, 2, castling[1]);
                    totalMoves++;
                }
            } else {
                if((stateHistory.back().castlingRights & 4) != 0 && (occupiedBitboard & 0x6000000000000000) == 0 && !squareIsUnderAttack(61)) {
                    moves[totalMoves] = Move(60, 62, castling[2]);
                    totalMoves++;
                }
                if((stateHistory.back().castlingRights & 8) != 0 && (occupiedBitboard & 0xE00000000000000) == 0 && !squareIsUnderAttack(59)) {
                    moves[totalMoves] = Move(60, 58, castling[3]);
                    totalMoves++; 
                }
            }
        }
    }
    uint64_t mask = stateHistory.back().coloredBitboards[colorToMove] ^ getColoredPieceBitboard(colorToMove, Pawn);
    // the rest of the pieces
    while(mask != 0) {
        uint8_t startSquare = popLSB(mask);
        uint8_t currentType = getType(pieceAtIndex(startSquare));
        uint64_t total = 0;
        if(currentType == Knight) {
            total = getKnightAttacks(startSquare);
        } else if(currentType == Bishop) {
            total = getBishopAttacks(startSquare, occupiedBitboard);
        } else if(currentType == Rook) {
            total = getRookAttacks(startSquare, occupiedBitboard);
        } else if(currentType == Queen) {
            total = getRookAttacks(startSquare, occupiedBitboard) | getBishopAttacks(startSquare, occupiedBitboard);
        } else if(currentType == King) {
            total = getKingAttacks(startSquare);
        }
        // get rid of capturing your own pieces
        total ^= (total & stateHistory.back().coloredBitboards[colorToMove]); 
        while(total != 0) {
            moves[totalMoves] = Move(startSquare, popLSB(total), Normal);
            totalMoves++;
        }
    }
    // pawn pushes
    uint64_t pawnBitboard = getColoredPieceBitboard(colorToMove, Pawn);
    uint64_t emptyBitboard = ~occupiedBitboard;
    uint64_t pawnPushes = getPawnPushes(pawnBitboard, emptyBitboard, colorToMove);
    uint64_t doublePawnPushes = getDoublePawnPushes(pawnPushes, emptyBitboard, colorToMove);
    uint64_t pawnPushPromotions = pawnPushes & getRankMask(7 * colorToMove);
    pawnPushes ^= pawnPushPromotions;
    while(pawnPushes != 0) {
        uint8_t index = popLSB(pawnPushes);
        uint8_t startSquare = (index + directionalOffsets[colorToMove]);
        moves[totalMoves] = Move(startSquare, index, Normal);
        totalMoves++;
    }
    while(doublePawnPushes != 0) {
        uint8_t index = popLSB(doublePawnPushes);
        uint8_t startSquare = (index + (directionalOffsets[colorToMove] * 2));
        assert(getType(pieceAtIndex(startSquare)) == Pawn);
        moves[totalMoves] = Move(startSquare, index, DoublePawnPush);
        totalMoves++;
    }
    while(pawnPushPromotions != 0) {
        uint8_t index = popLSB(pawnPushPromotions);
        uint8_t startSquare = (index + directionalOffsets[colorToMove]);
        for(int type = Knight; type < King; type++) {
            moves[totalMoves] = Move(startSquare, index, promotions[type-1]);
            totalMoves++;
        }
    }
    // pawn captures
    uint64_t capturable = stateHistory.back().coloredBitboards[1 - colorToMove];
    if(stateHistory.back().enPassantIndex != 64) {
        capturable |= squareToBitboard[stateHistory.back().enPassantIndex];
    }

    uint64_t leftCaptures = (colorToMove == 0 ? pawnBitboard >> 9 : pawnBitboard << 7);
    leftCaptures &= ~getFileMask(7);
    leftCaptures &= capturable;
    uint64_t leftCapturePromotions = leftCaptures & getRankMask(7 * colorToMove);
    leftCaptures ^= leftCapturePromotions;

    uint64_t rightCaptures = (colorToMove == 0 ? pawnBitboard >> 7 : pawnBitboard << 9);
    rightCaptures &= ~getFileMask(0);
    rightCaptures &= capturable;
    uint64_t rightCapturePromotions = rightCaptures & getRankMask(7 * colorToMove);
    rightCaptures ^= rightCapturePromotions;

    while(leftCaptures != 0) {
        int index = popLSB(leftCaptures);
        int startSquare = index + (colorToMove == 0 ? 9 : -7);
        moves[totalMoves] = Move(startSquare, index, (index == stateHistory.back().enPassantIndex ? EnPassant : Normal));
        totalMoves++;
    }
    while(rightCaptures != 0) {
        int index = popLSB(rightCaptures);
        int startSquare = index + (colorToMove == 0 ? 7 : -9);
        moves[totalMoves] = Move(startSquare, index, (index == stateHistory.back().enPassantIndex ? EnPassant : Normal));
        totalMoves++;
    }
    while(leftCapturePromotions != 0) {
        int index = popLSB(leftCapturePromotions);
        int startSquare = index + (colorToMove == 0 ? 9 : -7);
        for(int type = Knight; type < King; type++) {
            moves[totalMoves] = Move(startSquare, index, promotions[type-1]);
            totalMoves++;
        }
    }
    while(rightCapturePromotions != 0) {
        int index = popLSB(rightCapturePromotions);
        int startSquare = index + (colorToMove == 0 ? 7 : -9);
        for(int type = Knight; type < King; type++) {
            moves[totalMoves] = Move(startSquare, index, promotions[type-1]);
            totalMoves++;
        }
    }
    return totalMoves;
}

int Board::getMovesQSearch(std::array<Move, 256> &moves) {
    const uint64_t occupiedBitboard = getOccupiedBitboard();
    int totalMoves = 0;
    uint64_t mask = stateHistory.back().coloredBitboards[colorToMove] ^ getColoredPieceBitboard(colorToMove, Pawn);
    // the rest of the pieces
    while(mask != 0) {
        const uint8_t startSquare = popLSB(mask);
        const uint8_t currentType = getType(pieceAtIndex(startSquare));
        uint64_t total = 0;
        if(currentType == Knight) {
            total = getKnightAttacks(startSquare);
        } else if(currentType == Bishop) {
            total = getBishopAttacks(startSquare, occupiedBitboard);
        } else if(currentType == Rook) {
            total = getRookAttacks(startSquare, occupiedBitboard);
        } else if(currentType == Queen) {
            total = getRookAttacks(startSquare, occupiedBitboard) | getBishopAttacks(startSquare, occupiedBitboard);
        } else if(currentType == King) {
            total = getKingAttacks(startSquare);
        }
        // Only captures or (not done yet) checks
        total &= stateHistory.back().coloredBitboards[1 - colorToMove]; 
        while(total != 0) {
            moves[totalMoves] = Move(startSquare, popLSB(total), Normal);
            totalMoves++;
        }
    }
    // pawn push promotions
    const uint64_t pawnBitboard = getColoredPieceBitboard(colorToMove, Pawn);
    const uint64_t emptyBitboard = ~occupiedBitboard;
    const uint64_t pawnPushes = getPawnPushes(pawnBitboard, emptyBitboard, colorToMove);
    uint64_t pawnPushPromotions = pawnPushes & getRankMask(7 * colorToMove);
    while(pawnPushPromotions != 0) {
        const uint8_t index = popLSB(pawnPushPromotions);
        const uint8_t startSquare = (index + directionalOffsets[colorToMove]);
        for(int type = Knight; type < King; type++) {
            moves[totalMoves] = Move(startSquare, index, promotions[type-1]);
            totalMoves++;
        }
    }

    // captures
    uint64_t capturable = stateHistory.back().coloredBitboards[1 - colorToMove];
    if(stateHistory.back().enPassantIndex != 64) {
        capturable |= squareToBitboard[stateHistory.back().enPassantIndex];
    }

    uint64_t leftCaptures = (colorToMove == 0 ? pawnBitboard >> 9 : pawnBitboard << 7);
    leftCaptures &= ~getFileMask(7);
    leftCaptures &= capturable;
    uint64_t leftCapturePromotions = leftCaptures & getRankMask(7 * colorToMove);
    leftCaptures ^= leftCapturePromotions;

    uint64_t rightCaptures = (colorToMove == 0 ? pawnBitboard >> 7 : pawnBitboard << 9);
    rightCaptures &= ~getFileMask(0);
    rightCaptures &= capturable;
    uint64_t rightCapturePromotions = rightCaptures & getRankMask(7 * colorToMove);
    rightCaptures ^= rightCapturePromotions;

    while(leftCaptures != 0) {
        int index = popLSB(leftCaptures);
        int startSquare = index + (colorToMove == 0 ? 9 : -7);
        moves[totalMoves] = Move(startSquare, index, (index == stateHistory.back().enPassantIndex ? EnPassant : Normal));
        totalMoves++;
    }
    while(rightCaptures != 0) {
        int index = popLSB(rightCaptures);
        int startSquare = index + (colorToMove == 0 ? 7 : -9);
        moves[totalMoves] = Move(startSquare, index, (index == stateHistory.back().enPassantIndex ? EnPassant : Normal));
        totalMoves++;
    }
    while(leftCapturePromotions != 0) {
        int index = popLSB(leftCapturePromotions);
        int startSquare = index + (colorToMove == 0 ? 9 : -7);
        for(int type = Knight; type < King; type++) {
            moves[totalMoves] = Move(startSquare, index, promotions[type-1]);
            totalMoves++;
        }
    }
    while(rightCapturePromotions != 0) {
        int index = popLSB(rightCapturePromotions);
        int startSquare = index + (colorToMove == 0 ? 7 : -9);
        for(int type = Knight; type < King; type++) {
            moves[totalMoves] = Move(startSquare, index, promotions[type-1]);
            totalMoves++;
        }
    }
    return totalMoves;
}

uint8_t Board::getColorToMove() const {
    return colorToMove;
}

bool Board::isInCheck() {
    return squareIsUnderAttack(stateHistory.back().kingSquares[colorToMove]);
}

// thanks ciekce, shoutout stormphrax
bool Board::squareIsUnderAttack(int square) const {
    if(stateHistory.back().threats != 0) {
        return (stateHistory.back().threats & (1ULL << square));
    } else {
        const auto opponent = 1 - colorToMove;

        const auto queens = getColoredPieceBitboard(opponent, Queen);

        uint64_t mask = (getRookAttacks(square, getOccupiedBitboard()) & (queens | getColoredPieceBitboard(opponent, Rook)));
        if(mask != 0)
            return true;

        mask = (getBishopAttacks(square, getOccupiedBitboard()) & (queens | getColoredPieceBitboard(opponent, Bishop)));
        if(mask != 0)
            return true;

        mask = getKnightAttacks(square) & getColoredPieceBitboard(opponent, Knight);
        if(mask != 0)
            return true;

        mask = getPawnAttacks(square, colorToMove) & getColoredPieceBitboard(opponent, Pawn);
        if(mask != 0)
            return true;

        mask = getKingAttacks(square) & getColoredPieceBitboard(opponent, King);
        if(mask != 0)
            return true;

        return false;
    }
}

template <bool PushNNUE> void Board::makeMove(Move move) {
    //std::cout << "move " << toLongAlgebraic(move) << " on position " << getFenString() << std::endl;
    //std::cout << "makemove " << toLongAlgebraic(move) << std::endl;
    // push to vectors
    stateHistory.push_back(stateHistory.back());
    NetworkUpdates updates;
    stateHistory.back().threats = 0;

    // get information
    int start = move.getStartSquare();
    int end = move.getEndSquare();
    int movedPiece = pieceAtIndex(move.getStartSquare());
    int victim = pieceAtIndex(move.getEndSquare());
    int flag = move.getFlag();
    assert(movedPiece != None);
    bool isCapture = victim != None;
    int movedPieceType = getType(movedPiece);
    
    // hundred Ply (fifty move) counter
    stateHistory.back().hundredPlyCounter++;
    if(isCapture || movedPieceType == Pawn) {
        stateHistory.back().hundredPlyCounter = 0;
    }

    // king square updates
    if(movedPieceType == King) {
        if(refreshRequired(colorToMove, start, end)) updates.pushBucket(end, colorToMove);
        stateHistory.back().kingSquares[colorToMove] = end;
    }

    // actually make the move
    if(isCapture) {
        removePiece<false>(end, victim);
        updates.pushSub(end, victim);
    }
    removePiece<false>(start, movedPiece);
    updates.pushSub(start, movedPiece);
    if(flag < promotions[0]) {
        addPiece<false>(end, movedPiece);
        updates.pushAdd(end, movedPiece);
    }

    // En Passant
    stateHistory.back().enPassantIndex = 64;
    
    // castling rights updates!
    if((stateHistory.back().castlingRights & kingRightMasks[1 - colorToMove]) != 0) {
        switch(movedPieceType) {
            case Rook:
                stateHistory.back().castlingRights &= rookRightMasks[start];
                break;
            case King:
                stateHistory.back().castlingRights &= kingRightMasks[colorToMove];
                break;
            default:
                break;
        }
    }
    if(getType(victim) == Rook) {
        switch(end) {
            case 7:
                stateHistory.back().castlingRights &= rookRightMasks[end];
                break;
            case 0:
                stateHistory.back().castlingRights &= rookRightMasks[end];
                break;
            case 63:
                stateHistory.back().castlingRights &= rookRightMasks[end];
                break;
            case 56:
                stateHistory.back().castlingRights &= rookRightMasks[end];
                break;
            default:
                break;
        }
    }

    // edge cases!
    switch(flag) {
        // castling cases
        case castling[0]:
            assert(pieceAtIndex(7) != None);
            movePiece<false>(7, Rook | White, 5, None);
            updates.pushSub(7, Rook | White);
            updates.pushAdd(5, Rook | White);
            break;
        case castling[1]:
            assert(pieceAtIndex(0) != None);
            movePiece<false>(0, Rook | White, 3, None);
            updates.pushSub(0, Rook | White);
            updates.pushAdd(3, Rook | White);
            break;
        case castling[2]:
            assert(pieceAtIndex(63) != None);
            movePiece<false>(63, Rook | Black, 61, None);
            updates.pushSub(63, Rook | Black);
            updates.pushAdd(61, Rook | Black);
            break;
        case castling[3]:
            assert(pieceAtIndex(56) != None);
            movePiece<false>(56, Rook | Black, 59, None);
            updates.pushSub(56, Rook | Black);
            updates.pushAdd(59, Rook | Black);
            break;
        // double pawn push
        case DoublePawnPush:
            stateHistory.back().enPassantIndex = end + directionalOffsets[colorToMove];
            break;
        // en passant
        case EnPassant:
            assert(pieceAtIndex(move.getEndSquare() + directionalOffsets[colorToMove]) != None);
            removePiece<false>(end + directionalOffsets[colorToMove], Pawn | (8 * !colorToMove));
            updates.pushSub(end + directionalOffsets[colorToMove], Pawn | (8 * !colorToMove));
            break;
        // promotion cases
        case promotions[0]:
            addPiece<false>(end, Knight | (8 * colorToMove));
            updates.pushAdd(end, Knight | (8 * colorToMove));
            break;
        case promotions[1]:
            addPiece<false>(end, Bishop | (8 * colorToMove));
            updates.pushAdd(end, Bishop | (8 * colorToMove));
            break;
        case promotions[2]:
            addPiece<false>(end, Rook | (8 * colorToMove));
            updates.pushAdd(end, Rook | (8 * colorToMove));
            break;
        case promotions[3]:
            addPiece<false>(end, Queen | (8 * colorToMove));
            updates.pushAdd(end, Queen | (8 * colorToMove));
            break;
        default:
            break;
    }
    plyCount++;
    if constexpr(PushNNUE) {
        nnueState.performUpdatesAndPush(updates, stateHistory.back().kingSquares[0], stateHistory.back().kingSquares[1], stateHistory.back());
    } else {
        nnueState.performUpdates(updates, stateHistory.back().kingSquares[0], stateHistory.back().kingSquares[1], stateHistory.back());
    }
    colorToMove = 1 - colorToMove;
    stateHistory.back().threats = calculateThreats();
    stateHistory.back().checkers = getAttackers(stateHistory.back().kingSquares[colorToMove]);
    stateHistory.back().zobristHash ^= zobColorToMove;
}

template <bool PushNNUE> void Board::undoMove() {
    //std::cout << "undomove\n";
    stateHistory.pop_back();
    if constexpr(PushNNUE) nnueState.pop();
    plyCount--;
    colorToMove = 1 - colorToMove;
    //std::cout << "position fen " << getFenString() << std::endl;
    //std::cout << "Changing Color To Move in undo move\n";
    // no zobrist update here because the saved zobrist hash is before the color changed
}

uint64_t Board::getCurrentPlayerBitboard() const {
    return stateHistory.back().coloredBitboards[colorToMove];
}

void Board::changeColor() {
    stateHistory.push_back(stateHistory.back());
    nnueState.push();
    stateHistory.back().enPassantIndex = 64;
    stateHistory.back().hundredPlyCounter++;
    colorToMove = 1 - colorToMove;
    stateHistory.back().threats = calculateThreats();
    stateHistory.back().checkers = getAttackers(stateHistory.back().kingSquares[colorToMove]);
    stateHistory.back().zobristHash ^= zobColorToMove;
}

void Board::undoChangeColor() {
    stateHistory.pop_back();
    nnueState.pop();
    colorToMove = 1 - colorToMove;
}

int Board::getEvaluation() {   
    //std::cout << "position fen " << getFenString() << std::endl;
    //std::cout << "evaluate" << std::endl;
    int eval = int(double(nnueState.evaluate(colorToMove, __builtin_popcountll(getOccupiedBitboard()))));
    //nnueState.fullRefresh(stateHistory.back(), stateHistory.back().kingSquares[0], stateHistory.back().kingSquares[1]);
    //assert(eval == nnueState.evaluate(colorToMove, __builtin_popcountll(getOccupiedBitboard())));
    int phase =  3 * __builtin_popcountll(stateHistory.back().pieceBitboards[Knight])
               + 3 * __builtin_popcountll(stateHistory.back().pieceBitboards[Bishop])
               + 5 * __builtin_popcountll(stateHistory.back().pieceBitboards[Rook])
               + 10 * __builtin_popcountll(stateHistory.back().pieceBitboards[Queen]);    

    eval = eval * (206 + phase) / 256; 

    eval = eval * (200 - stateHistory.back().hundredPlyCounter) / 200;

    return eval;
}

int Board::getCastlingRights() const {
    return stateHistory.back().castlingRights;
}

int Board::getEnPassantIndex() const {
    return stateHistory.back().enPassantIndex;
}

void initializeZobrist() {
    // random number stuff
    //std::random_device rd;
    std::mt19937_64 gen(0xABBABA5ED);
    std::uniform_int_distribution<uint64_t> dis;
    // color to move
    zobColorToMove = dis(gen);
    // squares
    for(int i = 0; i < 64; i++) {
        for(int j = 0; j < 14; j++) {
            zobTable[i][j] = dis(gen);
        }
    }
}

uint64_t Board::fullZobristRegen() {
    //std::cout << "Beginning Full Regen\n";
    uint64_t mask = getOccupiedBitboard();
    uint64_t result = 0;
    while(mask != 0) {
        int index = popLSB(mask);
        int piece = pieceAtIndex(index);
        //std::cout << "Adding piece of type " << std::to_string(piece) << " at index " << std::to_string(index) << '\n';
        result ^= zobTable[index][piece];
    }
    if(colorToMove == 1) {
        //std::cout << "Changing Color To Move in full regen\n";
        result ^= zobColorToMove;
    }
    //std::cout << "Result: " << std::to_string(result) << '\n';
    return result;
}

bool Board::isRepeatedPosition() {
    int size = std::ssize(stateHistory);
    for(int i = size - 5; i >= size - stateHistory.back().hundredPlyCounter - 1; i -= 2) {
        if(stateHistory[i].zobristHash == stateHistory.back().zobristHash) {
            return true;
        }
    }
    return false;
}

bool Board::isLegal(const Move& move) {
    std::array<Move, 256> moves;
    const int totalMoves = getMoves(moves);
    for(int i = 0; i < totalMoves; i++) {
        if(moves[i] == move) return true;
    }
    return false;
}

uint64_t Board::getAttackers(int square) const {
    uint64_t occupied = getOccupiedBitboard();
    return (getPawnAttacks(square, 0) & getColoredPieceBitboard(1, Pawn))
        | (getPawnAttacks(square, 1) & getColoredPieceBitboard(0, Pawn))
        | (getKnightAttacks(square) & stateHistory.back().pieceBitboards[Knight])
        | (getRookAttacks(square, occupied) & (stateHistory.back().pieceBitboards[Rook] | stateHistory.back().pieceBitboards[Queen]))
        | (getBishopAttacks(square, occupied) & (stateHistory.back().pieceBitboards[Bishop] | stateHistory.back().pieceBitboards[Queen]))
        | (getKingAttacks(square) & stateHistory.back().pieceBitboards[King]);
}

uint64_t Board::getColoredBitboard(int color) const {
    return stateHistory.back().coloredBitboards[color];
}
uint64_t Board::getPieceBitboard(int piece) const {
    return stateHistory.back().pieceBitboards[piece];
}

int Board::getFiftyMoveCount() const {
    return stateHistory.back().hundredPlyCounter / 2;
}

uint64_t Board::getZobristHash() const {
    return stateHistory.back().zobristHash;
}

BoardState Board::getBoardState() const {
    return stateHistory.back();
}

Board::Board(BoardState s, int ctm) {
    stateHistory.push_back(s);
    colorToMove = ctm;
    stateHistory.back().hundredPlyCounter = 0;
    plyCount = 0;
    stateHistory.reserve(256);
}

// estimates the resulting zobrist key from a move
uint64_t Board::keyAfter(const Move move) const {
    const int startSquare = move.getStartSquare();
    const int endSquare = move.getEndSquare();
    
    const int moving = pieceAtIndex(startSquare);
    const int captured = pieceAtIndex(endSquare);

    uint64_t key = stateHistory.back().zobristHash;

    key ^= zobTable[startSquare][moving];
    key ^= zobTable[endSquare][moving];

    if (captured != None) {
        key ^= zobTable[endSquare][captured];
    }

    key ^= zobColorToMove;

    return key;
}

int Board::getPlyCount() const {
    return plyCount;
}

bool Board::isPKEndgame() const {
    uint64_t occupied = getOccupiedBitboard();
    uint64_t pk = getPieceBitboard(Pawn) | getPieceBitboard(King);
    return ((getColoredBitboard(colorToMove) & (occupied ^ pk)) == 0);
}

int Board::getPawnHashIndex() const {
	// last 15 bits
    return stateHistory.back().pawnHash & 0b111111111111111;
}

uint64_t Board::calculateThreats() {
    const int them = 1 - colorToMove;
    const uint64_t occupied = getOccupiedBitboard();

    uint64_t threats = 0;
    
    const uint64_t queens = getColoredPieceBitboard(them, Queen);

    uint64_t rooks = queens | getColoredPieceBitboard(them, Rook);
    while(rooks) {
        const int rook = popLSB(rooks);
        threats |= getRookAttacks(rook, occupied);
    }

    uint64_t bishops = queens | getColoredPieceBitboard(them, Bishop);
    while(bishops) {
        const int bishop = popLSB(bishops);
        threats |= getBishopAttacks(bishop, occupied);
    }

    uint64_t knights = getColoredPieceBitboard(them, Knight);
    while(knights) {
        const int knight = popLSB(knights);
        threats |= getKnightAttacks(knight);
    }

    const uint64_t pawns = getColoredPieceBitboard(them, Pawn);
    if(them == 0)
        threats |= ((pawns >> 9) & ~getFileMask(7)) | ((pawns >> 7) & ~getFileMask(0));
    else threats |= ((pawns << 7) & ~getFileMask(7)) | ((pawns << 9) & ~getFileMask(0));

    threats |= getKingAttacks(stateHistory.back().kingSquares[them]);

    return threats;
}

uint64_t Board::getThreats() const {
    return stateHistory.back().threats;
}