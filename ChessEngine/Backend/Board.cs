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
    public struct BoardState {
        public byte[] squares = new byte[64];
        public byte[] kingSquares = new byte[2];  
        public int enPassantIndex;
        public bool[] castlingRights = new bool[4];
        public int fiftyMoveCounter;
        public BoardState(Board board) {
            Array.Copy(board.squares, squares, 64);
            Array.Copy(board.kingSquares, kingSquares, 2);
            enPassantIndex = board.enPassantIndex;
            Array.Copy(board.castlingRights, castlingRights, 4);
            fiftyMoveCounter = board.fiftyMoveCounter;
        }
        public BoardState(BoardState state) {
            Array.Copy(state.squares, squares, 64);
            Array.Copy(state.kingSquares, kingSquares, 2);
            enPassantIndex = state.enPassantIndex;
            Array.Copy(state.castlingRights, castlingRights, 4);
            fiftyMoveCounter = state.fiftyMoveCounter;
        }
    }
    public class Board {
        // board specific valus
        public byte[] squares = new byte[64];
        public byte[] kingSquares = new byte[2];
        public int colorToMove = 1;
        public ulong occupiedBitboard = 0;
        public ulong emptyBitboard = 0;
        // bitboards of the entire color: 0 is black 1 is white
        public ulong[] coloredBitboards = new ulong[2];
        // bitboards of specific pieces of specific color, using the numbers above.
        public ulong[,] coloredPieceBitboards = new ulong[2,6];
        // the index of the en-passantable square, from 0-63, and if it's 64 no en passant is legal.
        public int enPassantIndex = 64;
        // white kingside, white queenside, black kingside, black kingside
        public bool[] castlingRights = new bool[4];
        public int plyCount = 0;
        public int fiftyMoveCounter;
        // random values needed later
        private readonly static ulong[,] zobTable = new ulong[64,15];
        public static readonly int[] directionalOffsets = {8, -8, -1, 1, 7, -7, 9, -9};
        public static readonly int[] knightOffsetsRank = {2, 2, -2, -2, 1, -1, 1, -1};
        public static readonly int[] knightOffsetsFile = {1, -1, 1, -1, 2, 2, -2, -2};
        public void ReadBoardState(BoardState state) {
            Array.Copy(state.squares, squares, 64);
            Array.Copy(state.kingSquares, kingSquares, 2);
            enPassantIndex = state.enPassantIndex;
            Array.Copy(state.castlingRights, castlingRights, 4);
            fiftyMoveCounter = state.fiftyMoveCounter;
        }
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
        public Board(bool isStartPos) {
            LoadFenToPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
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
                        kingSquares[0] = (byte)i;
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
            // en passant rules
            if(parts[3] != "-") {
                char[] characters = parts[3].ToCharArray();
                enPassantIndex = characters[0] - 'a' + (characters[1] - 1) * 8;
                Console.WriteLine(enPassantIndex);
            }
            fiftyMoveCounter = int.Parse(parts[4]);
            plyCount = int.Parse(parts[5]) * 2 - colorToMove;
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

            // castling
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

            // En Passant
            fen += ' ';
            if(enPassantIndex != 64) {
                fen += (char)((enPassantIndex & 7) + 'a');
                if(enPassantIndex < 23) {
                    fen += '3';
                } else {
                    fen += '6';
                }
            } else {
                fen += '-';
            }

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
            BoardState state = new(this);
            List<Move> moves = new();
            // castlingi;
            if(castlingRights[0] && (occupiedBitboard & 0x60) == 0) {
                moves.Add(new Move(4, 6, 0, state));
            }
            if(castlingRights[1] && (occupiedBitboard & 0xE) == 0) {
                moves.Add(new Move(4, 2, 0, state));
            }
            if(castlingRights[2] && (occupiedBitboard & 0x6000000000000000) == 0) {
                moves.Add(new Move(60, 62, 0, state));
            }
            if(castlingRights[3] && (occupiedBitboard & 0xE00000000000000) == 0) {
                moves.Add(new Move(60, 58, 0, state));
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
                                moves.Add(new Move(startSquare, targetSquareIndex, 0, state));
                                // the piece at the end of the line can be captured, you just added the capture to the list, end the cycle
                                if((targetPiece >> 3) != colorToMove) {
                                    break;
                                }
                            }
                        }
                    } else if((currentPiece & 0b0111) == 1) {
                        if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1]] == 0) {
                            moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1], 0, state));
                            if(startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1] >> 3 == (colorToMove == 1 ? 7 : 0)) {
                                for(int type = 2; type < 6; type++) {
                                    moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1], type, state));
                                }
                            }
                            if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1] * 2] == 0 && startSquare >> 3 == (colorToMove == 1 ? 1 : 6)) {
                                moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 0 : 1] * 2, 0, state));
                            }
                            if(startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4] > -1 && startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4] < 64) {
                                if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4]] >> 3 != colorToMove || squares[startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4]] >> 3 == enPassantIndex) {
                                    moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 4 : 5], 0, state));
                                }
                                if(startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4] >> 3 == (colorToMove == 1 ? 7 : 0)) {
                                    for(int type = 2; type < 6; type++) {
                                        moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 5 : 4], type, state));
                                    }
                                }
                            }
                            if(startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6] > -1 && startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6] < 64) {
                                if(squares[startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6]] >> 3 != colorToMove || squares[startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6]] >> 3 == enPassantIndex) {
                                    moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 6 : 7], 0, state));
                                }
                                if(startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6] >> 3 == (colorToMove == 1 ? 7 : 0)) {
                                    for(int type = 2; type < 6; type++) {
                                        moves.Add(new Move(startSquare, startSquare + directionalOffsets[colorToMove == 1 ? 7 : 6], type, state));
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
                                moves.Add(new Move(startSquare, targetRank * 8 + targetFile, 0, state));
                            }
                        }

                    } else if((currentPiece & 0b0111) == 6) {
                        for(byte direction = 0; direction < 8; direction++) {
                            int targetSquareIndex = startSquare + directionalOffsets[direction];
                            if(targetSquareIndex < 64 && targetSquareIndex > -1) {
                                byte targetPiece = squares[targetSquareIndex];  
                                if((targetPiece >> 3) != colorToMove) {
                                    moves.Add(new Move(startSquare, targetSquareIndex, 0, state));
                                }
                            }
                        }
                    }
                }
            }
            // legal check
            List<Move> illegalMoves = new();
            colorToMove = colorToMove == 1 ? 0 : 1;
            foreach(Move move in moves) {
                MakeMove(move);
                if(IsInCheck()) illegalMoves.Add(move);
                UndoMove(move);
            }
            colorToMove = colorToMove == 1 ? 0 : 1;
            foreach(Move move in illegalMoves) {
                moves.Remove(move);
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
            if(move.endSquare > -1 && move.endSquare < 64 && move.startSquare > -1 && move.startSquare < 64) {
                if(colorToMove != 1) {
                    fiftyMoveCounter++;
                }
                enPassantIndex = 64;
                // fifty move counter
                if(squares[move.endSquare] != 0 || (squares[move.startSquare] & 7) == Piece.Pawn) {
                    fiftyMoveCounter = 0;
                }
                // king square updates
                if((squares[move.startSquare] & 7) == Piece.King) {
                    kingSquares[colorToMove] = (byte)move.endSquare;
                    castlingRights[colorToMove == 1 ? 0 : 2] = false;
                    castlingRights[colorToMove == 1 ? 1 : 3] = false;
                }
                // for each start one set the start square to 0
                if(castlingRights[0] && move.startSquare == 4 && move.endSquare == 6) {
                    // castling 1
                    // switch 4 with 6 and 7 with 5
                    squares[6] = squares[4];
                    squares[4] = 0;
                    squares[7] = squares[5];
                    squares[5] = 0;
                } else if(castlingRights[1] && move.startSquare == 4 && move.endSquare == 2) {
                    // castling 2
                    // switch 4 with 2 and 0 with 3
                    squares[6] = squares[4];
                    squares[4] = 0;
                    squares[7] = squares[5];
                    squares[5] = 0;
                } else if(castlingRights[2] && move.startSquare == 60 && move.endSquare == 62) {
                    // castling 3
                    // switch 60 with 62 and 63 with 61
                    squares[6] = squares[4];
                    squares[4] = 0;
                    squares[7] = squares[5];
                    squares[5] = 0;
                } else if(castlingRights[3] && move.startSquare == 60 && move.endSquare == 58) {
                    // castling 4
                    // switch 60 with 58 and 56 with 59
                    squares[6] = squares[4];
                    squares[4] = 0;
                    squares[7] = squares[5];
                    squares[5] = 0;
                } else if(move.endSquare != 0 && move.endSquare == enPassantIndex) {
                    // en passant
                    // switch first square with second square and set the square either ahead or behind with colorToMove ? 8 : -8 to 0
                    squares[move.endSquare] = squares[move.startSquare];
                    squares[move.startSquare] = 0;
                    squares[move.endSquare + (colorToMove == 1 ? -8 : 8)] = 0;
                } else {
                    // move normally
                    // switch first square with second
                    if((squares[move.startSquare] & 7) == Piece.Pawn && move.endSquare == move.startSquare + (colorToMove == 1 ? 16 : -16)) {
                        // its a double move, make the first square the en passant index
                        enPassantIndex = move.startSquare + (colorToMove == 1 ? 8 : -8);
                    }
                    squares[move.endSquare] = squares[move.startSquare];
                    squares[move.startSquare] = 0;
                }
                // promotions
                if(move.promotionType != 0) {
                    squares[move.endSquare] = (byte)move.promotionType;
                }
                colorToMove = colorToMove == 1 ? 0 : 1;
                plyCount++;
                UpdateBitboards();
            }
        }
        public void UndoMove(Move move) {
            ReadBoardState(move.state); 
            plyCount--;
            colorToMove = colorToMove == 1 ? 0 : 1;
            UpdateBitboards();
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
            emptyBitboard = ~occupiedBitboard;
        }
        public bool SquareIsAttackedByOpponent(int square) {
            bool isCheck = false;
            // orthagonal
            for(int direction = 0; direction < 4; direction++) {
                for(byte i = 0; i < squaresToEdge[square, direction]; i++) {
                    byte targetSquareIndex = (byte)(square + directionalOffsets[direction] * (i + 1));
                    byte targetPiece = squares[targetSquareIndex];
                    if(targetPiece != 0) {
                        if(targetPiece >> 3 == colorToMove) {
                            break;
                        }
                        if((targetPiece & 7) == Piece.Rook || (targetPiece & 7) == Piece.Queen) {
                            isCheck = true;
                            break;
                        }
                    }
                }
                int targetRank = (kingSquares[colorToMove] >> 3) + knightOffsetsRank[direction];
                int targetFile = (kingSquares[colorToMove] & 7) + knightOffsetsFile[direction];
                if(targetRank > -1 && targetRank < 8 && targetFile > -1 && targetFile < 8 && (squares[targetRank * 8 + targetFile] & 7) == Piece.Knight) {
                    isCheck = true;
                    break;
                }
            }
            // diagonals
            for(int direction = 4; direction < 8; direction++) {
                // pawns
                if(squaresToEdge[kingSquares[colorToMove], direction] > 0) {
                    if((direction & 1) != colorToMove) {
                        if(squares[square + directionalOffsets[direction]] == (Piece.Pawn | (colorToMove == 0 ? Piece.White : Piece.Black))) {
                            isCheck = true;
                            break;
                        }
                    }
                }
                for(byte i = 0; i < squaresToEdge[square, direction]; i++) {
                    byte targetSquareIndex = (byte)(square + directionalOffsets[direction] * (i + 1));
                    byte targetPiece = squares[targetSquareIndex];
                    if(targetPiece != 0) {
                        if(((targetPiece & 7) == Piece.Bishop || (targetPiece & 7) == Piece.Queen) && targetPiece >> 3 != colorToMove) {
                            isCheck = true;
                            break;
                        }
                    }
                }
                int targetRank = (square >> 3) + knightOffsetsRank[direction];
                int targetFile = (square     & 7) + knightOffsetsFile[direction];
                if(targetRank > -1 && targetRank < 8 && targetFile > -1 && targetFile < 8 && (squares[targetRank * 8 + targetFile] & 7) == Piece.Knight) {
                    isCheck = true;
                    break;
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
        public BoardState state;
        /// <summary>
        /// Makes a new move with the starting and ending squares.
        /// </summary>
        /// <param name="start">Starting square of the move</param>
        /// <param name="end">Ending square of the move</param>
        /// <param name="pType">The promotion piece type, if any</param>
        public Move(int start, int end, int pType, BoardState s) {
            startSquare = start;
            endSquare = end;
            promotionType = pType;
            state = new(s);
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