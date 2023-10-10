#include "globals.h"

int Move::getValue() {
    return value;
}

int Move::getStartSquare() {
    return value & 0b111111;
}

int Move::getEndSquare() {
    return (value >> 6) & 0b111111;
}

int Move::getFlag() {
    return value >> 12;
}

Move::Move(int startSquare, int endSquare, int flag) {
    assert(startSquare < 64);
    assert(endSquare < 64);
    assert(flag < 0b1011);
    value = ((flag << 12) | (endSquare << 6) | startSquare);
}

Move::Move() {
    value = 0;
}

// long algebraic form constructor for uci shenanigans.
Move::Move(std::string longAlgebraic, const Board& board) {
    // the old code here was, special to say the least
    int startSquare = 0;
    int endSquare = 0;
    // read from the squares
    startSquare += longAlgebraic[0] - 'a';
    startSquare += (longAlgebraic[1] - '1') * 8;
    endSquare += longAlgebraic[2] - 'a';
    endSquare += (longAlgebraic[3] - '1') * 8;
    // flags
    int flag = 0;
    if(longAlgebraic.length() > 4) {
        // promotions
        switch(longAlgebraic[4]) {
            case 'n':
                flag = promotions[0];
                break;
            case 'b':
                flag = promotions[1];
                break;
            case 'r':
                flag = promotions[2];
                break;
            case 'q':
                flag = promotions[3];
                break;
            default:
                std::cout << "invalid promotion type\n";
                assert(false);
        }
    } else {
        // other flags, 0 is normal so if it's a normal move leave it at zero
        int castlingRights = board.getCastlingRights();
        if(endSquare == board.getEnPassantIndex() && getType(board.pieceAtIndex(startSquare)) == Pawn) {
            flag = EnPassant;
        } else if((castlingRights & 1) != 0 && longAlgebraic == "e1g1") {
            flag = castling[0];
        } else if((castlingRights & 2) != 0 && longAlgebraic == "e1c1") {
            flag = castling[1];
        } else if((castlingRights & 4) != 0 && longAlgebraic == "e8g8") {
            flag = castling[2];
        } else if((castlingRights & 8) != 0 && longAlgebraic == "e8c8") {
            flag = castling[3];
        } else if(getType(board.pieceAtIndex(startSquare)) == Pawn && abs(startSquare - endSquare) == 16) {
            flag = DoublePawnPush;
        }
    }
    value = ((flag << 12) | (endSquare << 6) | startSquare);
}