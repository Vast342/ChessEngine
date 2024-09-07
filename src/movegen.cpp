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
#include "masks.h"
#include "slidey.h"

// this is getting the attacks from either pext or magic bitboard move generation, which is decided by which build is being done
uint64_t getRookAttacks(int square, uint64_t occupiedBitboard) {
    assert(square < 64);
    return getRookAttacksFromTable(occupiedBitboard, square);
}
uint64_t getBishopAttacks(int square, uint64_t occupiedBitboard) {
    assert(square < 64);
    return getBishopAttacksFromTable(occupiedBitboard, square);
}
// pawn pushes
uint64_t getPawnPushes(uint64_t pawnBitboard, uint64_t emptyBitboard, int colorToMove) {
	return (colorToMove == 0 ? pawnBitboard >> 8 : pawnBitboard << 8) & emptyBitboard;
}
uint64_t getDoublePawnPushes(uint64_t pawnAttacks, uint64_t emptyBitboard, int colorToMove) {
	pawnAttacks &= getRankMask(colorToMove == 1 ? 2 : 5);
	return (colorToMove == 0 ? pawnAttacks >> 8 : pawnAttacks << 8) & emptyBitboard;
}
// using the lookups from Homura
uint64_t getPawnAttacks(int square, int colorToMove) {
    return pawnAttacks[colorToMove][square];
}
uint64_t getKnightAttacks(int square) {
    return knightAttacks[square];
}
uint64_t getKingAttacks(int square) {
    return kingAttacks[square];
}
// using my own lookups, cool
uint64_t getPassedPawnMask(int square, int colorToMove) {
    return passedPawnMasks[colorToMove][square];
}
