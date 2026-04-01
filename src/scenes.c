/**
 * Presentation layer: scene_manager hooks and settings submenu wiring.
 * Single TU keeps Flipper build simple and avoids dozens of tiny scene files.
 */
#include "include/app_types.h"
#include "include/clock.h"
#include "include/db.h"
#include "include/export.h"
#include "include/fs_compat.h"
#include "include/stock.h"
#include "include/ui.h"
#include "../version.h"
#include <furi.h>
#include <furi_hal_random.h>
#include <gui/canvas.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <inttypes.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- Scan ----- */

typedef struct {
  NfcStockApp *app;
  Nfc *nfc;
  NfcPoller *poller;
} ScanContext;

static ScanContext g_scan;

static NfcCommand nfc_stock_scene_scan_callback(NfcGenericEvent event,
                                                void *context) {
  ScanContext *ctx = (ScanContext *)context;
  if (event.protocol != NfcProtocolIso14443_3a || !event.event_data) {
    return NfcCommandContinue;
  }

  const Iso14443_3aPollerEvent *iso_ev =
      (const Iso14443_3aPollerEvent *)event.event_data;
  if (iso_ev->type != Iso14443_3aPollerEventTypeReady) {
    return NfcCommandContinue;
  }

  const Iso14443_3aData *data =
      (const Iso14443_3aData *)nfc_poller_get_data(ctx->poller);
  if (!data || data->uid_len == 0) {
    return NfcCommandContinue;
  }

  NfcStockApp *app = ctx->app;
  const uint8_t *uid = data->uid;
  uint8_t uid_len = data->uid_len;

  StockItem found;
  if (stock_db_find_by_uid(app->active_db_path, uid, uid_len, &found)) {
    app->item_is_new = false;
    app->current_item = found;
    view_dispatcher_send_custom_event(app->view_dispatcher,
                                      NfcStockEventScanKnown);
  } else {
    memset(&app->current_item, 0, sizeof(StockItem));
    memcpy(app->current_item.uid, uid, uid_len);
    app->current_item.uid_len = uid_len;
    stock_item_init(&app->current_item, "NEW", 0, 0, "Unassigned");
    app->item_is_new = true;
    view_dispatcher_send_custom_event(app->view_dispatcher,
                                      NfcStockEventScanUnknown);
  }

  return NfcCommandStop;
}

void nfc_stock_scene_scan_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  g_scan.app = app;
  g_scan.nfc = nfc_alloc();
  g_scan.poller = nfc_poller_alloc(g_scan.nfc, NfcProtocolIso14443_3a);
  nfc_poller_start(g_scan.poller, nfc_stock_scene_scan_callback, &g_scan);

  widget_reset(app->ui->widget);
  widget_add_string_element(app->ui->widget, 0, 10, AlignLeft, AlignTop,
                            FontPrimary, "Scanning...");
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

bool nfc_stock_scene_scan_on_event(void *context, SceneManagerEvent event) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (event.type == SceneManagerEventTypeCustom) {
    if (event.event == NfcStockEventScanKnown) {
      scene_manager_next_scene(app->scene_manager, NfcStockSceneDetails);
      return true;
    }
    if (event.event == NfcStockEventScanUnknown) {
      scene_manager_next_scene(app->scene_manager, NfcStockSceneEdit);
      return true;
    }
  }
  return false;
}

void nfc_stock_scene_scan_on_exit(void *context) {
  UNUSED(context);
  if (g_scan.poller) {
    nfc_poller_stop(g_scan.poller);
    nfc_poller_free(g_scan.poller);
    g_scan.poller = NULL;
  }
  if (g_scan.nfc) {
    nfc_free(g_scan.nfc);
    g_scan.nfc = NULL;
  }
}
/* ----- Details ----- */

static bool nfc_stock_details_input_callback(InputEvent *event, void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (event->type == InputTypeShort) {
    if (event->key == InputKeyUp) {
      view_dispatcher_send_custom_event(app->view_dispatcher,
                                        NfcStockEventDetailsStockUp);
      return true;
    }
    if (event->key == InputKeyDown) {
      view_dispatcher_send_custom_event(app->view_dispatcher,
                                        NfcStockEventDetailsStockDown);
      return true;
    }
    if (event->key == InputKeyOk) {
      view_dispatcher_send_custom_event(app->view_dispatcher,
                                        NfcStockEventDetailsOpenEdit);
      return true;
    }
  }
  return false;
}

