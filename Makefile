MODULES = ai_toolkit
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

# AI SDK C++ paths
AI_SDK_DIR = ai-sdk-cpp
AI_SDK_BUILD = $(AI_SDK_DIR)/build
AI_SDK_INCLUDE = $(AI_SDK_DIR)/include
AI_SDK_JSON = $(AI_SDK_DIR)/third_party/nlohmann_json_patched/include

# Compiler flags for C++
PG_CPPFLAGS = -I$(AI_SDK_INCLUDE) -I$(AI_SDK_JSON) -std=c++20 -DAI_SDK_HAS_OPENAI -DAI_SDK_HAS_ANTHROPIC

# Override to use g++ for linking C++ code
override SHLIB_LINK = -Wl,--whole-archive $(AI_SDK_BUILD)/libai-sdk-cpp-openai.a $(AI_SDK_BUILD)/libai-sdk-cpp-anthropic.a $(AI_SDK_BUILD)/libai-sdk-cpp-core.a -Wl,--no-whole-archive -lstdc++ -lssl -lcrypto -lpthread -lcurl

include $(PGXS)

# Force g++ for linking
%.so: %.o
	g++ $(CFLAGS) $(LDFLAGS) -shared -o $@ $< $(SHLIB_LINK)