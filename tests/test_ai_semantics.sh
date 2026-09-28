#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
compiler="$root/arino_ast_compiler"
valid='datos entrenamiento : tensor[100,3]; modelo M entrada tensor[3] salida tensor[2]; capa oculta : densa[3,4] para M; capa final : densa[4,2] para M; entrenar M con entrenamiento; clasificar M con entrenamiento;'
printf '%s' "$valid" >"$tmp/valid.ari"
"$compiler" --check "$tmp/valid.ari" >"$tmp/valid.log"
grep -F 'OK análisis semántico Ariño: 4 símbolos' "$tmp/valid.log"
grep -F 'HIR_TYPED id=1 slot=0 tipo=TENSOR<F64>' "$tmp/valid.log"
grep -F 'forma=[3,4]' "$tmp/valid.log"
"$compiler" --ast "$tmp/valid.ari" | grep -F 'HIR_TYPED id=4 slot=3'

reject() {
  local source="$1" needle="$2" name="$3"
  printf '%s' "$source" >"$tmp/$name.ari"
  if "$compiler" --check "$tmp/$name.ari" >"$tmp/$name.log" 2>&1; then
    echo "expected rejection: $name" >&2; exit 1
  fi
  grep -F "$needle" "$tmp/$name.log"
}
reject 'datos x : tensor[8,3]; datos x : tensor[8,3];' 'símbolo duplicado' duplicate
reject 'datos x : tensor[8,3]; entrenar M con x;' "referencia no declarada 'M'" undeclared-model
reject 'modelo M entrada tensor[3] salida tensor[2]; capa L : densa[3,2] para M; entrenar M con x;' "referencia no declarada 'x'" undeclared-data
reject 'datos x : tensor[8,];' 'se esperaba una dimensión entera positiva' incomplete-shape
reject 'datos x : tensor[8.5,3];' 'la dimensión debe ser I64, no F64' float-dimension
reject 'datos x : tensor[8,0];' 'las dimensiones deben ser mayores que cero' zero-dimension
reject 'datos x : tensor[8,3]; modelo M entrada tensor[3,2] salida tensor[2];' 'rango 1' model-rank
reject 'modelo M entrada tensor[3] salida tensor[2]; capa L : densa[4,2] para M;' 'entrada esperada=3, recibida=4' layer-input-mismatch
reject 'modelo M entrada tensor[3] salida tensor[2]; capa L : densa[3,4] para M;' 'salida declarada del modelo=2' model-output-mismatch

# Whole-program topology checking also applies to declared-but-unused models.
# A model with no layers remains legal until an action tries to use it.
unused_valid='modelo M entrada tensor[3] salida tensor[2]; capa oculta : densa[3,4] para M; capa final : densa[4,2] para M;'
printf '%s' "$unused_valid" >"$tmp/unused-valid.ari"
"$compiler" --check "$tmp/unused-valid.ari" | grep -F 'OK análisis semántico Ariño'
printf '%s' 'modelo M entrada tensor[3] salida tensor[2];' >"$tmp/unused-no-layers.ari"
"$compiler" --check "$tmp/unused-no-layers.ari" | grep -F 'OK análisis semántico Ariño'
reject 'modelo M entrada tensor[3] salida tensor[2]; capa L : densa[4,2] para M;' 'entrada esperada=3, recibida=4' unused-first-layer-input
reject 'modelo M entrada tensor[3] salida tensor[2]; capa A : densa[3,4] para M; capa B : densa[5,2] para M;' 'entrada esperada=4, recibida=5' unused-disconnected-layers
reject 'modelo M entrada tensor[3] salida tensor[2]; capa L : densa[3,4] para M;' 'salida declarada del modelo=2' unused-final-layer-output
# Offsets are source byte offsets, including preceding multibyte UTF-8 text.
printf '%s' 'datos ñ : tensor[2,3]; modelo M entrada tensor[3] salida tensor[2]; capa L : densa[4,2] para M;' >"$tmp/unused-offset.ari"
if "$compiler" --check "$tmp/unused-offset.ari" >"$tmp/unused-offset.log" 2>&1; then
  echo 'expected byte-offset diagnostic for invalid unused topology' >&2; exit 1
fi
grep -E 'Error semántico en byte [0-9]+: .*entrada esperada=3, recibida=4' "$tmp/unused-offset.log"
reject 'datos x : tensor[8,4]; modelo M entrada tensor[3] salida tensor[2]; capa L : densa[3,2] para M; entrenar M con x;' 'espera características=3, los datos recibidos tienen 4' data-shape-mismatch
reject 'datos x : tensor[8,3]; modelo M entrada tensor[3] salida tensor[2]; capa L : densa[3,2] para M; clasificar M con x;' 'debe entrenarse antes' predict-before-train
reject 'entrenar M;' "se esperaba 'con'" incomplete-action
reject 'entrenar con x;' 'se esperaba un identificador explícito' missing-model

# AI words remain usable as scalar identifiers outside the AI declaration/action grammar.
printf '%s' 'modelo = 7; modelo más 1' >"$tmp/scalar-name.ari"
"$compiler" --compile "$tmp/scalar-name.ari" "$tmp/scalar-name.arino" >/dev/null
test "$("$root/arino_vm" --run "$tmp/scalar-name.arino")" = 'RESULT I64(8)'
printf '%s' 'entrenar = 7; entrenar más 1' >"$tmp/scalar-action-name.ari"
"$compiler" --compile "$tmp/scalar-action-name.ari" "$tmp/scalar-action-name.arino" >/dev/null
test "$("$root/arino_vm" --run "$tmp/scalar-action-name.arino")" = 'RESULT I64(8)'

# Semantic analysis must succeed before the backend capability error, and must
# not create a partial output artifact.
printf '%s' "$valid" >"$tmp/compile.ari"
if "$compiler" --compile "$tmp/compile.ari" "$tmp/partial.arino" >"$tmp/compile.log" 2>&1; then
  echo 'AI compile unexpectedly succeeded without tensor VM opcodes' >&2; exit 1
fi
grep -F 'no se generó archivo .arino' "$tmp/compile.log"
test ! -e "$tmp/partial.arino"
# A semantic failure also leaves no output.
printf '%s' 'entrenar M con x;' >"$tmp/fail.ari"
if "$compiler" --compile "$tmp/fail.ari" "$tmp/failed.arino" >"$tmp/fail.log" 2>&1; then exit 1; fi
test ! -e "$tmp/failed.arino"
echo 'AI semantic tests: PASS'
