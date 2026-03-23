Name:           rutlix
Version:        2.1
Release:        1%{?dist}
Summary:        Gravador de ISO para Linux (Windows e Linux ISOs)
License:        MIT
URL:            https://github.com/ailtonmartins/rutlix
Source0:        rutlix_v2.tar.gz

BuildRequires:  gcc
BuildRequires:  gtk3-devel
BuildRequires:  glib2-devel
BuildRequires:  nasm

Requires:       gtk3 >= 3.22
Requires:       polkit
Requires:       parted
Requires:       dosfstools
Requires:       ntfs-3g
Suggests:       wimlib-utils

%description
Rutlix Record é um gravador de imagens ISO para dispositivos USB,
com suporte a ISOs do Windows (incluindo split automático de install.wim
maior que 4 GB) e qualquer distribuição Linux.

Desenvolvido em C + Assembly x86-64 com interface GTK3.
Não precisa de terminal — a autenticação de administrador
é feita graficamente via polkit.

%prep
%autosetup -n rutlix_v2

%build
# Compilar com Assembly se NASM disponível
if command -v nasm &>/dev/null; then
    mkdir -p build
    nasm -f elf64 -o build/utils.o asm/utils.asm
    gcc %{optflags} -Wno-unused-result \
        $(pkg-config --cflags gtk+-3.0) -Isrc \
        -o rutlix src/main.c src/disk_ops.c src/iso_detect.c build/utils.o \
        $(pkg-config --libs gtk+-3.0) -lpthread -lm
else
    gcc %{optflags} -DNO_ASM -Wno-unused-result \
        $(pkg-config --cflags gtk+-3.0) -Isrc \
        -o rutlix src/main.c src/disk_ops.c src/iso_detect.c \
        $(pkg-config --libs gtk+-3.0) -lpthread -lm
fi

%install
install -D -m 755 rutlix %{buildroot}%{_bindir}/rutlix

# Launcher com pkexec
cat > rutlix-launcher << 'WRAPPER'
#!/bin/bash
exec pkexec /usr/bin/rutlix "$@"
WRAPPER
install -D -m 755 rutlix-launcher %{buildroot}%{_bindir}/rutlix-launcher

# Ícone
mkdir -p %{buildroot}%{_datadir}/pixmaps
cat > %{buildroot}%{_datadir}/pixmaps/rutlix.svg << 'SVG'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <rect width="64" height="64" rx="12" fill="#0078d4"/>
  <text x="32" y="44" font-size="36" text-anchor="middle" fill="white" font-family="sans-serif" font-weight="bold">R</text>
</svg>
SVG

# .desktop
install -D -m 644 /dev/stdin \
    %{buildroot}%{_datadir}/applications/rutlix.desktop << 'DESKTOP'
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

# Política polkit
install -D -m 644 /dev/stdin \
    %{buildroot}%{_datadir}/polkit-1/actions/io.github.ailtonmartins.rutlix.policy << 'POLICY'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC
  "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
  "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <action id="io.github.ailtonmartins.rutlix.run">
    <description>Gravar imagem ISO no dispositivo USB</description>
    <description xml:lang="en">Write ISO image to USB device</description>
    <message>O Rutlix Record precisa de permissão de administrador.</message>
    <message xml:lang="en">Rutlix Record needs administrator permission.</message>
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

%post
update-desktop-database %{_datadir}/applications &>/dev/null || :
gtk-update-icon-cache %{_datadir}/pixmaps &>/dev/null || :

%postun
update-desktop-database %{_datadir}/applications &>/dev/null || :

%files
%license README.md
%doc README.md
%{_bindir}/rutlix
%{_bindir}/rutlix-launcher
%{_datadir}/applications/rutlix.desktop
%{_datadir}/pixmaps/rutlix.svg
%{_datadir}/polkit-1/actions/io.github.ailtonmartins.rutlix.policy

%changelog
* 2024-01-01 Ailton Martins <ailton.martins.031227@gmail.com> - 2.1-1
- Tema claro por padrão
- Suporte a split automático de install.wim > 4 GB
- Interface GTK3 bilíngue (PT-BR / EN)
