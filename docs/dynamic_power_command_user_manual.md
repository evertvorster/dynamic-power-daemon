# Dynamic Power — User Manual

> **Audience:** New users  
> **Applies to:** `dynamic_power_command` (Qt GUI) working with the `dynamic_power` daemon

---

## Overview

**Dynamic Power** helps your laptop pick the right power/performance behavior automatically and gives you a quick way to override it when you need to. 
The GUI shows your system load over time, the active power profile and provides simple editors for power‑saving rules that require root‑level sysfs writes.
There is a button for power saving features, and this is where you can set various features that are aimed at saving power when your laptop is unplugged from power. 

### What you can do

- **See current status** at a glance (active mode and profile). When the UI is closed, the panel icon will change to show you something significant has happened.
- **Override** the daemon’s behavior temporarily. This override ignores power supply concerns, so use sparingly when on battery.
- **Watch the system load** and threshold lines that guide automatic switching.
- **Manage root-required features** (e.g., sysfs knobs) with per‑power‑source values.

> The GUI talks to the root‑owned `dynamic_power` daemon over D‑Bus. Make sure the daemon is installed and running.

---

## Quick Start

1. **Install**
On Arch, use your favorite AUR helper and install dynamic-power-daemon.
```
sudo systemctl enable --now dynamic_power.service
```
2. **Launch the app**: run `dynamic_power_command` from your menu or terminal.
3. **Read the status**:
   - The top **User Override** button shows  
     `User Override: <Mode> - Current power profile: <Profile>`.
   - The graph titled **Load Average** shows recent system load and the **Low/High** threshold lines used for automatic decisions.
4. **(Optional) Set an override**: click the **User Override** button and choose a mode (e.g., *Dynamic*, *Powersave*, *Balanced*, *Performance*).  
   - When not **Dynamic**, the button highlights to remind you an override is active.
5. **(Optional) Edit root-required features**: open the **Root‑required features** editor to add or adjust entries (explained below).

---

## Main Window Guide

### 1) User Override button

- **Format:** `User Override: {Mode} - Current power profile: {Profile}`
  - **Mode** is what *you* requested (e.g., **Dynamic**, **Balanced**, **Performance**, **Powersave**).
  - **Current power profile** is the power mode of the system, in the Dynamic mode, these are not always the same.
- **Click** to open the menu and choose a mode.  
  - **Dynamic** hands control back to the daemon’s automatic behavior.  
  - Any non‑Dynamic choice applies an override until you switch back.

> Tip: If the profile displayed does not match your override immediately, give it a moment; some changes depend on current load or the daemon’s safety checks.

---

### 2) Load Average graph

- **Title:** centered at the top: **Load Average**.
- **Threshold lines:** two subtle horizontal lines (Low and High) show the boundaries the daemon uses to decide when to change profiles.
  - Line style may be solid or dotted depending on your version; opacity is tuned to be visible but not distracting.
- **Axes/labels:** Y‑axis labels are centered in the left margin; extra numeric overlays are hidden for clarity.

**Reading the graph**
- When the load is:
  - **Below Low** → the daemon tends toward power saving.
  - **Between Low and High** → balanced behavior.
  - **Above High** → performance is preferred.

> The exact policies are set by the daemon; the graph lets you understand *why* a change happened.

---

### 3) Process Matches

- The process match controlling the system behavior is highlighted. 
- More process matches not controlling the system are indicated.
- This section summarizes processes that match your configured rules (if any).  
- Some commonly used overrides are here as examples. Modify, add or delete as you see fit.
- Matched processes do not apply if the system is on battery, this is by design. 
- Use it to confirm that expected applications are being detected.
- What happens when there is more than one process match? 
  - Since there is only one system, only one proces can control system behavior.
  - Each process match has a priority slider, so you pick which one actually controls your system

---

### 4) Profile configuration editor

You can and must configure exactly what happens when a profile is set, especially on the first use of this utility.

**Features to be set**
- CPU governor to be used.
- ACPI system state.
- ASPM system state.
All of these have a disable option per profile, in which case the daemon won't try to set this feature for the profile, and leave it to the previous set setting.

What the profile does not set is EPP state. These change depending on what the other settings are set to, so if you have a utility like Asus' ROG Control Center, you can apply EPP modes to system profiles, and these will automatically be set when the profile changes.

### 5) Power features editor
Some power‑saving tweaks require writing to sysfs files (root-only). The **Root‑required features** editor lists these settings with values for **AC** and **Battery**.

