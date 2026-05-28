# Legendary Impact - Eventmanager

Modern Guild Wars 2 event management directly inside Nexus.


# Features

- Full Legendary Impact event integration
- Automatic event synchronization
- Event reminder notifications
- Quick Access support
- Markdown event descriptions
- Attendee overview with roles and boons
- Squad join shortcut support
- Modern C++20 architecture
- Optimized worker threads and mutex handling


# Screenshots

![Preview](docs/screen.png)


# Requirements

- Guild Wars 2
- Nexus Addon Loader
- Windows x64


# Installation

1. Download the latest release
2. Extract the addon into:

```text
Guild Wars 2/addons/
```

3. Start Guild Wars 2
4. Open Nexus
5. Enable `Legendary Impact - Eventmanager`


# Configuration

The addon can be configured directly inside the Nexus options menu.

Available settings:

- Legendary Impact token
- Auto sync interval
- Reminder settings
- Window visibility
- Keybind configuration

---

# Default Keybind

```text
F8
```

Can be changed inside the Nexus keybind settings.


# Technologies

- C++20
- Nexus API
- ImGui
- WinHTTP
- nlohmann/json


# Building

Recommended setup:

- Visual Studio 2022
- C++20 enabled

Compiler settings:

```text
/std:c++20
/permissive-
```


# Credits

Created by Backxtar

Special thanks to:

- Raidcore
- Nexus

---

# License

This project is provided as-is for the Guild Wars 2 community.