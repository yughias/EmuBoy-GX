SRC := $(wildcard src/*.c) $(wildcard src/**/*.c)
OBJ := $(patsubst src/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)

CC := gcc
EXE := emuboyGX.exe
CFLAGS := -Iinclude -O2 -mavx -flto=auto
DEBUG_FLAGS := -pg -no-pie
LIBS := -Llib -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lshlwapi -lcomdlg32 -lole32

all: $(EXE)

$(EXE): $(OBJ)
	windres config.rc -O coff -o config.res
	$(CC) $(OBJ) $(CFLAGS) config.res $(LIBS) -o $(EXE)
	rm config.res

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD -MP $(CFLAGS) $< -o $@

emcc:
	emcc -Iinclude $(SRC) -O3 -flto=full \
	-sUSE_SDL=2 \
	-sINVOKE_RUN=0 \
	-sTOTAL_MEMORY=128MB \
	-msimd128 \
	--embed-file data/vba_bios.bin \
	-o website/emulator.js

codegen-thumb:
	gcc codegen/src/thumb.c -o codegen_thumb.exe
	codegen_thumb.exe src/arm7tdmi/thumb.c
	rm codegen_thumb.exe

codegen-arm:
	gcc codegen/src/arm.c -o codegen_arm.exe
	codegen_arm.exe src/arm7tdmi/arm.c
	rm codegen_arm.exe

codegen-readio:
	gcc codegen/src/readIo.c -o codegen_readIo.exe
	codegen_readIo.exe include/readIo.h
	rm codegen_readIo.exe

codegen-writeio:
	gcc codegen/src/writeIo.c -o codegen_writeIo.exe
	codegen_writeIo.exe include/writeIo.h
	rm codegen_writeIo.exe

codegen: codegen-arm codegen-thumb codegen-readio codegen-writeio

debug-compile:
	gcc $(DEBUG_FLAGS) -Iinclude $(SRC) $(LIBS) -o $(EXE)
	
debug-graph:
	gprof $(EXE) | gprof2dot -n0 -e0 | dot -Tsvg -o graph.svg

clean:
	rm -f -R obj
	rm -f emuboyGX.exe

-include $(DEP)

.SILENT: clean
.PHONY: emcc clean codegen codegen-thumb codegen-arm codegen-readio codegen-writeio debug-compile debug-graph