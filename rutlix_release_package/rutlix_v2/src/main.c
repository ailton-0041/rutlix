/*
 * Rutlix Record v2.1 - Gravador de ISO para Linux
 * src/main.c
 *
 * Compilar:
 *   nasm -f elf64 -o build/utils.o asm/utils.asm
 *   gcc -Wall -Wextra -O2 -Wno-format-truncation -Wno-stringop-truncation \
 *       -Wno-unused-result $(pkg-config --cflags gtk+-3.0) -Isrc \
 *       -o rutlix src/main.c src/disk_ops.c src/iso_detect.c \
 *       build/utils.o $(pkg-config --libs gtk+-3.0) -lpthread -lm
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "disk_ops.h"
#include "iso_detect.h"
#include "asm_utils.h"
#include "i18n.h"

#define APP_NAME     "Rutlix"
#define APP_SUBTITLE "Record"
#define APP_VERSION  "2.1"
#define MAX_DEVICES  32
#define MAX_PATH     4096

/* =========================================================
 * CSS — Tema Escuro (único tema, bonito e profissional)
 * ========================================================= */
static const char *CSS_DARK =
/* ── Reset base ── */
"* { font-family: 'Ubuntu', 'Cantarell', 'DejaVu Sans', sans-serif; font-size: 12px; }\n"

/* ── Janela principal ── */
"window, .background { background-color: #f5f5f5; color: #1a1a1a; }\n"

/* ── Headerbar ── */
"headerbar { background-color: #e8e8e8; border-bottom: 1px solid #c8c8c8;"
"  color: #1a1a1a; min-height: 42px; }\n"
"headerbar .title { font-size: 14px; font-weight: bold; color: #111111; letter-spacing: 1px; }\n"
"headerbar .subtitle { font-size: 9px; color: #666; letter-spacing: 1px; }\n"

/* ── Separadores ── */
"separator { background-color: #d0d0d0; min-height: 1px; margin: 3px 0; }\n"
".section-title { font-size: 11px; font-weight: bold; color: #555; letter-spacing: 1px; }\n"

/* ── Labels ── */
"label { color: #1a1a1a; }\n"
"label.dim-label   { color: #777; font-size: 11px; }\n"
"label.win-badge   { color: #0277bd; font-weight: bold; font-size: 11px; }\n"
"label.linux-badge { color: #e65100; font-weight: bold; font-size: 11px; }\n"
"label.warn-label  { color: #e65100; font-size: 11px; }\n"
"label.about-ver   { color: #888; font-size: 10px; }\n"

/* ── Entry ── */
"entry { background-color: #ffffff; color: #1a1a1a;"
"  border: 1px solid #c0c0c0; border-radius: 4px; padding: 5px; }\n"
"entry:focus { border-color: #0078d4; }\n"
"entry:disabled { background-color: #ebebeb; color: #aaa; }\n"

/* ── Combobox ── */
"combobox button, combobox button.combo { background-color: #ffffff; color: #1a1a1a;"
"  border: 1px solid #c0c0c0; border-radius: 4px; }\n"
"combobox button:hover { background-color: #f0f0f0; border-color: #aaa; }\n"
"combobox button arrow { color: #666; }\n"
/* Popup do combobox */
"menu, .menu, .context-menu { background-color: #ffffff; color: #1a1a1a;"
"  border: 1px solid #c0c0c0; }\n"
"menuitem { background-color: #ffffff; color: #1a1a1a; padding: 4px 8px; }\n"
"menuitem:hover { background-color: #0068b5; color: #fff; }\n"
"menuitem label { color: inherit; }\n"
/* Scrollbar dentro de popups */
"scrolledwindow { background-color: #f5f5f5; }\n"
"scrollbar trough { background-color: #e0e0e0; }\n"
"scrollbar slider { background-color: #b0b0b0; border-radius: 4px; }\n"
"scrollbar slider:hover { background-color: #999; }\n"

/* ── Botões genéricos ── */
"button { background-color: #ebebeb; color: #1a1a1a;"
"  border: 1px solid #c0c0c0; border-radius: 4px; padding: 5px 12px; }\n"
"button:hover { background-color: #dcdcdc; border-color: #aaa; }\n"
"button:active { background-color: #d0d0d0; }\n"
"button:disabled { background-color: #f0f0f0; color: #aaa; border-color: #d8d8d8; }\n"

/* ── Botão INICIAR ── */
"button.action-btn { background-color: #0078d4; color: #fff;"
"  font-weight: bold; border: none; border-radius: 4px; padding: 7px 22px; }\n"
"button.action-btn:hover { background-color: #0085e0; }\n"
"button.action-btn:active { background-color: #005fa3; }\n"
"button.action-btn:disabled { background-color: #d0d0d0; color: #aaa; }\n"

/* ── Botão FECHAR ── */
"button.close-btn { background-color: #ebebeb; color: #555;"
"  border: 1px solid #c0c0c0; border-radius: 4px; padding: 7px 16px; }\n"
"button.close-btn:hover { background-color: #fde8e8; color: #c0392b; border-color: #e0aaaa; }\n"

