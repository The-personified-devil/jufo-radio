#
# Makefile
#

# Included Makefiles
include external/lvgl/lvgl.mk

# Variables
CC = g++
DIR ?= ${shell pwd}
OBJEXT ?= .o
BIN = ${OUT_DIR}/radio

MKDIR_P = mkdir -p
OUT_DIR = bin
OBJ_DIR = obj

LDFLAGS ?= -lSDL2 -lm -lpthread -g -lvlc
CFLAGS += -std=c++17

WARNINGS ?= -w -Wall -Wextra \
			-Wshadow -Wundef -Wmaybe-uninitialized -Wmissing-prototypes -Wno-discarded-qualifiers \
			-Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized \
			-Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default  \
			-Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated  \
			-Wempty-body -Wshift-negative-value -Wstack-usage=2048 \
            -Wtype-limits -Wsizeof-pointer-memaccess -Wpointer-arith


#Collect the files to compile
MAINSRC = src/main.cpp
CSRCS += $(MAINSRC)


AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS_RAW_1 = $(CSRCS:.c=$(OBJEXT))
COBJS_RAW = $(COBJS_RAW_1:.cpp=$(OBJEXT))
COBJS = $(shell for i in $(COBJS_RAW); do \
            cobj=$(OBJ_DIR)/$$i; \
			mkdir -p $$(dirname $$cobj); \
			printf " $$cobj"; \
        done)


SRCS = $(ASRCS) $(CSRCS)
OBJS = $(AOBJS) $(COBJS)


all: directories default

directories: ${OUT_DIR} ${OBJ_DIR}

print:
	@echo $(COBJS_RAW2)
	@echo fuckme\n
	@echo $(COBJS_RAW)

default: $(AOBJS) $(COBJS)
	@$(CC) -o $(BIN) $(AOBJS) $(COBJS) $(LDFLAGS)  # /usr/local/lib/libvlc.so.5.6.0

clean: 
	rm -f $(BIN) $(AOBJS) $(COBJS)


# Create the base directories
${OBJ_DIR}:
	${MKDIR_P} ${OBJ_DIR}

${OUT_DIR}:
	${MKDIR_P} ${OUT_DIR}

# Compile the Source Code
${OBJ_DIR}/%.o: %.cpp
	@$(CC) $(CFLAGS) -c -o $@ $<

${OBJ_DIR}/%.o: %.c
	@gcc $(CFLAGS) -c -o $@ $<


.PHONY: directories print
