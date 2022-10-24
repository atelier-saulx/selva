include ../common.mk
CFLAGS += -fPIC -g -fvisibility=hidden -I../../include
export

MODULES := \
		   demo_await \
		   demo_sock \
		   demo_timeout \
		   signal

all: $(MODULES)

$(MODULES):
	$(MAKE) -C "$@"

.PHONY: $(MODULES)