/* ── Botões de ferramenta (barra inferior) ── */
"button.tool-btn { background-color: #e8e8e8; color: #555;"
"  border: 1px solid #c8c8c8; border-radius: 4px; padding: 4px 10px; font-size: 13px; min-width: 32px; }\n"
"button.tool-btn:hover { background-color: #dcdcdc; color: #222; border-color: #aaa; }\n"

/* ── Botão SELECIONAR ── */
"button.select-btn { background-color: #e0e0e0; color: #1a1a1a;"
"  border: 1px solid #b8b8b8; border-radius: 4px; font-weight: bold; padding: 6px 16px; }\n"
"button.select-btn:hover { background-color: #d4d4d4; border-color: #999; }\n"

/* ── Progressbar ── */
"progressbar trough { background-color: #e0e0e0; border: 1px solid #c8c8c8;"
"  border-radius: 4px; min-height: 22px; }\n"
"progressbar progress { background-color: #0068b5; border-radius: 3px; }\n"
"progressbar.done progress  { background-color: #2e7d32; }\n"
"progressbar.error progress { background-color: #c62828; }\n"
"progressbar > trough > progress { color: #1a1a1a; }\n"

/* ── Checkbutton ── */
"checkbutton { color: #333; font-size: 11px; }\n"
"checkbutton check { background-color: #ffffff; border: 1px solid #b0b0b0; border-radius: 3px; }\n"
"checkbutton:checked check { background-color: #0078d4; border-color: #0068b5; }\n"
"checkbutton:disabled { color: #aaa; }\n"
"checkbutton:disabled check { background-color: #ebebeb; border-color: #d0d0d0; }\n"

/* ── Status bar ── */
".status-bar { background-color: #e8e8e8; color: #666; font-size: 11px; padding: 4px 10px; }\n"
"frame { border: none; }\n"
"frame border { border: none; }\n"

/* ── Diálogos e janelas modais (CLARO) ── */
"dialog { background-color: #f5f5f5; }\n"
"dialog .dialog-vbox, dialog box, dialog > box { background-color: #f5f5f5; }\n"
"messagedialog { background-color: #f5f5f5; }\n"
"messagedialog .dialog-vbox { background-color: #f5f5f5; }\n"
".dialog-action-area { background-color: #f5f5f5; border-top: 1px solid #d8d8d8; padding: 8px; }\n"

/* ── Link buttons ── */
"linkbutton { color: #0069c0; }\n"
"linkbutton:hover { color: #004f99; }\n"
"linkbutton:visited { color: #6a1b9a; }\n"
;



/* =========================================================
 * Estado da aplicação
 * ========================================================= */
typedef struct {
    GtkWidget      *window;
    GtkWidget      *hbar;
    GtkWidget      *combo_device;
    GtkWidget      *entry_iso_path;
    GtkWidget      *btn_select;
    GtkWidget      *combo_partition;
    GtkWidget      *combo_target;
    GtkWidget      *combo_fs;
    GtkWidget      *combo_cluster;
    GtkWidget      *entry_volume;
    GtkWidget      *chk_quick_format;
    GtkWidget      *chk_extended_names;
    GtkWidget      *chk_bad_blocks;
    GtkWidget      *combo_passes;
    GtkWidget      *progress;
    GtkWidget      *lbl_status;
    GtkWidget      *btn_start;
    GtkWidget      *btn_close;
    GtkWidget      *btn_lang;    /* botão globo — abre seletor de idioma */
    GtkWidget      *btn_about;   /* botão ℹ — abre sobre */
    GtkWidget      *btn_wiki;    /* botão 📖 — wiki */
    GtkWidget      *lbl_iso_type;
    GtkWidget      *lbl_warn;
    /* Labels traduzíveis */
    GtkWidget      *lbl_drive_sec;
    GtkWidget      *lbl_format_sec;
    GtkWidget      *lbl_status_sec;
    GtkWidget      *lbl_device;
    GtkWidget      *lbl_boot;
    GtkWidget      *lbl_partition;
    GtkWidget      *lbl_target;
    GtkWidget      *lbl_volume_name;
    GtkWidget      *lbl_fs;
    GtkWidget      *lbl_cluster;
    GtkWidget      *lbl_adv_drive;
    GtkWidget      *lbl_hide_format;

    GtkCssProvider *css_provider;

    char            iso_path[MAX_PATH];
    DriveInfo       drives[MAX_DEVICES];
    int             num_drives;
    int             is_writing;
    int             lang;
    IsoType         iso_type;
    pthread_t       write_thread;
    WriteArgs      *write_args;
    guint           progress_timer;
} AppData;

static AppData g_app;

/* =========================================================
 * Progresso inter-thread
 * ========================================================= */
static volatile double g_frac  = 0.0;
static volatile int    g_done  = 0;
static volatile int    g_error = 0;
static volatile char   g_msg[512] = "";
static pthread_mutex_t g_pmutex   = PTHREAD_MUTEX_INITIALIZER;

static void progress_cb(const WriteProgress *p, void *ud) {
    (void)ud;
    pthread_mutex_lock(&g_pmutex);
    g_frac  = p->fraction;
    g_done  = p->done;
    g_error = p->error;
    memset((char*)g_msg, 0, sizeof(g_msg));
    memcpy((char*)g_msg, p->message,
           strlen(p->message) < sizeof(g_msg)-1 ? strlen(p->message) : sizeof(g_msg)-1);
    pthread_mutex_unlock(&g_pmutex);
}

