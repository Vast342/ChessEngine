#pragma once

#include "globals.h"
#include "tt.h"

constexpr int mateScore = -10000000;

extern int ASP_BaseDelta;
extern double ASP_DeltaMultiplier;
extern int ASP_DepthCondition;

extern int MVV_VictimScoreMultiplier;

extern int FirstKillerScore;
extern int SecondKillerScore;

extern int RFP_DepthCondition;
extern int RFP_Multiplier;

extern int IIR_DepthCondition;

extern int FP_DepthCondition;
extern int FP_Base;
extern int FP_Multiplier;

extern int LMP_DepthCondition;
extern int LMP_Base;

extern int SPR_DepthCondition;
extern int SPR_CaptureThreshold;
extern int SPR_QuietThreshold;

extern int NMP_Divisor; 
extern int NMP_Subtractor;

extern int badCaptureScore;

struct StackEntry {
    // conthist!
    CHEntry *ch_entry;
    // killer moves, 2 per ply
    std::array<Move, 2> killers;
    // static eval used for improving
    int staticEval;
    bool inCheck;
};

struct Engine {
    public: 
        void resizeTT(int newSize);
        void resetEngine();
        Move think(Board board, int softBound, int hardBound, bool info);
        int benchSearch(Board board, int depthToSearch);
        Move fixedDepthSearch(Board board, int depthToSearch, bool info);
        std::pair<Move, int> dataGenSearch(Board board, int nodeCap);
        bool timesUp = false;
    private:
        bool dataGeneration = false;

        Move rootBestMove = Move();

        int nodes = 0;

        int hardLimit = 0;

        int seldepth = 0;

        std::array<StackEntry, 120> stack;

        TranspositionTable TT;

        std::array<std::array<std::array<int, 64>, 64>, 2> historyTable;
        std::array<std::array<std::array<std::array<int, 6>, 64>, 6>, 2> captureHistoryTable;
        CHTable conthistTable;

        std::array<std::array<int, 64>, 64> nodeTMTable;

        std::chrono::steady_clock::time_point begin;
        void clearHistory();
        int estimateMoveValue(const Board& board, const int end, const int flag);
        bool see(const Board& board, Move move, int threshold);
        void scoreMoves(const Board& board, std::array<Move, 256> &moves, std::array<int, 256> &values, int numMoves, Move ttMove, int ply, bool inQSearch);
        int qSearch(Board &board, int alpha, int beta, int ply);
        void updateHistory(const int colorToMove, const int start, const int end, const int piece, const int bonus, const int ply);
        void updateCaptureHistory(const int colorToMove, const int piece, const int end, const int victim, const int bonus);
        int negamax(Board &board, int depth, int alpha, int beta, int ply, bool nmpAllowed);
        std::string getPV(Board board, std::vector<uint64_t> &hashVector, int numEntries);
        void outputInfo(const Board& board, int score, int depth, int elapsedTime);
};