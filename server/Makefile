sharcsd: main.c ../packet.c connections.c
	gcc $^ -o bin/$@ -std=c89 -ldl -g

clean:
	@echo "Cleaning..."
	@rm -rf bin/sharcsd
