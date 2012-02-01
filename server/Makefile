sharcsd: main.c ../packet.c connections.c
	gcc $^ -o bin/$@ -std=c89 -ldl

clean:
	@echo "Cleaning..."
	@rm -rf bin/sharcsd
