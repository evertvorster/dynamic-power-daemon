# 🛑 Cardinal Development Rules for `dynamic_power`

These rules **must always be followed** by GPT-4/o3 during development of the `dynamic_power` codebase.

## 🔒 1. Minimal Impact Policy

- **Never modify existing working code** unless:
  - It is directly relevant to the feature being implemented.
  - It has a verified bug.
- All changes must be **additive**, leaving unrelated behavior untouched.

## 🧪 2. Patch Safety Requirements

Every patch **must**:
- Pass `python -m py_compile` on every modified Python file.
- Introduce a meaningful change visible via `git diff`.
- Be tested across the full application lifecycle (startup → idle → shutdown).
- Fail gracefully if optional hardware or dependencies are missing.

## 🛠 3. File Integrity Standards

- **Do not add code before the shebang line** (`#!/usr/bin/env python3`).
- Avoid introducing global state unless explicitly required.
- Use `global` only where absolutely necessary, and **never duplicate it**.

## ✅ 4. Verification Checklist (GPT Must Run This Before Patch)

Before preparing a downloadable patch:
- [ ] Was `python -m py_compile` run on every changed file?
- [ ] Does `git diff` show a minimal and relevant change?
- [ ] Is the change fully scoped to the current feature?
- [ ] Did you verify the application starts, runs, and shuts down normally?
- [ ] Was all debug/test code removed before export?

---

These rules exist to protect the reliability and maintainability of this complex, multi-process system.

Any deviation must be discussed and justified **before** implementation.
