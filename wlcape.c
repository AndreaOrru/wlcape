/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

// Maximum number of supported keyboards.
#define MAX_KEYBOARDS 16

// State of a key.
typedef enum {
  UP = 0,
  DOWN = 1,
} PressedState;

PressedState capslock_state = UP;    // Pressed state of CAPSLOCK.
struct timeval capslock_press_time;  // Time when CAPSLOCK was pressed.

// If CAPSLOCK is released within this timeout, send ESC instead.
int timeout_ms = 200;

// Error handling.
void die(const char *msg) {
  if (errno) {
    perror(msg);
  } else {
    fprintf(stderr, "%s\n", msg);
  }
  exit(1);
}

// Logging function.
void warn(const char *msg) { fprintf(stderr, "%s\n", msg); }

// Return milliseconds elapsed since the given time, or -1 on error.
long timeSince(struct timeval *t) {
  struct timeval now;
  if (gettimeofday(&now, NULL) < 0) {
    return -1;
  }
  return (now.tv_sec - t->tv_sec) * 1000 + (now.tv_usec - t->tv_usec) / 1000;
}

// Force uinput synchronization. Return 0 on success, -1 on error.
int forceSync(int uinput_fd) {
  struct input_event ev;

  memset(&ev, 0, sizeof(ev));
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;

  if (write(uinput_fd, &ev, sizeof(ev)) < 0) {
    return -1;
  }
  return 0;
}

// Synchronously write a key event to uinput. Return 0 on success, -1 on error.
int uinputWriteEvent(int uinput_fd, struct input_event *base_ev, int code,
                     int value) {
  struct input_event ev = *base_ev;
  ev.code = code;
  ev.value = value;

  if (write(uinput_fd, &ev, sizeof(struct input_event)) < 0) {
    return -1;
  }

  return forceSync(uinput_fd);
}

// Open all keyboard devices for reading, returning the respective file
// descriptors via the fds parameter, and the number of connected keyboards
// via the return value. Fails on error.
int getKeyboardsFds(int *fds) {
  // Get udev context.
  struct udev *udev = udev_new();
  if (udev == NULL) {
    die("Error creating udev context");
  }
  // Get udev enumerate context.
  struct udev_enumerate *enumerate = udev_enumerate_new(udev);
  if (enumerate == NULL) {
    die("Error creating udev enumerate context");
  }

  // Filter to only input devices.
  if (udev_enumerate_add_match_subsystem(enumerate, "input") < 0) {
    die("Error adding 'input' subsystem match");
  }
  // Filter to only keyboard devices.
  if (udev_enumerate_add_match_property(enumerate, "ID_INPUT_KEYBOARD", "1") <
      0) {
    die("Error adding 'ID_INPUT_KEYBOARD=1' property match");
  }

  // Get the list of filtered devices.
  if (udev_enumerate_scan_devices(enumerate) < 0) {
    die("Error scanning udev devices");
  }
  struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
  if (devices == NULL) {
    die("Error getting udev device list");
  }

  // Iterate over the list of devices.
  int i = 0;
  struct udev_list_entry *entry;
  udev_list_entry_foreach(entry, devices) {
    // Get device information.
    const char *path = udev_list_entry_get_name(entry);
    struct udev_device *device = udev_device_new_from_syspath(udev, path);
    if (device == NULL) {
      die("Error getting udev device");
    }

    // Get the devnode path associated with the device.
    const char *devnode = udev_device_get_devnode(device);
    if (devnode != NULL) {
      // Ensure we don't overflow the fds array.
      if (i >= MAX_KEYBOARDS) {
        warn("Too many keyboards, ignoring the rest");
        break;
      }
      // Open the keyboard device for reading.
      fds[i] = open(devnode, O_RDONLY);
      if (fds[i] < 0) {
        die("Error opening keyboard device");
      }
      i++;
    }
    udev_device_unref(device);
  }

  // Clean up.
  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  return i;
}

