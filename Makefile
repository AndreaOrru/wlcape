# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

PREFIX=/usr/local
SYSTEMD_INSTALL_DIR=/usr/lib/systemd/system

TARGET := wlcape

CFLAGS  += -O2 -Wall -Wextra
LDFLAGS += -ludev

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	install -m 644 $(TARGET).service $(SYSTEMD_INSTALL_DIR)/$(TARGET).service

clean:
	rm -f $(TARGET)
