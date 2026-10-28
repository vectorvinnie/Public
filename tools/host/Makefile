# Desktop builds of the parts of PocketPet that don't need the device.
#
#   make test                          rules tests (no extra downloads)
#   make screenshots M5GFX=../M5GFX    render screens to docs/screenshots
#   make sim M5GFX=../M5GFX            run PocketPet.ino through a scripted play session
#
# Screenshots need a checkout of https://github.com/m5stack/M5GFX and libpng.

M5GFX ?= M5GFX
BUILD ?= build
SRC = ../../PocketPet
CXXFLAGS = -std=c++17 -O1 -g -Wall -Wno-unused-function
LGFX_FLAGS = -DLGFX_LINUX_FB -I. -I$(M5GFX)/src -Wno-all
LGFX_SRCS = LGFXBase.cpp lgfx_fonts.cpp lgfx_v1.cpp lgfx_v1_panel.cpp lgfx_v1_platforms.cpp lgfx_v1_touch.cpp
LGFX_C_SRCS = utility/lgfx_tjpgd.c utility/lgfx_pngle.c utility/lgfx_qoi.c utility/lgfx_miniz.c \
  utility/lgfx_qrcode.c Fonts/efont/lgfx_efont_tw.c Fonts/efont/lgfx_efont_kr.c \
  Fonts/efont/lgfx_efont_ja.c Fonts/efont/lgfx_efont_cn.c Fonts/IPA/lgfx_font_japan.c
LGFX_OBJS = $(addprefix $(BUILD)/lgfx_,$(LGFX_SRCS:.cpp=.o)) \
  $(addprefix $(BUILD)/lgfxc_,$(notdir $(LGFX_C_SRCS:.c=.o)))
vpath %.c $(addprefix $(M5GFX)/src/lgfx/,$(sort $(dir $(LGFX_C_SRCS))))

.PHONY: test screenshots sim clean

test: $(BUILD)/test_pet
	$(BUILD)/test_pet

$(BUILD)/test_pet: test_pet.cpp $(SRC)/pet.cpp $(SRC)/pet.h $(SRC)/config.h | $(BUILD)
	$(CXX) $(CXXFLAGS) test_pet.cpp $(SRC)/pet.cpp -o $@

screenshots: $(BUILD)/render
	mkdir -p ../../docs/screenshots
	$(BUILD)/render ../../docs/screenshots

$(BUILD)/render: render.cpp $(SRC)/ui.cpp $(SRC)/ui.h $(SRC)/pet.cpp $(SRC)/pet.h $(SRC)/config.h $(LGFX_OBJS)
	$(CXX) $(CXXFLAGS) $(LGFX_FLAGS) render.cpp $(SRC)/ui.cpp $(SRC)/pet.cpp $(LGFX_OBJS) -lpng -lpthread -o $@

sim: $(BUILD)/sim
	mkdir -p $(BUILD)/sim-shots
	$(BUILD)/sim $(BUILD)/sim-shots

$(BUILD)/sim: sim/sim.cpp sim/M5Unified.h sim/Arduino.h sim/Preferences.h $(SRC)/PocketPet.ino $(SRC)/ui.cpp $(SRC)/ui.h $(SRC)/pet.cpp $(SRC)/pet.h $(SRC)/config.h $(LGFX_OBJS)
	$(CXX) $(CXXFLAGS) $(LGFX_FLAGS) -Isim sim/sim.cpp $(SRC)/ui.cpp $(SRC)/pet.cpp $(LGFX_OBJS) -lpng -lpthread -o $@

$(BUILD)/lgfx_%.o: $(M5GFX)/src/lgfx/v1/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(LGFX_FLAGS) -c $< -o $@

$(BUILD)/lgfxc_%.o: %.c | $(BUILD)
	$(CC) -O1 -I$(M5GFX)/src -w -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
