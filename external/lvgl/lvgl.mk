#
# Makefile
#
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= external/lvgl
CFLAGS = -O0 -g -pg -I$(LVGL_DIR)/


include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk
