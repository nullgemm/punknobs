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
