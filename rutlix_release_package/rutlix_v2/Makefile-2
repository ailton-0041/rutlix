# ============================================================
# Rutlix Record v2.1 - Makefile
# ============================================================

CC      = gcc
NASM    = nasm

CFLAGS  = -Wall -Wextra -O2 -g \
          -Wno-format-truncation \
          -Wno-stringop-truncation \
          -Wno-unused-result \
          -Wno-nested-externs \
          $(shell pkg-config --cflags gtk+-3.0) \
          -Isrc

LDFLAGS = $(shell pkg-config --libs gtk+-3.0) \
          -lpthread -lm

SRCDIR   = src
ASMDIR   = asm
BUILDDIR = build

C_SRCS   = $(SRCDIR)/main.c \
            $(SRCDIR)/disk_ops.c \
            $(SRCDIR)/iso_detect.c

TARGET   = rutlix

.PHONY: all fallback clean install run help

all: $(BUILDDIR)
	@if which $(NASM) > /dev/null 2>&1; then \
		echo "  NASM  $(ASMDIR)/utils.asm"; \
		$(NASM) -f elf64 -o $(BUILDDIR)/utils.o $(ASMDIR)/utils.asm; \
		echo "  CC+LD $(TARGET) (C + Assembly)"; \
		$(CC) $(CFLAGS) -o $(TARGET) $(C_SRCS) $(BUILDDIR)/utils.o $(LDFLAGS); \
	else \
		echo "  WARN  nasm não encontrado — compilando sem Assembly"; \
		$(CC) $(CFLAGS) -DNO_ASM -o $(TARGET) $(C_SRCS) $(LDFLAGS); \
	fi
	@echo ""
	@echo "  ✓ $(TARGET) compilado!  Execute: sudo ./$(TARGET)"

fallback: $(BUILDDIR)
	$(CC) $(CFLAGS) -DNO_ASM -o $(TARGET) $(C_SRCS) $(LDFLAGS)
	@echo "  ✓ $(TARGET) compilado sem Assembly!"

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

install: all
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "  ✓ Instalado em /usr/local/bin/$(TARGET)"

run: all
	sudo ./$(TARGET)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
