# PunKnobs
PunKnobs is a portable gamepad abstraction library for Linux, Windows and macOS.

## API compatibility
 - Linux: evdev
 - Windows: DirectInput, Xinput
 - macOS: IOHID.Framework

## API feature matrix
```
                           +--------------------------------+------------+
                           | info                           | background |
                           | retrieval                      | input      |
+--------------------------+--------------------------------+------------+
| evdev                    | events through file descriptor | yes        |
| RawInput                 | events in message loop         | no         |
| DirectInput              | callback or polling (abstract) | yes        |
| Xinput                   | polling (abstract)             | yes        |
| Windows.Gaming.Input     | polling (abstract)             | no         |
| IOHID.Framework          | callback                       | yes        |
| GameController.Framework | callback or polling (abstract) | yes        |
+--------------------------+--------------------------------+------------+
```

## Input report model
PunKnobs abstracts input reports behind a single callback.
For APIs with input callback support, it is ran in a fairly straightforward way.
When the API reports inputs through events, it is executed as part of a loop.
In the unfortunate case of an API that only supports polling, the user is
expected to set a polling rate (a guide about that is available below) and the
callback will be called as part of an internal loop as well, paced as requested.

The goal here is to make sure any current and future game input API can be
supported, while providing maximum abstraction and flexibility.
For a typical use in video games, you would make use of some third-party event
library to send input info to the thread of your choice for common processing
of gamepad, keyboard and mouse events.

## Library architecture
PunKnobs follows a modular architecture:
 - The core library defines the process of dealing with gamepads for developers.
   They register an API backend in the core for it to be used behind-the-scenes.
 - The API backends are responsible for getting the information returned through
   abstract callbacks using a specific API, which means the actual input
   identifiers you get in the abstract callback in the end will be in a format
   unique to that backend. The values are always in a common format of course,
   but you will have to handle input binding by yourself!

## Building
The build system for PunKnobs consists in bash scripts generating ninja scripts.
The first step is to generate the ninja scripts for all the components needed.
Then, these ninja scripts must be ran to build the actual binaries.

## Dependencies
PunKnobs relies on the following system libraries:
 - libpthread (Linux/FreeBSD)
 - libevdev (Linux/FreeBSD, evdev)
 - DXGUID.lib (Windows)
 - Dinput8.lib (Windows, DirectInput)
 - Xinput.lib (Windows, Xinput)
 - Foundation.Framework (macOS)
 - IOHID.Framework (macOS, IOHID Framework)

## Headers
Linux:
 - libevdev.h
 - linux/input.h
 - pthread.h
 - semaphore.h
 - sys/epoll.h
 - sys/inotify.h
 - sys/stat.h
 - sys/types.h
 - unistd.h

Windows:
 - dinput.h
 - guiddef.h
 - process.h
 - sysinfoapi.h
 - windows.h
 - xinput.h

macOS:
 - IOHIDDevice.h
 - IOHIDManager.h
 - Foundation.h

## Testing
Linux:
PunKnobs fully supports modern-day Linux input stack madness, and is capable of
detecting when systemd blesses your input devices with updated ACL permissions.
Of course some extra setup might be required for your peripherals to work and be
detected on Linux in the first place, and for your user to get the permissions.
On most distributions everything should work out-of-the-box.

Windows:
Everything with a driver exposing a DirectInput gamepad interface is supported.
XInput also works, and for devices supporting both DirectInput and XInput,
only the XInput interface will be acknowledged, ignoring DirectInput.

Wine:
Yes, Wine is fully supported. However, to be able to use DirectInput devices,
you will need to get a native "dinput8.dll" (typically from `winetricks`),
before changing its overrides to "Native then Builtin" (in `winecfg`).
Now beware: for most gamepads Wine will emulate an XInput device using the SDL,
in an effort to make it easier for users to play games without extra homework.

To fall back to a more "faithful" emulation using DirectInput instead of XInput,
you can also use a native dll for "xinput_1_3" (also available in `winetricks`),
set the overrides for it to "Native then Builtin" as well (still in `winecfg`),
and add this registry key to your wine prefix to disable the SDL backend:
```
wine reg add "HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /t REG_DWORD /d 0
```
This will help you test device plugging and unplugging for all DirectInput pads,
with the somewhat unfortunate drawback of not getting any input from them...
Wine will instead report missing implementation features, as code for this has
yet to be written by fellow courageous programmers (Hi!).
