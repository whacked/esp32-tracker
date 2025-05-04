src/generated/python_codegen.py: ./codegen.clj
	bb $< $@

src/generated/cpp_bt_commands_codegen.h: ./codegen.clj
	bb $< $@