/* =========================================================
 * Timer GTK — atualiza barra de progresso
 * ========================================================= */
static gboolean tick_progress(gpointer ud) {
    AppData *app = (AppData*)ud;

    pthread_mutex_lock(&g_pmutex);
    double frac = g_frac;
    int    done = g_done;
    int    err  = g_error;
    char   msg[512]; memcpy(msg,(const char*)g_msg,511); msg[511]=0;
    pthread_mutex_unlock(&g_pmutex);

    double safe = frac < 0 ? 0 : frac > 1 ? 1 : frac;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress), safe);
    char pct[16]; snprintf(pct,sizeof(pct),"%.0f%%",safe*100.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->progress), pct);
    gtk_label_set_text(GTK_LABEL(app->lbl_status), msg);

    GtkStyleContext *sc = gtk_widget_get_style_context(app->progress);
    if (done || err) {
        gtk_style_context_remove_class(sc,"done");
        gtk_style_context_remove_class(sc,"error");
        if (done) {
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress),1.0);
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->progress),"100%");
            gtk_style_context_add_class(sc,"done");
        } else {
            gtk_style_context_add_class(sc,"error");
        }
        gtk_widget_set_sensitive(app->btn_start, TRUE);
        app->is_writing = 0;
        app->progress_timer = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

/* =========================================================
 * Aplica CSS
 * ========================================================= */
static void apply_theme(AppData *app) {
    gtk_css_provider_load_from_data(app->css_provider, CSS_DARK, -1, NULL);
}

/* =========================================================
 * Aplica tradução
 * ========================================================= */
static void apply_lang(AppData *app) {
    i18n_set(app->lang);

    gtk_header_bar_set_title(GTK_HEADER_BAR(app->hbar), APP_NAME);
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(app->hbar), T("subtitle"));

    gtk_label_set_text(GTK_LABEL(app->lbl_drive_sec),    T("drive_props"));
    gtk_label_set_text(GTK_LABEL(app->lbl_format_sec),   T("format_opts"));
    gtk_label_set_text(GTK_LABEL(app->lbl_status_sec),   T("status"));
    gtk_label_set_text(GTK_LABEL(app->lbl_device),       T("device"));
    gtk_label_set_text(GTK_LABEL(app->lbl_boot),         T("boot_sel"));
    gtk_label_set_text(GTK_LABEL(app->lbl_partition),    T("part_scheme"));
    gtk_label_set_text(GTK_LABEL(app->lbl_target),       T("target_sys"));
    gtk_label_set_text(GTK_LABEL(app->lbl_volume_name),  T("volume_name"));
    gtk_label_set_text(GTK_LABEL(app->lbl_fs),           T("filesystem"));
    gtk_label_set_text(GTK_LABEL(app->lbl_cluster),      T("cluster_size"));
    gtk_label_set_text(GTK_LABEL(app->lbl_adv_drive),    T("show_adv_drive"));
    gtk_label_set_text(GTK_LABEL(app->lbl_hide_format),  T("hide_adv_format"));

    gtk_button_set_label(GTK_BUTTON(app->chk_quick_format),   T("quick_fmt"));
    gtk_button_set_label(GTK_BUTTON(app->chk_extended_names), T("ext_names"));
    gtk_button_set_label(GTK_BUTTON(app->chk_bad_blocks),     T("bad_blocks"));

    gtk_button_set_label(GTK_BUTTON(app->btn_select), T("select_btn"));
    gtk_button_set_label(GTK_BUTTON(app->btn_start),  T("start_btn"));
    gtk_button_set_label(GTK_BUTTON(app->btn_close),  T("close_btn"));

    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_iso_path),
                                   T("iso_placeholder"));
    if (!app->is_writing)
        gtk_label_set_text(GTK_LABEL(app->lbl_status), T("no_iso"));
}

/* =========================================================
 * Diálogo: Seletor de Idioma (botão 🌐)
 * ========================================================= */
static void lang_btn_clicked(GtkButton *btn, gpointer dlg) {
    gpointer lang_ptr = g_object_get_data(G_OBJECT(btn), "lang");
    g_object_set_data(G_OBJECT(dlg), "chosen", lang_ptr);
    gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
}

static void on_lang_dialog(GtkButton *b, gpointer ud) {
    (void)b;
    AppData *app = (AppData*)ud;

    GtkWidget *dlg = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dlg), "Idioma / Language");
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(app->window));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 300, 160);

    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(box), 18);
    gtk_box_set_spacing(GTK_BOX(box), 12);

    GtkWidget *lbl = gtk_label_new("Selecione o idioma / Select language:");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    GtkWidget *hrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *btn_pt = gtk_button_new_with_label("🇧🇷  Português");
    GtkWidget *btn_en = gtk_button_new_with_label("🇺🇸  English");
    gtk_widget_set_hexpand(btn_pt, TRUE);
    gtk_widget_set_hexpand(btn_en, TRUE);

    if (app->lang == LANG_PT)
        gtk_style_context_add_class(gtk_widget_get_style_context(btn_pt), "action-btn");
    else
        gtk_style_context_add_class(gtk_widget_get_style_context(btn_en), "action-btn");

    g_object_set_data(G_OBJECT(btn_pt), "lang", GINT_TO_POINTER(LANG_PT));
    g_object_set_data(G_OBJECT(btn_en), "lang", GINT_TO_POINTER(LANG_EN));
    g_object_set_data(G_OBJECT(dlg), "chosen", GINT_TO_POINTER(app->lang));

    g_signal_connect(btn_pt, "clicked", G_CALLBACK(lang_btn_clicked), dlg);
    g_signal_connect(btn_en, "clicked", G_CALLBACK(lang_btn_clicked), dlg);

    gtk_box_pack_start(GTK_BOX(hrow), btn_pt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hrow), btn_en, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), hrow, FALSE, FALSE, 0);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));

    int chosen = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dlg), "chosen"));
    gtk_widget_destroy(dlg);

    if (chosen != app->lang) {
        app->lang = chosen;
        apply_lang(app);
    }
}

