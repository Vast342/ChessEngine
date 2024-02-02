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
#pragma once

#include "globals.h"

constexpr int inputSize = 768;
constexpr int layer1Size = 512;

// organizing this somewhat similarly to code I've seen, mostly from clarity_sp_nnue, made by Ciekce.

struct Network {
    alignas(32) std::array<std::int16_t, inputSize * layer1Size> featureWeights;
    alignas(32) std::array<std::int16_t, layer1Size> featureBiases;
    alignas(32) std::array<std::int16_t, layer1Size * 2> outputWeights;
    std::int16_t outputBias;
};

struct Accumulator {
    alignas(32) std::array<std::int16_t, layer1Size> black;
    alignas(32) std::array<std::int16_t, layer1Size> white;
    void initialize(std::span<const std::int16_t, layer1Size> bias);
};

class NetworkState {
    public:
        void reset();
        void activateFeature(int square, int type);
        void disableFeature(int square, int type);
        int evaluate(int colorToMove);
    private:
        Accumulator currentAccumulator;
        static std::pair<uint32_t, uint32_t> getFeatureIndices(int square, int type);
        int forward(const std::span<std::int16_t, layer1Size> us, const std::span<std::int16_t, layer1Size> them, const std::array<std::int16_t, layer1Size * 2> weights);
};
