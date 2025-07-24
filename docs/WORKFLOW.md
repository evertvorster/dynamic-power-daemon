# Contributor Workflow for *dynamic‑power‑daemon*

**Last updated:** 2025‑07‑23

This project uses a chat‑assisted, copy‑paste‑friendly development loop that keeps friction low while still giving us full control of the codebase.

---

## 1 · Roles

| Actor | Responsibility |
|-------|----------------|
| **ChatGPT (o3)** | Drafts complete files or patches on request, explains concepts, helps debug. |
| **Maintainer (you)** | Copies proposed file contents into the local checkout, tests, commits, and pushes upstream. |

---

## 2 · End‑to‑End Flow

1. **Feature discussion.** We brainstorm options and pick a design.<br>`→` *Markdown chat only (no code yet).*
2. **File delivery.** ChatGPT provides the **entire content** of every file that changes, as downloadable attachments.<br>`→` *You copy‑paste into VS Code.*  
   *Rationale: makes diffs obvious and avoids fuzzy patches.*
3. **Local test.** You install the modified package locally (`makepkg -si`, `pip install -e`, etc.) and run real‑world scenarios.
4. **Bugfix loop.** If errors occur, you share stack traces. ChatGPT supplies updated files (again full‑file).
5. **Upstream push.** Once satisfied, you commit locally and push to GitHub / AUR.

---

## 3 · Ground Rules

* **No partial snippets** – always full files so VS Code shows a clean diff.
* **No direct `git apply`** – copy‑paste keeps the repo history tidy and transparent.
* **One feature per session** whenever possible to keep review small.
* **Markdown conventions**  
  * `✅` done, `🟡` next, `🟠` later.  
  * Dates in **YYYY‑MM‑DD** ISO format.

---

## 4 · Versioning / Releases

* `major.minor.patch` mirrored into the AUR `pkgver`.
* Increment **`pkgrel`** only for packaging‑only tweaks (PKGBUILD changes).
* Schema upgrades bump `schema_version` in `config.py` and trigger an in‑place upgrader.

---

## 5 · Asking ChatGPT for help

* **“Give me the files for XYZ.”** → Expect a downloadable zip or individual file links.
* **“Why is this broken?”** → Paste the traceback and the current file content so context is clear.
* **“Let’s explore options.”** → We’ll chat ideas first, no code until a path is chosen.

Happy hacking! :rocket:
