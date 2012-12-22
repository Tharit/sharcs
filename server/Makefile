sharcsd: main.c ../packet.c connections.c
	gcc $^ -o bin/$@ -std=c99 -ldl -g -D_BSD_SOURCE -D_GNU_SOURCE -lpthread

clean:
	@echo "Cleaning..."
	@rm -rf bin/sharcsd
