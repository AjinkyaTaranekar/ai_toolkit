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
SHLIB_LINK = -L$(AI_SDK_BUILD) -lai-sdk-cpp-openai -lai-sdk-cpp-anthropic -lai-sdk-cpp-core -lstdc++ -lssl -lcrypto -lpthread

include $(PGXS)