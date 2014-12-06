#include "stdio.h"
#include "string.h"
#include "malloc.h"
#include "DNSClient.h"


int main()
{
	char url[100] = {0};
	dns_cache_init();
	
	sprintf(url, "abc.com");
	dns_check_cache(url, strlen(url));
	sprintf(url, "def.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "ghi.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "jkl.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "mno.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "pqr.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "stu.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "wxy.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "google.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "itm.com");
        dns_check_cache(url, strlen(url));
	sprintf(url, "microsoft.com");
        dns_check_cache(url, strlen(url));			
	
	dns_dump_cache();

	dns_check_cache(url, strlen(url));
	sprintf(url, "mno.com");
        dns_check_cache(url, strlen(url));
	dns_dump_cache();
	
	dns_cache_destroy();
}