/* =========================================================
 * Diálogo: Sobre / About (botão ℹ)
 * ========================================================= */
static void on_about_dialog(GtkButton *b, gpointer ud) {
    (void)b;
    AppData *app = (AppData*)ud;

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        app->lang == LANG_PT ? "Sobre o Rutlix" : "About Rutlix",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_OK", GTK_RESPONSE_OK,
        NULL);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 340, 320);

    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_box_set_spacing(GTK_BOX(box), 8);

    /* Logo / título */
    GtkWidget *lbl_app = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_app),
        "<span size='20000' weight='bold' foreground='#111111'>Rutlix</span>"
        "<span size='13000' foreground='#666666'> Record</span>");
    gtk_widget_set_halign(lbl_app, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), lbl_app, FALSE, FALSE, 0);

    GtkWidget *lbl_ver = gtk_label_new("v" APP_VERSION " — Gravador de ISO para Linux");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_ver), "about-ver");
    gtk_widget_set_halign(lbl_ver, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), lbl_ver, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* Criador */
    GtkWidget *lbl_by = gtk_label_new(
        app->lang == LANG_PT ? "Criado por:" : "Created by:");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_by), "dim-label");
    gtk_widget_set_halign(lbl_by, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl_by, FALSE, FALSE, 0);

    GtkWidget *lbl_name = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_name),
        "<span size='13000' weight='bold' foreground='#111111'>Ailton Martins</span>");
    gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl_name, FALSE, FALSE, 0);

    /* Links */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_top(grid, 4);

    struct { const char *icon; const char *label; const char *url; } links[] = {
        { "📧", "ailton.martins.031227@gmail.com",
          "mailto:ailton.martins.031227@gmail.com" },
        { "📸", "@ailton_0041 (Instagram)",
          "https://instagram.com/ailton_0041" },
        { "🐙", "ailton-am (GitHub)",
          "https://github.com/ailton-am" },
        { NULL, NULL, NULL }
    };

    for (int i = 0; links[i].icon; i++) {
        GtkWidget *icon_lbl = gtk_label_new(links[i].icon);
        gtk_widget_set_halign(icon_lbl, GTK_ALIGN_END);

        GtkWidget *link = gtk_link_button_new_with_label(links[i].url, links[i].label);
        gtk_widget_set_halign(link, GTK_ALIGN_START);

        gtk_grid_attach(GTK_GRID(grid), icon_lbl, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), link,     1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* Projeto */
    GtkWidget *lbl_proj = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_proj),
        app->lang == LANG_PT
        ? "<small>Projeto open-source — C + Assembly x86-64 + GTK3\n"
          "Licença: MIT</small>"
        : "<small>Open-source project — C + Assembly x86-64 + GTK3\n"
          "License: MIT</small>");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_proj), "dim-label");
    gtk_widget_set_halign(lbl_proj, GTK_ALIGN_CENTER);
    gtk_label_set_justify(GTK_LABEL(lbl_proj), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), lbl_proj, FALSE, FALSE, 0);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

/* =========================================================
 * Diálogo: Wiki do Projeto (botão 📖)
 * ========================================================= */
static void on_wiki_dialog(GtkButton *b, gpointer ud) {
    (void)b;
    AppData *app = (AppData*)ud;

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        "Wiki / Documentação",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Fechar", GTK_RESPONSE_CLOSE,
        NULL);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 360, 300);

    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_box_set_spacing(GTK_BOX(box), 10);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='13000' weight='bold'>📖  Rutlix Record — Wiki</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    const char *sections[][2] = {
        { "GitHub (código-fonte)",  "https://github.com/ailton-am/rutlix" },
        { "Wiki do projeto",        "https://github.com/ailton-am/rutlix/wiki" },
        { "Reportar bug / Issue",   "https://github.com/ailton-am/rutlix/issues" },
        { "Licença MIT",            "https://github.com/ailton-am/rutlix/blob/main/LICENSE" },
        { NULL, NULL }
    };

    for (int i = 0; sections[i][0]; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *bullet = gtk_label_new("▸");
        gtk_style_context_add_class(gtk_widget_get_style_context(bullet), "dim-label");
        GtkWidget *link = gtk_link_button_new_with_label(sections[i][1], sections[i][0]);
        gtk_widget_set_halign(link, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), bullet, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), link,   FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(box),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    GtkWidget *note = gtk_label_new(
        "Contribuições são bem-vindas!\n"
        "Abra um issue ou pull request no GitHub.");
    gtk_style_context_add_class(gtk_widget_get_style_context(note), "dim-label");
    gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), note, FALSE, FALSE, 0);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

