#include "include/ui.h"
#include <gui/canvas.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void nfc_stock_ui_submenu_callback(void *context, uint32_t index) {
  SceneManager *scene_manager = (SceneManager *)context;
  scene_manager_next_scene(scene_manager, (NfcStockScene)index);
}

NfcStockUi *nfc_stock_ui_alloc(void) {
  NfcStockUi *ui = malloc(sizeof(NfcStockUi));
  ui->submenu = submenu_alloc();
  ui->widget = widget_alloc();
  ui->text_input = text_input_alloc();
  ui->inventory_list = variable_item_list_alloc();
  ui->edit_list = variable_item_list_alloc();
  return ui;
}

void nfc_stock_ui_free(NfcStockUi *ui) {
  submenu_free(ui->submenu);
  widget_free(ui->widget);
  text_input_free(ui->text_input);
  variable_item_list_free(ui->inventory_list);
  variable_item_list_free(ui->edit_list);
  free(ui);
}

void nfc_stock_ui_configure(NfcStockUi *ui, ViewDispatcher *view_dispatcher,
                            SceneManager *scene_manager) {
  (void)scene_manager;

  view_dispatcher_add_view(view_dispatcher, NfcStockViewSubmenu,
                           submenu_get_view(ui->submenu));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewWidget,
                           widget_get_view(ui->widget));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewTextInput,
                           text_input_get_view(ui->text_input));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewInventory,
                           variable_item_list_get_view(ui->inventory_list));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewEdit,
                           variable_item_list_get_view(ui->edit_list));
}

void nfc_stock_ui_apply_main_menu(NfcStockUi *ui, SceneManager *scene_manager,
                                  const char *active_db_path) {
  const char *slash = strrchr(active_db_path, '/');
  const char *base = slash ? slash + 1 : active_db_path;
  char header[40];
  snprintf(header, sizeof(header), "DB: %s", base);

  submenu_reset(ui->submenu);
  submenu_set_header(ui->submenu, header);
  submenu_add_item(ui->submenu, "Scan tag", NfcStockSceneScan,
                   nfc_stock_ui_submenu_callback, scene_manager);
  submenu_add_item(ui->submenu, "Inventory", NfcStockSceneInventory,
                   nfc_stock_ui_submenu_callback, scene_manager);
  submenu_add_item(ui->submenu, "Settings", NfcStockSceneSettings,
                   nfc_stock_ui_submenu_callback, scene_manager);
}

void nfc_stock_ui_show_item(NfcStockUi *ui, const StockItem *item) {
  char buffer[128];
  widget_reset(ui->widget);

  widget_add_string_element(ui->widget, 0, 0, AlignLeft, AlignTop, FontPrimary,
                            "Up:+1 Down:-1 OK:Edit Back:Save");

  snprintf(buffer, sizeof(buffer), "Name: %s", item->name);
  widget_add_string_element(ui->widget, 0, 15, AlignLeft, AlignTop, FontPrimary,
                            buffer);

  snprintf(buffer, sizeof(buffer), "Stock: %" PRId32, item->quantity);
  widget_add_string_element(ui->widget, 0, 30, AlignLeft, AlignTop, FontPrimary,
                            buffer);

  snprintf(buffer, sizeof(buffer), "Location: %s", item->location);
  widget_add_string_element(ui->widget, 0, 45, AlignLeft, AlignTop, FontPrimary,
                            buffer);
}
