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
#include "globals.h"
#include "immintrin.h"

#ifdef _MSC_VER
#define SP_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#define INCBIN_PREFIX g_
#include "external/incbin.h"

#ifdef SP_MSVC
#pragma pop_macro("_MSC_VER")
#undef SP_MSVC
#endif

namespace {
    INCBIN(network, NetworkFile);
    const Network *network = reinterpret_cast<const Network *>(g_networkData);
}

void NetworkState::reset() {
    currentAccumulator.initialize(network->featureBiases);
}

void Accumulator::initialize(std::span<const int16_t, layer1Size> bias) {
    std::copy(bias.begin(), bias.end(), black.begin());
    std::copy(bias.begin(), bias.end(), white.begin());
}

constexpr uint32_t ColorStride = 64 * 6;
constexpr uint32_t PieceStride = 64;
constexpr int Scale = 400;
constexpr int Qa = 255;
constexpr int Qb = 64;
constexpr int Qab = Qa * Qb;


std::pair<uint32_t, uint32_t> NetworkState::getFeatureIndices(int square, int type) {
    assert(type != None);
    assert(square != 64);

    const uint32_t pieceType = static_cast<uint32_t>(getType(type));
    const uint32_t color = getColor(type) == 1 ? 0 : 1;

    const uint32_t blackIdx = !color * ColorStride + pieceType * PieceStride + (static_cast<uint32_t>(flipIndex(square)));
    const uint32_t whiteIdx =  color * ColorStride + pieceType * PieceStride +  static_cast<uint32_t>(square)        ;

    return {blackIdx, whiteIdx};
}

/*
    A technique that I am using here was invented yesterday by SomeLizard, developer of the engine Lizard
    I am using the SCReLU activation function, which is CReLU(x)^2 * W
    the technique is to use CReLU(x) * w * CReLU(x) which allows you (assuming weight is in (-127, 127))
    to fit the resulting number in a 16 bit integer, allowing you to perform the remaining functions
    on twice as many numbers at once, leading to a pretty sizeable speedup
*/

#if defined(__AVX512F__) && defined(__AVX512BW__)
using Vector = __m512i;
constexpr int weightsPerVector = sizeof(Vector) / sizeof(int16_t);
// SCReLU!
int NetworkState::forward(const std::span<int16_t, layer1Size> us, const std::span<int16_t, layer1Size> them, const std::array<int16_t, layer1Size * 2> weights) {
    Vector sum = _mm512_setzero_si512();
    Vector vector0, vector1;

    for(int i = 0; i < layer1Size / weightsPerVector; ++i)
    {
        // us
        vector0 = _mm512_max_epi16(_mm512_min_epi16(_mm512_load_si512(reinterpret_cast<const Vector *>(&us[i * weightsPerVector])), _mm512_set1_epi16(Qa)), _mm512_setzero_si512());
        vector1 = _mm512_mullo_epi16(vector0, _mm512_load_si512(reinterpret_cast<const Vector *>(&weights[i * weightsPerVector])));
        vector1 = _mm512_madd_epi16(vector0, vector1);
        sum = _mm512_add_epi32(sum, vector1);
        
        // them
        vector0 = _mm512_max_epi16(_mm512_min_epi16(_mm512_load_si512(reinterpret_cast<const Vector *>(&them[i * weightsPerVector])), _mm512_set1_epi16(Qa)), _mm512_setzero_si512());
        vector1 = _mm512_mullo_epi16(vector0, _mm512_load_si512(reinterpret_cast<const Vector *>(&weights[layer1Size + i * weightsPerVector])));
        vector1 = _mm512_madd_epi16(vector0, vector1);
        sum = _mm512_add_epi32(sum, vector1);
    }

    return _mm512_reduce_add_epi32(sum);
}