**Layout - Root features**
- **Enabled** (checkbox): whether to apply this rule.
- **Root** (button): quickly choose from known root paths or confirm permissions (varies by setup).
- **Path**: the absolute sysfs file (e.g., `/sys/devices/.../power/control`).
- **AC value**: value written when on mains power.
- **Battery value**: value written when on battery.
- **Delete**: remove the row.

**How to add or change a rule**
1. Open the **Root‑required features** editor.
2. Click to add a new row (or edit an existing one).
3. Fill in the **Path**, **AC value**, and **Battery value**.
4. **Enable** the row to activate it.  
5. Save/close. The configuration is stored in your daemon config and applied by the system components that handle these features, independently of the user interface running.

> **Caution:** Writing the wrong value to sysfs can break devices or reduce stability. Only use documented paths/values from trusted sources.

What features to set? You need to deep-dive into what hardware your system has, and how to enable sleep states, turn things off and on. This is a very advanced feature, and you should probably leave it alone until/unless you have a very specific need to set something here. 
Other projects like Laptop Mode Tools and TLP set exactly the same features, and their documentation will give you clues as to what and how to set these features. 
User utilities like PowerTOP and others can help you identify hardware settings and locations.

Test the feature on YOUR system before putting it in here.

**Layout - User features**
Currently there are only two user features: 
- Screen refresh rates can dynamically adjust screen refresh rates. 
- Panel Autohide may be a weird one at first glance, but KDE polls the panel aggressively and wastes power on battery.

---

## Frequently Asked Questions

**Q: What’s the difference between *Mode* and *Current power profile* on the button?**  
**A:** *Mode* is your request (e.g., Dynamic/Performance). *Current power profile* is what **power‑profiles‑daemon** is set to right now (e.g., powersave / balanced / performance). The daemon maps your request to a profile based on policy and current conditions.

**Q: The graph shows thresholds — can I change them here?**  
**A:** Thresholds shown reflect the daemon’s current configuration. Simply drag the threshold to where you want it if you are not happy with the current setting.It takes a little time to apply.

**Q: Do I need root to run the GUI?**  
**A:** No. Only the daemon runs as root. The GUI communicates via D‑Bus. The **Root‑required features** entries are saved in the daemon configuration file; applying them is handled by the appropriate system components.

**Q: Why doesn’t my override take effect immediately?**  
**A:** Some transitions depend on safety timing, current load, or other guards. If nothing changes after a short time, check that the daemon is running and D‑Bus is reachable.

---

## Troubleshooting
- **Current power profile shows "Error"**
  - Immediately inspect your system logs. The daemon is trying to set a value and it is not happening. The journal will shed some light on what it was trying to do, and why it is not working. Most probably the profile configuration is not correct.

- **No data / graph doesn’t move**  
  - Ensure the **dynamic_power** daemon is running.  
  - Check that **power‑profiles‑daemon** is installed and active.  
  - Verify D‑Bus is functioning (user and system buses).

- **Override button doesn’t respond**  
  - Try returning to **Dynamic**, then re‑select your mode.  
  - Restart the GUI; if the issue persists, check daemon logs.
  - Report a bug at: https://github.com/evertvorster/dynamic-power-daemon

- **Root‑required rule not applied**  
  - Double‑check the **Path** and values.  
  - Ensure the entry is **Enabled**.  
  - Some paths differ across kernels and hardware.

---

## Configuration & Files

- **User configuration**  
  - Stored under `~/.config/dynamic_power/config.yaml` (contains your UI-configurable settings, including root‑required features). 
  - There is no need to manually edit this file, everything is done through GUI, and changes here are quite likely to be wiped out. 

- **Daemon**  
  - Runs as root via systemd.  
  - Exposes status and accepts requests over D‑Bus (used by the GUI).

---

## Tips

- Keep **Mode** on **Dynamic** for everyday use; use overrides for special cases (e.g., compiling or long battery stretches).
- Add root‑required entries gradually; test each one so you can quickly pinpoint regressions.
- If you find yourself setting a user override often to run a certain program, find the name of that program with ps -ef | grep -i <program>. Then make a process match rule and your system will automatically switch to that mode when it detects the program running.

If this application is doing something unexpected, or there is a feature that you would like to see implemented, contact the developers at: https://github.com/evertvorster/dynamic-power-daemon
---

## About

- **License:** GPL‑3.0-or-later
- **Project:** Dynamic Power Daemon `dynamic_power_command` GUI (Qt 6)

If you spot anything unclear or want more advanced controls, open an issue or PR with your suggestions.
