#include "arino_lexer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t run(const char *text, ArinoToken *tokens, size_t capacity, ArinoStatus *status)
{
    return arino_lex((const uint8_t *)text, strlen(text), tokens, capacity, status);
}

static void test_fnv1a_machine_code_helper(void)
{
    const uint8_t text[] = { 'h', 'e', 'l', 'l', 'o' };
    assert(arino_fnv1a32_x86_64(text, sizeof(text)) == UINT32_C(0x4f9f2cab));
}

static void test_span_and_sample_sentence(void)
{
    const char text[] = "entrena el modelo con estos datos";
    ArinoToken tokens[8];
    ArinoStatus status;
    const size_t count = run(text, tokens, 8u, &status);

    assert(status.code == ARINO_OK);
    assert(count == 6u);
    assert(tokens[0].kind == ARINO_TOKEN_ACTION);
    assert(tokens[1].kind == ARINO_TOKEN_WORD);
    assert(tokens[2].kind == ARINO_TOKEN_ENTITY);
    assert(tokens[5].kind == ARINO_TOKEN_ENTITY);
    assert(tokens[2].length == 6u);
    assert(memcmp(tokens[2].lexeme, "modelo", 6u) == 0);
    for (size_t i = 0; i < count; ++i) {
        assert(tokens[i].lexeme == (const uint8_t *)text + tokens[i].offset);
    }
}

static void test_keyword_categories(void)
{
    const char text[] = "entrenar entrena clasificar clasifica predecir predice modelo datos tensor capa si entonces mientras";
    const ArinoTokenKind expected[] = {
        ARINO_TOKEN_ACTION, ARINO_TOKEN_ACTION, ARINO_TOKEN_ACTION,
        ARINO_TOKEN_ACTION, ARINO_TOKEN_ACTION, ARINO_TOKEN_ACTION,
        ARINO_TOKEN_ENTITY, ARINO_TOKEN_ENTITY, ARINO_TOKEN_ENTITY,
        ARINO_TOKEN_ENTITY, ARINO_TOKEN_LOGIC, ARINO_TOKEN_LOGIC,
        ARINO_TOKEN_LOGIC
    };
    ArinoToken tokens[16];
    ArinoStatus status;
    const size_t count = run(text, tokens, 16u, &status);

    assert(status.code == ARINO_OK);
    assert(count == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < count; ++i) {
        assert(tokens[i].kind == expected[i]);
        assert(tokens[i].hash != 0u);
    }
}

static void test_float_and_ieee754_payload(void)
{
    const char text[] = "con un factor de 0.05";
    ArinoToken tokens[8];
    ArinoStatus status;
    const size_t count = run(text, tokens, 8u, &status);

    assert(status.code == ARINO_OK);
    assert(count == 5u);
    assert(tokens[4].kind == ARINO_TOKEN_NUMBER);
    assert(tokens[4].length == 4u);
    assert(tokens[4].ieee754_bits == UINT64_C(0x3fa999999999999a));
}

static void test_utf8_bytes_and_controls(void)
{
    const char text[] = "\t\nacci\xc3\xb3n\r modelo";
    ArinoToken tokens[4];
    ArinoStatus status;
    const size_t count = run(text, tokens, 4u, &status);

    assert(status.code == ARINO_OK);
    assert(count == 2u);
    assert(tokens[0].kind == ARINO_TOKEN_WORD);
    assert(tokens[0].length == 7u);
    assert(tokens[1].kind == ARINO_TOKEN_ENTITY);
}

static void test_symbols(void)
{
    const char text[] = "modelo,";
    ArinoToken tokens[4];
    ArinoStatus status;
    const size_t count = run(text, tokens, 4u, &status);

    assert(status.code == ARINO_OK);
    assert(count == 2u);
    assert(tokens[0].kind == ARINO_TOKEN_ENTITY);
    assert(tokens[1].kind == ARINO_TOKEN_SYMBOL);
    assert(tokens[1].length == 1u);
    assert(tokens[1].lexeme[0] == (uint8_t)',');
}

static void test_bad_number_offsets(void)
{
    const char decimal[] = "valor 1.";
    const char sign[] = "x - y";
    ArinoToken tokens[8];
    ArinoStatus status;
    size_t count;

    count = run(decimal, tokens, 8u, &status);
    assert(count == 1u);
    assert(status.code == ARINO_ERROR_BAD_NUMBER);
    assert(status.error_offset == 7u);

    count = run(sign, tokens, 8u, &status);
    assert(count == 1u);
    assert(status.code == ARINO_ERROR_BAD_NUMBER);
    assert(status.error_offset == 2u);
}

static void test_number_range_offset(void)
{
    char text[311];
    ArinoToken token[1];
    ArinoStatus status;
    memset(text, '9', sizeof(text) - 1u);
    text[sizeof(text) - 1u] = '\0';

    assert(run(text, token, 1u, &status) == 0u);
    assert(status.code == ARINO_ERROR_NUMBER_RANGE);
    assert(status.error_offset < sizeof(text) - 1u);
}

static void test_output_capacity_error(void)
{
    const char text[] = "uno dos";
    ArinoToken tokens[1];
    ArinoStatus status;
    const size_t count = run(text, tokens, 1u, &status);

    assert(count == 1u);
    assert(status.code == ARINO_ERROR_OUTPUT_FULL);
    assert(status.error_offset == 4u);
}

int main(void)
{
    test_fnv1a_machine_code_helper();
    test_span_and_sample_sentence();
    test_keyword_categories();
    test_float_and_ieee754_payload();
    test_utf8_bytes_and_controls();
    test_symbols();
    test_bad_number_offsets();
    test_number_range_offset();
    test_output_capacity_error();
    puts("arino lexer tests: OK");
    return 0;
}
