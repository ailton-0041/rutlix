#!/bin/bash
# ============================================================
# Rutlix Record — Script de build completo
# Gera .deb, AppImage e .rpm de uma vez
# ============================================================

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "╔══════════════════════════════════════════╗"
echo "║     Rutlix Record — Build de Pacotes     ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# Verificar dependências
check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "  ✗ $1 não encontrado — $2"
        return 1
    fi
    echo "  ✓ $1"
    return 0
}

echo "==> Verificando dependências..."
check_dep gcc        "instale build-essential (Debian) ou gcc (Fedora)"
check_dep pkg-config "instale pkg-config"
check_dep nasm       "opcional — instale nasm para versão com Assembly"

echo ""
echo "Qual pacote deseja gerar?"
echo "  1) .deb  (Ubuntu/Debian/Mint)"
echo "  2) AppImage (qualquer distro)"
echo "  3) .rpm  (Fedora/openSUSE/RHEL)"
echo "  4) Todos"
echo ""
read -rp "Escolha [1-4]: " CHOICE

case "${CHOICE}" in
    1)
        bash "${SCRIPT_DIR}/deb/build-deb.sh"
        ;;
    2)
        bash "${SCRIPT_DIR}/appimage/build-appimage.sh"
        ;;
    3)
        bash "${SCRIPT_DIR}/rpm/build-rpm.sh"
        ;;
    4)
        echo ""
        echo "--- Gerando .deb ---"
        bash "${SCRIPT_DIR}/deb/build-deb.sh" || echo "WARN: .deb falhou"
        echo ""
        echo "--- Gerando AppImage ---"
        bash "${SCRIPT_DIR}/appimage/build-appimage.sh" || echo "WARN: AppImage falhou"
        echo ""
        echo "--- Gerando .rpm ---"
        if command -v rpmbuild &>/dev/null; then
            bash "${SCRIPT_DIR}/rpm/build-rpm.sh" || echo "WARN: .rpm falhou"
        else
            echo "WARN: rpmbuild não encontrado — pulando .rpm"
            echo "      Instale: sudo dnf install rpm-build"
        fi
        ;;
    *)
        echo "Opção inválida."
        exit 1
        ;;
esac

echo ""
echo "═══════════════════════════════════════"
echo "✓ Pronto! Verifique os pacotes gerados."
echo "═══════════════════════════════════════"