#elif defined(__AVX2__)
using Vector = __m256i;
constexpr int weightsPerVector = sizeof(Vector) / sizeof(int16_t);
// SCReLU!
int NetworkState::forward(const std::span<int16_t, layer1Size> us, const std::span<int16_t, layer1Size> them, const std::array<int16_t, layer1Size * 2> weights) {
    Vector sum = _mm256_setzero_si256();
    Vector vector0, vector1;

    for(int i = 0; i < layer1Size / weightsPerVector; ++i)
    {
        // us
        vector0 = _mm256_max_epi16(_mm256_min_epi16(_mm256_load_si256(reinterpret_cast<const Vector *>(&us[i * weightsPerVector])), _mm256_set1_epi16(Qa)), _mm256_setzero_si256());
        vector1 = _mm256_mullo_epi16(vector0, _mm256_load_si256(reinterpret_cast<const Vector *>(&weights[i * weightsPerVector])));
        vector1 = _mm256_madd_epi16(vector0, vector1);
        sum = _mm256_add_epi32(sum, vector1);
        
        // them
        vector0 = _mm256_max_epi16(_mm256_min_epi16(_mm256_load_si256(reinterpret_cast<const Vector *>(&them[i * weightsPerVector])), _mm256_set1_epi16(Qa)), _mm256_setzero_si256());
        vector1 = _mm256_mullo_epi16(vector0, _mm256_load_si256(reinterpret_cast<const Vector *>(&weights[layer1Size + i * weightsPerVector])));
        vector1 = _mm256_madd_epi16(vector0, vector1);
        sum = _mm256_add_epi32(sum, vector1);
    }

    __m128i sum0;
    __m128i sum1;
    // divide into halves
    sum0 = _mm256_castsi256_si128(sum);
    sum1 = _mm256_extracti128_si256(sum, 1);
    // add the halves
    sum0 = _mm_add_epi32(sum0, sum1);
    // get half of the result
    sum1 = _mm_unpackhi_epi64(sum0, sum0);
    // add the halves:
    sum0 = _mm_add_epi32(sum0, sum1);
    // reorder so it's right
    sum1 = _mm_shuffle_epi32(sum0, _MM_SHUFFLE(2, 3, 0, 1));
    // final add
    sum0 = _mm_add_epi32(sum0, sum1);
    // cast back to int
    return _mm_cvtsi128_si32(sum0);
}
#else
using Vector = __m128i;
constexpr int weightsPerVector = sizeof(Vector) / sizeof(int16_t);
// SCReLU!
int NetworkState::forward(const std::span<int16_t, layer1Size> us, const std::span<int16_t, layer1Size> them, const std::array<int16_t, layer1Size * 2> weights) {
    Vector sum = _mm_setzero_si128();
    Vector vector0, vector1;

    for(int i = 0; i < layer1Size / weightsPerVector; ++i)
    {
        // us
        vector0 = _mm_max_epi16(_mm_min_epi16(_mm_load_si128(reinterpret_cast<const Vector *>(&us[i * weightsPerVector])), _mm_set1_epi16(Qa)), _mm_setzero_si128());
        vector1 = _mm_mullo_epi16(vector0, _mm_load_si128(reinterpret_cast<const Vector *>(&weights[i * weightsPerVector])));
        vector1 = _mm_madd_epi16(vector0, vector1);
        sum = _mm_add_epi32(sum, vector1);
        
        // them
        vector0 = _mm_max_epi16(_mm_min_epi16(_mm_load_si128(reinterpret_cast<const Vector *>(&them[i * weightsPerVector])), _mm_set1_epi16(Qa)), _mm_setzero_si128());
        vector1 = _mm_mullo_epi16(vector0, _mm_load_si128(reinterpret_cast<const Vector *>(&weights[layer1Size + i * weightsPerVector])));
        vector1 = _mm_madd_epi16(vector0, vector1);
        sum = _mm_add_epi32(sum, vector1);
    }

    const auto high64 = _mm_unpackhi_epi64(sum, sum);
    const auto sum64 = _mm_add_epi32(sum, high64);

    const auto high32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
    const auto sum32 = _mm_add_epi32(sum64, high32);

    return _mm_cvtsi128_si32(sum32);
}

#endif
void NetworkState::activateFeature(int square, int type){ 
    const auto [blackIdx, whiteIdx] = getFeatureIndices(square, type);

    // change values for all of them
    for(int i = 0; i < layer1Size; ++i) {
        currentAccumulator.black[i] += network->featureWeights[blackIdx * layer1Size + i];
        currentAccumulator.white[i] += network->featureWeights[whiteIdx * layer1Size + i];
    }
}

void NetworkState::disableFeature(int square, int type) {
    const auto [blackIdx, whiteIdx] = getFeatureIndices(square, type);

    // change values for all of them
    for(int i = 0; i < layer1Size; ++i) {
        currentAccumulator.black[i] -= network->featureWeights[blackIdx * layer1Size + i];
        currentAccumulator.white[i] -= network->featureWeights[whiteIdx * layer1Size + i];
    }
}

int NetworkState::evaluate(int colorToMove) {
    const auto output = colorToMove == 0 ? forward(currentAccumulator.black, currentAccumulator.white, network->outputWeights) : forward(currentAccumulator.white, currentAccumulator.black, network->outputWeights);
    return (output / Qa + network->outputBias) * Scale / Qab;
}
