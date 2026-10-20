Sanitized contract fixtures derived from these workspace sources:

- `nexus-n3-core/nexus_n3/core/core.py`: `initialize_system` completed result,
  sensor connection callback and `_on_algorithm_result`.
- `nexus-n3-core/nexus_n3/distributed/master_node.py`: `_finish_initialization`.
- `nexus-n3-core/nexus_n3/core/orchestrators/event_assembler.py`:
  `build_compute_payload`.
- `nexus-n3-plugin-catalog/algorithms/nexus-n3-algorithm-standard-loading-intensity/src/standard_loading_intensity/core_schema.py`
  and `core.py`: real-time bands and `axis_values.mag`.
- NEIA `neia-api/app/gateway/{manager,zeromq_client}.py` forwards these envelopes
  unchanged; `app/api/gateway.py` sends an unrelated initial `voice_status`.

Identifiers/context are invented test values. Band order is deliberately
shuffled to prove lookup by name. These are contract-derived fixtures, not a
capture from running hardware.
