# DBus Interface Reference

## Object Path
`/com/evertvorster/dynamic_power`

## Interface Name
`com.evertvorster.dynamic_power`

## Methods

| Method                   | Parameters                              | Called By                   | Description                                      |
|--------------------------|------------------------------------------|-----------------------------|--------------------------------------------------|
| `SetProfile`             | `string profile, boolean is_user`       | `dynamic_power_user`, GUI   | Sets active profile. is_user flag bypasses battery guardrails. |
| `SetLoadThresholds`      | `double low, double high`               | `dynamic_power_user`        | Sets new dynamic load thresholds.                |
| `PowerStateChanged`      | `string state`                          | Internal (root daemon)      | Emits signal when power source changes.          |
| `GetCurrentProfile`      | `→ string`                              | Planned (GUI)               | Query the current power profile.                 |
