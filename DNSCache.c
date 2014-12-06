#include "stdio.h"
#include "malloc.h"
#include "stdlib.h"
#include "string.h"
#include "DNSClient.h"


//global variable for cache head.
CACHE_HEAD cache_head;

/*
Description:    This function is responisible for initialize the cache.
Input:          None;
OutPut:         None;
*/

void dns_cache_init(){
	memset((void*)&cache_head, 0, sizeof(CACHE_HEAD));
	cache_head.init=1;
	if (DNS_CLIENT_DEBUG)
		printf("DNS_CACHE: Cache init\n");
}


/*
Description:    This function is responisible to detroy and free the memory of the cache.
Input:          None;
OutPut:         None
*/

void dns_cache_destroy()
{
	CACHE_NODE *node=NULL, *nextNode=NULL;
	int count=0;

	//cache was not initialized.	
	if (!cache_head.init) return ;

	//get the first node of the cache, go through all the node
	//free the memory of each node.
	node = cache_head.first_node;
	while (node) {
		nextNode = node->queue_next;
		free (node);
		node = nextNode;
	}
	//sanitize the first node and last node pointer.
	cache_head.first_node = cache_head.last_node = NULL;
	
	//sanitize the bucket list.
	for (count=0;count<CACHE_MAX_BUCKET;count++)
		cache_head.hash_bucket[count] = NULL;
	
	//reset the node counter.
	cache_head.node_count = 0;

	if (DNS_CLIENT_DEBUG)
		printf("DNS_CACHE: Cache destroyed\n");
}

/*
Description:    This function is responisible for:
		Access the cache and search for node.
		If node exist, refresh the node i.e. bring the node in front of the list.
		If node does not exist, then:
			If list is full, then delete the last node i.e. least recently used node.
			create a new node and add in the front of the list.
Input:          FQDN String, String length;
OutPut:         -1 Cache Miss, 1 Cache Hit
*/
int dns_cache_check(unsigned char* string, int strLen)
{
	
	int hash = 0, count=0;
	CACHE_NODE *node = NULL;

	//error check
	if (!cache_head.init || !string || !strLen) return -1;

	//calculate the bucket node.
	for (count=0; count<strLen; count++)
		hash = hash+(int)(string[count]);
	
	hash = hash %CACHE_MAX_BUCKET;

	if (DNS_CLIENT_DEBUG)
                printf("DNS_CACHE: Checking for %s:%d:%d\n", string, strLen, hash);

	//get the link list of node, present at that bucket.
	node = cache_head.hash_bucket[hash];
	
	//got through each node in list and look for the match.
	while(node) {
		if (!strncmp(node->string, string, node->str_len)) 
			break;
		else 
			node = node->hash_next;
	}
	if (node)  {
		//node found. lets refresh the node.
		if (DNS_CLIENT_DEBUG)
			printf("DNS_CACHE: DNS_CACHE_HIT\n");
		dns_cache_referesh_node(node);
		return 1;
	} else {
		//node not found, create a new one.
		if (DNS_CLIENT_DEBUG)
			printf("DNS_CACHE: DNS_CACHE_MISS\n");
		dns_cache_add_node(string, strLen, hash);
	}
	
	//node not found,
	return -1;
}

/*
Description:    This function is responisible to add a new node in  cache.
Input:         	FQDN String, String length, and pre calculated hash/bucket value;
OutPut:         -1 for error, 1 for success.
*/
int dns_cache_add_node(unsigned char* string, int strLen, int hash)
{
	int count=0;
	CACHE_NODE *node=NULL, *tmp_node=NULL;
	
	//basic error check.
	if (!cache_head.init) return -1;

	//if cache is full, delete last node.
	if (cache_head.node_count >= CACHE_MAX_NODE) 
		dns_cache_del_last_node();

	//create a new node.
	node = (CACHE_NODE*) malloc(sizeof(CACHE_NODE));
	if(!node) {
		//if malloc failed
		if (DNS_CLIENT_DEBUG)
			printf("Error: DNS_CACHE:Out of memory, total node:%d\n", cache_head.node_count);
		return -1;
	}
	//set the value of the node.
	memset(node, 0, sizeof(CACHE_NODE));
	strcpy(node->string, string);
	node->str_len = strLen;
	node->bucket_num = hash;

	//First add in the link list.
	//if cache was empty, add directly.
	if (cache_head.first_node != NULL) 
		cache_head.first_node->queue_prev = node;

	//insert the node.
	node->queue_next =  cache_head.first_node;
	cache_head.first_node = node;
	node->queue_prev = NULL;
	
	//set the last node pointer.
	if (cache_head.last_node == NULL) cache_head.last_node = node;

	
	//Now add in hash 
	tmp_node = cache_head.hash_bucket[hash];
	if (tmp_node != NULL)
		tmp_node->hash_prev = node;
	
	node->hash_next = tmp_node;
	cache_head.hash_bucket[hash] = node;
	node->hash_prev = NULL;
	
	//increase the node counter.
	cache_head.node_count++;	
	if (DNS_CLIENT_DEBUG)
		printf("DNS_CACHE: Successfully added %s node, total ndoe:%d\n", string, cache_head.node_count);
	
	//Good job.
	return 1;
}

