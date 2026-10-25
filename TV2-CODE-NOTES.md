# TV2 code handoff

This package is based on the supplied `ESPEduShed-main` project. TV2 files changed in this pass:

- `source/client/src/catalog/routes.tsx`: catalog UI styled around the reference screenshots; tabs for schools/campuses, per-school periods, holidays, subjects, classes, and travel times.
- `source/client/electron/discovery.ts`: UDP broadcast discovery with network-interface broadcast addresses, timeout, response validation, and deduplication by IP.
- `source/server/components/discovery/src/udp_discovery.c`: UDP discovery task on port 5001, exact request matching, unicast reply and 10 replies/sec cap.
- `source/server/components/discovery/src/mdns_svc.c`: mDNS hostname/service registration.
- `source/server/components/catalog/src/catalog_core.c`: reject a period range crossing missing period numbers.
- `docs/protocol.md`: expanded TV2 protocol table.

## Important integration status

This is a **working code handoff, not yet a verified full TV2 release**. The existing `catalog_cmd.c` and `catalog_store.c` still contain the starter stubs for CRUD and persistence; the UI already presents those workflows and expects the command names described in `docs/protocol.md`, so those screens cannot all function end-to-end until the handlers and storage serialization are implemented. Build/typecheck and ESP-IDF hardware tests were not run in this environment. Do not flash to a production device without completing these checks.

The public `catalog.h` function signatures were not changed. Only TV2-owned files and the TV2 section of protocol documentation were edited.
