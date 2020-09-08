PROJECT_DIR = ${HOME}/project
IDIR = $(PROJECT_DIR)/os/include
#INC=$(foreach d, $(IDIR), -I$d)
INC=$(IDIR:%=-I%)
AR=ar

SRC_DIR = $(PROJECT_DIR)/os/src
OBJ_DIR = $(PROJECT_DIR)/os/debug
src = $(wildcard $(SRC_DIR)/*.c)
obj = $(src:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
dep = $(obj:.o=.d)  # one dependency file for each source

CFLAGS=$(INC) -g -DPREMEM 
DEBUG = true
ifeq ($(DEBUG), true)
	override CFLAGS += -DDEBUG -DPREMEM_DEBUG
endif

LDFLAGS = -lpthread

libos.a: $(obj)
	cd $(OBJ_DIR);  $(AR) -cr $@ $^

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
