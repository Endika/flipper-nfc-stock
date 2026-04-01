# NFC Stock Manager for Flipper Zero

NFC-based inventory app for Flipper Zero.  
Scan a tag, load or create an item, update stock, and persist data in a local database file on the SD card.

## Current Status

- Alpha project (`v0.1.0` in `version.h`).
- Scan-first UX:
  - Known tag -> details view (UP/DOWN stock, OK to edit).
  - Unknown tag -> edit view to create the item.
- Single DB path is configurable in Settings.

## Features

- NFC UID lookup in local stock database.
- Create/update items with:
  - Name
  - Quantity
  - Minimum stock
  - Location
- Inventory list with edit/delete.
- CSV export support.
- Safe persistence behavior (`upsert` for existing items, no full DB overwrite on single-item save).

## Build and Development

### Requirements

- `gcc`
- `make`
- `clang-format`
- `cppcheck`
- Local Flipper firmware checkout for `.fap` builds

### Common Commands

- `make test` - Run host unit tests.
- `make linter` - Run static analysis (`cppcheck`).
- `make format` - Format C/H files with `clang-format`.
- `make prepare` - Symlink project into firmware `applications_user`.
- `make fap` - Build `.fap` with `fbt`.
- `make clean` - Remove local host build artifacts.

## Build for Flipper Zero

1. Clone [flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware).
2. Set `FLIPPER_FIRMWARE_PATH` in `Makefile` if needed.
3. Run:

```bash
make fap
```

The resulting app is built in the firmware output directory.

## Project Layout

Layered structure optimized for maintainability and constrained devices:

- `main.c` - Composition root (scene manager + dispatcher wiring).
- `src/scenes.c` - Scene flow and event handling (single translation unit).
- `src/ui/ui.c` - Reusable UI setup helpers.
- `src/domain/stock.c` - Domain rules and stock mutations.
- `src/persistence/` - DB/file persistence and export.
- `src/platform/storage_helper.c` - Active DB storage path management.
- `include/` - Public headers.
- `tests/test_stock.c` - Host-side unit tests.
- `docs/ARCHITECTURE.md` - Layer and responsibility overview.

## Versioning and Release

- `version.h` (`APP_VERSION`) is the canonical app version string used in UI and release automation.
- `application.fam` (`fap_version`) is Flipper package metadata.
- Release automation uses `release-please` via GitHub Actions.

## Credits

- Author: Endika
- Repository: [github.com/endika/flipper-nfc-stock](https://github.com/endika/flipper-nfc-stock)