void nfc_stock_scene_details_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_ui_show_item(app->ui, &app->current_item);
  View *view = widget_get_view(app->ui->widget);
  view_set_context(view, app);
  view_set_input_callback(view, nfc_stock_details_input_callback);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

bool nfc_stock_scene_details_on_event(void *context, SceneManagerEvent event) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (event.type == SceneManagerEventTypeCustom) {
    if (event.event == NfcStockEventDetailsStockUp) {
      stock_item_update_quantity(&app->current_item, 1);
    } else if (event.event == NfcStockEventDetailsStockDown) {
      stock_item_update_quantity(&app->current_item, -1);
    } else if (event.event == NfcStockEventDetailsOpenEdit) {
      scene_manager_next_scene(app->scene_manager, NfcStockSceneEdit);
      return true;
    } else {
      return false;
    }
    nfc_stock_ui_show_item(app->ui, &app->current_item);
    return true;
  }
  return false;
}

void nfc_stock_scene_details_on_exit(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  View *view = widget_get_view(app->ui->widget);
  view_set_input_callback(view, NULL);
  view_set_context(view, NULL);
  stock_db_upsert(app->active_db_path, &app->current_item);
}
/* ----- Edit ----- */

typedef enum {
  EditFieldName = 0,
  EditFieldQty,
  EditFieldMin,
  EditFieldLoc,
} EditFieldIndex;

static char s_text_buf[64];
static uint32_t s_edit_field_index;

static void nfc_stock_edit_refresh_list(NfcStockApp *app);

static void nfc_stock_edit_after_text_input(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  app->edit_text_input_blocking_back = false;
  StockItem *it = &app->current_item;

  switch ((EditFieldIndex)s_edit_field_index) {
  case EditFieldName:
    strncpy(it->name, s_text_buf, MAX_ITEM_NAME - 1);
    it->name[MAX_ITEM_NAME - 1] = '\0';
    break;
  case EditFieldQty: {
    char *end = NULL;
    long v = strtol(s_text_buf, &end, 10);
    if (end != s_text_buf && v >= 0 && v <= (long)INT32_MAX) {
      it->quantity = (int32_t)v;
    }
    break;
  }
  case EditFieldMin: {
    char *end = NULL;
    long v = strtol(s_text_buf, &end, 10);
    if (end != s_text_buf && v >= 0 && v <= (long)INT32_MAX) {
      it->min_quantity = (int32_t)v;
    }
    break;
  }
  case EditFieldLoc:
    strncpy(it->location, s_text_buf, MAX_LOCATION - 1);
    it->location[MAX_LOCATION - 1] = '\0';
    break;
  default:
    break;
  }
  it->last_updated = app_now_timestamp();
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewEdit);
  nfc_stock_edit_refresh_list(app);
}

static void nfc_stock_edit_enter_cb(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;
  s_edit_field_index = index;
  const StockItem *it = &app->current_item;

  text_input_reset(app->ui->text_input);
  memset(s_text_buf, 0, sizeof(s_text_buf));

  switch ((EditFieldIndex)index) {
  case EditFieldName:
    text_input_set_header_text(app->ui->text_input, "Name");
    strncpy(s_text_buf, it->name, sizeof(s_text_buf) - 1);
    break;
  case EditFieldQty:
    text_input_set_header_text(app->ui->text_input, "Stock (digits)");
    snprintf(s_text_buf, sizeof(s_text_buf), "%" PRId32, it->quantity);
    break;
  case EditFieldMin:
    text_input_set_header_text(app->ui->text_input, "Min stock (digits)");
    snprintf(s_text_buf, sizeof(s_text_buf), "%" PRId32, it->min_quantity);
    break;
  case EditFieldLoc:
    text_input_set_header_text(app->ui->text_input, "Location");
    strncpy(s_text_buf, it->location, sizeof(s_text_buf) - 1);
    break;
  default:
    return;
  }

  text_input_set_result_callback(app->ui->text_input,
                                 nfc_stock_edit_after_text_input, app,
                                 s_text_buf, sizeof(s_text_buf), false);
  app->edit_text_input_blocking_back = true;
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewTextInput);
}

