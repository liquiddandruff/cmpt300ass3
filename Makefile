process: smaugProcess.c
	gcc -o smaugProcess smaugProcess.c -I.

thread: smaugThread.c
	gcc -o smaugThread smaugThread.c -lpthread -I.
