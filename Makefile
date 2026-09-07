.PHONY: build run clean list_commands test help

help:
	@echo "Erika Makefile commands:"
	@echo ""
	@echo "  make help                Show this help message"
	@echo "  make build               Configure and build Erika"
	@echo "  make run                 Build and run voice mode"
	@echo "  make clean               Remove the build directory"
	@echo "  make list_commands       List supported voice commands"
	@echo "  make test <text>         Test a command without the microphone"
	@echo ""
	@echo "Examples:"
	@echo "  make test open firefox"
	@echo "  make test play bohemian rhapsody"
	@echo "  make test what is the capital of japan"

build:
	cmake -S . -B build
	cmake --build build -j4

run: build
	./build/erika

list_commands: build
	./build/erika --list-commands

test: build
	./build/erika --command "$(filter-out $@,$(MAKECMDGOALS))"

clean:
	rm -rf build

%:
	@: