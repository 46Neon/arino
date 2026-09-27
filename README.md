# Ariño — lexer nativo binario

Prototipo de lexer de frases en español distribuido como binarios ELF nativos para Linux x86-64 (System V). El repositorio conserva `libarino_lexer.so`, su ejecutable de pruebas `arino_lexer_tests` y `arino_lexer.h`, que define la interfaz pública; no conserva la implementación C ni ensamblador.

## Binarios y compatibilidad

- `libarino_lexer.so`: biblioteca ELF x86-64 de 64 bits, compartida y enlazada dinámicamente.
- `arino_lexer_tests`: ejecutable ELF x86-64 que carga la biblioteca desde su propio directorio (`$ORIGIN`).
- Los binarios se prueban en GitHub Actions sobre `ubuntu-latest`. Requieren Linux x86-64 y un entorno compatible con GNU/Linux; no son código fuente portable ni se pueden reconstruir desde este repositorio.
- GitHub Actions ejecuta el binario de pruebas y publica ambos archivos como artefacto `arino-lexer-linux-x86_64`.

## Interfaz

La declaración ABI está en `arino_lexer.h`. La función exportada `arino_lex` recibe un búfer de entrada, su longitud en bytes, un arreglo de salida preasignado de tokens, su capacidad y un estado opcional. Devuelve la cantidad de tokens producidos. No asigna memoria: cada lexema es una vista (puntero y longitud) al búfer de entrada del llamador. El llamador debe mantener vivo ese búfer mientras use los tokens.

Cada token expone tipo, offset de bytes, hash FNV-1a de 32 bits para lexemas de palabra y, para números, el patrón de bits IEEE-754 binary64. `ArinoStatus` informa el código de error y el offset del byte; en éxito, el offset es la longitud de entrada. Los enums y la disposición de estructuras son los definidos en el header y forman parte de la ABI.

## Comportamiento léxico

- Tabla de clases de 256 entradas y DFA con matrices de estados/acciones.
- Bytes UTF-8 conservados dentro del lexema; no valida secuencias UTF-8 malformadas ni normaliza mayúsculas/minúsculas.
- Verbos-Acción: `entrenar`, `entrena`, `clasificar`, `clasifica`, `predecir`, `predice`.
- Entidades-IA: `modelo`, `datos`, `tensor`, `capa`.
- Operadores-Lógicos: `si`, `entonces`, `mientras`.
- Las demás palabras son `ARINO_TOKEN_WORD`; los signos son `ARINO_TOKEN_SYMBOL`. Se descartan espacios y controles ASCII.
- Números decimales con signo opcional; se reconoce punto decimal, no exponentes como `1e-3`.

## Límites

- Es un lexer, no un parser: informa errores léxicos con offset; la gramática corresponde a una capa posterior.
- El lexer no elige la dirección de memoria de un tensor. Devuelve bits binary64; un asignador/optimizador posterior debe ubicar el valor y asociar su dirección.
- Calcular FNV-1a cuesta O(n) en la longitud de la palabra; la búsqueda posterior en la tabla fija de palabras es O(1) acotado.
- El formato es específico de Linux x86-64; no debe confundirse con bytecode portable ni código fuente reconstruible.
