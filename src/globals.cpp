#include "globals.h"
#include "slidey.h"
#include "psqt.h"

// takes a piece number and gets the type of it
int getType(int value) {
    return value & 7;
}
// takes a piece number and gets the color of it
int getColor(int value) {
    return value >> 3;
}
// a mask for a single file on the board
uint64_t fileMask = 0b100000001000000010000000100000001000000010000000100000001;
// a mask for a single rank on the board
uint64_t rankMask = 0b11111111;

// gets a mask for either a rank or file.
uint64_t getFileMask(int file) {
    return fileMask << file;
}
uint64_t getRankMask(int rank) {
    return rankMask << (8 * rank);
}

// finds the least significant bit in the uint64_t, gets rid of it, and returns its index
int popLSB(uint64_t &bitboard) {
    int lsb = std::countr_zero(bitboard);
    bitboard &= bitboard - 1;
    return lsb;
}

// converts a move to long algebraic form
std::string toLongAlgebraic(Move move) {
    std::string longAlgebraic = "";
    longAlgebraic += squareNames[move.getStartSquare()];
    longAlgebraic += squareNames[move.getEndSquare()];
    switch(move.getFlag()) {
        case promotions[0]:
            longAlgebraic += 'n';
            break;
        case promotions[1]:
            longAlgebraic += 'b';
            break;
        case promotions[2]:
            longAlgebraic += 'r';
            break;
        case promotions[3]:
            longAlgebraic += 'q';
            break;
        default:
            break;
    }
    return longAlgebraic;
}

// calculates the reductions used for LMR, ran on startup
std::array<std::array<uint8_t, 218>, 50> reductions;

void calculateReductions() {
    for(int depth = 0; depth < 50; depth++) {
        for(int move = 0; move < 218; move++) {
            reductions[depth][move] = uint8_t(0.77 + log(depth) * log(move) * 0.42);
        }
    }
}

/*ran on startup, does 4 things:
1: generates the lookups for sliding pieces
2: adds material values to the psqts to save time later
3: generates the numbers used for zobrist hashing
4: calculates the numbers for LMR
 */
void initialize() {
    generateLookups();
    computePSQTs();
    initializeZobrist();
    calculateReductions();
}

// splits a string into segments based on the seperator
std::vector<std::string> split(const std::string string, const char seperator) {
    std::stringstream stream(string);
    std::string segment;
    std::vector<std::string> list;

    // every time that it can get a segment
    while (std::getline(stream, segment, seperator)) {
        // add it to the vector
        list.push_back(segment);
    }
    
    return list;
}

// sorts the moves all at once
void sortMoves(std::array<int, 256> &values, std::array<Move, 256> &moves, int numMoves) {
    int lowestIndex;
 
    // for each value
    for(int i = 0; i < numMoves - 1; i++) {
 
        // find the lowest number that hasn't been sorted yet
        lowestIndex = i;
        for(int j = i + 1; j < numMoves; j++) {
            if (values[j] < values[lowestIndex])
                lowestIndex = j;
        }
 
        // swap the elements
        if(lowestIndex != i) {
            std::swap(values[lowestIndex], values[i]);
            std::swap(moves[lowestIndex], moves[i]);
        }
    }
}

// flips the index vertically, used for indexing the psqts
int flipIndex(int index) {
    return index ^ 56;
}

// thanks z5
// also I completely misunderstood how this was supposed to work, this isn't it lol
void incrementalSort(std::array<int, 256> &values, std::array<Move, 256> &moves, int numMoves, int i) {
    for (int j = i + 1; j < numMoves; j++) {
        if (values[j] > values[i]) {
            std::swap(values[j], values[i]);
            std::swap(moves[j], moves[i]);
        }
    }
}