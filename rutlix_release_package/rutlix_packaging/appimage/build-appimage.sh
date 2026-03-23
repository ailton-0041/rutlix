#!/bin/bash
# ============================================================
# Rutlix Record — Gerador de AppImage
# Uso: ./build-appimage.sh
# Requer: wget, gcc, libgtk-3-dev, FUSE
# O AppImage roda em qualquer distro Linux sem instalação!
# ============================================================

set -e

APP_NAME="Rutlix"
VERSION="2.1"
ARCH="x86_64"
APPDIR="RutlixRecord.AppDir"

echo "==> Compilando Rutlix Record v${VERSION}..."
cd "$(dirname "$0")/../../rutlix_v2"

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

echo "==> Baixando appimagetool..."
TOOLDIR="$(dirname "$0")/tools"
mkdir -p "${TOOLDIR}"
if [ ! -f "${TOOLDIR}/appimagetool" ]; then
    wget -q -O "${TOOLDIR}/appimagetool" \
        "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "${TOOLDIR}/appimagetool"
fi

echo "==> Montando estrutura AppDir..."
STAGING="$(dirname "$0")/${APPDIR}"
rm -rf "${STAGING}"

mkdir -p "${STAGING}/usr/bin"
mkdir -p "${STAGING}/usr/lib"
mkdir -p "${STAGING}/usr/share/applications"
mkdir -p "${STAGING}/usr/share/icons/hicolor/64x64/apps"
mkdir -p "${STAGING}/usr/share/polkit-1/actions"

# Binário principal
install -m 755 rutlix "${STAGING}/usr/bin/rutlix"

# AppRun — ponto de entrada do AppImage
cat > "${STAGING}/AppRun" << 'APPRUN'
#!/bin/bash
# AppRun — inicializa o ambiente e chama pkexec para autenticação gráfica
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

# Instalar política polkit se não estiver presente (precisa de root)
POLICY_DEST="/usr/share/polkit-1/actions/io.github.ailtonmartins.rutlix.policy"
if [ ! -f "${POLICY_DEST}" ]; then
    echo "Instalando política de autenticação... (requer sudo uma única vez)"
    sudo install -m 644 \
        "${HERE}/usr/share/polkit-1/actions/io.github.ailtonmartins.rutlix.policy" \
        "${POLICY_DEST}" || true
fi

exec pkexec "${HERE}/usr/bin/rutlix" "$@"
APPRUN
chmod +x "${STAGING}/AppRun"

# Política polkit
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

# Ícone
cat > "${STAGING}/usr/share/icons/hicolor/64x64/apps/rutlix.svg" << 'SVG'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <rect width="64" height="64" rx="12" fill="#0078d4"/>
  <text x="32" y="44" font-size="36" text-anchor="middle" fill="white" font-family="sans-serif" font-weight="bold">R</text>
</svg>
SVG

# Cópia raiz do ícone (obrigatório para AppImage)
cp "${STAGING}/usr/share/icons/hicolor/64x64/apps/rutlix.svg" \
   "${STAGING}/rutlix.svg"

# .desktop
cat > "${STAGING}/rutlix.desktop" << 'DESKTOP'
[Desktop Entry]
Version=1.0
Type=Application
Name=Rutlix Record
GenericName=ISO Writer
Comment=Grave imagens ISO em pendrives USB
Exec=rutlix
Icon=rutlix
Terminal=false
Categories=Utility;System;
Keywords=iso;usb;boot;pendrive;
StartupNotify=true
DESKTOP

cp "${STAGING}/rutlix.desktop" \
   "${STAGING}/usr/share/applications/rutlix.desktop"

# Empacotar com appimagetool
echo "==> Empacotando AppImage..."
cd "$(dirname "$0")"
ARCH=${ARCH} "${TOOLDIR}/appimagetool" "${APPDIR}" \
    "Rutlix_Record-${VERSION}-${ARCH}.AppImage"

echo ""
echo "✓ AppImage gerado: Rutlix_Record-${VERSION}-${ARCH}.AppImage"
echo ""
echo "Para usar (sem instalar):"
echo "  chmod +x Rutlix_Record-${VERSION}-${ARCH}.AppImage"
echo "  ./Rutlix_Record-${VERSION}-${ARCH}.AppImage"
echo ""
echo "Para integrar ao menu de aplicativos:"
echo "  Instale o AppImageLauncher: https://github.com/TheAssassin/AppImageLauncher"
