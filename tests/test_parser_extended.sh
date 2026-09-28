#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
compiler="$root/arino_ast_compiler"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cat >"$tmp/valid.ari" <<'ARI'
define agente Asistente {
  datos entrenamiento : tensor[64, 28, 28];
  modelo clasificador entrada tensor[784] salida tensor[10];
  capa oculta : densa[784, 128] para clasificador;
  entrena el modelo clasificador con los datos entrenamiento;
  clasifica el modelo clasificador con los datos entrenamiento;
  predice clasificador con imagen;
  invoca la función revisar con (resultado, 2 mas 3 por 4);
  si resultado mayor o igual que 2 entonces {
    si nivel menor que 1 entonces { valor = sumar 1 y 2; } sino { valor = 0; }
  } sino { valor = 1; }
}
ARI
"$compiler" --check "$tmp/valid.ari" >"$tmp/check.log"
grep -F 'OK análisis sintáctico Ariño:' "$tmp/check.log"
"$compiler" --ast "$tmp/valid.ari" >"$tmp/ast.log"
for node in AGENT_DECL DATA_DECL MODEL_DECL LAYER_DECL TRAIN_MODEL AI_CALL IF GE LT ADD MUL ASSIGN; do
  grep -F "$node" "$tmp/ast.log" >/dev/null
done
grep -F 'name=Asistente' "$tmp/ast.log" >/dev/null
# The multiply node must be nested inside the add tree (normal precedence).
grep -A3 -F 'ADD span=' "$tmp/ast.log" | grep -F 'MUL span=' >/dev/null

# Spans are UTF-8 byte offsets. The two-byte ñ occupies bytes 14..16.
printf '%s' 'define agente ñ { valor = sumar 1 y 2; }' >"$tmp/utf8.ari"
"$compiler" --ast "$tmp/utf8.ari" >"$tmp/utf8.log"
grep -F 'AGENT_DECL span=0..41 name=ñ' "$tmp/utf8.log" >/dev/null

# Recover after independent statement errors and report both in Spanish.
cat >"$tmp/recovery.ari" <<'ARI'
define agente A {
  datos D : tensor[0];
  capa L : densa[0, 2] para M;
  valor = 7;
}
ARI
if "$compiler" --check "$tmp/recovery.ari" >"$tmp/recovery.log" 2>&1; then
  echo 'expected syntax errors' >&2; exit 1
fi
test "$(grep -c '^Error sintáctico en byte ' "$tmp/recovery.log")" -ge 2
grep -F 'la dimensión debe ser mayor que cero' "$tmp/recovery.log" >/dev/null
# The valid assignment following the errors is still represented if errors are recovered.

printf '%s' 'define agente A { si x mayor que 0 entonces { y = 1; }' >"$tmp/malformed-block.ari"
if "$compiler" --check "$tmp/malformed-block.ari" >"$tmp/malformed.log" 2>&1; then
  echo 'expected missing-brace rejection' >&2; exit 1
fi
grep -F "se esperaba '}' para cerrar el bloque" "$tmp/malformed.log" >/dev/null
printf '%s' 'define agente A { valor = 1 + 2; }' >"$tmp/lexer-error.ari"
if "$compiler" --check "$tmp/lexer-error.ari" >"$tmp/lexer-error.log" 2>&1; then
  echo 'expected lexer error in extended syntax' >&2; exit 1
fi
grep -F 'el lexer no reconoce un token' "$tmp/lexer-error.log" >/dev/null

# The parser front accepts syntax but does not claim IL lowering yet, and writes no output.
if "$compiler" --compile "$tmp/valid.ari" "$tmp/partial.arino" >"$tmp/backend.log" 2>&1; then
  echo 'expected parser-only backend rejection' >&2; exit 1
fi
grep -F 'aún no tienen emisión IL v2' "$tmp/backend.log" >/dev/null
test ! -e "$tmp/partial.arino"

# Legacy scalar input continues through the prior native core.
printf '%s' 'sumar 2 y 3' >"$tmp/scalar.ari"
"$compiler" --compile "$tmp/scalar.ari" "$tmp/scalar.arino" >/dev/null
test "$("$root/arino_vm" --run "$tmp/scalar.arino")" = 'RESULT I64(5)'
echo 'Extended parser tests: PASS'
