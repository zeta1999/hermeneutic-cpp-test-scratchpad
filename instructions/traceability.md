# Requirement Traceability Matrix

| Requirement (source) | Implementation references | Validation / evidence | Status |
| --- | --- | --- | --- |
| Aggregator ingests 3 CEX feeds and exposes a consolidated gRPC stream (PDF) | `services/aggregator_service/main.cpp`, `src/aggregator/*`, `proto/aggregator.proto`, `config/aggregator*.json` | `tests/aggregator/test_aggregator.cpp`, `tests/aggregator/test_aggregator_service_exec.cpp`, README build/run steps | ✅ Implemented |
| Client variations publish to stdout: BBO | `services/bbo_service/main.cpp`, `src/bbo/bbo_publisher.*` | `tests/services/test_publishers.cpp` (BBO format assertion), README “Client services” section | ✅ Implemented/logged |
| Client variations publish to stdout: Volume Bands | `services/volume_bands_service/main.cpp`, `src/volume_bands/volume_bands_publisher.*` | `tests/services/test_publishers.cpp` (volume formatter), docker demo (stdout + CSV) | ✅ Implemented/logged |
| Client variations publish to stdout: Price Bands | `services/price_bands_service/main.cpp`, `src/price_bands/price_bands_publisher.*` | `tests/services/test_publishers.cpp` (price formatter) | ✅ Implemented/logged |
| Deliverables: code repo, Dockerfiles per service, compose topology (PDF) | `docker/Dockerfile.*`, `docker/compose.yml`, `scripts/docker_build.sh`, `scripts/docker_run.sh` | README “Docker + demo stack” instructions | ✅ |
| README describes build/run decisions (PDF) | `README.md` top-level sections | Manual review | ✅ |
| Demo data rich enough for showcase (polish.md) | `data/*.ndjson` generated via `scripts/generate_demo_data.py` | README “Demo data” instructions; script usage | ✅ |
| Ability to refresh demo data | `scripts/generate_demo_data.py`, README instructions | Run script with new seeds; documented commands | ✅ |
| Capture CSV outputs off containers (polish.md) | `docker/compose.yml` host volume mounts (`../output/...`), `scripts/docker_run.sh` | README “Docker + demo stack” and “Collecting CSV output” sections | ✅ |
| Outstanding polish backlog (xxx.txt) | `instructions/todo.txt` checklist | Pending | ⚠️ (tracked) |

*Sources: Senior_C___Engineer_Test_Assignment.pdf, instructions/polish.md, xxx.txt.*
