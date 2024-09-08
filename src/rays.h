#include "globals.h"
#include "masks.h"	

inline std::array<std::array<uint64_t, 64>, 64> generateBetweenRays() {
	std::array<std::array<uint64_t, 64>, 64> result{};

	for(int from = 0; from < 64; from++) {
		const uint64_t fromMask = 1ULL << from;

		const uint64_t rookAttacks = getRookAttacks(from, 0);
		const uint64_t bishopAttacks = getBishopAttacks(from, 0);

		for(int to = 0; to < 64; to++) {
			if(from == to) continue;
			const uint64_t toMask = 1ULL << to;

			if((rookAttacks & toMask) != 0) {
				result[from][to] = getRookAttacks(from, toMask) & getRookAttacks(to, fromMask);
			} else if((bishopAttacks & toMask) != 0) {
				result[from][to] = getBishopAttacks(from, toMask) & getBishopAttacks(to, fromMask);
			}
		}
	}

	return result;
}

inline std::array<std::array<uint64_t, 64>, 64> generateIntersectingRays() {
	std::array<std::array<uint64_t, 64>, 64> result{};

	for(int from = 0; from < 64; from++) {
		const uint64_t fromMask = 1ULL << from;

		const uint64_t rookAttacks = getRookAttacks(from, 0);
		const uint64_t bishopAttacks = getBishopAttacks(from, 0);

		for(int to = 0; to < 64; to++) {
			if(from == to) continue;
			const uint64_t toMask = 1ULL << to;

			if((rookAttacks & toMask) != 0) {
				result[from][to] = (fromMask | getRookAttacks(from, 0)) & (toMask | getRookAttacks(to, 0));
			} else if((bishopAttacks & toMask) != 0) {
				result[from][to] = (fromMask | getBishopAttacks(from, 0)) & (toMask | getBishopAttacks(to, 0));
			}
		}
	}

	return result;
}

inline std::array<std::array<uint64_t, 64>, 64> BetweenRays;
inline std::array<std::array<uint64_t, 64>, 64> IntersectingRays;

inline auto rayBetween(int src, int dst) {
	return BetweenRays[src][dst];
}

inline auto rayIntersecting(int src, int dst) {
	return IntersectingRays[src][dst];
}