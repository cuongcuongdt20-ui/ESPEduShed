# TV2 code handoff

This package is based on the supplied `ESPEduShed-main` project. TV2 files changed in this pass:

- `source/client/src/catalog/routes.tsx`: catalog UI styled around the reference screenshots; tabs for schools/campuses, per-school periods, holidays, subjects, classes, and travel times.
- `source/client/electron/discovery.ts`: UDP broadcast discovery with network-interface broadcast addresses, timeout, response validation, and deduplication by IP.
- `source/server/components/discovery/src/udp_discovery.c`: UDP discovery task on port 5001, exact request matching, unicast reply and 10 replies/sec cap.
- `source/server/components/discovery/src/mdns_svc.c`: mDNS hostname/service registration.
- `source/server/components/catalog/src/catalog_core.c`: reject a period range crossing missing period numbers.
- `docs/protocol.md`: expanded TV2 protocol table.

## Status after integration

The starter stubs mentioned in the original handoff have since been replaced: `catalog_cmd.c` implements all 25 catalog commands and `catalog_store.c` implements JSON loading/saving with the same schema used by the UI. `make catalog_cmd` covers the handlers on the host (`178/178` checks); `make catalog` still covers the date/period helpers. The code has not been flashed to a physical ESP32-S3, so hardware-level checks remain pending.

The public `catalog.h` function signatures were not changed. Only TV2-owned files and the TV2 section of protocol documentation were edited.
