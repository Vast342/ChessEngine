#include "globals.h"
#include "masks.h"

namespace rays {
	consteval std::array<std::array<uint64_t, 64>, 64> generateBetweenRays() {
		std::array<std::array<uint64_t, 64>, 64> result{};

		for(int from = 0; from < 64; from++) {
			const uint64_t fromMask = 1ULL << from;

			const uint64_t rookAttacks = rookMasks[from];
			const uint64_t bishopAttacks = bishopMasks[from];

			for(int to = 0; to < 64; to++) {
				if(from == to) continue;
				const uint64_t toMask = 1ULL << to;

				if((rookAttacks & toMask) != 0) {
					result[from][to] = getRookAttacksOld(from, toMask) & getRookAttacksOld(to, fromMask);
                } else if((bishopAttacks & toMask) != 0) {
					result[from][to] = getBishopAttacksOld(from, toMask) & getBishopAttacksOld(to, fromMask);
                }
			}
		}

		return result;
	}

	consteval std::array<std::array<uint64_t, 64>, 64> generateIntersectingRays() {
        std::array<std::array<uint64_t, 64>, 64> result{};

		for(int from = 0; from < 64; from++) {
			const uint64_t fromMask = 1ULL << from;

			const uint64_t rookAttacks = rookMasks[from];
			const uint64_t bishopAttacks = bishopMasks[from];

			for(int to = 0; to < 64; to++) {
				if(from == to) continue;
				const uint64_t toMask = 1ULL << to;

				if((rookAttacks & toMask) != 0) {
					result[from][to] = (fromMask | getRookAttacksOld(from, 0)) & (toMask | getRookAttacksOld(to, 0));
                } else if((bishopAttacks & toMask) != 0) {
					result[from][to] = (fromMask | getBishopAttacksOld(from, 0)) & (toMask | getBishopAttacksOld(to, 0));
                }
			}
		}

		return result;
	}

	constexpr std::array<std::array<uint64_t, 64>, 64> BetweenRays = generateBetweenRays();
	constexpr std::array<std::array<uint64_t, 64>, 64> IntersectingRays = generateIntersectingRays();

	constexpr auto rayBetween(int src, int dst)
	{
		return BetweenRays[src][dst];
	}

	constexpr auto rayIntersecting(int src, int dst)
	{
		return IntersectingRays[src][dst];
	}
}