PROJECT_DIR ?= ${HOME}/project
PROJECT_MODULE_DIR = $(PROJECT_DIR)/os/src/osXmlParser
IDIR = $(PROJECT_DIR)/os/include $(PROJECT_MODULE_DIR)/include
INC=$(IDIR:%=-I%)

SRC_DIR = $(PROJECT_MODULE_DIR)/src
OBJ_DIR = $(PROJECT_MODULE_DIR)/debug
src = $(wildcard $(SRC_DIR)/*.c)
obj = $(src:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
dep = $(obj:.o=.d)  # one dependency file for each source

ifdef $(APP_BASE)
    include $(APP_BASE)/Makefile.cflags
else
    CC=gcc
    AR=ar
    CFLAGS=$(INC) -g -DPREMEM -std=gnu99
    DEBUG = true
    ifeq ($(DEBUG), true)
        override CFLAGS += -DDEBUG -DPREMEM_DEBUG
    endif
endif

LDFLAGS = -lpthread

libxmlparser.a: $(obj)
	cd $(OBJ_DIR);	$(AR) -cr $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	
-include $(dep)   # include all dep files in the makefile

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@


.PHONY: clean
clean:
	rm -f $(dep) $(obj) $(OBJ_DIR)/*.a

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: all
all:
	$(info $(obj))
	$(info $(dep))
	@mkdir -p test
