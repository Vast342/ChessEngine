namespace Chess {
    public static class Piece {
        public const byte None = 0;
        public const byte Pawn = 1;
        public const byte Knight = 2;
        public const byte Bishop = 3;
        public const byte Rook = 4;
        public const byte Queen = 5;
        public const byte King = 6;

        public const byte White = 8;
        public const byte Black = 0;
    }
    public class Board {
        // board specific valus
        public byte[] squares = new byte[64];
        public byte[] kingSquares = new byte[2];
        public int colorToMove = 1;
        public ulong occupiedBitboard = 0;
        // bitboards of the entire color, 0 is black 1 is white
        public ulong[] coloredBitboards = new ulong[2];
        // bitboards of specific pieces of specific color, using the numbers above.
        public ulong[,] coloredPieceBitboards = new ulong[2,6];
        // the index of the en-passantable square, from 0-63, and if it's -1 no en passant is legal.
        public int enPassantIndex = 0;
        // white kingside, white queenside, black kingside, black kingside
        public bool[] castlingRights = new bool[4];
        public int plyCount;
        public int fiftyMoveCounter;
        // random values needed later
        private readonly static ulong[,] zobTable = new ulong[64,15];
        public static readonly int[] directionalOffsets = {8, -8, -1, 1, 7, -7, 9, -9};
        public static readonly int[] knightOffsetsRank = {2, 2, -2, -2, 1, -1, 1, -1};
        public static readonly int[] knightOffsetsFile = {1, -1, 1, -1, 2, 2, -2, -2};
        /// <summary>
        /// Establishes a new board instance, with the position and information contained within a FEN string.
        /// </summary>
        /// <param name="fen">The fen string in question.</param>
        public Board(string fen) {
            LoadFenToPosition(fen);
            MoveData();
            UpdateBitboards();
            InitializeZobrist();
        }
        /// <summary>
        /// Loads the information contained within a string of Forsyth–Edwards Notation into the current board instance's values.
        /// </summary>
        /// <param name="fen">The FEN string in question.</param>
        public void LoadFenToPosition(string fen) {
            string[] parts = fen.Split(" ");
            string[] ranks = parts[0].Split('/');
            Array.Reverse(ranks);
            int i = 0;
            foreach(string rank in ranks) {
                foreach(char c in rank) {
                    if(c == 'p') {
                        squares[i] = Piece.Pawn | Piece.Black;
                        i++;
                    } else if(c == 'P') {
                        squares[i] = Piece.Pawn | Piece.White;
                        i++;
                    } else if(c == 'n') {
                        squares[i] = Piece.Knight | Piece.Black;
                        i++;
                    } else if(c == 'N') {
                        squares[i] = Piece.Knight | Piece.White;
                        i++;
                    } else if(c == 'b') {
                        squares[i] = Piece.Bishop | Piece.Black;
                        i++;
                    } else if(c == 'B') {
                        squares[i] = Piece.Bishop | Piece.White;
                        i++;
                    } else if(c == 'r') {
                        squares[i] = Piece.Rook | Piece.Black;
                        i++;
                    } else if(c == 'R') {
                        squares[i] = Piece.Rook | Piece.White;
                        i++;
                    } else if(c == 'q') {
                        squares[i] = Piece.Queen | Piece.Black;
                        i++;
                    } else if(c == 'Q') {
                        squares[i] = Piece.Queen | Piece.White;
                        i++;
                    } else if(c == 'k') {
                        squares[i] = Piece.King | Piece.Black;
                        kingSquares[1] = (byte)i;
                        i++;
                    } else if(c == 'K') {
                        squares[i] = Piece.King | Piece.White;
                        kingSquares[1] = (byte)i;
                        i++;
                    } else {
                        // for anyone wondering about this line, it's basically char.GetNumericValue but a bit simpler.
                        i += c - '0';
                    }
                }
            }
            // whose turn is it?
            colorToMove = parts[1] == "w" ? 1 : 0;
            // castling rights
            if(parts[2] != "-") {
                foreach(char right in parts[2]) {
                    if(right == 'Q') {
                        castlingRights[1] = true;
                    } else if(right == 'K') {
                        castlingRights[0] = true;
                    } else if(right == 'q') {
                        castlingRights[3] = true;
                    } else if(right == 'k') {
                        castlingRights[2] = true;
                    }
                }
            }
        }
        /// <summary>
        /// Outputs the fen string of the current position
        /// </summary>
        /// <returns>the fen string</returns>
        // the voices are back but FUCK IT, FEN PARSER
        public string GetFenString() {
            string fen = "";

            for (int rank = 7; rank >= 0; rank--)
            {
                int numEmptyFiles = 0;
                for (int file = 0; file < 8; file++)
                {
                    int piece = squares[8*rank+file];
                    if (piece != 0)
                    {
                        if (numEmptyFiles != 0)
                        {
                            fen += numEmptyFiles;
                            numEmptyFiles = 0;
                        }
                        bool isBlack = piece >> 3 == 0;
                        int pieceType = piece & 7;
                        char pieceChar = ' ';
                        switch (pieceType)
                        {
                            case Piece.Rook:
                                pieceChar = 'R';
                                break;
                            case Piece.Knight:
                                pieceChar = 'N';
                                break;
                            case Piece.Bishop:
                                pieceChar = 'B';
                                break;
                            case Piece.Queen:
                                pieceChar = 'Q';
                                break;
                            case Piece.King:
                                pieceChar = 'K';
                                break;
                            case Piece.Pawn:
                                pieceChar = 'P';
                                break;
                        }
                        fen += isBlack ? pieceChar.ToString().ToLower() : pieceChar.ToString();
                    }
                    else
                    {
                        numEmptyFiles++;
                    }

                }
                if (numEmptyFiles != 0)
                {
                    fen += numEmptyFiles;
                }
                if (rank != 0)
                {
                    fen += '/';
                }
            }

            fen += ' ';
            fen += colorToMove == 1 ? 'w' : 'b';

            // castling, nothing yet
            bool thingAdded = false;
            fen += ' ';
            if(castlingRights[0]) {
                fen += 'K';
                thingAdded = true;
            }
            if(castlingRights[1]) {
                fen += 'Q';
                thingAdded = true;
            }
            if(castlingRights[2]) {
                fen += 'k';
                thingAdded = true;
            }
            if(castlingRights[3]) {
                fen += 'q';
                thingAdded = true;
            }
            if(!thingAdded) {
                fen += '-';
            }

            // En Passant (nothing yet)
            fen += ' ';
            fen += '-';

            // 50 move counter
            fen += ' ';
            fen += fiftyMoveCounter;

            // Full-move count (should be one at start, and increase after each move by black)
            fen += ' ';
            fen += plyCount / 2 + colorToMove;
            return fen;
        }
        public static readonly byte[,] squaresToEdge = new byte[64,8];
        static void MoveData() {
            for(byte file = 0; file < 8; file++) {
                for(byte rank = 0; rank < 8; rank++) {
                    byte north = (byte)(7 - rank);
                    byte south = rank;
                    byte west = file;
                    byte east = (byte)(7 - file);
                    byte index = (byte)(rank * 8 + file);
                    squaresToEdge[index, 0] = north;
                    squaresToEdge[index, 1] = south;
                    squaresToEdge[index, 2] = west;
                    squaresToEdge[index, 3] = east;
                    squaresToEdge[index, 4] = Math.Min(north, west);
                    squaresToEdge[index, 5] = Math.Min(south, east);
                    squaresToEdge[index, 6] = Math.Min(north, east);
                    squaresToEdge[index, 7] = Math.Min(south, west);
                }
            }
        }
        public List<Move> GetLegalMoves() {
            List<Move> moves = new();
            // castlingi;
            if(castlingRights[0] && (occupiedBitboard & 0x60) == 0) {
                moves.Add(new Move(4, 6, 0));
            }
            if(castlingRights[1] && (occupiedBitboard & 0xE) == 0) {
                moves.Add(new Move(4, 2, 0));
            }
            if(castlingRights[2] && (occupiedBitboard & 0x6000000000000000) == 0) {
                moves.Add(new Move(60, 62, 0));
            }
            if(castlingRights[3] && (occupiedBitboard & 0xE00000000000000) == 0) {
                moves.Add(new Move(60, 58, 0));
            }
            // the rest of the pieces
            for(int startSquare = 0; startSquare < 64; startSquare++) {
                byte currentPiece = squares[startSquare];
                // checks if it's the right color
                if((currentPiece >> 3) == colorToMove) {
                    // a check if it's a sliding piece
                    if((currentPiece & 0b0111) > 2 && (currentPiece & 0b0111) != 6) {
                        byte startDirection = (byte)((currentPiece & 0b0111) == 3 ? 4 : 0);
                        byte endDirection = (byte)((currentPiece & 0b0111) == 4 ? 8 : 4);
                        for(byte direction = startDirection; direction < endDirection; direction++) {
                            for(byte i = 0; i < squaresToEdge[startSquare, direction]; i++) {
                                byte targetSquareIndex = (byte)(startSquare + directionalOffsets[direction] * (i + 1));
                                byte targetPiece = squares[targetSquareIndex];
                                // can't capture the piece at the end of the line, end the cycle
                                if((targetPiece >> 3) == colorToMove) {
                                    break;
                                }
                                moves.Add(new Move(startSquare, targetSquareIndex, 0));
                                // the piece at the end of the line can be captured, you just added the capture to the list, end the cycle
                                if((targetPiece >> 3) != colorToMove) {
                                    break;
                                }
                            }
                        }
                    } else if((currentPiece & 0b0111) == 1) {
                        if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1]] == 0) {
                            moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 1 : 0], 0));
                            if(startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1] >> 3 == (colorToMove == 1 ? 7 : 0)) {
                                for(int type = 2; type < 6; type++) {
                                    moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 1 : 0], type));
                                }
                            }
                            if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1] * 2] == 0 && startSquare >> 3 == (colorToMove == 1 ? 1 : 6)) {
                                moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 1 : 0] * 2, 0));
                            }
                            if(startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4] > -1 && startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4] < 64) {
                                if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4]] >> 3 != colorToMove || squares[startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4]] >> 3 == enPassantIndex) {
                                    moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 4 : 5], 0));
                                }
                                if(startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4] >> 3 == (colorToMove == 1 ? 7 : 0)) {
                                    for(int type = 2; type < 6; type++) {
                                        moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4], type));
                                    }
                                }
                            }
                            if(startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6] > -1 && startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6] < 64) {
                                if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6]] >> 3 != colorToMove || squares[startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6]] >> 3 == enPassantIndex) {
                                    moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 6 : 7], 0));
                                }
                                if(startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6] >> 3 == (colorToMove == 1 ? 7 : 0)) {
                                    for(int type = 2; type < 6; type++) {
                                        moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6], type));
                                    }
                                }
                            }
                        }
                    } else if((currentPiece & 0b0111) == 2) {
                        // for knight moves I am considering the board using a rank and file so I can determine if a move would end up off of the board or not.
                        for(byte direction = 0; direction < 8; direction++) {
                            int targetRank = (startSquare >> 3) + knightOffsetsRank[direction];
                            int targetFile = (startSquare & 7) + knightOffsetsFile[direction];
                            if(targetRank > -1 && targetRank < 8 && targetFile > -1 && targetFile < 8 && squares[targetRank * 8 + targetFile] == 0) {
                                moves.Add(new Move(startSquare, targetRank * 8 + targetFile, 0));
                            }
                        }

                    } else if((currentPiece & 0b0111) == 6) {
                        for(byte direction = 0; direction < 8; direction++) {
                            int targetSquareIndex = startSquare + directionalOffsets[direction];
                            if(targetSquareIndex < 64 && targetSquareIndex > -1) {
                                byte targetPiece = squares[targetSquareIndex];  
                                if((targetPiece >> 3) != colorToMove) {
                                    moves.Add(new Move(startSquare, targetSquareIndex, 0));
                                }
                            }
                        }
                    }
                }
            }
            return moves;
        }
        /// <summary>
        /// initializes a table used later for zobrist hashing, done automatically if you create the board using a fen string.
        /// </summary>
        public void InitializeZobrist() {
            var rng = new Random();
            for(int i = 0; i < 64; i++) {
                for(int j = 0; j < 15; j++) {
                    zobTable[i,j] = (ulong)rng.Next(0, (int)Math.Pow(2, 64)-1);
                }
            }
        }
        /// <summary>
        /// The function to generate a zobrist hash of the current position
        /// </summary>
        /// <returns>The zobrist hash</returns>
        public ulong CreateHash() {
            ulong hash = 0;
            for(int i = 0; i < 64; i++) {
                hash ^= zobTable[i, squares[i]];
            }
            return hash;
        }
        public void MakeMove(Move move) {

        }
        public void UndoMove(Move move) {

        }
        /// <summary>
        /// Updates the bitboards
        /// </summary>
        public void UpdateBitboards() {
            occupiedBitboard = 0;
            coloredBitboards[0] = 0;
            coloredBitboards[1] = 0;
            for(int i = 0; i < 6; i++) {
                coloredPieceBitboards[0,i] = 0;
                coloredPieceBitboards[1,i] = 0;
            }
            for(int index = 0; index < 64; index++) {
                if(squares[index] != 0) {
                    coloredPieceBitboards[squares[index] >> 3, (squares[index] & 7) - 1] |= (ulong)1 << index; 
                }
                if(squares[index] == 6 || squares[index] == 14) {
                    kingSquares[squares[index] == 14 ? 1 : 0] = (byte)index;
                } 
            }
            coloredBitboards[0] = coloredPieceBitboards[0,0] | coloredPieceBitboards[0,1] | coloredPieceBitboards[0,2] | coloredPieceBitboards[0,3] | coloredPieceBitboards[0,4] | coloredPieceBitboards[0,5];
            coloredBitboards[1] = coloredPieceBitboards[1,0] | coloredPieceBitboards[1,1] | coloredPieceBitboards[1,2] | coloredPieceBitboards[1,3] | coloredPieceBitboards[1,4] | coloredPieceBitboards[1,5];
            occupiedBitboard = coloredBitboards[0] | coloredBitboards[1];
        }
        public bool SquareIsAttackedByOpponent(int square) {
            bool isCheck = false;
            // orthagonal
            for(int direction = 0; direction < 4; direction++) {
                for(byte i = 0; i < squaresToEdge[square, direction]; i++) {
                    byte targetSquareIndex = (byte)(square + directionalOffsets[direction] * (i + 1));
                    byte targetPiece = squares[targetSquareIndex];
                    if(targetPiece >> 3 == colorToMove) {
                        break;
                    }
                    if((targetPiece & 7) == 3 || (targetPiece & 7) == 5) {
                        isCheck = true;
                        break;
                    }
                }
            }
            // diagonals
            for(int direction = 4; direction < 8; direction++) {
                // pawns
                if(squaresToEdge[kingSquares[colorToMove], direction] > 0) {
                    if((direction & 1) == colorToMove) {
                        if(squares[square + directionalOffsets[direction]] == (Piece.Pawn | (colorToMove == 1 ? Piece.White : Piece.Black))) {
                            isCheck = true;
                            break;
                        }
                    }
                }
                for(byte i = 0; i < squaresToEdge[square, direction]; i++) {
                    byte targetSquareIndex = (byte)(square + directionalOffsets[direction] * (i + 1));
                    byte targetPiece = squares[targetSquareIndex];
                    if(((targetPiece & 7) == 4 || (targetPiece & 7) == 5) && targetPiece >> 3 != colorToMove) {
                        isCheck = true;
                        break;
                    }
                }
            }
            return isCheck;
        }
        public bool IsInCheck() {
            return SquareIsAttackedByOpponent(kingSquares[colorToMove]);
        }
    }
    public struct Move {
        public int startSquare;
        public int endSquare;
        public int promotionType;
        /// <summary>
        /// Makes a new move with the starting and ending squares.
        /// </summary>
        /// <param name="start">Starting square of the move</param>
        /// <param name="end">Ending square of the move</param>
        /// <param name="pType">The promotion piece type, if any</param>
        public Move(int start, int end, int pType) {
            startSquare = start;
            endSquare = end;
            promotionType = pType;
        }
        public string ConvertToLongAlgebraic() {
            int startRank = startSquare >> 3;
            int startFile = startSquare & 7;
            int endRank = endSquare >> 3;
            int endFile = endSquare & 7;
            string name = "";
            name += (char)(startFile + 'a');
            name += startRank + 1;
            name += (char)(endFile + 'a');
            name += endRank + 1;
            return name;
        }
    }
} 