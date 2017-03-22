Woodotool
=========

This is the interface description for the protocol to emulate virtual
devices in a Wayland compositor (henceforce the Compositor).

This project is **not** an implementation. It's a discussion point for the
required interfaces together with a basic implementation that can verify
the concept and test clients or servers.

Why woodotool?
--------------

The basic goal is to provide functionality similar to xdotool in Wayland
compositors. xdotool's requirements are out of scope for a wayland protocol.
Wayland is a display server protocol, not a protocol to emulate input
devices.

This is a dbus interface instead.

DBus Interface
==============

***NOTE: THIS INTERFACE IS IN DEVELOPMENT AND SUBJECT TO CHANGE***


This interface centers around the idea that each client has their own
virtual device(s) separate from any other clients. The compositor creates
these on-the-fly as the client registers on dbus.

These fake input events come from a different event stream than real input
devices and can thus be enabled or disabled depending on the current
context. For example, the Compositor may not allow fake events while lock
screen is active.

Security considerations
-----------------------

One of the drawbacks of the XTest interface is that it provides no way to
restrict the applications that can fake input events. Moving to a dbus-based
interface allows identifying the client endpoint and thus the PID of the
calling process. Going beyond this is an unsolved problem. Per-process
permissions are not available with DBus existing tools.

A pre-shared key between the appliation and the compositor requires
considerations in the API and provides minimal extra security - all clients
and the compositor need to avoid keeping the key unencrypted in memory.

Configuration of any filtering is out of scope of this protocol. How dbus
clients are matched with the whitelist is out of scope of this protocol.

The split into separate sub-interfaces allows clients to be
restricted to a specific feature set in a discoverable manner.

WooDoTool DBus Interface
========================

Interfaces:

- org.freedesktop.Woodotool.Manager
- org.freedesktop.Woodotool.Keyboard
- org.freedesktop.Woodotool.Mouse
- org.freedesktop.Woodotool.Desktop

Interface descriptions
----------------------

- **`org.freedesktop.Woodotool.Manager`**

  The manager interface is a singleton representing the compositor.

  - Methods:

    - ***`GetKeyboard(String name) -> object`***

      Service creates a new object implementing
      **org.freedesktop.Woodotool.Keyboard** interface for the
      requesting process.

      MUST create at least one keyboard or throw an exception on EACCESS

      MAY create multiple objects if called repeatedly

    - ***`GetMouse(String name) -> object`***

      Service creates a new object implementing
      **org.freedesktop.Woodotool.Mouse** interface for the
      requesting process.

      MUST create at least one mouse device or throw an exception on EACCESS

      MAY create multiple objects if called repeatedly

    - ***`GetDesktop(String name) -> object`***

      Service returns the singleton object implementing the
      **org.freedesktop.Woodotool.Mouse** interface. Only one object
      implementing this service exists.

- **`org.freedesktop.Woodotool.Keyboard`**
  - Properties:

    - **`Name (String)`**
      Compositor-assigned name, for human consumption/debugging only

  - Methods

    - **`SetXKBKeymap(int fd, uint32 size) -> ()`**
        fd: an memory-mapable file-descriptor to a libxkbcommon-compatible keymap.

        Sets the keymap for this keyboard. This method must be called before
        any events are to be sent from the keyboard.

    - **`Press(uint32  keycode) -> ()`**
        keycode: a keycode for the given layout

        Emulate key press event for the key with the given keycode. If the
        key is already down, nothing happens.

    - **`Release(uint32 keycode) -> ()`**
        keycode: a keycode for the given layout

        Emulate key release event for the key with the given keycode. If the
        key is already up, nothing happens.

- **`org.freedesktop.Woodotool.Mouse`**
  - Properties:

    - **`Name (String)`**
      Compositor-assigned name, for human consumption/debugging only

  - Methods

    - **`MoveRelative(int x, int y) -> ()`**
        Moves the pointer by the given distance in logical pixels. Note that
        in High-DPI screen a logical pixel may differ to a physical pixel.

    - **`MoveAbsolute(int x, int y) -> ()`**
        Moves the pointer to the given location in logical pixels. Note that
        in High-DPI screen a logical pixel may differ to a physical pixel.

    - **`Press(uint32 code) -> ()`**
        Press the button with the given evdev code (e.g. BTN_LEFT)

    - **`Release(uint32 code) -> ()`**
        Release the button with the given evdev code (e.g. BTN_LEFT)

- **`org.freedesktop.Woodotool.Desktop`**
  - Signals

    - **`Edge(enum edge)`**
        Signals that the pointer has hit an edge or corner. Valid arguments
        are `top` (0), `bottom` (1), `left` (2), `right` (3) for edges and
        `top-left` (4), `bottom-left` (5), `top-right` (6) and
        `bottom-right` (7) for corners.

        It is up to the Compositor to decide where the edge stops and the
        corner starts.

        It is up to the Compositor to decide which corners are active. For
        the top-left trigger may only signal on the primary display.