// Handle a keyboard event. Return 0 on success, -1 on error.
int handleEvent(int uinput_fd, struct input_event *ev) {
  if (ev->type == EV_KEY) {
    if (ev->code == KEY_CAPSLOCK) {
      if (ev->value == DOWN) {
        // Start a timer when CAPSLOCK is pressed.
        if (gettimeofday(&capslock_press_time, NULL) < 0) {
          warn("Error while getting the time of CAPSLOCK press");
          return -1;
        }
        capslock_state = DOWN;
      } else if (ev->value == UP) {
        // Check how long it has been since CAPSLOCK was pressed.
        long elapsed = timeSince(&capslock_press_time);
        if (elapsed < 0) {
          warn("Error while getting the time of CAPSLOCK release");
          return -1;
        }
        if (capslock_state == DOWN && elapsed < timeout_ms) {
          // If CAPSLOCK was released within the timeout, simulate ESC.
          if (uinputWriteEvent(uinput_fd, ev, KEY_ESC, DOWN) < 0) {
            warn("Error while sending ESC key press");
            return -1;
          }
          if (uinputWriteEvent(uinput_fd, ev, KEY_ESC, UP) < 0) {
            warn("Error while sending ESC key release");
            return -1;
          }
        }
        capslock_state = UP;
      }
    } else if (capslock_state == DOWN) {
      // If we press another key while CAPSLOCK is being held down, we don't
      // want the CAPSLOCK press to be eligible for ESC simulation.
      capslock_state = UP;
    }
  }
  return 0;
}

void printHelp(const char *program_name) {
  printf("Usage: %s [-t TIMEOUT_MS] [-h]\n", program_name);
  printf("Options:\n");
  printf("  -t TIMEOUT_MS  Timeout for generating an ESC event.\n");
  printf("  -h             Display this help message.\n");
}

int main(int argc, char *argv[]) {
  // Option parsing.
  int opt;
  while ((opt = getopt(argc, argv, "ht:")) != -1) {
    switch (opt) {
      case 't':
        timeout_ms = atoi(optarg);
        break;
      case 'h':
        printHelp(argv[0]);
        return 0;
      default:
        printHelp(argv[0]);
        return 1;
    }
  }

  // Open keyboard fds.
  int kbd_fds[MAX_KEYBOARDS];
  int n_keyboards = getKeyboardsFds((int *)&kbd_fds);

  // Setup uinput device.
  int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (uinput_fd < 0) {
    die("Error opening uinput");
  }

  // We are going to be generating EV_KEY events.
  if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) < 0) {
    die("Error setting EV_KEY on uinput's EVBIT");
  }
  // We are going to support CAPSLOCK and ESC.
  if (ioctl(uinput_fd, UI_SET_KEYBIT, KEY_CAPSLOCK) < 0) {
    die("Error setting KEY_CAPSLOCK on uinput's KEYBIT");
  }
  if (ioctl(uinput_fd, UI_SET_KEYBIT, KEY_ESC) < 0) {
    die("Error setting KEY_ESC on uinput's KEYBIT");
  }

  // Define a virtual keyboard device.
  struct uinput_setup usetup;
  memset(&usetup, 0, sizeof(usetup));
  usetup.id.bustype = BUS_USB;
  usetup.id.vendor = 0x0001;
  usetup.id.product = 0x0001;
  strcpy(usetup.name, "wlcape");

  // Setup and create uinput virtual keyboard device.
  if (ioctl(uinput_fd, UI_DEV_SETUP, &usetup) < 0) {
    die("Error setting up uinput device");
  }
  if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
    die("Error creating uinput device");
  }

  // Create the epoll instance.
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    die("Error creating epoll instance");
  }
  // Add each keyboard fd to the epoll instance.
  for (int i = 0; i < n_keyboards; i++) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = kbd_fds[i];
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, kbd_fds[i], &ev) < 0) {
      die("Error adding keyboard fd to epoll instance");
    }
  }

  // Event processing loop.
  struct input_event ev;
  struct epoll_event epoll_events[MAX_KEYBOARDS];

  while (1) {
    // Wait for events on the epoll instance.
    int n_events = epoll_wait(epoll_fd, epoll_events, n_keyboards, -1);
    if (n_events < 0) {
      die("Error waiting for events on epoll instance");
    }

    // Process epoll events.
    for (int i = 0; i < n_events; i++) {
      if (epoll_events[i].events & EPOLLIN) {
        // Read an event from one of the keyboards.
        int kbd_fd = epoll_events[i].data.fd;
        if (read(kbd_fd, &ev, sizeof(ev)) < 0) {
          warn("Error reading event from keyboard device");
          continue;
        }
        // Handle the keyboard event.
        handleEvent(uinput_fd, &ev);
      }
    }
  }

  // Close all open file descriptors.
  close(uinput_fd);
  for (int i = 0; i < n_keyboards; i++) {
    close(kbd_fds[i]);
  }

  return 0;
}
