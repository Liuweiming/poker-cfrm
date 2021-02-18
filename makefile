CC	 = gcc  -fopenmp
CXX	 = g++ -g  -fopenmp

CFLAGS ?=-std=c11 -Wall -g -O3
LDLIBS ?=-c

CXXFLAGS ?= -std=c++11 -MMD -MP

INCLUDES +=-I ./include\
		   -I lib/libpoker/include\
		   -I lib/libecalc/include \
		   -I lib/absl/include

CPP_LIBRARIES +=-L lib/libpoker/lib/release -lpoker\
				-L lib/libecalc/lib/release -lecalc\
				-L lib/absl/lib/\
					-Wl,--start-group -labsl_bad_optional_access\
					-labsl_bad_variant_access\
					-labsl_base\
					-labsl_city\
					-labsl_civil_time\
					-labsl_debugging_internal\
					-labsl_demangle_internal\
					-labsl_dynamic_annotations\
					-labsl_examine_stack\
					-labsl_failure_signal_handler\
					-labsl_flags\
					-labsl_flags_config\
					-labsl_flags_handle\
					-labsl_flags_internal\
					-labsl_flags_marshalling\
					-labsl_flags_parse\
					-labsl_flags_registry\
					-labsl_flags_usage\
					-labsl_graphcycles_internal\
					-labsl_hash\
					-labsl_hashtablez_sampler\
					-labsl_int128\
					-labsl_leak_check\
					-labsl_leak_check_disable\
					-labsl_malloc_internal\
					-labsl_raw_hash_set\
					-labsl_scoped_set_env\
					-labsl_spinlock_wait\
					-labsl_stacktrace\
					-labsl_str_format_internal\
					-labsl_strings\
					-labsl_strings_internal\
					-labsl_symbolize\
					-labsl_synchronization\
					-labsl_throw_delegate\
					-labsl_time\
					-labsl_time_zone -Wl,--end-group\
				-lpthread -lboost_program_options

ifeq ($(target),debug)
	CXXFLAGS +=-O0 -Wall
else
	target = release
	CXXFLAGS +=-O3 -Wall
endif

OBJ_PATH 	= obj/$(target)/
SERVER_PATH = ../../acpc_server/

C_FILES     = $(addprefix $(SERVER_PATH),game.c net.c rng.c)
C_FILES     += $(addprefix src/,deck.c hand_index.c)
C_OBJ_FILES = $(addprefix obj/$(target)/,$(notdir $(C_FILES:.c=.o)))

CPP_FILES 	  = $(wildcard src/*.cpp)
CPP_EXCLUDE	  = $(addprefix $(OBJ_PATH),cfrm-main.o player-main.o cluster-abs-main.o potential-abs-main.o)
CPP_OBJ_FILES = $(addprefix $(OBJ_PATH),$(notdir $(CPP_FILES:.cpp=.o)))
CPP_OBJ_FILES_CORE = $(filter-out $(CPP_EXCLUDE), $(CPP_OBJ_FILES))

DEP_FILES = $(CPP_OBJ_FILES:.o=.d)

all: prepare $(C_OBJ_FILES) $(CPP_OBJ_FILES) cfrm player cluster-abs potential-abs

prepare:
	mkdir -p obj/$(target)

obj/$(target)/%.o: src/%.c
	$(CC) $(INCLUDES) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) $< -o $@

obj/$(target)/%.o: $(SERVER_PATH)/%.c
	$(CC) $(INCLUDES) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) $< -o $@

obj/$(target)/%.o: src/%.cpp 
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c -o $@ $<

cfrm: $(C_OBJ_FILES) $(CPP_OBJ_FILES) 
	$(CXX) $(INCLUDES) $(OBJ_PATH)cfrm-main.o $(CPP_OBJ_FILES_CORE) $(C_OBJ_FILES) $(CPP_LIBRARIES) -o cfrm

cluster-abs: $(C_OBJ_FILES) $(CPP_OBJ_FILES) 
	$(CXX) $(INCLUDES) $(OBJ_PATH)cluster-abs-main.o $(CPP_OBJ_FILES_CORE) $(C_OBJ_FILES) $(CPP_LIBRARIES) -o cluster-abs

potential-abs: $(C_OBJ_FILES) $(CPP_OBJ_FILES) 
	$(CXX) $(INCLUDES) $(OBJ_PATH)potential-abs-main.o $(CPP_OBJ_FILES_CORE) $(C_OBJ_FILES) $(CPP_LIBRARIES) -o potential-abs

player: $(C_OBJ_FILES) $(CPP_OBJ_FILES) prepare 
	$(CXX) $(INCLUDES) $(OBJ_PATH)player-main.o $(CPP_OBJ_FILES_CORE) $(C_OBJ_FILES) $(CPP_LIBRARIES) -o player 

clean:
	rm -r -f obj cfrm player cluster-abs potential-abs
	rm -f $(DEP_FILES)

.PHONY: clean all cfrm player potential-abs

-include $(DEP_FILES)
