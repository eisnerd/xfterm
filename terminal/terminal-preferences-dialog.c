/* $Id$ */
/*-
 * Copyright (c) 2004 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <terminal/terminal-enum-types.h>
#include <terminal/terminal-preferences-dialog.h>
#include <terminal/terminal-shortcut-editor.h>



enum
{
  PIXBUF_COLUMN,
  TEXT_COLUMN,
  INDEX_COLUMN,
  LAST_COLUMN,
};



static void     terminal_preferences_dialog_class_init              (TerminalPreferencesDialogClass *klass);
static void     terminal_preferences_dialog_init                    (TerminalPreferencesDialog      *dialog);
static void     terminal_preferences_dialog_finalize                (GObject                        *object);
static void     terminal_preferences_dialog_response                (TerminalPreferencesDialog      *dialog,
                                                                     gint                            response);
static void     terminal_preferences_open_image_file                (GtkWidget                      *button,
                                                                     GtkWidget                      *entry);
static void     terminal_preferences_dialog_reset_compat            (GtkWidget                      *button,
                                                                     TerminalPreferencesDialog      *dialog);



static GObjectClass *parent_class;



G_DEFINE_TYPE (TerminalPreferencesDialog, terminal_preferences_dialog, GTK_TYPE_DIALOG);



static void
terminal_preferences_dialog_class_init (TerminalPreferencesDialogClass *klass)
{
  GObjectClass *gobject_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = terminal_preferences_dialog_finalize;
}



static void
converter_enum_int (GValue  *value,
                    gboolean in,
                    gpointer user_data)
{
  gint ival;

  if (in)
    {
      ival = g_value_get_int (value);
      g_value_unset (value);
      g_value_init (value, GPOINTER_TO_INT (user_data));
      g_value_set_enum (value, ival);
    }
  else
    {
      ival = g_value_get_enum (value);
      g_value_unset (value);
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, ival);
    }
}



static void
converter_double_uint (GValue    *value,
                       gboolean   in,
                       gpointer   user_data)
{
  guint uval;

  if (in)
    {
      uval = g_value_get_uint (value);
      g_value_unset (value);
      g_value_init (value, G_TYPE_DOUBLE);
      g_value_set_double (value, uval);
    }
  else
    {
      uval = (guint) g_value_get_double (value);
      g_value_unset (value);
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, uval);
    }
}



/* Convert int -> boolean (i == 1) */
static void
converter_is1 (GValue  *value,
               gboolean in,
               gpointer user_data)
{
  gint ival;

  g_assert (in);

  ival = g_value_get_int (value);
  g_value_unset (value);
  g_value_init (value, G_TYPE_BOOLEAN);
  g_value_set_boolean (value, ival == 1);
}



/* Convert int -> boolean (i == 1 || i == 2) */
static void
converter_is1_or_is2 (GValue  *value,
                      gboolean in,
                      gpointer user_data)
{
  gint ival;

  g_assert (in);

  ival = g_value_get_int (value);
  g_value_unset (value);
  g_value_init (value, G_TYPE_BOOLEAN);
  g_value_set_boolean (value, ival == 1 || ival == 2);
}



