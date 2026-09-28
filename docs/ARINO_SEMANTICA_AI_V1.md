# Semántica AI de Ariño — subconjunto explícito v1

## Estado y límites

`arino_ast_compiler` incorpora un parser y una pasada de resolución/semántica para el subconjunto de datos, modelos densos y acciones descrito abajo. `--check` valida y `--ast` muestra un HIR tipado con IDs estables, slots, formas y spans en bytes UTF-8. Esta pasada **no ejecuta ni entrena redes neuronales**. La VM IL v2 actual solo tiene operaciones escalares I64/F64/BOOL y no ofrece opcodes de tensor, capa, entrenamiento o clasificación. Por eso `--compile` informa el límite del backend y falla sin crear un `.arino` para cualquier programa con construcciones AI. Las operaciones aritméticas escalares existentes siguen usando su compilador y emisor IL v2 anteriores.

Los nombres y palabras clave distinguen mayúsculas de minúsculas; las palabras reservadas se escriben en minúsculas. Cada sentencia termina en `;`. Las declaraciones deben aparecer antes de las acciones. Las referencias se escriben con un identificador explícito: el compilador no deduce qué modelo o datos se quisieron decir.

## Gramática admitida

```ebnf
programa       = { declaracion } , { accion } ;
declaracion    = datos | modelo | capa ;
datos          = "datos" , identificador , ":" , "tensor" , forma , ";" ;
modelo         = "modelo" , identificador , "entrada" , "tensor" , vector ,
                 "salida" , "tensor" , vector , ";" ;
capa           = "capa" , identificador , ":" , "densa" , "[" , entero , "," , entero , "]" ,
                 "para" , identificador , ";" ;
forma          = "[" , entero , { "," , entero } , "]" ;
vector         = "[" , entero , "]" ;
accion         = entrenar | inferir ;
entrenar       = ("entrenar" | "entrena") , identificador , "con" , identificador , ";" ;
inferir        = ("clasificar" | "clasifica" | "predice" | "predecir") ,
                 identificador , "con" , identificador , ";" ;
identificador  = palabra no reservada del lexer ;
entero         = literal decimal I64 estrictamente positivo ;
```

Ejemplo que pasa `--check`:

```arino
datos entrenamiento : tensor[100,3];
modelo M entrada tensor[3] salida tensor[2];
capa oculta : densa[3,4] para M;
capa final : densa[4,2] para M;
entrenar M con entrenamiento;
clasificar M con entrenamiento;
```

## Tipos, símbolos y validaciones

- `datos` declara un tensor estático de elementos `F64`. Sus formas admiten de 1 a 8 dimensiones positivas; para las acciones de esta versión los datos deben tener rango 2 `[muestras, características]`.
- `modelo` declara una entrada y salida vectoriales de rango 1. Sus anchos deben ser enteros positivos.
- Cada `capa` declara una capa `densa[ancho_entrada, ancho_salida]`. El compilador resuelve el modelo de `para` y valida la cadena en orden de declaración para todo modelo que tenga capas, incluso si el modelo no se usa en ninguna acción: la primera entrada coincide con el ancho de entrada del modelo, cada capa siguiente recibe el ancho de salida de la anterior y la última salida coincide con la salida del modelo. Un modelo sin capas sigue siendo válido si no se usa; las acciones continúan exigiendo al menos una capa.
- Una acción de entrenamiento requiere datos de rango 2 y coincidencia de características con la entrada del modelo; un modelo debe tener al menos una capa y su topología debe ser compatible. Un modelo solo se entrena una vez en una unidad de compilación. `clasificar`/`predice` solo es válido después del entrenamiento y con dimensiones compatibles.
- Los símbolos reciben IDs/slots deterministas por orden de primera declaración. Los nombres se comparten en un espacio de símbolos: duplicados, referencias no declaradas y referencias a una clase de símbolo incorrecta fallan. Los mensajes identifican el offset de byte; los conflictos de dimensión indican valores esperados y recibidos.

## Rechazos deliberados

No se admiten tamaños inferidos, expresiones aritméticas en formas, dimensiones cero/negativas/decimales, modelos no vectoriales, tipos de capa distintos de `densa`, referencias omitidas, acciones sin `con`, acciones antes de terminar declaraciones, scopes anidados, tensores de datos con forma desconocida, dataset auto-seleccionado, aliases implícitos ni sintaxis conversacional libre como `entrena el modelo con estos datos`. Esa frase no nombra ni un modelo ni datos declarados por identificador y por tanto no se adivina. Tampoco se aceptan ramas, optimizadores, épocas, pesos o ejecución de modelos.

## CLI y artefactos

```sh
./arino_ast_compiler --check programa.ari
./arino_ast_compiler --ast programa.ari
./arino_ast_compiler --compile programa.ari salida.arino
```

El primer par efectúa análisis completo y presenta el HIR tipado. `--compile` devuelve error de backend para AI porque IL v2 carece de operaciones ejecutables de tensor; no crea bytecode parcial. El frontend conserva el compilador escalar previo como binario auxiliar `arino_ast_compiler_scalar`, que atiende el subconjunto aritmético y sus casos compatibles. `arino_ast_compiler` es el punto de entrada ELF x86-64 y no usa un wrapper Python para validar topologías; el núcleo nativo `arino_ast_compiler_core` realiza el análisis semántico. Los ejecutables publicados son binarios nativos.
