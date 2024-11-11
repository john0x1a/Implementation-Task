#include <stdlib.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include <utee_defines.h>

#include <pta_attestation.h>

#include <rock_paper_scissors_ta.h>

#define MAX_KEYSIZE 4096

/*
 * Called when the instance of the TA is created. This is the first call in
 * the TA.
 */
TEE_Result TA_CreateEntryPoint(void) {
    DMSG("has been called");

    return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed if the TA has not
 * crashed or panicked. This is the last call in the TA.
 */
void TA_DestroyEntryPoint(void) {
    DMSG("has been called");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
                                    TEE_Param __maybe_unused params[4],
                                    void __maybe_unused** sess_ctx) {
    uint32_t exp_param_types =
        TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
                        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

    DMSG("has been called");

    if (param_types != exp_param_types) {
        IMSG("Bad parameters.\n");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    /*
     * Unused params. The following lines basically silence compiler warnings.
     */
    (void)&params;
    (void)&sess_ctx;

    /* If return value != TEE_SUCCESS the session will not be created. */
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused* sess_ctx) {
    // unused parameter
    (void)&sess_ctx;
    IMSG("Closing session!\n");
}

static Game_Result calculate_result(Weapon host_weapon, Weapon ta_weapon) {
    if (host_weapon == UNDEFINED || ta_weapon == UNDEFINED) {
        return NOT_DETERMINED;
    }

    if (host_weapon == ta_weapon) {
        return DRAW;
    } else if ((host_weapon == ROCK && ta_weapon == SCISSORS) ||
               (host_weapon == PAPER && ta_weapon == ROCK) ||
               (host_weapon == SCISSORS && ta_weapon == PAPER)) {
        return HOST_WIN;
    } else {
        return TA_WIN;
    }

    return NOT_DETERMINED;
}

/*
 * TA function that "plays" the rock-paper-scissor game.
 * Reads the host weapon selection. Selects randomly a weapon for the TA
 * and calculates and stores the result.
 *
 * [in]       value[0]        Host "weapon" selection.
 * [out]      value[1]        Result of the game
 *
 * Return codes:
 * TEE_SUCCESS
 * TEE_ERROR_GENERIC - If the winner cannot be determined.
 * TEE_ERROR_BAD_PARAMETERS - If the passed params are not of the expected type.
 */
static TEE_Result play_game(uint32_t param_types, TEE_Param params[4]) {
    uint32_t exp_param_types =
        TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_VALUE_OUTPUT,
                        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

    DMSG("has been called");

    if (param_types != exp_param_types) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    // Get the passed host weapon and print it
    Weapon host_weapon = (Weapon)params[0].value.a;
    IMSG("Host weapon is : %s", weapon_to_string(host_weapon));

    /*
     * Use rand to generate a pseudo-random number in range 0-2 for the TA
     * weapon and print it.
     */
    Weapon ta_weapon = (Weapon)((rand() % 3) + 1);
    IMSG("TA Weapon is : %s", weapon_to_string(ta_weapon));

    /*
     * Try to calculate the game result and store it to the second param.
     * In case of not determined result return TEE_ERROR_GENERIC.
     */
    if ((params[1].value.a = (int)calculate_result(host_weapon, ta_weapon)) ==
        NOT_DETERMINED) {
        IMSG("The result could not be determined.\n");
        return TEE_ERROR_GENERIC;
    }

    return TEE_SUCCESS;
}

/* Helper function that prints data in hex, optinally adding a newline */
void print_hex(uint8_t* data, size_t size, bool add_newline) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X", data[i]);
    }
    if (add_newline) {
        printf("\n");
    }
}

/*
 * Performs the attestation as described in task 3.
 * The pseudo TA (PTA) attestation module is used for that purpose.
 * The respective header was used for reference:
 * https://github.com/OP-TEE/optee_os/blob/master/lib/libutee/include/pta_attestation.h
 * To get the public key the PTA_ATTESTATION_GET_PUBKEY function was used.
 * The data it returns though is not an RSA key but instead, the modulus and the
 * exponent of key. To get the hash the function PTA_ATTESTATION_HASH_TA_MEMORY
 * is used. The nonce is 32-byte randomly generated value with
 * TEE_GenerateRandom. Also the regression xtests contain useful examples:
 * https://github.com/OP-TEE/optee_test/blob/master/host/xtest/regression_1000.c#L2900-L2969
 *
 * Return codes:
 * TEE_SUCCESS
 * Attestation function codes. Refer to:
 * https://github.com/OP-TEE/optee_os/blob/master/core/pta/attestation.c
 */
