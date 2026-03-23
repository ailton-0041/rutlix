#!/bin/bash
# ============================================================
# Rutlix Record — Gerador de pacote .rpm
# Uso: ./build-rpm.sh
# Requer: rpmbuild, gcc, gtk3-devel (Fedora/openSUSE/RHEL)
# ============================================================

set -e

VERSION="2.1"
ARCH="x86_64"

echo "==> Preparando ambiente rpmbuild..."
RPMBUILD_DIR="${HOME}/rpmbuild"
mkdir -p "${RPMBUILD_DIR}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

echo "==> Empacotando código fonte..."
SRCDIR="$(dirname "$0")/../.."
tar -czf "${RPMBUILD_DIR}/SOURCES/rutlix_v2.tar.gz" \
    -C "${SRCDIR}" rutlix_v2/

echo "==> Copiando spec..."
cp "$(dirname "$0")/rutlix.spec" "${RPMBUILD_DIR}/SPECS/"

echo "==> Construindo RPM..."
rpmbuild -bb "${RPMBUILD_DIR}/SPECS/rutlix.spec"

echo ""
RPM_FILE=$(find "${RPMBUILD_DIR}/RPMS" -name "rutlix-${VERSION}*.rpm" | head -1)
echo "✓ RPM gerado: ${RPM_FILE}"
echo ""
echo "Para instalar no Fedora/RHEL:"
echo "  sudo dnf install ${RPM_FILE}"
echo ""
echo "Para instalar no openSUSE:"
echo "  sudo zypper install ${RPM_FILE}"
