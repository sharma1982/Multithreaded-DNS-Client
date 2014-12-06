#ifndef __DNS_H__
#define __DNS_H__

#define DNS_SERVER "8.8.8.8"
#define MAX_FQDN_LENGTH 100
#define MAX_DNS_SERVER 5
#define DNS_PORT 53
#define INDIR_MASK 192
#define T_PTR 12
#define T_A 1
#define MAXLABEL 63
#define CACHE_MAX_NODE 200
#define MAX_FILE_NAME 100
#define CACHE_MAX_BUCKET 17
#define DNS_CLIENT_DEBUG 0 

typedef struct DNS_QUESTION_TYPE
{
   unsigned short       query_type;
   unsigned short       query_class;
}  DNS_QUESTION_TYPE;

typedef struct  DNS_FLAG
{
    unsigned char       rd:1;
    unsigned char       tc:1;
    unsigned char       aa:1;
    unsigned char       opcode:4;
    unsigned char       qr:1;
    unsigned char       rcode:4;
    unsigned char       cd:1;
    unsigned char       ad:1;
    unsigned char       unused:1;
    unsigned char       ra:1;

} DNS_FLAG;


typedef struct DNS_HEADER
{
    unsigned short              identification;
    DNS_FLAG                    flags;
    unsigned short              num_question_rr;
    unsigned short              num_ans_rr;
    unsigned short              num_auth_rr;
    unsigned short              num_add_rr;
} DNS_HEADER;


/* resource record format */
typedef struct DNS_RR
{
        unsigned short          rr_type;
        unsigned short          rr_class;
        unsigned int            ttl;
        unsigned short          rr_length;
} DNS_RR;


typedef struct cache_node {
	char string[100];
	int str_len;
	int bucket_num;
	struct cache_node *hash_next;
	struct cache_node *hash_prev;
	struct cache_node *queue_next;
        struct cache_node *queue_prev;
}CACHE_NODE;

typedef struct cache_head{
	int node_count;
	int init;
	struct cache_node *first_node;
	struct cache_node *last_node;
	struct cache_node *hash_bucket[CACHE_MAX_BUCKET];
}CACHE_HEAD;



void dns_init_server();
void DNSClientThreadFucntion(void *threadID);


int dns_parse_response(unsigned char* buffer, int bufferLen, unsigned int *s_addr, unsigned short *ttl);
int dns_convert_qname(unsigned char* qname, unsigned char* fqdn);
int dns_construct_query(char* dnsBuffer, char* fqdn, int id);
unsigned char *dns_skip_query(unsigned char *name_ptr, unsigned int *data_length);


void dns_cache_init();
void dns_cache_destroy();
int dns_check_cache(unsigned char* string, int strLen);
int dns_cache_add_node(unsigned char* string, int strLen, int hash);
int dns_cache_del_last_node();
int dns_cache_referesh_node(CACHE_NODE *node);
int dns_cache_dump();

#endif