static TEE_Result remote_attestation() {
    TEE_Result res;
    TEE_TASessionHandle session = TEE_HANDLE_NULL;
    uint32_t ret_origin = 0;

    // UUID for the attestation PTA
    TEE_UUID pta_attestation_uuid = PTA_ATTESTATION_UUID;

    // ------------ Open Session to PTA ---------
    res = TEE_OpenTASession(&pta_attestation_uuid, 0, 0, NULL, &session,
                            &ret_origin);
    if (res != TEE_SUCCESS) {
        EMSG("Could not open session with PTA: %u (origin: %u)\n", res,
             ret_origin);
        return res;
    }

    uint8_t n[MAX_KEYSIZE / 8] = {};
    uint8_t e[3] = {}; /* We know exponent == 65537 for RSA */
    size_t n_sz = sizeof(n);
    size_t e_sz = sizeof(e);
    TEE_Param key_params[4];
    key_params[0].memref.buffer = e;
    key_params[0].memref.size = e_sz;
    key_params[1].memref.buffer = n;
    key_params[1].memref.size = n_sz;

    uint32_t ta_param_types_get_key = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE);

    // ------------ Get Public Key for Later Verification ---------
    res = TEE_InvokeTACommand(session, 0, PTA_ATTESTATION_GET_PUBKEY,
                              ta_param_types_get_key, key_params, &ret_origin);
    if (res != TEE_SUCCESS) {
        EMSG("Could not invoke PTA command to get public key: %u\n", res);
        return res;
    }

    // Print the key in the format key(N, e), where N = modulus, e = exponent
    if (key_params[0].memref.size > 0 && key_params[1].memref.size > 0) {
        printf("Public Key : (");
        print_hex((uint8_t*)key_params[0].memref.buffer,
                  key_params[0].memref.size, false);
        printf(",");
        print_hex((uint8_t*)key_params[1].memref.buffer,
                  key_params[1].memref.size, false);
        printf(")\n");
    }

    TEE_Param ta_params[4];
    uint8_t nonce[32];
    TEE_GenerateRandom(nonce, sizeof(nonce));
    ta_params[0].memref.buffer = nonce;
    ta_params[0].memref.size = sizeof(nonce);
    uint8_t signed_hash[TEE_SHA256_HASH_SIZE + (MAX_KEYSIZE / 8)] = {};
    ta_params[1].memref.buffer = signed_hash;
    ta_params[1].memref.size = sizeof(signed_hash);
    uint32_t ta_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

    // ------------ Get the Signed Hash for the TA ---------
    res = TEE_InvokeTACommand(session, 0, PTA_ATTESTATION_HASH_TA_MEMORY,
                              ta_param_types, ta_params, &ret_origin);
    if (res != TEE_SUCCESS) {
        EMSG("Could not invoke PTA command to get signed hash: %u\n", res);
        return res;
    }

    printf("TA nonce: \n");
    print_hex((uint8_t*)ta_params[0].memref.buffer, ta_params[0].memref.size,
              true);

    /*
     * First 64 bytes (256 bits) are the SHA-256 hash.
     * It is followed by the 3072-bit RSA signature.
     */
    printf("TA signed hash: \n");
    print_hex((uint8_t*)ta_params[1].memref.buffer, ta_params[1].memref.size,
              true);

    // ------------ Close Session to PTA ---------//
    TEE_CloseTASession(session);

    return TEE_SUCCESS;
}

/*
 * Called when a TA is invoked. sess_ctx hold that value that was
 * assigned by TA_OpenSessionEntryPoint(). The rest of the paramters
 * comes from normal world.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused* sess_ctx,
                                      uint32_t cmd_id,
                                      uint32_t param_types,
                                      TEE_Param params[4]) {
    (void)&sess_ctx; /* Unused parameter */

    /* Run the attestation here and if it succeeeds proceed with the command */
    if (remote_attestation() != TEE_SUCCESS) {
        EMSG("Attestation failed. \n");
        return TEE_ERROR_GENERIC;
    }

    switch (cmd_id) {
        case ROCK_PAPER_SCISSORS_TA_CMD_PLAY_VALUE: {
            return play_game(param_types, params);
        }
        default: {
            return TEE_ERROR_BAD_PARAMETERS;
        }
    }
}
