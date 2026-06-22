#!/bin/bash
#
# build_assimp.sh — Compila a biblioteca estática libassimp.a
#
# Uso:
#   cd /caminho/do/projeto
#   ./scripts/build_assimp.sh
#
# O script faz:
#   1. Clona o código fonte do assimp (branch v5.4.3 — compatível com os headers do projeto)
#   2. Compila como biblioteca estática com CMake
#   3. Copia libassimp.a para lib-assimp/
#   4. Remove os fontes temporários
#
# Requisitos: cmake, make, g++, git
#

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/assimp_build"
SRC_DIR="$PROJECT_DIR/build/assimp_src"
OUT_DIR="$PROJECT_DIR/lib-assimp"
ASSIMP_VERSION="v5.4.3"

echo "=== Build Assimp $ASSIMP_VERSION ==="
echo "Projeto: $PROJECT_DIR"
echo "Fonte:   $SRC_DIR"
echo "Build:   $BUILD_DIR"
echo "Saída:   $OUT_DIR/libassimp.a"
echo ""

# 1. Clona o assimp se ainda não existir
if [ ! -d "$SRC_DIR" ]; then
    echo "[1/4] Clonando assimp $ASSIMP_VERSION..."
    git clone --depth 1 --branch "$ASSIMP_VERSION" \
        https://github.com/assimp/assimp.git "$SRC_DIR"
else
    echo "[1/4] Fontes já existem em $SRC_DIR, pulando clone."
fi

# 2. Cria diretório de build
echo "[2/4] Configurando CMake..."
mkdir -p "$BUILD_DIR"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DASSIMP_BUILD_TESTS=OFF \
    -DASSIMP_BUILD_SAMPLES=OFF \
    -DASSIMP_BUILD_ZLIB=ON \
    -DASSIMP_INSTALL=OFF \
    -DASSIMP_NO_EXPORT=ON \
    -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF \
    -DASSIMP_BUILD_FBX_IMPORTER=ON \
    -DASSIMP_BUILD_GLTF_IMPORTER=OFF \
    -DASSIMP_BUILD_OBJ_IMPORTER=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_CXX_STANDARD=17

# 3. Compila
echo "[3/4] Compilando..."
cmake --build "$BUILD_DIR" --config Release -j$(nproc)

# 4. Copia libassimp.a
echo "[4/4] Copiando libassimp.a para $OUT_DIR/"
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/lib/libassimp.a" "$OUT_DIR/libassimp.a"

echo ""
echo "=== Pronto! ==="
ls -lh "$OUT_DIR/libassimp.a"