static void
terminal_preferences_dialog_init (TerminalPreferencesDialog *dialog)
{
  ExoPropertyProxy  *proxy;
  ExoPropertyProxy  *proxy2;
  GtkListStore      *store;
  GParamSpec        *pspec;
  GtkTreeIter        iter;
  GdkPixbuf         *icon;
  GtkWidget         *button;
  GtkWidget         *entry;
  GtkWidget         *hbox;
  GtkWidget         *swin;
  GtkWidget         *box;
  GtkWidget         *frame;
  GtkWidget         *vbox;
  GtkWidget         *table;
  GtkWidget         *combo;
  GtkWidget         *label;
  GtkWidget         *image;
  GtkWidget         *ibox;
  GtkWidget         *editor;
  gint               index;

  dialog->preferences = terminal_preferences_get ();

  dialog->tooltips = gtk_tooltips_new ();
  g_object_ref (G_OBJECT (dialog->tooltips));
  gtk_object_sink (GTK_OBJECT (dialog->tooltips));

  g_object_set (G_OBJECT (dialog),
                "title", _("Terminal Preferences"),
                "destroy-with-parent", TRUE,
                "has-separator", FALSE,
                "resizable", FALSE,
                NULL);

  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CLOSE);
  gtk_widget_show (button);

  button = gtk_button_new_from_stock (GTK_STOCK_HELP);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_HELP);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (terminal_preferences_dialog_response), NULL);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  swin = gtk_scrolled_window_new (NULL, NULL);
  g_object_set (G_OBJECT (swin),
                "hscrollbar-policy", GTK_POLICY_NEVER,
                "shadow-type", GTK_SHADOW_IN,
                "vscrollbar-policy", GTK_POLICY_NEVER,
                NULL);
  gtk_box_pack_start (GTK_BOX (hbox), swin, FALSE, FALSE, 0);
  gtk_widget_show (swin);

  store = gtk_list_store_new (LAST_COLUMN, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);

  dialog->icon_bar = exo_icon_bar_new_with_model (GTK_TREE_MODEL (store));
  exo_icon_bar_set_pixbuf_column (EXO_ICON_BAR (dialog->icon_bar), PIXBUF_COLUMN);
  exo_icon_bar_set_text_column (EXO_ICON_BAR (dialog->icon_bar), TEXT_COLUMN);
  gtk_container_add (GTK_CONTAINER (swin), dialog->icon_bar);
  gtk_widget_show (dialog->icon_bar);

  dialog->notebook = gtk_notebook_new ();
  g_object_set (G_OBJECT (dialog->notebook),
                "show-border", FALSE,
                "show-tabs", FALSE,
                NULL);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->notebook, TRUE, TRUE, 0);
  gtk_widget_show (dialog->notebook);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->notebook), "page", NULL, NULL, NULL);
  exo_property_proxy_add (proxy, G_OBJECT (dialog->icon_bar), "active", NULL, NULL, NULL);
  g_object_unref (G_OBJECT (proxy));

  
  /*
     General
   */
  box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  index = gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), box, NULL);
  gtk_widget_show (box);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Title</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);
  
  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = g_object_new (GTK_TYPE_LABEL,
                        "wrap", TRUE,
                        "label", _("The command running inside the terminal may dynamically set a new title."),
                        "xalign", 0.0,
                        NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 2, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new_with_mnemonic (_("_Initial title:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "title-initial");
  exo_property_proxy_add (proxy, G_OBJECT (entry), "text", NULL, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (entry);

  label = gtk_label_new_with_mnemonic (_("_Dynamically-set title:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (hbox);
  
  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Replaces initial title"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Goes before initial title"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Goes after initial title"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Isn't displayed"));
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, TRUE, 0);
  gtk_widget_show (combo);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->preferences), "title-mode",
                          converter_enum_int, GINT_TO_POINTER (TERMINAL_TYPE_TITLE), NULL);
  exo_property_proxy_add (proxy, G_OBJECT (combo), "active", NULL, NULL, NULL);
  g_object_unref (G_OBJECT (proxy));

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Command</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  button = gtk_check_button_new_with_mnemonic (_("_Run command as login shell"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "command-login-shell");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("_Update utmp/wtmp records when command is launched"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "command-update-records");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Misc</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);
  
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 24);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("_Scrollbar is:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 6);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Disabled"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("On the left side"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("On the right side"));
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->preferences), "scrolling-bar",
                          converter_enum_int, GINT_TO_POINTER (TERMINAL_TYPE_SCROLLBAR), NULL);
  exo_property_proxy_add (proxy, G_OBJECT (combo), "active", NULL, NULL, NULL);
  g_object_unref (G_OBJECT (proxy));

  label = gtk_label_new_with_mnemonic (_("Scr_ollback:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  button = gtk_spin_button_new_with_range (0.0, 5 * 1024.0 * 1024.0, 1.0);
  proxy = terminal_preferences_get_proxy (dialog->preferences, "scrolling-lines");
  exo_property_proxy_add (proxy, G_OBJECT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (button))),
                          "value", converter_double_uint, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
  gtk_widget_show (table);

  button = gtk_check_button_new_with_mnemonic (_("Sc_roll on output"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "scrolling-on-output");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("Scroll on key_stroke"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "scrolling-on-keystroke");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("Show _menubar by default"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "misc-menubar-default");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 2, 3,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("Terminal _bell"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "misc-bell");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 2, 3,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("Show _borders by default"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "misc-borders-default");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_tooltips_set_tip (dialog->tooltips, button,
                        _("Enable to show window decorations around newly "
                          "created terminal windows."), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 3, 4,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("Cursor blin_ks"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "misc-cursor-blinks");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 3, 4,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  icon = xfce_themed_icon_load ("terminal-general", 48);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      PIXBUF_COLUMN, icon,
                      TEXT_COLUMN, _("General"),
                      INDEX_COLUMN, index,
                      -1);
  if (icon != NULL)
    g_object_unref (icon);


  /*
     Appearance
   */
  box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  index = gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), box, NULL);
  gtk_widget_show (box);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Font</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  button = g_object_new (GTK_TYPE_FONT_BUTTON, "title", _("Choose Terminal Font"), "use-font", TRUE, NULL);
  proxy = terminal_preferences_get_proxy (dialog->preferences, "font-name");
  exo_property_proxy_add (proxy, G_OBJECT (button), "font-name", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Colors</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Text color:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-foreground");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  label = gtk_label_new_with_mnemonic (_("_Background color:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-background");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Terminal applications have this color palette available to them:"),
                        "wrap", TRUE,
                        NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  table = gtk_table_new (2, 8, TRUE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 0);
  gtk_table_set_col_spacings (GTK_TABLE (table), 0);
  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, TRUE, 0);
  gtk_widget_show (table);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette1");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette1");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette2");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette2");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette3");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette3");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette4");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette4");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 3, 4, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette5");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette5");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 4, 5, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette6");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette6");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 5, 6, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette7");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette7");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 6, 7, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette8");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette8");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 7, 8, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette9");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette9");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette10");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette10");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette11");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette11");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette12");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette12");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 3, 4, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette13");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette13");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 4, 5, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette14");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette14");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 5, 6, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette15");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette15");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 6, 7, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_color_button_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "color-palette16");
  exo_property_proxy_add (proxy, G_OBJECT (button), "color", NULL, NULL, NULL);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences), "color-palette16");
  gtk_tooltips_set_tip (dialog->tooltips, button, g_param_spec_get_blurb (pspec), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 7, 8, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Background</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 24);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("None (use solid color)"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Background image"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Transparent background"));
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
  gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, TRUE, 0);
  gtk_widget_show (combo);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->preferences), "background-mode",
                          converter_enum_int, GINT_TO_POINTER (TERMINAL_TYPE_BACKGROUND), NULL);
  exo_property_proxy_add (proxy, G_OBJECT (combo), "active", NULL, NULL, NULL);

  ibox = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), ibox, FALSE, TRUE, 0);
  gtk_widget_show (ibox);

  exo_property_proxy_add (proxy, G_OBJECT (ibox), "visible", converter_is1, NULL, NULL);

  label = gtk_label_new_with_mnemonic (_("_Image file:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (ibox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (ibox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  label = g_object_new (GTK_TYPE_ALIGNMENT, "width-request", 12, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  proxy2 = terminal_preferences_get_proxy (dialog->preferences, "background-image-file");
  exo_property_proxy_add (proxy2, G_OBJECT (entry), "text", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  button = gtk_button_new ();
  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), image);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (terminal_preferences_open_image_file), entry);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (image);
  gtk_widget_show (button);

  hbox = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (ibox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  label = g_object_new (GTK_TYPE_ALIGNMENT, "width-request", 12, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  ibox = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), ibox, FALSE, TRUE, 0);
  gtk_widget_show (ibox);

  exo_property_proxy_add (proxy, G_OBJECT (ibox), "visible", converter_is1_or_is2, NULL, NULL);

  label = gtk_label_new (_("Shade transparent or image background:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (ibox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (ibox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  label = g_object_new (GTK_TYPE_ALIGNMENT, "width-request", 12, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<small><i>None</i></small>"), "use-markup", TRUE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  button = gtk_hscale_new_with_range (0.0, 1.0, 0.05);
  proxy2 = terminal_preferences_get_proxy (dialog->preferences, "background-darkness");
  exo_property_proxy_add (proxy2, G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (button))), "value", NULL, NULL, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (button), FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<small><i>Maximum</i></small>"), "use-markup", TRUE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  g_object_unref (G_OBJECT (proxy));

  icon = xfce_themed_icon_load ("terminal-appearance.png", 48);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      PIXBUF_COLUMN, icon,
                      TEXT_COLUMN, _("Appearance"),
                      INDEX_COLUMN, index,
                      -1);
  if (icon != NULL)
    g_object_unref (icon);


  /*
     Shortcuts
   */
  box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  index = gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), box, NULL);
  gtk_widget_show (box);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Menubar accelerator</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  button = gtk_check_button_new_with_mnemonic (_("Disable m_enu shortcut key (F10 by default)"));
  proxy = terminal_preferences_get_proxy (dialog->preferences, "shortcuts-no-menukey");
  exo_property_proxy_add (proxy, G_OBJECT (button), "active", NULL, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Shortcut keys</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  ibox = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ibox),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (ibox),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (ibox), 12);
  gtk_container_add (GTK_CONTAINER (frame), ibox);
  gtk_widget_show (ibox);

  editor = terminal_shortcut_editor_new ();
  gtk_container_add (GTK_CONTAINER (ibox), editor);
  gtk_widget_show (editor);

  icon = xfce_themed_icon_load ("terminal-shortcuts.png", 48);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      PIXBUF_COLUMN, icon,
                      TEXT_COLUMN, _("Shortcuts"),
                      INDEX_COLUMN, index,
                      -1);
  if (icon != NULL)
    g_object_unref (icon);


  /*
     Advanced
   */
  box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  index = gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), box, NULL);
  gtk_widget_show (box);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Compatibility</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (5, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new (_("These options may cause some applications to behave incorrectly. They are only "
                           "here to allow you to work around certain applications and operating systems "
                           "that expect different terminal behavior."));
  g_object_set (G_OBJECT (label), "wrap", TRUE, "xalign", 0.0, NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 3, 0, 1,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new_with_mnemonic (_("_Backspace key generates:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("ASCII DEL"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Escape sequence"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Control-H"));
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->preferences), "binding-backspace",
                          converter_enum_int, GINT_TO_POINTER (TERMINAL_TYPE_ERASE_BINDING), NULL);
  exo_property_proxy_add (proxy, G_OBJECT (combo), "active", NULL, NULL, NULL);
  g_object_unref (G_OBJECT (proxy));

  label = gtk_label_new_with_mnemonic (_("_Delete key generates:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("ASCII DEL"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Escape sequence"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Control-H"));
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->preferences), "binding-delete",
                          converter_enum_int, GINT_TO_POINTER (TERMINAL_TYPE_ERASE_BINDING), NULL);
  exo_property_proxy_add (proxy, G_OBJECT (combo), "active", NULL, NULL, NULL);
  g_object_unref (G_OBJECT (proxy));

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<tt>$TERM</tt> setting:"), "use-markup", TRUE, NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  proxy = terminal_preferences_get_proxy (dialog->preferences, "term");
  exo_property_proxy_add (proxy, G_OBJECT (entry), "text", NULL, NULL, NULL);
  gtk_tooltips_set_tip (dialog->tooltips, entry, _("This specifies the value the $TERM environment variable is set "
                                                   "to, when a new terminal tab or terminal window is opened. The default "
                                                   "should be ok for most systems. If you have problems with colors in "
                                                   "some applications, try xterm-color here."), NULL);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 3, 4,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (entry);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, 4, 5,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (hbox);

  button = gtk_button_new_with_mnemonic (_("_Reset compatibility options to defaults"));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (terminal_preferences_dialog_reset_compat), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = g_object_new (GTK_TYPE_LABEL, "label", _("<b>Double click</b>"), "use-markup", TRUE, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  label = gtk_label_new (_("Consider the following characters part of a word when double clicking:"));
  g_object_set (G_OBJECT (label), "wrap", TRUE, "xalign", 0.0, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
  gtk_widget_show (entry);

  proxy = exo_property_proxy_new ();
  exo_property_proxy_add (proxy, G_OBJECT (dialog->preferences), "word-chars", NULL, NULL, NULL);
  exo_property_proxy_add (proxy, G_OBJECT (entry), "text", NULL, NULL, NULL);
  g_object_unref (G_OBJECT (proxy));

  icon = xfce_themed_icon_load ("terminal-advanced.png", 48);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      PIXBUF_COLUMN, icon,
                      TEXT_COLUMN, _("Advanced"),
                      INDEX_COLUMN, index,
                      -1);
  if (icon != NULL)
    g_object_unref (icon);


  g_object_unref (G_OBJECT (store));
}



static void
terminal_preferences_dialog_finalize (GObject *object)
{
  TerminalPreferencesDialog *dialog = TERMINAL_PREFERENCES_DIALOG (object);
  g_object_unref (G_OBJECT (dialog->preferences));
  g_object_unref (G_OBJECT (dialog->tooltips));
  G_OBJECT_CLASS (parent_class)->finalize (object);
}



static void
terminal_preferences_dialog_response (TerminalPreferencesDialog      *dialog,
                                      gint                            response)
{
  GtkWidget *message;
  GError    *error = NULL;
  gchar     *command;

  if (response == GTK_RESPONSE_HELP)
    {
      command = g_strconcat (TERMINAL_HELP_BIN, " preferences", NULL);
      if (!g_spawn_command_line_async (command, &error))
        {
          message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            _("Unable to launch online help: %s"),
                                            error->message);
          g_signal_connect (G_OBJECT (message), "response",
                            G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_widget_show (message);

          g_error_free (error);
        }
      g_free (command);
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}



static void
terminal_preferences_open_image_file (GtkWidget *button,
                                      GtkWidget *entry)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;
  gchar     *filename;

  toplevel = gtk_widget_get_toplevel (entry);
  dialog = gtk_file_chooser_dialog_new (_("Select image file"),
                                        GTK_WINDOW (toplevel),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);

  filename = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  if (G_LIKELY (filename != NULL))
    {
      if (*filename != '\0')
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename);
      g_free (filename);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      gtk_entry_set_text (GTK_ENTRY (entry), filename);
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}



static void
terminal_preferences_dialog_reset_compat (GtkWidget                 *button,
                                          TerminalPreferencesDialog *dialog)
{
  GParamSpec *spec;
  GValue      value = {0, };

  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences),
                                       "binding-backspace");
  if (G_LIKELY (spec != NULL))
    {
      g_value_init (&value, spec->value_type);
      g_param_value_set_default (spec, &value); 
      g_object_set_property (G_OBJECT (dialog->preferences), "binding-backspace", &value);
      g_value_unset (&value);
    }

  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences),
                                       "binding-delete");
  if (G_LIKELY (spec != NULL))
    {
      g_value_init (&value, spec->value_type);
      g_param_value_set_default (spec, &value); 
      g_object_set_property (G_OBJECT (dialog->preferences), "binding-delete", &value);
      g_value_unset (&value);
    }

  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (dialog->preferences),
                                       "term");
  if (G_LIKELY (spec != NULL))
    {
      g_value_init (&value, spec->value_type);
      g_param_value_set_default (spec, &value); 
      g_object_set_property (G_OBJECT (dialog->preferences), "term", &value);
      g_value_unset (&value);
    }
}



/**
 * terminal_preferences_dialog_new:
 * @parent      : A #GtkWindow or %NULL.
 *
 * Return value :
 **/
GtkWidget*
terminal_preferences_dialog_new (GtkWindow *parent)
{
  GtkWidget *dialog;
  
  dialog = g_object_new (TERMINAL_TYPE_PREFERENCES_DIALOG, NULL);
  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  return dialog;
}