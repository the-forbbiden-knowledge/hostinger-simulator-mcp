# Hostinger Simulator — Windows Native Core + Elixir/OTP

> **Status:** technical bootstrap / local-first simulator. This repository is an independent prototype. It is **not** a live infrastructure controller, not an official provider implementation, and not yet a complete MCP server.

## Objective

Build a deterministic, secure, testable infrastructure and collaboration simulator for Windows 10/11 and Windows Server (target platforms; native Windows validation pending), with a C++ native engine compiled by CMake, an Elixir/OTP orchestration layer, and a future Phoenix/LiveView interface and MCP JSON-RPC adapter.

All operations are local mock previews. **No cloud accounts, DNS zones, virtual machines, mailboxes, or billing records are modified.** Real-provider integration is out of scope until explicitly authorized, designed, and separately tested.

## Technology baseline

- **Windows x64** development; Visual Studio 2022 C++ workload, CMake 3.24+, optional Ninja; PowerShell 7 recommended.
- **Native engine:** C++20 with RAII, strict input allowlists, bounded arguments, deterministic responses, thread-safe design principles, and explicit error handling.
- **Orchestration:** Elixir/Mix and Erlang/OTP; external executable isolated via Erlang Port. Native NIFs are not enabled by default.
- **UI (planned):** Phoenix + LiveView, not generated or installed in this bootstrap.
- **Protocol (planned):** Model Context Protocol over JSON-RPC 2.0 with tool discovery and validated tool invocations. The current CLI is *not* MCP wire-compatible.
- **Testing:** CTest + ExUnit; dependency scanning, fuzzing, static analysis, reproducibility tests and observability are delivery gates.

## Repository layout

```text
project/
  CMakeLists.txt               C++ build and test targets
  CMakePresets.json           Windows configuration presets
  include/simulator/          Native public interfaces
  src/native/                 Native simulator and CLI
  tests/                      Native deterministic tests
  elixir/                     Minimal Mix application with Port wrapper
  .github/workflows/          Windows build/test CI
```

The private reference library lives outside this Git repository at `../library/`; it is not committed or published.

## Quickstart — Windows PowerShell

Open **Developer PowerShell for Visual Studio 2022** in this directory:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc --config Release
ctest --preset windows-msvc -C Release --output-on-failure
.\out\build\windows-msvc\Release\simulator.exe list
.\out\build\windows-msvc\Release\simulator.exe call websites.list demo
.\out\build\windows-msvc\Release\simulator.exe call monitoring.health demo
```

Alternative Ninja workflow with an active compiler environment:

```powershell
cmake --preset windows-ninja
cmake --build --preset windows-ninja
ctest --preset windows-ninja --output-on-failure
.\out\build\windows-ninja\simulator.exe list
```

## Capability inventory — planned vs implemented

| Domain | Current local sample | Planned extension |
|---|---|---|
| Websites | `websites.list`, `websites.deploy_preview` | deployment status, preview fixtures |
| VPS | `vps.list`, `vps.power_state` | lifecycle, metrics, failure injection |
| Domains / DNS | `domains.list`, `dns.records` | zones, records, TTL validation |
| Mail | `mail.domains`, `mailboxes.list`, `mailboxes.create_preview` | mailbox quotas, aliases, protocol emulation |
| Collaboration | calendar, contacts, tasks preview | scheduling, delegation, synchronization |
| Subscriptions | `subscriptions.list` | synthetic billing schedules |
| Commerce | `ecommerce.store_preview` | simulated catalog and checkout status |
| Support | `support.ticket_preview` | tickets, priorities, response measurements |
| Backup | `backup.restore_preview` | recovery exercises and restore points |
| Monitoring | `monitoring.health` | availability, synthetic incidents and alerts |

These are **project-defined sample capabilities**. No claim is made that they match every feature of any upstream repository or provider. Exact compatibility requires an upstream source audit and contract tests.

## Engineering rules

- Treat inputs as untrusted. Enforce explicit tool allowlists, length limits, schema validation, least privilege and fail-closed errors.
- Native code must avoid unchecked buffer operations, ownership ambiguity, path traversal, use-after-free, integer overflow and unsynchronized shared state.
- Default to out-of-process native workers, supervised by OTP; move only narrowly scoped, measured operations to NIFs after ABI and failure analysis.
- Build in isolated directories, use locked dependencies when introduced, keep secrets outside source control, and inspect dependency licenses.
- Test happy paths, forbidden tools, hostile inputs, timeout/crash recovery, platform behavior, determinism and eventual MCP conformance.
- Keep production-style logs structured and redact secrets; separate incident simulation from genuine operational assertions.
- Availability and incident-response commitments are **synthetic test requirements**, never customer guarantees.

Architecture, service scenarios, the engineering technique matrix and delivery gates are documented below in this README.

## Architecture — Windows / native process integration

### Trust boundaries

```
MCP client (future) --> JSON-RPC validation (future) --> Phoenix / API (future)
                                                         |
                                                         v
                                             Elixir/OTP (adapter present)
                                               |
                                               v
                                   Erlang Port / bounded worker
                                               |
                                               v
                                    C++20 CMake native simulator
                                               |
                                               v
                                   Immutable fixtures + mock state
