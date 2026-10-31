# Offline authorization response contract — reference implementation

This repository contains a **synthetic, offline simulator**. The Python reference consumer in `tools/authorization_consumer.py` is for detecting inconsistent response contracts across the existing **A2A, MCP, RAG and native-agent fixture routes**. It is *not* a production access-control mechanism, an identity provider, an authenticated MCP/A2A client, a service connector, or an approval to deploy.

## Interpretation

- For a declared **operation**, `ok:true` alone or a CLI exit status of `0` never establishes authorization. The consumer additionally requires a recognized offline envelope, `side_effects:false`, the appropriate explicit `network_access:false` assertion, and an expected allow/deny decision.
- If an access operation says `decision:"deny"` and `ok:true`, classify it as `REJECT_INCONSISTENT_DENIAL` — **not** a successful authorization.
- For a declared **scenario**, a matching scenario ID together with `ok:true` and a negative `decision` records that the *scenario simulation* ran; it does not authorize access.
- A native-agent tool reply additionally requires an explicit expected **tool name and resource identifier** matching the response and a caller-supplied tool allowlist. The fixture's native registry remains the final tool allowlist.
- RAG, MCP and A2A require an explicit `network_access:false` assertion. The historical native-agent route does not supply this field; the consumer rejects it if present with any value other than `false`. This exception is documented, not a general trust assumption.
- Responses are parsed as strict UTF-8 JSON, reject duplicate keys and nonfinite numbers, and are limited to 65,536 output bytes. The subprocess reader independently limits stdout and stderr and enforces an elapsed-time deadline.
- The executable must match a supplied local SHA-256 before invocation. This **only compares file bytes** and is not a signature, trusted build attestation, sandbox, memory quota, or race-free filesystem identity guarantee.

## Tests and baseline audit

Run the 34 fixture unit tests (no real services, tokens, accounts, or network endpoints):

```shell
python -m unittest discover -s tests -p 'test_offline_*.py' -v
```

To audit an **already-built native simulator executable**, supply a local path and its SHA-256:

```shell
python tools/contract_matrix_audit.py --binary PATH_TO_SIMULATOR --binary-sha256 YOUR_64_HEX_SHA256
python tools/matrix_consumer_audit.py --binary PATH_TO_SIMULATOR --binary-sha256 YOUR_64_HEX_SHA256 --report LOCAL_METADATA_ONLY.json
```

The second command intentionally uses **exit status 2** on a known inconsistent current baseline (`NO_GO`), and does not modify code or perform network requests. The baseline covers 232 fixture cases; previously observed producer-side gaps include A2A and agent denial responses with `ok:true`, and a positive RAG response lacking `mode:"simulation_only"`. These are *known incompatibilities*, not demonstrated production exploits. Any eventual producer change must separately pass compatibility review and the two-platform CI before changing the documented baseline.

The user-facing JSON of these audit tools contains only synthetic classifications and bounded counts, never response bodies or secrets. Do not commit audits of private material, original books, external customer data, or source archives.
