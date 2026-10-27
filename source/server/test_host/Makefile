# Test logic C trên máy tính (không cần ESP32). Cần gcc và make (Linux, macOS, WSL hoặc MSYS2 trên Windows).
#   make              chạy cả 8 bộ test
#   make platform     TV1: ghép khung TCP, phiên đăng nhập, chống dò mật khẩu
#   make catalog      TV2: thư viện ngày giờ, quy đổi tiết
#   make catalog_cmd  TV2: handler 25 lệnh danh mục (cần cJSON của ESP-IDF)
#   make schedule     TV3: sinh buổi (dùng catalog giả)
#   make schedule_cmd TV3: handler lịch dạy, công việc, ngoại lệ, xung đột (cần cJSON)
#   make schedule_store TV3: đọc/ghi /data/schedule.json dạng JSON Lines (cần cJSON)
#   make calendar     TV4: trùng lịch (dùng schedule và catalog giả)
#   make calendar_cmd TV4: handler lịch tuần, khoảng ngày, xung đột (cần cJSON)
CC     ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter -g -Icommon -Ifakes \
          -I../components/platform/include -I../components/platform/src \
          -I../components/catalog/include -I../components/catalog/src \
          -I../components/schedule/include -I../components/schedule/src \
          -I../components/calendar/include -I../components/calendar/src
B := build
C := ../components

.PHONY: all platform catalog catalog_cmd schedule schedule_cmd schedule_store calendar calendar_cmd clean
all: platform catalog catalog_cmd schedule schedule_cmd schedule_store calendar calendar_cmd

platform: $(B)/test_platform
	./$(B)/test_platform
catalog: $(B)/test_catalog
	./$(B)/test_catalog
catalog_cmd: $(B)/test_catalog_cmd
	./$(B)/test_catalog_cmd
schedule: $(B)/test_schedule
	./$(B)/test_schedule
schedule_cmd: $(B)/test_schedule_cmd
	./$(B)/test_schedule_cmd
schedule_store: $(B)/test_schedule_store
	./$(B)/test_schedule_store
calendar: $(B)/test_calendar
	./$(B)/test_calendar
calendar_cmd: $(B)/test_calendar_cmd
	./$(B)/test_calendar_cmd

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

# cJSON nằm trong ESP-IDF. Đặt IDF_PATH (hoặc CJSON_DIR) nếu cài ở chỗ khác.
IDF_PATH ?= D:/.espressif/v5.5.6/esp-idf
CJSON_DIR ?= $(IDF_PATH)/components/json/cJSON

$(B)/test_catalog_cmd: catalog/test_catalog_cmd.c \
                        $(C)/catalog/src/catalog_cmd.c \
                        $(C)/catalog/src/catalog_core.c \
                        $(C)/catalog/src/dt.c \
                        $(CJSON_DIR)/cJSON.c | $(B)
		$(CC) $(CFLAGS) -I$(CJSON_DIR) $^ -o $@

$(B)/test_calendar_cmd: calendar/test_calendar_cmd.c \
                        $(C)/calendar/src/calendar_cmd.c \
                        $(C)/calendar/src/conflict.c \
                        $(C)/catalog/src/dt.c \
                        fakes/fake_catalog.c \
                        $(CJSON_DIR)/cJSON.c | $(B)
		$(CC) $(CFLAGS) -I$(CJSON_DIR) $^ -o $@

$(B)/test_schedule_store: schedule/test_schedule_store.c \
                        $(C)/schedule/src/schedule_store.c \
                        $(C)/schedule/src/schedule_core.c \
                        $(C)/catalog/src/dt.c \
                        $(CJSON_DIR)/cJSON.c | $(B)
		$(CC) $(CFLAGS) -I$(CJSON_DIR) $^ -o $@

$(B)/test_schedule_cmd: schedule/test_schedule_cmd.c \
                        $(C)/schedule/src/schedule_cmd.c \
                        $(C)/schedule/src/schedule_core.c \
                        $(C)/schedule/src/expand.c \
                        $(C)/catalog/src/dt.c \
                        fakes/fake_catalog.c \
                        $(CJSON_DIR)/cJSON.c | $(B)
		$(CC) $(CFLAGS) -Ifakes -I$(CJSON_DIR) $^ -o $@