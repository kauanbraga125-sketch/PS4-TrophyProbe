# PS4-TrophyProbe V0.1 — Test Plan

## Goal

Establish a clean baseline for the local trophy API before adding any unlock call.

## Expected sequence

The app reports one notification per step:

1. `USER_SERVICE_INIT`
2. `GET_INITIAL_USER`
3. `LOAD_NP_TROPHY`
4. `CREATE_CONTEXT`
5. `CREATE_HANDLE`
6. `REGISTER_CONTEXT`

For each step, record the hexadecimal return value exactly as shown.

## Test A — baseline

Run TrophyProbe normally on the target PS4 user.

Record:

```text
Firmware:
User:
USER_SERVICE_INIT:
GET_INITIAL_USER:
LOAD_NP_TROPHY:
CREATE_CONTEXT:
CREATE_HANDLE:
REGISTER_CONTEXT:
Final message:
```

If `/data/PS4-TrophyProbe.log` exists, preserve it as well.

## Interpretation

- Failure before `CREATE_CONTEXT`: environment/module/user-service problem.
- `CREATE_CONTEXT` succeeds but `REGISTER_CONTEXT` fails: trophy metadata/context binding is the first suspect.
- All six succeed: the base API path works and V0.2 can start probing title-specific requirements.

## V0.2

Only after the baseline is stable, add read-only inspection and controlled comparisons between the context available to an installed title and a title for which only trophy metadata is present.

No trophy unlock call belongs in V0.1.
