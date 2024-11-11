#ifndef ROCK_PAPER_SCISSORS_TA_H
#define ROCK_PAPER_SCISSORS_TA_H

/*
 * Shared enums for host/TA to represent game state
 */
typedef enum { UNDEFINED = 0, ROCK = 1, PAPER = 2, SCISSORS = 3 } Weapon;

typedef enum {
    NOT_DETERMINED = 0,
    DRAW = 1,
    HOST_WIN = 2,
    TA_WIN = 3
} Game_Result;

/*
 *  Prints the enum value as string.
 */
const char* weapon_to_string(Weapon wp) {
    switch (wp) {
        case ROCK: {
            return "Rock";
        }
        case PAPER: {
            return "Paper";
        }
        case SCISSORS: {
            return "Scissors";
        }
        default: {
            return "Undefined";
        }
    }
}

/*
 * UUID generated with uuidgen from util-linux 2.40.2
 */
#define ROCK_PAPER_SCISSORS_TA_UUID                        \
    {                                                      \
        0x7d513eb4, 0xeb04, 0x400b, {                      \
            0x96, 0x55, 0x66, 0xdc, 0x8c, 0x62, 0x43, 0x4e \
        }                                                  \
    }

// The exposed function IDs implemented in the rock_paper_scissors ta
#define ROCK_PAPER_SCISSORS_TA_CMD_PLAY_VALUE 0

#endif  // ROCK_PAPER_SCISSORS_TA_H
