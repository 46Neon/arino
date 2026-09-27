# Ariño — lexer binario mínimo

Prototipo inicial de un lexer de frases en español, centrado en una tabla de transición y código nativo x86-64. Tokeniza, por ejemplo, `entrena el modelo con estos datos` y `con un factor de 0.05`.

## Qué implementa

- Tabla de clases de 256 entradas: cada byte del búfer se clasifica mediante acceso directo; los bytes UTF-8 se conservan en el lexema y se tratan como parte de una palabra.
- Matrices `NEXT_STATE` y `ACTIONS`: la transición y las microoperaciones del lexer salen de tablas, no de una cadena `if/else` o `switch` por carácter.
- Micro-opcodes de un byte en `arino_opcodes.h` (`ADVANCE_CURSOR`, `SET_SPAN_START`, `EMIT_TOKEN`, `DISCARD_BYTE`, `REPORT_ERROR`, `RECORD_DECIMAL`). Están escritos como literales binarios y se compilan a instrucciones de máquina.
- Cero asignaciones dinámicas: el llamador proporciona el búfer de entrada y un arreglo fijo de tokens. Cada lexema es un par puntero/longitud que apunta al búfer original.
- FNV-1a de 32 bits en `fnv1a_x86_64.S`: el hash va en RAX/EAX y RBX recorre los bytes. La tabla fija de palabras reservadas separa verbos de acción, entidades de IA y operadores lógicos.
- Números decimales con signo opcional convertidos a bits IEEE-754 binary64. El token conserva tanto la vista del texto como el valor binario.
- Estado de error con offset exacto de byte para números incompletos/fuera de rango, argumentos inválidos o salida insuficiente.

Palabras reconocidas en minúsculas:

- Verbos-Acción: `entrenar`, `entrena`, `clasificar`, `clasifica`, `predecir`, `predice`.
- Entidades-IA: `modelo`, `datos`, `tensor`, `capa`.
- Operadores-Lógicos: `si`, `entonces`, `mientras`.

Las demás palabras se devuelven como `ARINO_TOKEN_WORD`; los signos de puntuación son tokens `ARINO_TOKEN_SYMBOL`. Se descartan bytes de control ASCII y espacios. Se reconoce el punto decimal, no exponentes como `1e-3`.

## Compilar y probar

Requiere un compilador C compatible con GNU C, GNU assembler y x86-64 Linux/System V:

```sh
make test
```

## Límites intencionales de esta capa

- Es un lexer, no un parser: informa errores léxicos con offset; la gramática y sus errores corresponden a una capa posterior.
- El lexer no puede elegir una dirección de memoria para un tensor que todavía no ha sido asignado. Devuelve los bits binary64 del literal; el asignador/optimizador debe colocar el valor en memoria y asociar su dirección.
- Calcular FNV-1a cuesta O(n) en la longitud de la palabra. La búsqueda posterior en la tabla fija de 32 slots es O(1) acotado; el hash no es O(1) respecto del tamaño del texto.
- Los bytes altos se preservan como bytes de palabra; esta primera versión no valida secuencias UTF-8 malformadas ni normaliza mayúsculas/minúsculas.
- El motor está escrito en C más una rutina x86-64 en ensamblador, y el compilador lo transforma en código máquina. Las tablas/opcodes son binarios; el código fuente se conserva para poder revisarlo y reconstruir el ejecutable.
