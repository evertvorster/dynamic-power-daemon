# 🔁 Development Workflow Guide for `dynamic_power`

This document defines the **structured workflow** used in the development of the `dynamic_power` project, and maps each phase to the most appropriate GPT engine.

---

## 📚 Workflow Phases

### 1. 🧠 Investigative Conceptualization
> *Explore possibilities and generate creative ideas.*

- Brainstorm architectures, system interactions, or user workflows.
- Compare tradeoffs across implementation strategies.
- Ask “what if…” and test feasibility of concepts.

💡 Use **GPT-4o** — fast, idea-rich, great for open-ended exploration.

---

### 2. 📐 Design
> *Formalize selected ideas into structured plans and rules.*

- Write specifications, requirement docs, and interface designs.
- Define file structure, responsibilities, and validation rules.
- Capture assumptions and constraints clearly.

💡 Use **GPT-4 (o3)** — excels at rule-following and deep structure.

---

### 3. 🛠 Implementation
> *Turn designs into functioning code.*

- Add new features with minimal changes to existing systems.
- Maintain project conventions and safeguard working modules.
- All edits must follow `CARDINAL_DEVELOPMENT_RULES.md`.

💡 Use **GPT-4 (o3)** — safest for scoped, rule-bound editing.

---

### 4. 🐛 Debugging & Testing
> *Fix broken behavior, review logs, iterate solutions quickly.*

- Catch errors from logs or stack traces.
- Rapidly test changes with tight feedback loops.
- Revert or adapt behavior dynamically.

💡 Use **GPT-4o** — great at parsing logs and accelerating iteration.

---

## 🎯 Engine-Phase Summary

| Phase                         | Best Engine | Reasoning                              |
|-------------------------------|-------------|----------------------------------------|
| Investigative Conceptualization | GPT-4o     | Fast idea generation and comparison    |
| Design                          | GPT-4 (o3) | Careful reasoning, rule-building       |
| Implementation                  | GPT-4 (o3) | Safety, accuracy, and minimal edits    |
| Debugging & Testing             | GPT-4o     | Responsiveness, log analysis           |

---

For long-term maintainability, this guide ensures that each phase uses the most effective tools available — with GPT engines treated as collaborators under clear role definitions.