/* =========================================================
 * Refresh de dispositivos
 * ========================================================= */
static void on_refresh_devices(GtkButton *b, gpointer ud) {
    (void)b; AppData *app = (AppData*)ud;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(app->combo_device));
    app->num_drives = disk_scan_removable(app->drives, MAX_DEVICES);
    if (app->num_drives == 0) {
        gtk_combo_box_text_append_text(
            GTK_COMBO_BOX_TEXT(app->combo_device), T("no_device"));
    } else {
        for (int i = 0; i < app->num_drives; i++) {
            char e[320];
            snprintf(e,sizeof(e),"%s  —  %s  [%s]",
                     app->drives[i].path,
                     app->drives[i].label,
                     app->drives[i].size_str);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_device),e);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_device),0);
    }
    char st[128]; snprintf(st,sizeof(st),T("dev_found"),app->num_drives);
    gtk_label_set_text(GTK_LABEL(app->lbl_status),st);
}

/* =========================================================
 * Badge de tipo de ISO
 * ========================================================= */
static void update_iso_badge(AppData *app) {
    if (!app->iso_path[0]) {
        gtk_label_set_text(GTK_LABEL(app->lbl_iso_type),""); return;
    }
    IsoInfo info;
    if (iso_analyze(app->iso_path,&info)) app->iso_type=info.type;
    else app->iso_type=ISO_UNKNOWN;

    GtkStyleContext *sc=gtk_widget_get_style_context(app->lbl_iso_type);
    gtk_style_context_remove_class(sc,"win-badge");
    gtk_style_context_remove_class(sc,"linux-badge");
    gtk_style_context_remove_class(sc,"dim-label");

    if (app->iso_type==ISO_WINDOWS) {
        gtk_label_set_text(GTK_LABEL(app->lbl_iso_type),"⊞  Windows Installation Media");
        gtk_style_context_add_class(sc,"win-badge");
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_partition),1);
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_target),1);
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_fs),1);
        if (info.wim_needs_split)
            gtk_label_set_text(GTK_LABEL(app->lbl_warn),T("win_large_warn"));
        else
            gtk_label_set_text(GTK_LABEL(app->lbl_warn),"");
    } else if (app->iso_type==ISO_LINUX) {
        gtk_label_set_text(GTK_LABEL(app->lbl_iso_type),"🐧  Linux ISO");
        gtk_style_context_add_class(sc,"linux-badge");
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_partition),0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_target),2);
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_fs),0);
        gtk_label_set_text(GTK_LABEL(app->lbl_warn),"");
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_iso_type),"💿  ISO");
        gtk_style_context_add_class(sc,"dim-label");
        gtk_label_set_text(GTK_LABEL(app->lbl_warn),"");
    }
}

/* =========================================================
 * Seleção de ISO
 * ========================================================= */
static void on_select_iso(GtkButton *b, gpointer ud) {
    (void)b; AppData *app=(AppData*)ud;

    GtkWidget *dlg=gtk_file_chooser_dialog_new(
        T("select_iso_title"),GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        T("cancel"),GTK_RESPONSE_CANCEL,
        T("open"),GTK_RESPONSE_ACCEPT,NULL);

    GtkFileFilter *fi=gtk_file_filter_new();
    gtk_file_filter_set_name(fi,T("filter_iso"));
    gtk_file_filter_add_pattern(fi,"*.iso");
    gtk_file_filter_add_pattern(fi,"*.ISO");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg),fi);

    GtkFileFilter *fa=gtk_file_filter_new();
    gtk_file_filter_set_name(fa,T("filter_all"));
    gtk_file_filter_add_pattern(fa,"*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg),fa);

    if (gtk_dialog_run(GTK_DIALOG(dlg))==GTK_RESPONSE_ACCEPT) {
        char *fname=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        strncpy(app->iso_path,fname,MAX_PATH-1);
        gtk_entry_set_text(GTK_ENTRY(app->entry_iso_path),fname);
        uint64_t sz=asm_get_file_size(fname);
        char szs[32]; disk_format_size(sz,szs,sizeof(szs));
        update_iso_badge(app);
        char st[256]; snprintf(st,sizeof(st),T("iso_selected_status"),szs);
        gtk_label_set_text(GTK_LABEL(app->lbl_status),st);
        g_free(fname);
    }
    gtk_widget_destroy(dlg);
}

/* =========================================================
 * Toggle bad-blocks checkbox — habilita combo de passadas
 * ========================================================= */
static void on_bad_blocks_toggled(GtkToggleButton *tb, gpointer ud) {
    AppData *app = (AppData*)ud;
    int active = gtk_toggle_button_get_active(tb);
    gtk_widget_set_sensitive(app->combo_passes, active);
}

/* =========================================================
 * Iniciar gravação
 * ========================================================= */
static void on_start_clicked(GtkButton *b, gpointer ud) {
    (void)b; AppData *app=(AppData*)ud;

    int idx=gtk_combo_box_get_active(GTK_COMBO_BOX(app->combo_device));
    if (idx<0||idx>=app->num_drives) {
        GtkWidget *d=gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,
            "%s",T("err_no_device"));
        gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d); return;
    }
    if (!app->iso_path[0]) {
        GtkWidget *d=gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,
            "%s",T("err_no_iso"));
        gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d); return;
    }

    char iso_err[512]={0};
    if (!disk_is_valid_iso(app->iso_path,iso_err,sizeof(iso_err))&&iso_err[0]) {
        GtkWidget *d=gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,GTK_MESSAGE_WARNING,GTK_BUTTONS_YES_NO,
            "%s\n\n%s",iso_err,T("continue_anyway"));
        int r=gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
        if (r!=GTK_RESPONSE_YES) return;
    }

    DriveInfo *drv=&app->drives[idx];
    GtkWidget *conf=gtk_message_dialog_new(GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL,GTK_MESSAGE_WARNING,GTK_BUTTONS_YES_NO,
        T("confirm_format"),drv->path,drv->label,drv->size_str);
    int r=gtk_dialog_run(GTK_DIALOG(conf)); gtk_widget_destroy(conf);
    if (r!=GTK_RESPONSE_YES) return;

    WriteArgs *wa=(WriteArgs*)calloc(1,sizeof(WriteArgs));
    strncpy(wa->iso_path,   app->iso_path,MAX_PATH-1);
    strncpy(wa->device_path,drv->path,127);
    wa->iso_type        =app->iso_type;
    wa->partition_scheme=gtk_combo_box_get_active(GTK_COMBO_BOX(app->combo_partition));
    wa->filesystem      =gtk_combo_box_get_active(GTK_COMBO_BOX(app->combo_fs));
    wa->quick_format    =gtk_toggle_button_get_active(
                            GTK_TOGGLE_BUTTON(app->chk_quick_format));
    wa->bad_blocks      =gtk_toggle_button_get_active(
                            GTK_TOGGLE_BUTTON(app->chk_bad_blocks));
    wa->bad_block_passes=gtk_combo_box_get_active(GTK_COMBO_BOX(app->combo_passes))+1;
    wa->cancel          =0;
    wa->callback        =progress_cb;
    wa->user_data       =NULL;
    const char *vol=gtk_entry_get_text(GTK_ENTRY(app->entry_volume));
    strncpy(wa->volume_label,vol,31);
    app->write_args=wa;

    pthread_mutex_lock(&g_pmutex);
    g_frac=0.0; g_done=0; g_error=0;
    strncpy((char*)g_msg,T("writing"),511);
    pthread_mutex_unlock(&g_pmutex);

    GtkStyleContext *sc=gtk_widget_get_style_context(app->progress);
    gtk_style_context_remove_class(sc,"done");
    gtk_style_context_remove_class(sc,"error");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress),0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->progress),"0%");
    gtk_label_set_text(GTK_LABEL(app->lbl_status),T("writing"));
    gtk_widget_set_sensitive(app->btn_start,FALSE);

    app->is_writing=1;
    pthread_create(&app->write_thread,NULL,disk_write_thread,wa);
    app->progress_timer=g_timeout_add(150,tick_progress,app);
}

static void on_close_clicked(GtkButton *b, gpointer ud) {
    (void)b; AppData *app=(AppData*)ud;
    if (app->is_writing) {
        GtkWidget *d=gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,
            "%s",T("confirm_abort"));
        int r=gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
        if (r!=GTK_RESPONSE_YES) return;
        if (app->write_args) app->write_args->cancel=1;
    }
    gtk_main_quit();
}

/* =========================================================
 * Helper: label alinhado à esquerda
 * ========================================================= */
static GtkWidget *mlabel(const char *txt, const char *cls) {
    GtkWidget *l=gtk_label_new(txt);
    gtk_widget_set_halign(l,GTK_ALIGN_START);
    if (cls) gtk_style_context_add_class(gtk_widget_get_style_context(l),cls);
    return l;
}

/* =========================================================
 * Constrói a interface GTK
 * ========================================================= */