/*
Description:    This function is responisible to delete the last ndoe in cache to make a room for new node.
Input:          None;
OutPut:         None
*/
int dns_cache_del_last_node()
{
	CACHE_NODE *node = NULL, *tmp_node = NULL;

	//basic error check
	if (!cache_head.init) return -1;

	//if list was empty.
	if (cache_head.last_node == NULL) return 0;
		
	
	tmp_node = cache_head.last_node;
	//if cache had only one node.
	if(cache_head.last_node == cache_head.first_node) {
		cache_head.last_node = cache_head.first_node = NULL;
	} else {
		//else take out the node from the list.
		tmp_node->queue_prev->queue_next = tmp_node->queue_next;
		cache_head.last_node = tmp_node->queue_prev;
	}
		
	//Remode the node from Hash too.
	if(cache_head.hash_bucket[node->bucket_num] == tmp_node) {
		if(tmp_node->hash_next) tmp_node->hash_next->hash_prev = tmp_node->hash_prev;
		cache_head.hash_bucket[node->bucket_num] = tmp_node->hash_next;
	} else {
		tmp_node->hash_prev->hash_next = tmp_node->hash_next;
		if (tmp_node->hash_next) tmp_node->hash_next->hash_prev = tmp_node->hash_prev;
	}
	if (DNS_CLIENT_DEBUG)
		printf("DNS_CACHE: Deleting last node %s\n", node->string);
	//Free the memory
	free(tmp_node);
	//decrement the node counter
	cache_head.node_count--;
	
	//Good job
	return 1;
}

/*
Description:    This function is responisible to pick the node and put it in front of the queue.
Input:          cache node;
OutPut:         1 for success
*/
int dns_cache_referesh_node(CACHE_NODE *node)
{
	// basic error check.
	if (!node || !cache_head.init) return -1;
	
	//if cache is empty.
	if (cache_head.first_node == node) return 1;

	//adjust the last node pointer.
	if (cache_head.last_node == node) cache_head.last_node = node->queue_prev;
	
	//pick out the node.
	if(node->queue_prev) node->queue_prev->queue_next = node->queue_next;
	if(node->queue_next) node->queue_next->queue_prev = node->queue_prev;

	//put it infront of the list.
	node->queue_next = cache_head.first_node;
	cache_head.first_node->queue_prev = node;
	cache_head.first_node = node;
	node->queue_prev = NULL;

	if (DNS_CLIENT_DEBUG)
		printf("DNS_CACHE: Node %s refreshed\n", node->string);
	//Good job
	return 1;

}

/*
Description:    This function is responisible to dump out the cache for debugging.
Input:          None;
OutPut:         None
*/
int dns_cache_dump()
{	
if (DNS_CLIENT_DEBUG) {
	int count=0, tmpCount=0;
	CACHE_NODE *node=NULL;

	if(!cache_head.init) printf("Cache is not yet initilized\n");
		
	printf("Dumping cache details:\nTotal nodes:%d, queue first node:%x  queue_last_node:%x\n", cache_head.node_count, cache_head.first_node, cache_head.last_node);

	printf("Printing Hash\n");
	for(count=0;count<CACHE_MAX_BUCKET;count++) {
		node = cache_head.hash_bucket[count];
		printf("Bucket[%d]: ", count);
		if (!node) {
			printf("Empty\n");
			continue;
		}
		tmpCount=0;
		while(node) {
			printf("%s ->", node->string);
			node = node->hash_next;
			tmpCount++;
			if ( !(tmpCount%5)) printf("\n");
		}
		printf(" NULL\n");
	
	}
	printf("\nPrinting queue (format <prev-add><string:hashbucket><next-add>):\n");
	node = cache_head.first_node;
	if (!node) printf("Empty queue\n");
	else {
		tmpCount =0;
		while(node) {
			printf("[%x]%s:%d[%x]->", node->queue_prev, node->string, node->bucket_num, node->queue_next);
			node = node->queue_next;
			tmpCount++;
			if ( !(tmpCount%5)) printf("\n");
		}
		printf(" NULL\n");
	}
	
	printf("Cache dump end\n");
}
	return 0;
}








