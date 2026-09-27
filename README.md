# Ariño — lexer nativo binario

Lexer inicial de frases en español. La implementación y el ejecutable de pruebas se distribuyen como binarios ELF nativos, no como código C o ensamblador.

## Binarios

- `libarino_lexer.so`: biblioteca compartida Linux x86-64 (System V ABI).
- `arino_lexer_tests`: ejecutable de pruebas nativo que carga la biblioteca desde su propio directorio.
- GitHub Actions ejecuta las pruebas y publica ambos binarios en cada build exitoso.

Los binarios son específicos de Linux x86-64 y de un entorno GNU/Linux compatible. El repositorio no conserva fuentes ni permite reconstruirlos. No son bytecode portable.

## Interfaz binaria

La biblioteca exporta `arino_lex`. Recibe un puntero al búfer de entrada, su longitud en bytes, un puntero a un arreglo de salida preasignado, su capacidad y un puntero opcional al estado. Devuelve la cantidad de tokens producidos. No reserva memoria: cada lexema es una vista al búfer original, que debe seguir vivo mientras se usen los tokens.

En la ABI Linux x86-64, cada token ocupa 40 bytes y se alinea a 8 bytes: puntero al lexema en el desplazamiento 0; longitud en bytes en 8; offset en la entrada en 16; tipo en 24; hash en 28; bits IEEE-754 binary64 en 32. El estado ocupa 16 bytes: código de error en 0 y offset de error en 8. Los punteros y `size_t` son de 64 bits; los campos de tipo y hash son de 32 bits.

Tipos de token: 0 palabra, 1 verbo de acción, 2 entidad de IA, 3 operador lógico, 4 número, 5 símbolo. Estados: 0 éxito, 1 argumento inválido, 2 número mal formado, 3 número fuera de rango, 4 salida insuficiente. En éxito, el offset de estado es la longitud de entrada; en error, identifica el byte problemático.

## Comportamiento léxico

Usa clases de byte y una tabla DFA de transiciones/acciones. Conserva los bytes UTF-8 dentro del lexema, pero no valida secuencias malformadas ni normaliza mayúsculas.

- Verbos de acción: `entrenar`, `entrena`, `clasificar`, `clasifica`, `predecir`, `predice`.
- Entidades de IA: `modelo`, `datos`, `tensor`, `capa`.
- Operadores lógicos: `si`, `entonces`, `mientras`.
- Las demás palabras se clasifican como palabra; signos como símbolo. Se descartan espacios y controles ASCII.
- Reconoce números decimales con signo opcional y punto decimal, y conserva su patrón binary64. No reconoce exponentes como `1e-3`.

El lexer no es un parser. Tampoco asigna direcciones de memoria a tensores: esa tarea corresponde a una capa posterior.