static void build_ui(AppData *app) {
    app->css_provider=gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(app->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* Janela */
    app->window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(app->window),540,660);
    gtk_window_set_resizable(GTK_WINDOW(app->window),FALSE);
    g_signal_connect(app->window,"delete-event",G_CALLBACK(gtk_main_quit),NULL);

    /* Headerbar */
    app->hbar=gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(app->hbar),FALSE);
    gtk_window_set_titlebar(GTK_WINDOW(app->window),app->hbar);

    /* Layout raiz */
    GtkWidget *vroot=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_container_add(GTK_CONTAINER(app->window),vroot);

    /* Área de conteúdo scrollável */
    GtkWidget *scroll=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vroot),scroll,TRUE,TRUE,0);

    GtkWidget *content=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_container_set_border_width(GTK_CONTAINER(content),12);
    gtk_container_add(GTK_CONTAINER(scroll),content);

    /* ── SEÇÃO: Drive Properties ── */
    app->lbl_drive_sec=mlabel("","section-title");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_drive_sec,FALSE,FALSE,2);
    gtk_box_pack_start(GTK_BOX(content),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    /* Dispositivo */
    app->lbl_device=mlabel("","dim-label");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_device,FALSE,FALSE,0);
    GtkWidget *hdev=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
    app->combo_device=gtk_combo_box_text_new();
    gtk_widget_set_hexpand(app->combo_device,TRUE);
    GtkWidget *btn_ref=gtk_button_new_with_label("↻");
    gtk_widget_set_tooltip_text(btn_ref,"Refresh / Atualizar");
    g_signal_connect(btn_ref,"clicked",G_CALLBACK(on_refresh_devices),app);
    gtk_box_pack_start(GTK_BOX(hdev),app->combo_device,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(hdev),btn_ref,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(content),hdev,FALSE,FALSE,0);

    /* ISO */
    app->lbl_boot=mlabel("","dim-label");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_boot,FALSE,FALSE,0);
    GtkWidget *hiso=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
    app->entry_iso_path=gtk_entry_new();
    gtk_widget_set_hexpand(app->entry_iso_path,TRUE);
    gtk_editable_set_editable(GTK_EDITABLE(app->entry_iso_path),FALSE);
    app->btn_select=gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_select),"select-btn");
    g_signal_connect(app->btn_select,"clicked",G_CALLBACK(on_select_iso),app);
    gtk_box_pack_start(GTK_BOX(hiso),app->entry_iso_path,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(hiso),app->btn_select,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(content),hiso,FALSE,FALSE,0);

    /* Badge */
    app->lbl_iso_type=mlabel("","dim-label");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_iso_type,FALSE,FALSE,0);
    app->lbl_warn=mlabel("","warn-label");
    gtk_label_set_line_wrap(GTK_LABEL(app->lbl_warn),TRUE);
    gtk_box_pack_start(GTK_BOX(content),app->lbl_warn,FALSE,FALSE,0);

    /* Grid: Partition / Target */
    GtkWidget *g1=gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g1),10);
    gtk_grid_set_row_spacing(GTK_GRID(g1),3);
    gtk_widget_set_margin_top(g1,4);
    app->lbl_partition=mlabel("","dim-label");
    app->lbl_target   =mlabel("","dim-label");
    app->combo_partition=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_partition),"MBR");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_partition),"GPT");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_partition),0);
    gtk_widget_set_hexpand(app->combo_partition,TRUE);
    app->combo_target=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_target),"BIOS (ou UEFI-CSM)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_target),"UEFI (não-CSM)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_target),"BIOS + UEFI");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_target),0);
    gtk_widget_set_hexpand(app->combo_target,TRUE);
    gtk_grid_attach(GTK_GRID(g1),app->lbl_partition,  0,0,1,1);
    gtk_grid_attach(GTK_GRID(g1),app->lbl_target,     1,0,1,1);
    gtk_grid_attach(GTK_GRID(g1),app->combo_partition,0,1,1,1);
    gtk_grid_attach(GTK_GRID(g1),app->combo_target,   1,1,1,1);
    gtk_box_pack_start(GTK_BOX(content),g1,FALSE,FALSE,0);

    app->lbl_adv_drive=mlabel("","dim-label");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_adv_drive,FALSE,FALSE,2);

    /* ── SEÇÃO: Format Options ── */
    app->lbl_format_sec=mlabel("","section-title");
    gtk_widget_set_margin_top(app->lbl_format_sec,6);
    gtk_box_pack_start(GTK_BOX(content),app->lbl_format_sec,FALSE,FALSE,2);
    gtk_box_pack_start(GTK_BOX(content),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    /* Volume */
    app->lbl_volume_name=mlabel("","dim-label");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_volume_name,FALSE,FALSE,0);
    app->entry_volume=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->entry_volume),"RUTLIX");
    gtk_box_pack_start(GTK_BOX(content),app->entry_volume,FALSE,FALSE,0);

    /* FS / Cluster */
    GtkWidget *g2=gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g2),10);
    gtk_grid_set_row_spacing(GTK_GRID(g2),3);
    gtk_widget_set_margin_top(g2,4);
    app->lbl_fs     =mlabel("","dim-label");
    app->lbl_cluster=mlabel("","dim-label");
    app->combo_fs=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_fs),"FAT32");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_fs),"NTFS");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_fs),"exFAT");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_fs),"ext4");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_fs),0);
    gtk_widget_set_hexpand(app->combo_fs,TRUE);
    app->combo_cluster=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_cluster),"4096 bytes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_cluster),"8192 bytes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_cluster),"16384 bytes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_cluster),"32768 bytes");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_cluster),0);
    gtk_widget_set_hexpand(app->combo_cluster,TRUE);
    gtk_grid_attach(GTK_GRID(g2),app->lbl_fs,      0,0,1,1);
    gtk_grid_attach(GTK_GRID(g2),app->lbl_cluster,  1,0,1,1);
    gtk_grid_attach(GTK_GRID(g2),app->combo_fs,      0,1,1,1);
    gtk_grid_attach(GTK_GRID(g2),app->combo_cluster,  1,1,1,1);
    gtk_box_pack_start(GTK_BOX(content),g2,FALSE,FALSE,0);

    app->lbl_hide_format=mlabel("","dim-label");
    gtk_box_pack_start(GTK_BOX(content),app->lbl_hide_format,FALSE,FALSE,2);

    /* Checkboxes */
    app->chk_quick_format  =gtk_check_button_new_with_label("");
    app->chk_extended_names=gtk_check_button_new_with_label("");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->chk_quick_format),TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->chk_extended_names),TRUE);
    gtk_box_pack_start(GTK_BOX(content),app->chk_quick_format,  FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(content),app->chk_extended_names,FALSE,FALSE,0);

    /* Bad blocks — agora com combo de passadas ativo */
    GtkWidget *hbad=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    app->chk_bad_blocks=gtk_check_button_new_with_label("");
    /* NÃO desabilitado — o usuário pode ativar */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->chk_bad_blocks),FALSE);

    app->combo_passes=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_passes),"1 passada");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_passes),"2 passadas");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_passes),"3 passadas");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_passes),"4 passadas");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_passes),0);
    gtk_widget_set_sensitive(app->combo_passes,FALSE); /* inicia desabilitado */

    g_signal_connect(app->chk_bad_blocks,"toggled",
                     G_CALLBACK(on_bad_blocks_toggled),app);

    gtk_box_pack_start(GTK_BOX(hbad),app->chk_bad_blocks,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbad),app->combo_passes,  FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(content),hbad,FALSE,FALSE,0);

    /* ── SEÇÃO: Status ── */
    app->lbl_status_sec=mlabel("","section-title");
    gtk_widget_set_margin_top(app->lbl_status_sec,6);
    gtk_box_pack_start(GTK_BOX(content),app->lbl_status_sec,FALSE,FALSE,2);
    gtk_box_pack_start(GTK_BOX(content),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    app->progress=gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app->progress),TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->progress),"PRONTO");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress),0.0);
    gtk_box_pack_start(GTK_BOX(content),app->progress,FALSE,FALSE,4);

    /* Status bar */
    app->lbl_status=mlabel("","status-bar");
    gtk_label_set_ellipsize(GTK_LABEL(app->lbl_status),PANGO_ELLIPSIZE_END);
    GtkWidget *sfr=gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(sfr),app->lbl_status);
    gtk_box_pack_end(GTK_BOX(vroot),sfr,FALSE,FALSE,0);

    /* ── Barra de botões inferior ── */
    GtkWidget *hbtns=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
    gtk_container_set_border_width(GTK_CONTAINER(hbtns),8);

    /* 🌐 Idioma */
    app->btn_lang=gtk_button_new_with_label("🌐");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_lang),"tool-btn");
    gtk_widget_set_tooltip_text(app->btn_lang,"Idioma / Language");
    g_signal_connect(app->btn_lang,"clicked",G_CALLBACK(on_lang_dialog),app);
    gtk_box_pack_start(GTK_BOX(hbtns),app->btn_lang,FALSE,FALSE,0);

    /* ℹ Sobre */
    app->btn_about=gtk_button_new_with_label("ℹ");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_about),"tool-btn");
    gtk_widget_set_tooltip_text(app->btn_about,"Sobre / About");
    g_signal_connect(app->btn_about,"clicked",G_CALLBACK(on_about_dialog),app);
    gtk_box_pack_start(GTK_BOX(hbtns),app->btn_about,FALSE,FALSE,0);

    /* 📖 Wiki */
    app->btn_wiki=gtk_button_new_with_label("📖");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_wiki),"tool-btn");
    gtk_widget_set_tooltip_text(app->btn_wiki,"Wiki / Documentação");
    g_signal_connect(app->btn_wiki,"clicked",G_CALLBACK(on_wiki_dialog),app);
    gtk_box_pack_start(GTK_BOX(hbtns),app->btn_wiki,FALSE,FALSE,0);

    /* ⚙ Configurações (placeholder) */
    GtkWidget *btn_cfg=gtk_button_new_with_label("⚙");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_cfg),"tool-btn");
    gtk_widget_set_tooltip_text(btn_cfg,"Configurações / Settings");
    gtk_box_pack_start(GTK_BOX(hbtns),btn_cfg,FALSE,FALSE,0);

    /* Espaçador */
    gtk_box_pack_start(GTK_BOX(hbtns),gtk_label_new(""),TRUE,TRUE,0);

    /* INICIAR */
    app->btn_start=gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_start),"action-btn");
    g_signal_connect(app->btn_start,"clicked",G_CALLBACK(on_start_clicked),app);
    gtk_box_pack_start(GTK_BOX(hbtns),app->btn_start,FALSE,FALSE,0);

    /* FECHAR */
    app->btn_close=gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_close),"close-btn");
    g_signal_connect(app->btn_close,"clicked",G_CALLBACK(on_close_clicked),app);
    gtk_box_pack_start(GTK_BOX(hbtns),app->btn_close,FALSE,FALSE,0);

    gtk_box_pack_end(GTK_BOX(vroot),hbtns,FALSE,FALSE,0);

    gtk_widget_show_all(app->window);
}

/* =========================================================
 * main
 * ========================================================= */
int main(int argc, char *argv[]) {
    gtk_init(&argc,&argv);
    memset(&g_app,0,sizeof(g_app));
    g_app.lang     =LANG_PT;
    g_app.iso_type =ISO_UNKNOWN;

    build_ui(&g_app);
    apply_theme(&g_app);
    apply_lang(&g_app);
    on_refresh_devices(NULL,&g_app);

    gtk_main();
    return 0;
}
