# The name of the binary to be created.
TARGET = modelviewer
# The type of the binary to be created - ps-exe is most common.
TYPE = ps-exe

# The list of sources files to compile within the binary.
SRCS = \
main.cpp \

# C++ flags - C++20 is required by PSYQo
CXXFLAGS = -std=c++20

# Uncomment for extra warnings as errors during development
# ifeq ($(TEST),true)
# CPPFLAGS += -Werror
# endif

# This will activate the PSYQo library and the rest of the toolchain.
include third_party/nugget/psyqo/psyqo.mk