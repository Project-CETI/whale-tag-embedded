TEST_SRC        = $(shell find src -type f -iname '*.c' 2> /dev/null)
 
# SRC        = $(filter-out ../src/launcher.c, $(wildcard ../src/*.c) $(wildcard ../src/*/*.c) $(wildcard ../src/*/*/*.c))
SRC			= ../src/utils/config.c ../src/aprs.c
TEST_APP        = $(TEST_SRC:.c=.test)
OBJ        = $(SRC:.c=.o)
CC         = gcc

UNITY_DIR    = lib/Unity

CFLAGS     = -Wall -g
LDFLAGS    = -lpthread -lm -L $(UNITY_DIR) -lunity -I $(UNITY_DIR)/src/ -I ../src

build:
	@

binary:
	@

test: $(TEST_APP)
	$(foreach test, $^, ./$(test);)
.PHONY : test

#test executables
%.test: %.c  $(OBJ) $(UNITY_DIR)/libunity.a 
	$(CC) $(CFLAGS) -o $@ $< $(OBJ) $(LDFLAGS) 

$(UNITY_DIR)/libunity.a: $(UNITY_DIR)/Makefile
	$(MAKE) -C $(UNITY_DIR)

$(UNITY_DIR)/Makefile:
	cd $(UNITY_DIR) && cmake .

clean:
	rm -f $(OBJ) $(TEST_APP)
.PHONY : clean

deep_clean:
	$(MAKE) clean
	$(MAKE) clean -C lib/Unity 
.PHONY : deep_clean