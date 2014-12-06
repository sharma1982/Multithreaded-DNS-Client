#include "stdio.h"
#include "malloc.h"
#include "stdlib.h"
#include "string.h"
#include "DNSClient.h"

/*
Description:    This function is responisible to construct the DNS query.
Input:          Buffer for forming the query, FQDN name and thread id;
OutPut:         -1 for error, query legth for success.
*/
int dns_construct_query(char* dnsBuffer, char* fqdn, int id)
{
        DNS_HEADER *dns = NULL;
        unsigned char *question = NULL;
        DNS_QUESTION_TYPE *question_type = NULL;
        int returnLen=0;

	//basic error check
        if (!dnsBuffer || !fqdn) {
                return -1;
        }

        if (strlen(fqdn) > MAX_FQDN_LENGTH) {
                printf("Warning: \"%s\" exceeded maxium length, skipping\n", fqdn);
                return -1;
        }
	//Generating the header.
        dns = (DNS_HEADER *) dnsBuffer;
        dns->identification     =       id;
        dns->flags.qr           =       0;
        dns->flags.opcode       =       0;
        dns->flags.aa           =       0;
        dns->flags.tc           =       0;
        dns->flags.rd           =       1;
        dns->flags.ra           =       0;
        dns->flags.unused       =       0;
        dns->flags.ad           =       0;
        dns->flags.cd           =       0;
        dns->flags.rcode        =       0;
        dns->num_question_rr    =       htons(0x0001);
        dns->num_ans_rr         =       0;
        dns->num_auth_rr        =       0;
        dns->num_add_rr         =       0;

	//taking out the pointer to generate the question section.
        question = (unsigned char*)(dnsBuffer + sizeof(struct DNS_HEADER));

	//convert the FQDN to Qname.
        returnLen = dns_convert_qname(question, fqdn);
	if(returnLen <0 ) return -1;

        question += returnLen;
        *question = 0;
        question++;

        question_type = (DNS_QUESTION_TYPE*)question;
        question_type->query_type = htons(0x0001);
        question_type->query_class = htons(0x0001);

	//calculate the total length.
	returnLen = returnLen + 1  + sizeof(DNS_HEADER) + sizeof(DNS_QUESTION_TYPE);

        return returnLen;
}

/*
Description:    This function is responisible to convert the FQDN to Qname.
		www.google.com --> 3www6google3com
Input:          buffer for Qname and FQDN string;
OutPut:         -1 for error, length of Qname for success..
*/
int dns_convert_qname(unsigned char* qname, unsigned char* fqdn) {

        int count=0, qnameCount=0, countBeforeDot=0;
        unsigned char*tmp = qname;


        if (!qname || !fqdn) {
                return -1;
        }
	//go through the FQDN strinf, come back and replace '.' with following number of characters.
        for (count=0, qnameCount=1; count < strlen(fqdn); count++, qnameCount++) {
                if (fqdn[count] == '.') {
                        tmp[qnameCount-countBeforeDot-1] = *(unsigned char*)(&countBeforeDot);
                        countBeforeDot =0;
                } else {
                        tmp[qnameCount] = fqdn[count];
                        countBeforeDot++;
                }

        }
	//replace the last '.'
        tmp[qnameCount-countBeforeDot-1] = countBeforeDot;
	//return the Qname length
        return qnameCount;
}


/*
Description:    This function is responisible to skip the query while parsing the DNS Response.
Input:          Query starting pointer and Response length;
OutPut:         NULL for error, Pointer pointing after the Query if success..
*/
unsigned char *dns_skip_query(unsigned char *name_ptr, unsigned int *data_length)
{
	unsigned char rr_type;
	rr_type = *name_ptr;

	while (rr_type <= MAXLABEL)
	{
		name_ptr++;
		(*data_length)--;

		if (rr_type == 0)
		{
			break;
		}
		else
		{
			name_ptr += rr_type;
			(*data_length) -= rr_type;
		}

		if (*data_length <= 0)
		{
			return NULL;
		}

		rr_type = *name_ptr;
	}

	return name_ptr;
}

/*
Description:    This function is responisible for parsing the DNS Response.
Input:          Pointer of Response , Response length, pointer for ipaddress(output) and pointer for ttl(output);
OutPut:         -1 for error, 1 for success.
*/
int dns_parse_response(unsigned char* buffer, int bufferLen, unsigned int *s_addr, unsigned short *ttl)
{
	unsigned int  resolved_address=0;
	unsigned short question_rr=0, answer_rr=0;
        unsigned char *ans_data_ptr=NULL;
        unsigned short ans_data_type=0, ans_data_length=0;
        unsigned char* tmpPtr = NULL;

	DNS_HEADER *dns_header= NULL;
	DNS_RR *dns_rr = NULL;
	
	if (!bufferLen) return -1;
	
	//Parse the DNS header.
	dns_header = (DNS_HEADER*) buffer;
	
	//get the number of quetions and answers in the response.
	question_rr = ntohs(dns_header->num_question_rr);
	answer_rr = ntohs(dns_header->num_ans_rr);
	
	//if there was no answer, return it as "no availability of the DNS", erorr
	if(answer_rr == 0) {
		return -1;
	}
	//got to the query section.
	tmpPtr = (unsigned char*) (buffer + sizeof(DNS_HEADER));
	bufferLen = bufferLen-sizeof(DNS_HEADER);
	//Skip the query part. As this would be the same as we sent earlier in DNS Request
	tmpPtr = dns_skip_query(tmpPtr, &bufferLen);
	
	//if we reached to the end of response. error
	if (NULL == tmpPtr)
		return -1;

	tmpPtr += 4;
	bufferLen -=4;
	
	//parse all the answers present in the query.
	//This function will read the first available ip address and come out.
	while(answer_rr) {
		ans_data_type = *tmpPtr;
		if (ans_data_type > INDIR_MASK) return -1;
		else {
			if (ans_data_type == INDIR_MASK)
			{
				tmpPtr = tmpPtr + 2;
				bufferLen -= 2;
			}
			else
				tmpPtr = dns_skip_query(tmpPtr, &bufferLen);
		}
		if (!tmpPtr) return -1;
	
		dns_rr = (DNS_RR*) tmpPtr;
		ans_data_type = ntohs (dns_rr->rr_type);
		ans_data_length = ntohs (dns_rr->rr_length);
		*ttl = (unsigned short)ntohs(dns_rr->ttl);
				
		if ( (ans_data_type == T_A) && (ans_data_length == 4)) {
			//here is the IP Address, store it and bail out.
			*s_addr = *(unsigned int*)(tmpPtr+sizeof(DNS_RR)-2);
			return 1;
		} else {
			//else go to the next answer.
			tmpPtr = tmpPtr + sizeof(DNS_RR)+ans_data_length;
			bufferLen -= (sizeof(DNS_RR)+ans_data_length);
			answer_rr--;
		}
		 
	}
	//error
	return -1;
}













































