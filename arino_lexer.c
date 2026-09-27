#include "arino_lexer.h"
#include "arino_opcodes.h"

#include <float.h>
#include <math.h>
#include <string.h>

_Static_assert(sizeof(double) == sizeof(uint64_t) && DBL_MANT_DIG == 53 &&
               DBL_MAX_EXP == 1024, "Ariño requires IEEE-754 binary64");

/* Byte classes: control/space, word/UTF-8 byte, digit, dot, sign, other. */
enum ByteClass {
    BC_CONTROL = 0,
    BC_WORD = 1,
    BC_DIGIT = 2,
    BC_DOT = 3,
    BC_SIGN = 4,
    BC_OTHER = 5,
    BC_COUNT = 6
};

enum LexerState {
    ST_IDLE = 0,
    ST_WORD = 1,
    ST_INTEGER = 2,
    ST_DECIMAL_DOT = 3,
    ST_FRACTION = 4,
    ST_SIGN = 5,
    ST_COUNT = 6
};

/* Raw input byte -> class is a direct 256-entry lookup. */
static const uint8_t BYTE_CLASS[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 5, 4, 3, 5,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5, 5,
    5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 5, 5,
    5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 5, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/* next_state[state][byte_class], then ACTIONS supplies binary micro-opcodes. */
static const uint8_t NEXT_STATE[ST_COUNT][BC_COUNT] = {
    { ST_IDLE, ST_WORD, ST_INTEGER, ST_IDLE, ST_SIGN, ST_IDLE },
    { ST_IDLE, ST_WORD, ST_WORD, ST_IDLE, ST_IDLE, ST_IDLE },
    { ST_IDLE, ST_IDLE, ST_INTEGER, ST_DECIMAL_DOT, ST_IDLE, ST_IDLE },
    { ST_IDLE, ST_IDLE, ST_FRACTION, ST_IDLE, ST_IDLE, ST_IDLE },
    { ST_IDLE, ST_IDLE, ST_FRACTION, ST_IDLE, ST_IDLE, ST_IDLE },
    { ST_IDLE, ST_IDLE, ST_INTEGER, ST_IDLE, ST_IDLE, ST_IDLE }
};

#define OP_ADV ARINO_OP_ADVANCE_CURSOR
#define OP_START ARINO_OP_SET_SPAN_START
#define OP_EMIT ARINO_OP_EMIT_TOKEN
#define OP_SKIP ARINO_OP_DISCARD_BYTE
#define OP_ERROR ARINO_OP_REPORT_ERROR
#define OP_DOT ARINO_OP_RECORD_DECIMAL
#define OP_SYMBOL ARINO_OP_EMIT_SYMBOL

static const uint8_t ACTIONS[ST_COUNT][BC_COUNT] = {
    { OP_SKIP | OP_ADV, OP_START | OP_ADV, OP_START | OP_ADV,
      OP_START | OP_ADV | OP_EMIT | OP_SYMBOL, OP_START | OP_ADV,
      OP_START | OP_ADV | OP_EMIT | OP_SYMBOL },
    { OP_EMIT, OP_ADV, OP_ADV, OP_EMIT, OP_EMIT, OP_EMIT },
    { OP_EMIT, OP_EMIT, OP_ADV, OP_ADV | OP_DOT, OP_EMIT, OP_EMIT },
    { OP_ERROR, OP_ERROR, OP_ADV, OP_ERROR, OP_ERROR, OP_ERROR },
    { OP_EMIT, OP_EMIT, OP_ADV, OP_EMIT, OP_EMIT, OP_EMIT },
    { OP_ERROR, OP_ERROR, OP_ADV, OP_ERROR, OP_ERROR, OP_ERROR }
};

#define KEYWORD_SLOTS 32u

typedef struct Keyword {
    uint32_t hash;
    uint8_t length;
    uint8_t kind;
    const char *text;
} Keyword;

/* FNV-1a hashes are precomputed; entrenar/tensor share slot 4 and probe once. */
static const Keyword KEYWORDS[KEYWORD_SLOTS] = {
    [4]  = { 0x65b85b24u, 8u, ARINO_TOKEN_ACTION, "entrenar" },
    [5]  = { 0x15725f24u, 6u, ARINO_TOKEN_ENTITY, "tensor" },
    [6]  = { 0x79c51206u, 10u, ARINO_TOKEN_ACTION, "clasificar" },
    [8]  = { 0x9adfa0a8u, 5u, ARINO_TOKEN_ENTITY, "datos" },
    [13] = { 0x2d4db0edu, 7u, ARINO_TOKEN_ACTION, "predice" },
    [14] = { 0xb723262eu, 4u, ARINO_TOKEN_ENTITY, "capa" },
    [15] = { 0x2071e56fu, 6u, ARINO_TOKEN_ENTITY, "modelo" },
    [16] = { 0x4cc381d0u, 9u, ARINO_TOKEN_ACTION, "clasifica" },
    [17] = { 0x405210f1u, 2u, ARINO_TOKEN_LOGIC, "si" },
    [18] = { 0xf627f132u, 8u, ARINO_TOKEN_LOGIC, "mientras" },
    [20] = { 0x8bc68414u, 8u, ARINO_TOKEN_LOGIC, "entonces" },
    [21] = { 0xea94e555u, 8u, ARINO_TOKEN_ACTION, "predecir" },
    [30] = { 0x8cc4bebeu, 7u, ARINO_TOKEN_ACTION, "entrena" }
};

static uint8_t classify_keyword(const uint8_t *bytes, size_t length, uint32_t hash)
{
    size_t slot = (size_t)(hash & (KEYWORD_SLOTS - 1u));
    size_t probes;

    for (probes = 0; probes < KEYWORD_SLOTS; ++probes) {
        const Keyword *entry = &KEYWORDS[slot];
        if (entry->text == NULL) {
            return ARINO_TOKEN_WORD;
        }
        if (entry->hash == hash && (size_t)entry->length == length &&
            memcmp(bytes, entry->text, length) == 0) {
            return entry->kind;
        }
        slot = (slot + 1u) & (KEYWORD_SLOTS - 1u);
    }
    return ARINO_TOKEN_WORD;
}

static ArinoError parse_decimal64(const uint8_t *bytes,
                                  size_t length,
                                  double *result,
                                  size_t *bad_index)
{
    size_t i = 0;
    int negative = 0;
    double value = 0.0;

    if (i < length && (bytes[i] == (uint8_t)'-' || bytes[i] == (uint8_t)'+')) {
        negative = (bytes[i] == (uint8_t)'-');
        ++i;
    }
    while (i < length && bytes[i] != (uint8_t)'.') {
        value = value * 10.0 + (double)(bytes[i] - (uint8_t)'0');
        if (!isfinite(value)) {
            *bad_index = i;
            return ARINO_ERROR_NUMBER_RANGE;
        }
        ++i;
    }
    if (i < length && bytes[i] == (uint8_t)'.') {
        double scale = 0.1;
        ++i;
        while (i < length) {
            value += (double)(bytes[i] - (uint8_t)'0') * scale;
            if (!isfinite(value)) {
                *bad_index = i;
                return ARINO_ERROR_NUMBER_RANGE;
            }
            scale *= 0.1;
            ++i;
        }
    }
    *result = negative ? -value : value;
    return ARINO_OK;
}

static ArinoError emit_token(const uint8_t *input,
                             size_t start,
                             size_t end,
                             uint8_t state,
                             uint8_t symbol,
                             ArinoToken *output,
                             size_t output_capacity,
                             size_t *count,
                             size_t *failure_offset)
{
    ArinoToken *token;
    ArinoTokenKind kind;

    if (*count >= output_capacity) {
        *failure_offset = start;
        return ARINO_ERROR_OUTPUT_FULL;
    }

    token = &output[*count];
    token->lexeme = input + start;
    token->length = end - start;
    token->offset = start;
    token->hash = 0u;
    token->ieee754_bits = 0u;

    if (symbol != 0u) {
        kind = ARINO_TOKEN_SYMBOL;
    } else if (state == ST_WORD) {
        token->hash = arino_fnv1a32_x86_64(token->lexeme, token->length);
        kind = (ArinoTokenKind)classify_keyword(token->lexeme,
                                                token->length,
                                                token->hash);
    } else {
        double number = 0.0;
        size_t bad_index = 0u;
        const ArinoError error = parse_decimal64(token->lexeme, token->length,
                                                &number, &bad_index);
        if (error != ARINO_OK) {
            *failure_offset = start + bad_index;
            return error;
        }
        kind = ARINO_TOKEN_NUMBER;
        memcpy(&token->ieee754_bits, &number, sizeof(number));
    }

    token->kind = kind;
    ++(*count);
    return ARINO_OK;
}

size_t arino_lex(const uint8_t *input,
                 size_t input_length,
                 ArinoToken *output,
                 size_t output_capacity,
                 ArinoStatus *status)
{
    size_t count = 0;
    size_t position = 0;
    size_t span_start = 0;
    size_t decimal_offset = 0;
    uint8_t state = ST_IDLE;

    if (status == NULL) {
        return 0;
    }
    status->code = ARINO_OK;
    status->error_offset = input_length;
    if ((input == NULL && input_length != 0u) ||
        (output == NULL && output_capacity != 0u)) {
        status->code = ARINO_ERROR_ARGUMENT;
        status->error_offset = 0u;
        return 0;
    }

    while (position < input_length) {
        const uint8_t byte_class = BYTE_CLASS[input[position]];
        const uint8_t opcode = ACTIONS[state][byte_class];

        if ((opcode & OP_ERROR) != 0u) {
            status->code = ARINO_ERROR_BAD_NUMBER;
            status->error_offset = (state == ST_DECIMAL_DOT) ? decimal_offset : span_start;
            return count;
        }
        if ((opcode & OP_START) != 0u) {
            span_start = position;
        }
        if ((opcode & OP_DOT) != 0u) {
            decimal_offset = position;
        }
        if ((opcode & OP_ADV) != 0u) {
            ++position;
        }
        if ((opcode & OP_SKIP) != 0u) {
            state = NEXT_STATE[state][byte_class];
            continue;
        }
        if ((opcode & OP_EMIT) != 0u) {
            size_t failure_offset = span_start;
            const ArinoError error = emit_token(input, span_start, position,
                                                state,
                                                (uint8_t)((opcode & OP_SYMBOL) != 0u),
                                                output, output_capacity, &count,
                                                &failure_offset);
            if (error != ARINO_OK) {
                status->code = error;
                status->error_offset = failure_offset;
                return count;
            }
            state = ST_IDLE;
            continue; /* boundary byte is reprocessed by the idle state */
        }
        state = NEXT_STATE[state][byte_class];
    }

    if (state == ST_DECIMAL_DOT || state == ST_SIGN) {
        status->code = ARINO_ERROR_BAD_NUMBER;
        status->error_offset = (state == ST_DECIMAL_DOT) ? decimal_offset : span_start;
        return count;
    }
    if (state == ST_WORD || state == ST_INTEGER || state == ST_FRACTION) {
        size_t failure_offset = span_start;
        const ArinoError error = emit_token(input, span_start, position, state, 0u,
                                            output, output_capacity, &count,
                                            &failure_offset);
        if (error != ARINO_OK) {
            status->code = error;
            status->error_offset = failure_offset;
        }
    }
    return count;
}
