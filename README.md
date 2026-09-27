# Ariño — lexer y máquina virtual IL en binarios nativos

Prototipo de infraestructura para Ariño, un lenguaje cuya sintaxis visible se mantiene en español. El repositorio distribuye artefactos binarios; no conserva fuentes C ni ensamblador de las implementaciones nativas.

## Lexer

- `libarino_lexer.so`: biblioteca Linux x86-64 / System V ABI.
- `arino_lexer_tests`: ejecutable de pruebas nativo que carga la biblioteca desde su propio directorio.
- El lexer reconoce palabras de acción, entidades de IA, operadores lógicos, símbolos y números decimales con signo opcional; preserva los lexemas como vistas zero-copy al búfer original.

GitHub Actions valida los binarios y ejecuta las pruebas del lexer.

## Ariño IL v1

La IL es un formato interno de bytecode; **no modifica la sintaxis visible en español**. `arino_il_vm` es una VM/verificador/desensamblador nativa Linux x86-64. Los archivos `.aril` son bytecode de la VM, no código máquina de la CPU. Consulta el [contrato del formato, la ISA y el mapeo previsto de AST/símbolos](docs/ARINO_IL_V1.md).

Uso:

```sh
./arino_il_vm --verify arino_il_demo.aril
./arino_il_vm --disasm arino_il_demo.aril
./arino_il_vm --run arino_il_demo.aril
./arino_il_vm --trace arino_il_demo.aril
./arino_il_vm --max-steps 20 arino_il_infinite.aril
```

La VM valida encabezado y bytecode (opcodes, operandos, índices y destinos de salto) y ejecuta operaciones I64/F64, comparaciones, saltos y slots locales; informa trampas de ejecución, incluidos overflow, división por cero, tipos incompatibles, underflow y agotamiento del límite de instrucciones. Las fixtures `.aril` cubren resultados correctos, control de flujo, constantes, operaciones de pila, aritmética/comparación F64, verificación negativa y traps.

CI prueba el formato del ELF, verifica y ejecuta las fixtures con resultados exactos, comprueba los rechazos/traps esperados y publica los binarios y fixtures como artefacto descargable.

## Límites

El lexer no es un parser. La VM hace ejecutable el bytecode escrito directamente en `.aril`, pero **todavía no hay parser/AST ni emisor que compile programas fuente Ariño a IL**; por tanto, Ariño no cuenta aún con un compilador integral. El mapeo descrito en la documentación es un contrato de diseño para esa etapa futura. Los binarios publicados son específicos de Linux x86-64 y no son portables a otras arquitecturas.
