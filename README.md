<div align="center">

<img src="https://img.shields.io/badge/versão-2.1-0078d4?style=for-the-badge&logo=linux&logoColor=white"/>
<img src="https://img.shields.io/badge/licença-MIT-18834a?style=for-the-badge"/>
<img src="https://img.shields.io/badge/linguagem-C%20%2B%20Assembly-e86000?style=for-the-badge"/>
<img src="https://img.shields.io/badge/interface-GTK3-5ab4ff?style=for-the-badge"/>

# Rutlix Record

**Gravador de ISO para Linux — sem terminal, sem complicação.**

Grava Windows 11/10 e qualquer distribuição Linux em pendrives USB com detecção automática de tipo de ISO, split de `install.wim` maior que 4 GB, interface gráfica GTK3 e autenticação via polkit.

[🌐 Site do projeto](https://rutlix-linux.netlify.app) · [📦 Releases](https://github.com/ailton-am/rutlix/releases) · [🐛 Issues](https://github.com/ailton-am/rutlix/issues) · [📖 Wiki](https://github.com/ailton-am/rutlix/wiki)

</div>

---

## ✨ O que o Rutlix faz

| Funcionalidade | Descrição |
|---|---|
| ⊞ **Windows ISO perfeito** | Detecta automaticamente, configura GPT + UEFI + NTFS e divide `install.wim` > 4 GB |
| 🐧 **Linux ISO raw** | Cópia raw (dd) preservando boot híbrido BIOS+UEFI embutido na ISO |
| 🔍 **Detecção inteligente** | Lê a estrutura ISO 9660 real — não apenas a extensão do arquivo |
| ⚙ **Auto-configuração** | Ao abrir a ISO, ajusta partição, filesystem e sistema de destino automaticamente |
| 🔒 **Sem terminal** | Autenticação gráfica via polkit — abre pelo menu de aplicativos como qualquer app |
| ☀ **Tema claro** | Interface GTK3 com tema claro moderno |
| 🌐 **Bilíngue** | PT-BR e English — troca com um clique, sem reiniciar |
| ⚡ **Assembly x86-64** | 6 funções críticas em NASM puro com fallback automático para C |

---

## 📦 Instalação

### Ubuntu / Debian / Mint — `.deb`

```bash
wget https://github.com/ailton-am/rutlix/releases/latest/download/rutlix_2.1_amd64.deb
sudo dpkg -i rutlix_2.1_amd64.deb
sudo apt-get install -f   # corrige dependências se necessário
```

Após instalar, procure **"Rutlix Record"** no menu de aplicativos.

### Qualquer distro — AppImage

```bash
wget https://github.com/ailton-am/rutlix/releases/latest/download/Rutlix_Record-2.1-x86_64.AppImage
chmod +x Rutlix_Record-2.1-x86_64.AppImage
./Rutlix_Record-2.1-x86_64.AppImage
```

> 💡 Instale o [AppImageLauncher](https://github.com/TheAssassin/AppImageLauncher) para integrar ao menu de apps automaticamente.

### Fedora / openSUSE / RHEL — `.rpm`

```bash
# Fedora / RHEL
sudo dnf install https://github.com/ailton-am/rutlix/releases/latest/download/rutlix-2.1-1.fc40.x86_64.rpm

# openSUSE
sudo zypper install rutlix-2.1-1.x86_64.rpm
```

---

## 🔨 Compilar do código fonte

### Dependências

```bash
# Ubuntu / Debian
sudo apt install build-essential libgtk-3-dev nasm parted dosfstools ntfs-3g

# Fedora
sudo dnf install gcc gtk3-devel nasm parted dosfstools ntfs-3g

# wimtools é OPCIONAL — o split de install.wim > 4 GB é feito em C puro
sudo apt install wimtools   # opcional, mas mais rápido
```

### Compilar

```bash
git clone https://github.com/ailton-am/rutlix.git
cd rutlix/rutlix_v2

make            # compila com Assembly (detecta NASM automaticamente)
make fallback   # compila sem Assembly — C puro
sudo ./rutlix
```

### Instalar no sistema

```bash
chmod +x install.sh && sudo ./install.sh
# ou
sudo make install
```

---

## 🗂 Estrutura do projeto

```
rutlix/
├── src/
│   ├── main.c          ← Interface GTK3 — tema, i18n, callbacks
│   ├── disk_ops.c      ← Scan USB, thread de gravação, split WIM
│   ├── disk_ops.h
│   ├── iso_detect.c    ← Análise real de ISO 9660 (Windows / Linux / UDF)
│   ├── iso_detect.h
│   ├── asm_utils.h     ← Declarações Assembly + fallback C puro
│   └── i18n.h          ← Tabela de strings PT-BR / EN
├── asm/
│   └── utils.asm       ← 6 funções Assembly x86-64 (NASM elf64)
├── rutlix_packaging/
│   ├── build-all.sh    ← Script mestre: gera .deb, AppImage e .rpm
│   ├── deb/            ← Scripts e estrutura para pacote .deb
│   ├── appimage/       ← Scripts e AppDir para AppImage
│   └── rpm/            ← rutlix.spec e script para pacote .rpm
├── Makefile
├── install.sh
└── README.md
```

---

## 🔥 Como gravar uma ISO Windows

O Rutlix garante boot perfeito de qualquer ISO Windows moderna:

1. **Abre** a ISO — o badge `⊞ Windows` aparece automaticamente
2. **Detecta** o tipo lendo a estrutura ISO 9660 (presença de `sources/install.wim`)
3. **Configura** automaticamente: GPT · UEFI · NTFS
4. **Formata** o pendrive com tabela GPT e partição NTFS
5. **Copia** todos os arquivos preservando a hierarquia de diretórios
6. **Split automático** de `install.wim` se maior que 4 GB:
   - Implementado em **C puro** — zero dependências externas obrigatórias
   - Usa `wimsplit` / `wimlib-imagex` se disponíveis (mais rápido)
   - Fallback: split binário nativo do formato WIM com ajuste de cabeçalho
   - Gera `install.swm`, `install2.swm`, ... reconhecidos pelo Setup do Windows

**Compatibilidade garantida:**
- Windows 10 (todas as versões, incluindo 22H2)
- Windows 11 (incluindo ISOs com `install.wim` > 4 GB)
- Windows Server 2019 / 2022
- Boot BIOS legacy e UEFI

---

## 🐧 Como gravar uma ISO Linux

ISOs Linux são copiadas em modo **raw (dd)**:

1. **Abre** a ISO — o badge `🐧 Linux` aparece automaticamente
2. **Configura** automaticamente: MBR · BIOS+UEFI · FAT32
3. **Copia** em modo raw, preservando o bootloader híbrido já embutido na ISO

Funciona com qualquer distribuição: Ubuntu, Fedora, Arch, Debian, Mint, openSUSE, Pop!_OS, etc.

---

## 🧠 Assembly x86-64

6 funções críticas implementadas em NASM puro, direto nas syscalls do kernel — sem libc.
Se NASM não estiver disponível, `make fallback` usa implementações C puras equivalentes.

| Função | Instrução / Syscall | Descrição |
|---|---|---|
| `asm_get_file_size` | `sys_stat` | Tamanho da ISO sem libc |
| `asm_str_ends_with_iso` | `OR 0x20` bitmask | Verifica extensão `.iso` — case-insensitive |
| `asm_compute_checksum` | Loop unrolled 8× | Validação de integridade dos dados |
| `asm_memcpy_fast` | `REP MOVSB` | Cópia otimizada pelo microcode |
| `asm_zero_memory` | `REP STOSB` | Zera buffers antes de uso |
| `asm_align_up` | Aritmética bitmask | Alinhamento de blocos para escrita em setor |

---

## 🖥 Interface GTK3

- **Detecção automática**: ao abrir uma ISO, o badge `⊞ Windows` ou `🐧 Linux` aparece e a interface se auto-configura
- **Progresso em tempo real**: barra com porcentagem e velocidade de gravação em MB/s
- **Idioma**: botão `EN`/`PT` no headerbar — troca toda a interface instantaneamente
- **Autenticação gráfica**: pkexec + polkit — a janela de senha do sistema aparece sozinha
- **Opções avançadas**: partição, filesystem, cluster size, formatação rápida, verificação de blocos defeituosos

---

## ⚙ Dependências em tempo de execução

| Pacote | Uso | Obrigatório |
|---|---|---|
| `libgtk-3-0` | Interface gráfica | ✅ Sim |
| `polkit` | Autenticação gráfica sem terminal | ✅ Sim |
| `parted` | Criação de tabela de partições | ✅ Sim |
| `dosfstools` | Formatar FAT32 | ✅ Sim |
| `ntfs-3g` | Formatar NTFS para Windows ISO | ✅ Sim |
| `wimtools` | Split de `install.wim` (mais rápido) | ⚡ Opcional |
| `nasm` | Compilar as funções Assembly | 🔨 Build only |

---

## 🔒 Licença

MIT — uso livre, modificação livre, distribuição livre.

```
Copyright (c) 2024 Ailton Martins
```

---

## 👤 Autor

**Ailton Martins**
- GitHub: [@ailton-am](https://github.com/ailton-am)
- Email: ailton.martins.031227@gmail.com
- Site do projeto: [https://rutlix-linux.netlify.app](https://rutlix-linux.netlify.app)

---

<div align="center">
<sub>Feito com ❤️ para a comunidade Linux brasileira</sub>
</div>
