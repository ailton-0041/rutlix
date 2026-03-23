/*
 * Rutlix Record — Internacionalização
 * src/i18n.h
 */
#ifndef I18N_H
#define I18N_H

#define LANG_PT 0
#define LANG_EN 1

static int g_lang = LANG_PT;
static inline void i18n_set(int lang) { g_lang = lang; }

typedef struct { const char *key; const char *pt; const char *en; } I18NEntry;

static const I18NEntry I18N_TABLE[] = {
  {"subtitle",          "gravador de iso",                  "iso writer"},
  {"drive_props",       "PROPRIEDADES DO DRIVE",            "DRIVE PROPERTIES"},
  {"format_opts",       "OPÇÕES DE FORMATAÇÃO",             "FORMAT OPTIONS"},
  {"status",            "STATUS",                           "STATUS"},
  {"device",            "Dispositivo",                      "Device"},
  {"boot_sel",          "Seleção de Boot (Imagem ISO)",     "Boot Selection (ISO Image)"},
  {"part_scheme",       "Esquema de Partição",              "Partition Scheme"},
  {"target_sys",        "Sistema de Destino",               "Target System"},
  {"volume_name",       "Nome do Volume",                   "Volume Name"},
  {"filesystem",        "Sistema de Arquivos",              "File System"},
  {"cluster_size",      "Tamanho do Cluster",               "Cluster Size"},
  {"quick_fmt",         "Formatação Rápida",                "Quick Format"},
  {"ext_names",         "Nomes Estendidos",                 "Extended Names"},
  {"bad_blocks",        "Verificar blocos defeituosos",     "Check for bad blocks"},
  {"passes_label",      "Passadas:",                        "Passes:"},
  {"select_btn",        "SELECIONAR",                       "SELECT"},
  {"start_btn",         "INICIAR",                          "START"},
  {"close_btn",         "FECHAR",                           "CLOSE"},
  {"no_iso",            "Nenhuma imagem ISO selecionada.",  "No ISO image selected."},
  {"no_device",         "Nenhum dispositivo encontrado",    "No device found"},
  {"dev_found",         "%d dispositivo(s) encontrado(s)", "%d device(s) found"},
  {"iso_selected_status","ISO selecionada  [%s]",           "ISO selected  [%s]"},
  {"writing",           "Gravando...",                      "Writing..."},
  {"confirm_format",    "AVISO: Todos os dados em %s (%s — %s) serão APAGADOS.\n\nDeseja continuar?",
                        "WARNING: All data on %s (%s — %s) will be ERASED.\n\nContinue?"},
  {"confirm_abort",     "Gravação em andamento.\nDeseja mesmo cancelar?",
                        "Writing in progress.\nDo you really want to cancel?"},
  {"continue_anyway",   "Deseja continuar mesmo assim?",    "Continue anyway?"},
  {"err_no_device",     "Nenhum dispositivo USB selecionado.",
                        "No USB device selected."},
  {"err_no_iso",        "Nenhuma imagem ISO selecionada.",  "No ISO image selected."},
  {"win_large_warn",    "⚠ install.wim > 4 GB — será dividido automaticamente (split WIM)",
                        "⚠ install.wim > 4 GB — will be split automatically (split WIM)"},
  {"iso_placeholder",   "Caminho da imagem ISO...",         "ISO image path..."},
  {"show_adv_drive",    "Mostrar opções avançadas de drive","Show advanced drive options"},
  {"hide_adv_format",   "Mostrar opções avançadas de formato","Show advanced format options"},
  {"select_iso_title",  "Selecionar imagem ISO",            "Select ISO image"},
  {"cancel",            "_Cancelar",                        "_Cancel"},
  {"open",              "_Abrir",                           "_Open"},
  {"filter_iso",        "Imagens ISO (*.iso)",              "ISO Images (*.iso)"},
  {"filter_all",        "Todos os arquivos",                "All files"},
  {NULL, NULL, NULL}
};

static inline const char *i18n_get(const char *key) {
    for (int i = 0; I18N_TABLE[i].key; i++) {
        if (__builtin_strcmp(I18N_TABLE[i].key, key) == 0)
            return (g_lang == LANG_EN) ? I18N_TABLE[i].en : I18N_TABLE[i].pt;
    }
    return key;
}

#define T(k) i18n_get(k)

#endif /* I18N_H */