static void nfc_stock_edit_refresh_list(NfcStockApp *app) {
  variable_item_list_reset(app->ui->edit_list);
  const StockItem *it = &app->current_item;
  char line[48];

  snprintf(line, sizeof(line), "Name: %s", it->name);
  variable_item_list_add(app->ui->edit_list, line, 0, NULL, NULL);
  snprintf(line, sizeof(line), "Stock: %" PRId32, it->quantity);
  variable_item_list_add(app->ui->edit_list, line, 0, NULL, NULL);
  snprintf(line, sizeof(line), "Min stock: %" PRId32, it->min_quantity);
  variable_item_list_add(app->ui->edit_list, line, 0, NULL, NULL);
  snprintf(line, sizeof(line), "Location: %s", it->location);
  variable_item_list_add(app->ui->edit_list, line, 0, NULL, NULL);

  variable_item_list_set_enter_callback(app->ui->edit_list,
                                        nfc_stock_edit_enter_cb, app);
}

void nfc_stock_scene_edit_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_edit_refresh_list(app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewEdit);
}

bool nfc_stock_scene_edit_on_event(void *context, SceneManagerEvent event) {
  UNUSED(context);
  UNUSED(event);
  return false;
}

void nfc_stock_scene_edit_on_exit(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (app->item_is_new) {
    if (strlen(app->current_item.name) == 0 ||
        strcmp(app->current_item.name, "NEW") == 0) {
      snprintf(app->current_item.name, MAX_ITEM_NAME, "ITEM_%04lX",
               (unsigned long)(furi_hal_random_get() & 0xFFFF));
    }
    app->item_is_new = false;
  }
  stock_db_upsert(app->active_db_path, &app->current_item);
}

bool nfc_stock_scene_edit_consume_nav_back(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (!app->edit_text_input_blocking_back) {
    return false;
  }
  app->edit_text_input_blocking_back = false;
  text_input_reset(app->ui->text_input);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewEdit);
  return true;
}
/* ----- Inventory ----- */

typedef enum {
  InvRow_Edit,
  InvRow_Delete,
} InventoryRowAction;

static uint32_t selected_index = 0;

static void nfc_stock_inventory_callback(void *context, uint32_t index);

