#!/bin/bash
# Rutlix Record v2.1 — Script de instalação
set -e

BLUE='\033[0;34m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'

echo -e "${BLUE}"
cat << 'EOF'
  ██████╗ ██╗   ██╗████████╗██╗     ██╗██╗  ██╗
  ██╔══██╗██║   ██║╚══██╔══╝██║     ██║╚██╗██╔╝
  ██████╔╝██║   ██║   ██║   ██║     ██║ ╚███╔╝ 
  ██╔══██╗██║   ██║   ██║   ██║     ██║ ██╔██╗ 
  ██║  ██║╚██████╔╝   ██║   ███████╗██║██╔╝ ██╗
  ╚═╝  ╚═╝ ╚═════╝    ╚═╝   ╚══════╝╚═╝╚═╝  ╚═╝
          R E C O R D   —   v2.1
          C + Assembly x86-64 + GTK3
          by Ailton Martins
EOF
echo -e "${NC}"

DISTRO=$(. /etc/os-release 2>/dev/null && echo "$ID" || echo "unknown")
echo -e "  Distro: ${GREEN}$DISTRO${NC}"

echo -e "\n  ${YELLOW}[1/3] Instalando dependências...${NC}"
case "$DISTRO" in
  ubuntu|debian|linuxmint|pop|zorin)
    sudo apt-get update -qq
    sudo apt-get install -y \
      build-essential libgtk-3-dev nasm pkg-config \
      parted dosfstools ntfs-3g util-linux
    # wimlib-imagex é opcional — o split WIM é feito em C puro se não encontrado
    sudo apt-get install -y wimtools 2>/dev/null || true
    ;;
  fedora)
    sudo dnf install -y \
      gcc make gtk3-devel nasm pkg-config \
      parted dosfstools ntfs-3g util-linux
    sudo dnf install -y wimlib 2>/dev/null || true
    ;;
  arch|manjaro|endeavouros)
    sudo pacman -S --noconfirm \
      base-devel gtk3 nasm pkg-config \
      parted dosfstools ntfs-3g util-linux
    sudo pacman -S --noconfirm wimlib 2>/dev/null || true
    ;;
  opensuse*|sles)
    sudo zypper install -y \
      gcc make gtk3-devel nasm pkg-config \
      parted dosfstools ntfs-3g util-linux
    ;;
  *)
    echo -e "  ${RED}Distro não reconhecida. Instale manualmente:${NC}"
    echo "  gcc make libgtk-3-dev nasm parted dosfstools ntfs-3g"
    echo "  (wimtools é opcional — split WIM funciona sem ele)"
    ;;
esac

echo -e "  ${GREEN}✓ Dependências instaladas!${NC}"
echo -e "\n  ${YELLOW}[2/3] Compilando...${NC}"

mkdir -p build
make all

echo -e "  ${GREEN}✓ Compilado com sucesso!${NC}"

echo -e "\n  ${YELLOW}[3/3] Pronto!${NC}"
echo ""
echo -e "  Executar:      ${GREEN}sudo ./rutlix${NC}"
echo -e "  Instalar:      ${GREEN}sudo make install${NC}"
echo ""
echo -e "  ${BLUE}Suporte a:${NC}"
echo "  ✓ ISO Linux (qualquer distro) — gravação raw direta"
echo "  ✓ ISO Windows — detecção automática, split de install.wim > 4 GB"
echo "  ✓ Tema claro / escuro"
echo "  ✓ Idioma PT-BR / EN"
echo ""
