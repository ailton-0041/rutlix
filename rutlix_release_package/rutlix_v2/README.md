# Rutlix Record v2.0 🔥
### Gravador de ISO para Linux — C + Assembly x86-64 + GTK3

---

## ✨ Novidades v2.0

| Feature | Descrição |
|---|---|
| 🌙 / ☀ **Tema claro/escuro** | Toggle no headerbar — sem cores extravagantes |
| 🌐 **Tradutor PT-BR ↔ EN** | Botão no headerbar, toda a interface se traduz |
| ⊞ **Windows ISO perfeito** | Detecção automática, split de install.wim >4 GB |
| 🐧 **Linux ISO perfeito** | Cópia raw dd com suporte a ISO híbrida BIOS+UEFI |
| 🔍 **Detecção de tipo** | Lê estrutura ISO 9660 real — não apenas extensão |
| ⚙ **Auto-config** | Ao selecionar ISO, ajusta partição/FS/target automaticamente |

---

## 🗂 Estrutura

```
rutlix/
├── src/
│   ├── main.c          ← Interface GTK3 (tema + i18n + callbacks)
│   ├── disk_ops.c      ← Scan USB, thread de gravação, split WIM
│   ├── disk_ops.h
│   ├── iso_detect.c    ← Análise real de ISO 9660 (Windows / Linux)
│   ├── iso_detect.h
│   ├── asm_utils.h     ← Declarações Assembly + fallback C
│   └── i18n.h          ← Tabela de strings PT-BR / EN
├── asm/
│   └── utils.asm       ← 6 funções Assembly x86-64 (NASM)
├── Makefile
├── install.sh
└── README.md
```

---

## ⚙ Dependências

```bash
# Ubuntu / Debian — dependências obrigatórias
sudo apt install build-essential libgtk-3-dev nasm \
                 parted dosfstools ntfs-3g

# wimtools é OPCIONAL — o split de install.wim > 4 GB
# é feito em C puro se não encontrado
sudo apt install wimtools   # opcional
```

---

## 🔨 Compilar e Executar

```bash
# Instalação automática de tudo:
chmod +x install.sh && ./install.sh

# Ou manualmente:
make          # compila (auto-detecta NASM)
make fallback # compila sem Assembly (fallback C puro)

# Executar (root necessário para gravar no pendrive):
sudo ./rutlix
```

---

## 🔥 Como Gravar Windows ISO com Perfeição

O Rutlix Record v2.0 garante boot perfeito de qualquer ISO Windows:

### Processo automático:
1. **Detecta** a ISO como Windows lendo a estrutura ISO 9660
2. **Configura automaticamente**: GPT + UEFI + NTFS
3. **Formata** o pendrive com tabela GPT e partição NTFS
4. **Copia** todos os arquivos preservando estrutura de diretórios
5. **Split automático** de `install.wim` se >4 GB (para FAT32)
   - **Implementado em C puro** — zero dependências externas
   - Usa `wimsplit` ou `wimlib-imagex` se disponíveis (mais rápido)
   - Fallback: split binário nativo do formato WIM com ajuste de cabeçalho
   - Gera `install.swm`, `install2.swm`, ... reconhecidos pelo Setup do Windows
6. Bootloader UEFI (`/EFI/BOOT/bootx64.efi`) já vem na ISO

### Compatibilidade Windows:
- Windows 10 (todas as versões, incluindo 22H2)
- Windows 11 (incluindo ISOs com install.wim > 4 GB)
- Windows Server 2019/2022
- Boot BIOS legacy e UEFI

---

## 🐧 Como Gravar Linux ISO

Linux ISOs são copiadas em modo **raw (dd)** diretamente:
- Preserva o boot híbrido (BIOS + UEFI) embutido na ISO
- Suporte a qualquer distribuição: Ubuntu, Fedora, Arch, Debian, etc.
- Configuração automática: MBR + BIOS+UEFI + FAT32

---

## 🧠 Assembly x86-64

| Função | Syscall/Instrução | Uso |
|---|---|---|
| `asm_get_file_size` | `sys_stat` direto | Tamanho da ISO sem libc |
| `asm_str_ends_with_iso` | `OR 0x20` bitmask | Verifica extensão |
| `asm_compute_checksum` | Loop unrolled 8x | Validação de dados |
| `asm_memcpy_fast` | `REP MOVSB` | Cópia otimizada pelo microcode |
| `asm_zero_memory` | `REP STOSB` | Zera buffers |
| `asm_align_up` | Aritmética bitmask | Alinhamento de blocos |

---

## 📋 Interface

- **Toggle tema**: botão 🌙/☀ no canto superior esquerdo
- **Toggle idioma**: botão `EN`/`PT` ao lado do tema
- **Detecção automática**: ao abrir ISO, badge `⊞ Windows` ou `🐧 Linux` aparece
- **Auto-configuração**: partition scheme, target e FS se ajustam sozinhos
- **Progresso**: barra com porcentagem e velocidade em MB/s

---

## 🔒 Licença

MIT — uso livre.