static void nfc_stock_inventory_show_empty(NfcStockApp *app) {
  variable_item_list_reset(app->ui->inventory_list);
  widget_reset(app->ui->widget);
  widget_add_string_element(app->ui->widget, 0, 10, AlignLeft, AlignTop,
                            FontPrimary, "No items in inventory");
  widget_add_string_element(app->ui->widget, 0, 26, AlignLeft, AlignTop,
                            FontSecondary, "Scan a tag or add from NFC");
  widget_add_string_element(app->ui->widget, 0, 42, AlignLeft, AlignTop,
                            FontSecondary, "Back: return");
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

static void nfc_stock_inventory_refresh(NfcStockApp *app) {
  variable_item_list_reset(app->ui->inventory_list);

  StockItem *items = NULL;
  size_t count = 0;
  if (!fs_read_all_stock_items(app->active_db_path, &items, &count) ||
      count == 0) {
    if (items) {
      free(items);
    }
    nfc_stock_inventory_show_empty(app);
    return;
  }

  for (size_t i = 0; i < count; i++) {
    char buffer[64];
    bool alert = (items[i].quantity <= items[i].min_quantity);
    snprintf(buffer, sizeof(buffer), "%s (%" PRId32 ")%s", items[i].name,
             items[i].quantity, alert ? " !" : "");
    variable_item_list_add(app->ui->inventory_list, buffer, 0, NULL, NULL);
  }
  free(items);

  variable_item_list_set_enter_callback(app->ui->inventory_list,
                                        nfc_stock_inventory_callback, app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewInventory);
}

static void nfc_stock_inventory_action_callback(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;

  if (index == InvRow_Edit) {
    if (fs_read_stock_at(app->active_db_path, selected_index,
                         &app->current_item)) {
      app->item_is_new = false;
      scene_manager_next_scene(app->scene_manager, NfcStockSceneEdit);
    }
  } else if (index == InvRow_Delete) {
    fs_delete_stock_at_index(app->active_db_path, selected_index);
    nfc_stock_inventory_refresh(app);
  }
}

static void nfc_stock_inventory_callback(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;
  selected_index = index;

  submenu_reset(app->ui->submenu);
  submenu_add_item(app->ui->submenu, "Edit", InvRow_Edit,
                   nfc_stock_inventory_action_callback, app);
  submenu_add_item(app->ui->submenu, "Delete", InvRow_Delete,
                   nfc_stock_inventory_action_callback, app);

  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

void nfc_stock_scene_inventory_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_inventory_refresh(app);
}
/* ----- Settings ----- */

typedef enum {
  Settings_ExportCSV,
  Settings_SelectDB,
  Settings_ResetStock,
  Settings_ClearAll,
  Settings_Credits,
} SettingsMenuAction;

static void nfc_stock_ui_export_csv_execute(NfcStockApp *app) {
  char csv_path[sizeof(app->active_db_path) + 8];
  snprintf(csv_path, sizeof(csv_path), "%s.csv", app->active_db_path);

  StockItem *items = NULL;
  size_t count = 0;
  if (!fs_read_all_stock_items(app->active_db_path, &items, &count)) {
    return;
  }

  stock_export_csv(csv_path, items, count);
  free(items);

  widget_reset(app->ui->widget);
  widget_add_string_element(app->ui->widget, 0, 10, AlignLeft, AlignTop,
                            FontPrimary, "Exported to:");
  widget_add_string_element(app->ui->widget, 0, 25, AlignLeft, AlignTop,
                            FontPrimary, csv_path);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

static void nfc_stock_ui_settings_callback(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;

  switch (index) {
  case Settings_ExportCSV:
    nfc_stock_ui_export_csv_execute(app);
    break;
  case Settings_SelectDB:
    break;
  case Settings_ResetStock: {
    StockItem *items = NULL;
    size_t count = 0;
    if (!fs_read_all_stock_items(app->active_db_path, &items, &count)) {
      break;
    }
    for (size_t i = 0; i < count; i++) {
      items[i].quantity = 0;
    }
    fs_write_replace(app->active_db_path, items, count * sizeof(StockItem));
    free(items);
    break;
  }
  case Settings_ClearAll:
    fs_write_replace(app->active_db_path, "", 0);
    break;
  case Settings_Credits: {
    char version_line[48];
    snprintf(version_line, sizeof(version_line), "v%s", APP_VERSION);
    widget_reset(app->ui->widget);
    widget_add_string_element(app->ui->widget, 0, 5, AlignLeft, AlignTop,
                              FontPrimary, "NFC Stock Manager");
    widget_add_string_element(app->ui->widget, 0, 20, AlignLeft, AlignTop,
                              FontSecondary, version_line);
    widget_add_string_element(app->ui->widget, 0, 35, AlignLeft, AlignTop,
                              FontSecondary, "by Endika");
    widget_add_string_element(app->ui->widget, 0, 50, AlignLeft, AlignTop,
                              FontSecondary, "https://github.com/endika");
    widget_add_string_element(app->ui->widget, 0, 50, AlignLeft, AlignTop,
                              FontSecondary, "/flipper-nfc-stock");
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
    break;
  }
  }
}

void nfc_stock_ui_configure_settings(NfcStockUi *ui, NfcStockApp *app) {
  submenu_reset(ui->submenu);
  submenu_add_item(ui->submenu, "Select warehouse", Settings_SelectDB,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Export CSV", Settings_ExportCSV,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Reset stock to 0", Settings_ResetStock,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Clear all inventory", Settings_ClearAll,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Credits", Settings_Credits,
                   nfc_stock_ui_settings_callback, app);
}