namespace Chess {
    public static class Mask {
        // These first 3 sets of masks (knight moves, king moves, and pawn attacks), were originally calculated by Ellie M, also known as RedBedHed, the developer of Homura.
        public static readonly ulong[] knightMasks = {
            0x0000000000020400L, 0x0000000000050800L,
            0x00000000000A1100L, 0x0000000000142200L,
            0x0000000000284400L, 0x0000000000508800L,
            0x0000000000A01000L, 0x0000000000402000L,
            0x0000000002040004L, 0x0000000005080008L,
            0x000000000A110011L, 0x0000000014220022L,
            0x0000000028440044L, 0x0000000050880088L,
            0x00000000A0100010L, 0x0000000040200020L,
            0x0000000204000402L, 0x0000000508000805L,
            0x0000000A1100110AL, 0x0000001422002214L,
            0x0000002844004428L, 0x0000005088008850L,
            0x000000A0100010A0L, 0x0000004020002040L,
            0x0000020400040200L, 0x0000050800080500L,
            0x00000A1100110A00L, 0x0000142200221400L,
            0x0000284400442800L, 0x0000508800885000L,
            0x0000A0100010A000L, 0x0000402000204000L,
            0x0002040004020000L, 0x0005080008050000L,
            0x000A1100110A0000L, 0x0014220022140000L,
            0x0028440044280000L, 0x0050880088500000L,
            0x00A0100010A00000L, 0x0040200020400000L,
            0x0204000402000000L, 0x0508000805000000L,
            0x0A1100110A000000L, 0x1422002214000000L,
            0x2844004428000000L, 0x5088008850000000L,
            0xA0100010A0000000L, 0x4020002040000000L,
            0x0400040200000000L, 0x0800080500000000L,
            0x1100110A00000000L, 0x2200221400000000L,
            0x4400442800000000L, 0x8800885000000000L,
            0x100010A000000000L, 0x2000204000000000L,
            0x0004020000000000L, 0x0008050000000000L,
            0x00110A0000000000L, 0x0022140000000000L,
            0x0044280000000000L, 0x0088500000000000L,
            0x0010A00000000000L, 0x0020400000000000L
        };
        public static readonly ulong[] kingMasks = {
            0x0000000000000302L, 0x0000000000000705L,
            0x0000000000000E0AL, 0x0000000000001C14L,
            0x0000000000003828L, 0x0000000000007050L,
            0x000000000000E0A0L, 0x000000000000C040L,
            0x0000000000030203L, 0x0000000000070507L,
            0x00000000000E0A0EL, 0x00000000001C141CL,
            0x0000000000382838L, 0x0000000000705070L,
            0x0000000000E0A0E0L, 0x0000000000C040C0L,
            0x0000000003020300L, 0x0000000007050700L,
            0x000000000E0A0E00L, 0x000000001C141C00L,
            0x0000000038283800L, 0x0000000070507000L,
            0x00000000E0A0E000L, 0x00000000C040C000L,
            0x0000000302030000L, 0x0000000705070000L,
            0x0000000E0A0E0000L, 0x0000001C141C0000L,
            0x0000003828380000L, 0x0000007050700000L,
            0x000000E0A0E00000L, 0x000000C040C00000L,
            0x0000030203000000L, 0x0000070507000000L,
            0x00000E0A0E000000L, 0x00001C141C000000L,
            0x0000382838000000L, 0x0000705070000000L,
            0x0000E0A0E0000000L, 0x0000C040C0000000L,
            0x0003020300000000L, 0x0007050700000000L,
            0x000E0A0E00000000L, 0x001C141C00000000L,
            0x0038283800000000L, 0x0070507000000000L,
            0x00E0A0E000000000L, 0x00C040C000000000L,
            0x0302030000000000L, 0x0705070000000000L,
            0x0E0A0E0000000000L, 0x1C141C0000000000L,
            0x3828380000000000L, 0x7050700000000000L,
            0xE0A0E00000000000L, 0xC040C00000000000L,
            0x0203000000000000L, 0x0507000000000000L,
            0x0A0E000000000000L, 0x141C000000000000L,
            0x2838000000000000L, 0x5070000000000000L,
            0xA0E0000000000000L, 0x40C0000000000000L
        };
        public static readonly ulong[,] pawnAttackMasks  = {
            {
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000002L,0x0000000000000005L,
                0x000000000000000AL,0x0000000000000014L,
                0x0000000000000028L,0x0000000000000050L,
                0x00000000000000A0L,0x0000000000000040L,
                0x0000000000000200L,0x0000000000000500L,
                0x0000000000000A00L,0x0000000000001400L,
                0x0000000000002800L,0x0000000000005000L,
                0x000000000000A000L,0x0000000000004000L,
                0x0000000000020000L,0x0000000000050000L,
                0x00000000000A0000L,0x0000000000140000L,
                0x0000000000280000L,0x0000000000500000L,
                0x0000000000A00000L,0x0000000000400000L,
                0x0000000002000000L,0x0000000005000000L,
                0x000000000A000000L,0x0000000014000000L,
                0x0000000028000000L,0x0000000050000000L,
                0x00000000A0000000L,0x0000000040000000L,
                0x0000000200000000L,0x0000000500000000L,
                0x0000000A00000000L,0x0000001400000000L,
                0x0000002800000000L,0x0000005000000000L,
                0x000000A000000000L,0x0000004000000000L,
                0x0000020000000000L,0x0000050000000000L,
                0x00000A0000000000L,0x0000140000000000L,
                0x0000280000000000L,0x0000500000000000L,
                0x0000A00000000000L,0x0000400000000000L,
                0x0002000000000000L,0x0005000000000000L,
                0x000A000000000000L,0x0014000000000000L,
                0x0028000000000000L,0x0050000000000000L,
                0x00A0000000000000L,0x0040000000000000L
            },
            {
                0x0000000000000200L,0x0000000000000500L,
                0x0000000000000A00L,0x0000000000001400L,
                0x0000000000002800L,0x0000000000005000L,
                0x000000000000A000L,0x0000000000004000L,
                0x0000000000020000L,0x0000000000050000L,
                0x00000000000A0000L,0x0000000000140000L,
                0x0000000000280000L,0x0000000000500000L,
                0x0000000000A00000L,0x0000000000400000L,
                0x0000000002000000L,0x0000000005000000L,
                0x000000000A000000L,0x0000000014000000L,
                0x0000000028000000L,0x0000000050000000L,
                0x00000000A0000000L,0x0000000040000000L,
                0x0000000200000000L,0x0000000500000000L,
                0x0000000A00000000L,0x0000001400000000L,
                0x0000002800000000L,0x0000005000000000L,
                0x000000A000000000L,0x0000004000000000L,
                0x0000020000000000L,0x0000050000000000L,
                0x00000A0000000000L,0x0000140000000000L,
                0x0000280000000000L,0x0000500000000000L,
                0x0000A00000000000L,0x0000400000000000L,
                0x0002000000000000L,0x0005000000000000L,
                0x000A000000000000L,0x0014000000000000L,
                0x0028000000000000L,0x0050000000000000L,
                0x00A0000000000000L,0x0040000000000000L,
                0x0200000000000000L,0x0500000000000000L,
                0x0A00000000000000L,0x1400000000000000L,
                0x2800000000000000L,0x5000000000000000L,
                0xA000000000000000L,0x4000000000000000L,
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000000L,0x0000000000000000L,
                0x0000000000000000L,0x0000000000000000L
            }
        };
        // these masks were generated by me
        public static readonly ulong[,] slideyPieceRays = {
            {72340172838076672, 144680345676153344, 289360691352306688, 578721382704613376, 1157442765409226752, 2314885530818453504, 4629771061636907008, 9259542123273814016, 72340172838076416, 144680345676152832, 289360691352305664, 578721382704611328, 1157442765409222656, 2314885530818445312, 4629771061636890624, 9259542123273781248, 72340172838010880, 144680345676021760, 289360691352043520, 578721382704087040, 1157442765408174080, 2314885530816348160, 4629771061632696320, 9259542123265392640, 72340172821233664, 144680345642467328, 289360691284934656, 578721382569869312, 1157442765139738624, 2314885530279477248, 4629771060558954496, 9259542121117908992, 72340168526266368, 144680337052532736, 289360674105065472, 578721348210130944, 1157442696420261888, 2314885392840523776, 4629770785681047552, 9259541571362095104, 72339069014638592, 144678138029277184, 289356276058554368, 578712552117108736, 1157425104234217472, 2314850208468434944, 4629700416936869888, 9259400833873739776, 72057594037927936, 144115188075855872, 288230376151711744, 576460752303423488, 1152921504606846976, 2305843009213693952, 4611686018427387904, 9223372036854775808, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, 128, 257, 514, 1028, 2056, 4112, 8224, 16448, 32896, 65793, 131586, 263172, 526344, 1052688, 2105376, 4210752, 8421504, 16843009, 33686018, 67372036, 134744072, 269488144, 538976288, 1077952576, 2155905152, 4311810305, 8623620610, 17247241220, 34494482440, 68988964880, 137977929760, 275955859520, 551911719040, 1103823438081, 2207646876162, 4415293752324, 8830587504648, 17661175009296, 35322350018592, 70644700037184, 141289400074368, 282578800148737, 565157600297474, 1130315200594948, 2260630401189896, 4521260802379792, 9042521604759584, 18085043209519168, 36170086419038336},
            {254, 252, 248, 240, 224, 192, 128, 0, 65024, 64512, 63488, 61440, 57344, 49152, 32768, 0, 16646144, 16515072, 16252928, 15728640, 14680064, 12582912, 8388608, 0, 4261412864, 4227858432, 4160749568, 4026531840, 3758096384, 3221225472, 2147483648, 0, 1090921693184, 1082331758592, 1065151889408, 1030792151040, 962072674304, 824633720832, 549755813888, 0, 279275953455104, 277076930199552, 272678883688448, 263882790666240, 246290604621824, 211106232532992, 140737488355328, 0, 71494644084506624, 70931694131085312, 69805794224242688, 67553994410557440, 63050394783186944, 54043195528445952, 36028797018963968, 0, 18302628885633695744, 18158513697557839872, 17870283321406128128, 17293822569102704640, 16140901064495857664, 13835058055282163712, 9223372036854775808, 0},
            {0, 1, 3, 7, 15, 31, 63, 127, 0, 256, 768, 1792, 3840, 7936, 16128, 32512, 0, 65536, 196608, 458752, 983040, 2031616, 4128768, 8323072, 0, 16777216, 50331648, 117440512, 251658240, 520093696, 1056964608, 2130706432, 0, 4294967296, 12884901888, 30064771072, 64424509440, 133143986176, 270582939648, 545460846592, 0, 1099511627776, 3298534883328, 7696581394432, 16492674416640, 34084860461056, 69269232549888, 139637976727552, 0, 281474976710656, 844424930131968, 1970324836974592, 4222124650659840, 8725724278030336, 17732923532771328, 35747322042253312, 0, 72057594037927936, 216172782113783808, 504403158265495552, 1080863910568919040, 2233785415175766016, 4539628424389459968, 9151314442816847872},
            {0, 256, 66048, 16909312, 4328785920, 1108169199616, 283691315109888, 72624976668147712, 0, 65536, 16908288, 4328783872, 1108169195520, 283691315101696, 72624976668131328, 145249953336262656, 0, 16777216, 4328521728, 1108168671232, 283691314053120, 72624976666034176, 145249953332068352, 290499906664136704, 0, 4294967296, 1108101562368, 283691179835392, 72624976397598720, 145249952795197440, 290499905590394880, 580999811180789760, 0, 1099511627776, 283673999966208, 72624942037860352, 145249884075720704, 290499768151441408, 580999536302882816, 1161999072605765632, 0, 281474976710656, 72620543991349248, 145241087982698496, 290482175965396992, 580964351930793984, 1161928703861587968, 2323857407723175936, 0, 72057594037927936, 144115188075855872, 288230376151711744, 576460752303423488, 1152921504606846976, 2305843009213693952, 4611686018427387904, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 8, 16, 32, 64, 128, 0, 516, 1032, 2064, 4128, 8256, 16512, 32768, 0, 132104, 264208, 528416, 1056832, 2113664, 4227072, 8388608, 0, 33818640, 67637280, 135274560, 270549120, 541097984, 1082130432, 2147483648, 0, 8657571872, 17315143744, 34630287488, 69260574720, 138521083904, 277025390592, 549755813888, 0, 2216338399296, 4432676798592, 8865353596928, 17730707128320, 35461397479424, 70918499991552, 140737488355328, 0, 567382630219904, 1134765260439552, 2269530520813568, 4539061024849920, 9078117754732544, 18155135997837312, 36028797018963968, 0},
            {9241421688590303744, 36099303471055872, 141012904183808, 550831656960, 2151686144, 8404992, 32768, 0, 4620710844295151616, 9241421688590303232, 36099303471054848, 141012904181760, 550831652864, 2151677952, 8388608, 0, 2310355422147510272, 4620710844295020544, 9241421688590041088, 36099303470530560, 141012903133184, 550829555712, 2147483648, 0, 1155177711056977920, 2310355422113955840, 4620710844227911680, 9241421688455823360, 36099303202095104, 141012366262272, 549755813888, 0, 577588851233521664, 1155177702467043328, 2310355404934086656, 4620710809868173312, 9241421619736346624, 36099165763141632, 140737488355328, 0, 288793326105133056, 577586652210266112, 1155173304420532224, 2310346608841064448, 4620693217682128896, 9241386435364257792, 36028797018963968, 0, 144115188075855872, 288230376151711744, 576460752303423488, 1152921504606846976, 2305843009213693952, 4611686018427387904, 9223372036854775808, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, 0, 256, 513, 1026, 2052, 4104, 8208, 16416, 0, 65536, 131328, 262657, 525314, 1050628, 2101256, 4202512, 0, 16777216, 33619968, 67240192, 134480385, 268960770, 537921540, 1075843080, 0, 4294967296, 8606711808, 17213489152, 34426978560, 68853957121, 137707914242, 275415828484, 0, 1099511627776, 2203318222848, 4406653222912, 8813306511360, 17626613022976, 35253226045953, 70506452091906, 0, 281474976710656, 564049465049088, 1128103225065472, 2256206466908160, 4512412933881856, 9024825867763968, 18049651735527937}
        };
        public const ulong FileMask = 0b100000001000000010000000100000001000000010000000100000001;
        public const ulong RankMask = 0b11111111;
        /// <summary>
        /// outputs a mask for a specific file, or column on the board;
        /// </summary>
        /// <param name="file">which number you want</param>
        /// <returns>the filemask</returns>
        public static ulong GetFileMask(int file) {
            return FileMask << file;
        }
        public static ulong GetRankMask(int rank) {
            return RankMask << (8 * rank);
        }
    }
    
}