```

The native CLI is the only component compiled and tested in the current bootstrap. The Elixir Port adapter exists in source but has not been run end-to-end on Windows. Phoenix and MCP transport are not implemented.

### Boundaries and safety

1. No provider API credentials or network mutations.
2. Tool names are allowlisted; unknown operations fail closed.
3. Resource identifiers are limited to 256 ASCII bytes from an explicit safe-character set; native tool names are allowlisted.
4. Responses have explicit `simulation_only` and `side_effects:false` markers.
5. OTP supervises any future long-lived worker. Use Ports first; NIF usage requires crash isolation strategy, ABI testing, scheduler-bound execution, and fuzzing.
6. Real MCP transport must implement JSON-RPC, initialization, `tools/list`, `tools/call`, version negotiation, lifecycle and protocol compliance tests before claiming support.
7. Future Phoenix integration requires authorization, CSRF/session defenses, rate-limiting and audit trails.

### Threat scenarios

- Input-induced crashes, memory faults, resource exhaustion and process hangs.
- Privilege confusion between mock and live environments.
- Unauthorized tool calls, spoofed sessions and leaked authorization context.
- Inconsistent fixture state and nondeterministic replay.
- Invalid deserialization and untrusted protocol messages.
- Accidental network effects, DNS changes or mail delivery from simulations.

### Implementation stages

- Gate A: deterministic C++ CLI, tests and clean Windows build.
- Gate B: real Elixir Port integration with supervision and restart tests.
- Gate C: strictly local MCP JSON-RPC tool adapter and protocol conformance suite.
- Gate D: Phoenix LiveView admin console and scenario editor.
- Gate E: service scenarios, Zimbra collaboration fixtures, trace metrics and security gates.


## Simulator service and collaboration scope

**All services below are hypothetical local fixtures; no production services are enabled.**

| Area | Planned mock workflow | Validation |
|---|---|---|
| Websites | list, preview deployment, status | no external deployment |
| VPS | list, start/stop preview, snapshot | no hypervisor access |
| Domains/DNS | list zones, preview records, TTL checks | no DNS mutation |
| Mail | domains, mailboxes, aliases, quotas, policies | no email transmission |
| Collaboration | calendar, contacts, tasks, documents, sharing | synthetic identity boundaries |
| Support | incident ticket, priority, triage, SLA clock | traceability and timestamps |
| Continuity | incremental/full backup fixture, restore preview | verify recovery scope |
| Security | roles, alerts, audit, antispam/antimalware events | test auth boundaries |
| Billing | subscription state and synthetic charges | no financial events |
| Ecommerce | catalog and checkout preview | no real payments |

### Historical service-model inspiration (not contractual promises)

The supplied examples distinguish **hosted** from **fully managed** administration, support windows of **8x5** with optional **24x7**, response priorities from emergency to scheduled, maintenance notification, recovery windows, and differential backup/restore capabilities. One example proposes a **100%** network-availability clause with exclusions while another uses **99.9%** server availability: these are *different scopes and historical sample claims*, not a single project SLO.

Proposed synthetic scenarios include `scheduled_maintenance`, `urgent_ticket`, `mailbox_restore`, `spam_filter`, `sync_failure`, `backup_validation`, `health_degradation`, `permissions_denied`, and `domain_propagation`.


## Engineering technique matrix

| Discipline | Technique | Evidence / gate | Current status |
|---|---|---|---|
| Native correctness | RAII, ownership clarity, const-correctness, explicit validation | review + tests | established guideline |
| Memory safety | ASan/UBSan in supported Clang builds, MSVC /sdl, static analysis | sanitizer CI | planned |
| Build engineering | CMake presets, out-of-tree build, deterministic flags | CTest matrix | implemented baseline |
| Supply chain | pinned dependencies, license checks, SBOM, secret scanning | CI attestation | planned |
| Native boundaries | Erlang Port first, bounded data, timeout, separate process | crash/timeout tests | basic adapter |
| RPC | JSON-RPC 2.0 request schemas, contract tests, capability negotiation | MCP conformance | planned |
| Secure API | strict allowlists, authentication, least privilege, rate limits | negative tests | partial, no network |
| Testing | unit, integration, property-based, fuzzing, fault injection | CTest / ExUnit | CTest present |
| Observability | structured logs, trace IDs, event correlation, health probes | testable dashboards | planned |
| Resilience | supervised restart, idempotency, backpressure, jittered retry | fault-injection suite | planned |
| Evidence | provenance, SHA-256 for private reference files, verified test outputs | manifest and logs | implemented locally |
| Delivery | requirements, risk review, acceptance gates, tracked metrics | release checklist | planned |
| Data hygiene | redaction, retention controls, immutable synthetic fixtures | static checks | guideline |
| Simulation | deterministic fixtures, no external effects, explicit mock markers | side-effect assertions | implemented baseline |

### Metrics

- Tool allowlist coverage and successful negative tests.
- API contract pass rate and simulation determinism rate.
- Native crash count under fuzzing; memory sanitizer findings.
- Median and p95 latency of isolated native calls.
- OTP worker recovery time after induced failure.
- Windows build/test success rate and dependency vulnerabilities.
- Availability simulation error against synthetic target SLO.

All performance and reliability claims must come from measured executions, not assumptions.


## Delivery criteria and release gates

1. Windows x64 Visual Studio 2022/CMake configure, build, CTest green.
2. Inputs/outputs match documented deterministic fixtures across 100 consecutive runs.
3. Native unknown-tool, malformed-input and oversized-input tests fail closed.
4. No network effects, secrets or customer information in committed artifacts.
5. Elixir Port adapter tested end-to-end on Windows and worker timeout/crash isolation verified.
6. RPC server validated against the protocol specification before describing it as MCP-compatible.
7. Phoenix dashboard authenticated, authorized and covered by request/LiveView tests before deployment.
8. Static analysis, SCA, vulnerability handling, compiler hardening, fuzzing and SBOM automated.
9. Observability instrumentation captures p50/p95 latency, error rate and worker restarts.
10. Private reference library remains outside Git; SHA-256 checks performed at intake.

### Project status as of bootstrap

- Gate 1: verification pending on a real Windows host; can be smoke-tested with portable CMake.
- Gate 2: not run at scale.
- Gate 3: native runtime checks and negative tests included; complete security coverage not claimed.
- Gate 4: repository content screened for private reference files; this is not a formal secret-scanning attestation.
- Gates 5-9: future work.
- Gate 10: private manifests generated in companion library.


## Git policy: consistent commit messages

This project permits multiple commits. Use the exact commit message `Initial commit` for every new commit.

```powershell
git add .
git commit -m "Initial commit"
git push origin main
```

Use normal fast-forward pushes by default. Do not rewrite remote history or use `--force` to enforce a single-commit history. If branch protection or remote changes reject a push, inspect and resolve the conflict rather than bypassing protections. Keep the working tree clean after each completed change.

## Source licensing and privacy

Do not commit proprietary reference PDFs, customer documents, private catalogs, authorization tokens, credentials, or third-party source archives. The private library is organized alongside the repository and is intentionally excluded from Git.
