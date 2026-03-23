#!/bin/bash
# ============================================================
# Rutlix Record — Gerador de pacote .deb
# Uso: ./build-deb.sh
# Requer: dpkg-deb, gcc, libgtk-3-dev
# ============================================================

set -e

APP_NAME="rutlix"
VERSION="2.1"
ARCH="amd64"
PKG_DIR="rutlix_${VERSION}_${ARCH}"

echo "==> Compilando Rutlix Record v${VERSION}..."
cd "$(dirname "$0")/../../rutlix_v2"

# Compilar (sem Assembly se NASM não estiver presente)
if command -v nasm &>/dev/null; then
    nasm -f elf64 -o build/utils.o asm/utils.asm
    gcc -Wall -O2 -Wno-unused-result \
        $(pkg-config --cflags gtk+-3.0) -Isrc \
        -o rutlix src/main.c src/disk_ops.c src/iso_detect.c build/utils.o \
        $(pkg-config --libs gtk+-3.0) -lpthread -lm
else
    echo "  WARN: nasm não encontrado — compilando sem Assembly"
    gcc -Wall -O2 -DNO_ASM -Wno-unused-result \
        $(pkg-config --cflags gtk+-3.0) -Isrc \
        -o rutlix src/main.c src/disk_ops.c src/iso_detect.c \
        $(pkg-config --libs gtk+-3.0) -lpthread -lm
fi

echo "==> Montando estrutura do pacote .deb..."
STAGING="$(dirname "$0")/${PKG_DIR}"
rm -rf "${STAGING}"

# Hierarquia FHS
mkdir -p "${STAGING}/DEBIAN"
mkdir -p "${STAGING}/usr/bin"
mkdir -p "${STAGING}/usr/share/applications"
mkdir -p "${STAGING}/usr/share/pixmaps"
mkdir -p "${STAGING}/usr/share/doc/rutlix"
mkdir -p "${STAGING}/usr/share/polkit-1/actions"

# Binário
install -m 755 rutlix "${STAGING}/usr/bin/rutlix"

# Wrapper com pkexec para pedir senha graficamente
cat > "${STAGING}/usr/bin/rutlix-launcher" << 'WRAPPER'
#!/bin/bash
# Launcher gráfico — pede senha via pkexec (janela gráfica de autenticação)
exec pkexec /usr/bin/rutlix "$@"
WRAPPER
chmod 755 "${STAGING}/usr/bin/rutlix-launcher"

# Política polkit (autenticação gráfica sem terminal)
cat > "${STAGING}/usr/share/polkit-1/actions/io.github.ailtonmartins.rutlix.policy" << 'POLICY'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC
  "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
  "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <action id="io.github.ailtonmartins.rutlix.run">
    <description>Gravar imagem ISO no dispositivo USB</description>
    <description xml:lang="en">Write ISO image to USB device</description>
    <message>O Rutlix Record precisa de permissão de administrador para gravar no dispositivo.</message>
    <message xml:lang="en">Rutlix Record needs administrator permission to write to the device.</message>
    <icon_name>drive-removable-media</icon_name>
    <defaults>
      <allow_any>auth_admin</allow_any>
      <allow_inactive>auth_admin</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
    <annotate key="org.freedesktop.policykit.exec.path">/usr/bin/rutlix</annotate>
    <annotate key="org.freedesktop.policykit.exec.allow_gui">true</annotate>
  </action>
</policyconfig>
POLICY

# Ícone SVG simples (substitua por um PNG real em produção)
cat > "${STAGING}/usr/share/pixmaps/rutlix.svg" << 'SVG'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <rect width="64" height="64" rx="12" fill="#0078d4"/>
  <text x="32" y="44" font-size="36" text-anchor="middle" fill="white" font-family="sans-serif" font-weight="bold">R</text>
</svg>
SVG

# Arquivo .desktop — aparece no menu de aplicativos
cat > "${STAGING}/usr/share/applications/rutlix.desktop" << 'DESKTOP'
[Desktop Entry]
Version=1.0
Type=Application
Name=Rutlix Record
Name[pt_BR]=Rutlix Record
GenericName=ISO Writer
GenericName[pt_BR]=Gravador de ISO
Comment=Grave imagens ISO em pendrives USB com facilidade
Comment[en]=Write ISO images to USB drives with ease
Exec=rutlix-launcher
Icon=rutlix
Terminal=false
Categories=Utility;System;
Keywords=iso;usb;boot;pendrive;gravar;windows;linux;
StartupNotify=true
DESKTOP

# Documentação
cp "$(dirname "$0")/../../rutlix_v2/README.md" \
   "${STAGING}/usr/share/doc/rutlix/README.md"

cat > "${STAGING}/usr/share/doc/rutlix/copyright" << 'COPYRIGHT'
Rutlix Record
Copyright (C) 2024 Ailton Martins
Licença: MIT
COPYRIGHT

# Arquivo de controle do pacote
cat > "${STAGING}/DEBIAN/control" << CONTROL
Package: rutlix
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Depends: libgtk-3-0 (>= 3.22), libglib2.0-0, polkit, parted, dosfstools, ntfs-3g
Recommends: wimtools
Maintainer: Ailton Martins <ailton.martins.031227@gmail.com>
Homepage: https://github.com/ailtonmartins/rutlix
Description: Gravador de ISO para Linux (Windows e Linux ISOs)
 Rutlix Record é um gravador de imagens ISO para dispositivos USB,
 com suporte a ISOs do Windows (incluindo split automático de install.wim
 maior que 4 GB) e qualquer distribuição Linux.
 .
 Desenvolvido em C + Assembly x86-64 com interface GTK3.
CONTROL

# Script pós-instalação
cat > "${STAGING}/DEBIAN/postinst" << 'POSTINST'
#!/bin/bash
set -e
# Atualizar banco de dados de ícones e menus
if command -v update-desktop-database &>/dev/null; then
    update-desktop-database /usr/share/applications || true
fi
if command -v gtk-update-icon-cache &>/dev/null; then
    gtk-update-icon-cache /usr/share/pixmaps || true
fi
echo "✓ Rutlix Record instalado! Procure por 'Rutlix' no menu de aplicativos."
POSTINST
chmod 755 "${STAGING}/DEBIAN/postinst"

# Construir o .deb
echo "==> Construindo ${PKG_DIR}.deb..."
cd "$(dirname "$0")"
dpkg-deb --build --root-owner-group "${PKG_DIR}"

echo ""
echo "✓ Pacote gerado: $(dirname "$0")/${PKG_DIR}.deb"
echo ""
echo "Para instalar:"
echo "  sudo dpkg -i ${PKG_DIR}.deb"
echo "  sudo apt-get install -f   # corrige dependências se necessário"
