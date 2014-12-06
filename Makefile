lient: DNSClient.c DNSParser.c DNSCache.c
	gcc -g -o DNSClient.o DNSClient.c DNSParser.c DNSCache.c -lpthread

clean:
	rm -f *.o

