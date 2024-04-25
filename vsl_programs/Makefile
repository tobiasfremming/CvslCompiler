VSLC := ../build/vslc

PS2_EXAMPLES := $(patsubst %.vsl, %.ast, $(wildcard ps2-parser/*.vsl))
PS2_GRAPHVIZ := $(patsubst %.vsl, %.svg, $(wildcard ps2-parser/*.vsl))
PS3_EXAMPLES := $(patsubst %.vsl, %.ast, $(wildcard ps3-simplify/*.vsl))
PS3_GRAPHVIZ := $(patsubst %.vsl, %.svg, $(wildcard ps3-simplify/*.vsl))
PS4_EXAMPLES := $(patsubst %.vsl, %.symbols, $(wildcard ps4-symbols/*.vsl))
PS5_EXAMPLES := $(patsubst %.vsl, %.S, $(wildcard ps5-codegen1/*.vsl))
PS5_ASSEMBLED := $(patsubst %.vsl, %.out, $(wildcard ps5-codegen1/*.vsl))
PS6_EXAMPLES := $(patsubst %.vsl, %.S, $(wildcard ps6-codegen2/*.vsl))
PS6_ASSEMBLED := $(patsubst %.vsl, %.out, $(wildcard ps6-codegen2/*.vsl))

PRINT_AST_OPTION := -T

.PHONY: all ps2 ps2-graphviz ps3 ps3-graphviz ps4 ps5 ps5-assemble ps6 ps6-assemble clean ps2-check ps3-check ps4-check ps5-check ps6-check

all: ps2 ps3 ps4 ps5 ps6

ps2: $(PS2_EXAMPLES)
ps2-graphviz: $(PS2_GRAPHVIZ)

# Make sure ps2 uses the -t flag instead of -T
ps2-parser/%.ast: PRINT_AST_OPTION := -t
ps2-parser/%.svg: PRINT_AST_OPTION := -t

ps3: $(PS3_EXAMPLES)
ps3-graphviz: $(PS3_GRAPHVIZ)

ps4: $(PS4_EXAMPLES)

ps5: $(PS5_EXAMPLES)
ps5-assemble: $(PS5_ASSEMBLED)

ps6: $(PS6_EXAMPLES)
ps6-assemble: $(PS6_ASSEMBLED)

%.ast: %.vsl $(VSLC)
	$(VSLC) $(PRINT_AST_OPTION) < $< > $@

%.svg: %.vsl $(VSLC)
	GRAPHVIZ_OUTPUT=1 $(VSLC) $(PRINT_AST_OPTION) < $< | dot -Tsvg -o $@

%.symbols: %.vsl $(VSLC)
	$(VSLC) -s < $< > $@

%.S: %.vsl $(VSLC)
	$(VSLC) -c < $< > $@

%.out: %.S
	gcc $< -o $@

clean:
	-rm -rf */*.ast */*.svg */*.symbols */*.S */*.out

ps2-check: ps2
	cd ps2-parser; \
	find * -wholename "suggested/*.ast" | awk -F/ '{print $$0 " " $$2}' | xargs -L 1 diff -s --unified=0
	@echo "No differences found in PS2!"

ps3-check: ps3
	cd ps3-simplify; \
	find * -wholename "suggested/*.ast" | awk -F/ '{print $$0 " " $$2}' | xargs -L 1 diff -s --unified=0
	@echo "No differences found in PS3!"

ps4-check: ps4
	cd ps4-symbols; \
	find * -wholename "suggested/*.symbols" | awk -F/ '{print $$0 " " $$2}' | xargs -L 1 diff -s --unified=0
	@echo "No differences found in PS4!"

ps5-check: ps5-assemble
	find ps5-codegen1 -wholename "*.vsl" | xargs -L 1 ./codegen-tester.py
	@echo "No differences found in PS5!"

ps6-check: ps6-assemble
	find ps6-codegen2 -wholename "*.vsl" | xargs -L 1 ./codegen-tester.py
	@echo "No differences found in PS6!"
