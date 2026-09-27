#ifndef ARINO_LEXER_H
#define ARINO_LEXER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ArinoTokenKind {
    ARINO_TOKEN_WORD = 0,
    ARINO_TOKEN_ACTION = 1,
    ARINO_TOKEN_ENTITY = 2,
    ARINO_TOKEN_LOGIC = 3,
    ARINO_TOKEN_NUMBER = 4,
    ARINO_TOKEN_SYMBOL = 5
} ArinoTokenKind;

typedef struct ArinoToken {
    const uint8_t *lexeme;       /* zero-copy view into the original input */
    size_t length;
    size_t offset;               /* byte offset from the start of input */
    ArinoTokenKind kind;
    uint32_t hash;               /* FNV-1a for word-like lexemes */
    uint64_t ieee754_bits;       /* binary64 payload for number tokens */
} ArinoToken;

typedef enum ArinoError {
    ARINO_OK = 0,
    ARINO_ERROR_ARGUMENT = 1,
    ARINO_ERROR_BAD_NUMBER = 2,
    ARINO_ERROR_NUMBER_RANGE = 3,
    ARINO_ERROR_OUTPUT_FULL = 4
} ArinoError;

typedef struct ArinoStatus {
    ArinoError code;
    size_t error_offset;
} ArinoStatus;

/*
 * Tokenizes UTF-8 byte buffers without allocating memory.
 * The caller owns input and the preallocated token output array.
 * On success, error_offset equals input_length.
 * On error, the offset identifies the offending byte in the original input.
 */
size_t arino_lex(const uint8_t *input,
                 size_t input_length,
                 ArinoToken *output,
                 size_t output_capacity,
                 ArinoStatus *status);

/* x86-64 System V implementation: input pointer in RDI, hash in EAX/RAX,
 * scan pointer in RBX. The helper preserves RBX as required by the ABI.
 */
uint32_t arino_fnv1a32_x86_64(const uint8_t *bytes, size_t length);

#ifdef __cplusplus
}
#endif

#endif
