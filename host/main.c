#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* For the UUID (found in the TA's h-file(s)) */
#include <rock_paper_scissors_ta.h>

#define RET_OK 0
#define RET_ERR 1

/* Helper function that prints the game options. */
void weapon_selection_prompt() {
    printf(
        "Enter the weapon of your choice: \n"
        "'r' = Rock, "
        "'p' = Paper, "
        "'s' = Scissors."
        "\n");
}

/* Evaluates the passed Game_Result enum and prints the winner.*/
static void evaluate_result(Game_Result result) {
    switch (result) {
        case DRAW: {
            printf("The game ended up as draw.\n");
            break;
        }
        case TA_WIN: {
            printf("TA won the game.\n");
            break;
        }
        case HOST_WIN: {
            printf("Host won the game. \n");
            break;
        }
        default: {
            printf("The result could not be determined.\n");
            break;
        }
    }
}

/*
 * Reads the weapon from the stdin.
 * Expects a character from set (r, p, s). Any other characters or longer
 * inputs, are not accepted. Returns RET_OK on success or RET_ERR on inability
 * to parse. In case of RET_ERR return code, the host_weapon remains UNDEFINED.
 */
int get_host_weapon(Weapon* host_weapon) {
    char input[3] = {0};
    *host_weapon = UNDEFINED;
    weapon_selection_prompt();

    if (fgets(input, sizeof(input), stdin) == NULL) {
        fprintf(stderr, "Error reading input from stdin.\n");
        return RET_ERR;
    }

    if (input[1] != '\n' && input[1] != '\0') {
        fprintf(stderr, "The input should be exactly one character\n");
        return RET_ERR;
    }

    switch (input[0]) {
        case 'r': {
            *host_weapon = ROCK;
            break;
        }
        case 'p': {
            *host_weapon = PAPER;
            break;
        }
        case 's': {
            *host_weapon = SCISSORS;
            break;
        }
        default: {
            fprintf(stderr, "Could not parse the input");
            return RET_ERR;
        }
    }

    return RET_OK;
}

int main(void) {
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = ROCK_PAPER_SCISSORS_TA_UUID;
    uint32_t err_origin;

    int rc = RET_OK;

    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
    }

    /* Open a session to TA */
    res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL,
                           &err_origin);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x", res,
             err_origin);
    }

    /* Clear the TEEC_Operation struct */
    memset(&op, 0, sizeof(op));

    /*
     * Prepare the argument. Pass a value in the first parameter.
     * The second is to be returned from the TA. The rest are unused. 
     */
    Weapon host_weapon = UNDEFINED;
    if ((rc = get_host_weapon(&host_weapon)) == RET_ERR) {
        goto cleanup;
    }

    if (host_weapon == UNDEFINED) {
        printf("Host weapon incorrectly parsed. Exiting.\n");
        rc = RET_ERR;
        goto cleanup;
    }

    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_VALUE_OUTPUT,
                                     TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = (int)host_weapon;

    /* Call TA ROCK_PAPER_SCISSORS_TA_CMD_PLAY_VALUE function. */
    res = TEEC_InvokeCommand(&sess, ROCK_PAPER_SCISSORS_TA_CMD_PLAY_VALUE, &op,
                             &err_origin);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res,
             err_origin);
    }

    Game_Result game_res = (Game_Result)op.params[1].value.a;

    evaluate_result(game_res);

cleanup:
    /* close the TA session and destroy the context. */
    TEEC_CloseSession(&sess);

    TEEC_FinalizeContext(&ctx);

    return rc;
}
