# Test logic C trên máy tính (không cần ESP32). Cần gcc và make (Linux, macOS, WSL hoặc MSYS2 trên Windows).
#   make            chạy cả 4 bộ test
#   make platform   TV1: ghép khung TCP, phiên đăng nhập, chống dò mật khẩu
#   make catalog    TV2: thư viện ngày giờ, quy đổi tiết
#   make schedule   TV3: sinh buổi (dùng catalog giả)
#   make calendar   TV4: trùng lịch (dùng schedule và catalog giả; FAIL cho tới khi TV4 làm xong)
CC     ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter -g -Icommon -Ifakes \
          -I../components/platform/include -I../components/platform/src \
          -I../components/catalog/include -I../components/catalog/src \
          -I../components/schedule/include -I../components/schedule/src \
          -I../components/calendar/include -I../components/calendar/src
B := build
C := ../components

.PHONY: all platform catalog schedule calendar clean
all: platform catalog schedule calendar

platform: $(B)/test_platform
	./$(B)/test_platform
catalog: $(B)/test_catalog
	./$(B)/test_catalog
schedule: $(B)/test_schedule
	./$(B)/test_schedule
calendar: $(B)/test_calendar
	./$(B)/test_calendar

$(B)/test_platform: platform/test_platform.c $(C)/platform/src/frame.c $(C)/platform/src/session.c \
                    $(C)/platform/src/limiter.c | $(B)
	$(CC) $(CFLAGS) $^ -o $@

$(B)/test_catalog: catalog/test_catalog.c $(C)/catalog/src/dt.c $(C)/catalog/src/catalog_core.c | $(B)
	$(CC) $(CFLAGS) $^ -o $@

$(B)/test_schedule: schedule/test_schedule.c $(C)/schedule/src/schedule_core.c $(C)/schedule/src/expand.c \
                    $(C)/catalog/src/dt.c fakes/fake_catalog.c | $(B)
	$(CC) $(CFLAGS) $^ -o $@

$(B)/test_calendar: calendar/test_calendar.c $(C)/calendar/src/conflict.c \
                    $(C)/catalog/src/dt.c fakes/fake_catalog.c fakes/fake_schedule.c | $(B)
	$(CC) $(CFLAGS) $^ -o $@

$(B):
	mkdir -p $(B)

clean:
	rm -rf $(B)
