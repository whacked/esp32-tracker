src/generated/python_autogen.py: ./codegen.clj
	bb $< $@

src/generated/cpp_bt_commands_autogen.h: ./codegen.clj
	bb $< $@

# Use environment variables set by nix shell
CXXFLAGS = -std=c++17 -I.
GTEST_FLAGS = $(shell pkg-config --cflags gtest)
GTEST_LIBS = $(shell pkg-config --libs gtest)

test_command_handler:
	$(CXX) $(CXXFLAGS) test/CommandHandlerTest.cpp $(GTEST_FLAGS) $(GTEST_LIBS) -o test/command_handler_test
	./test/command_handler_test