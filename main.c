#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define BUF_SIZE 1500

/*
* This software is licensed under the CC0.
*
* This is a _basic_ DNS Server for educational use.
* It does not prevent invalid packets from crashing
* the server.
*
* To test start the program and issue a DNS request:
*  dig @127.0.0.1 -p 9000 foo.bar.com 
*/


/*
* Masks and constants.
*/

static const uint32_t QR_MASK = 0x8000;
static const uint32_t OPCODE_MASK = 0x7800;
static const uint32_t AA_MASK = 0x0400;
static const uint32_t TC_MASK = 0x0200;
static const uint32_t RD_MASK = 0x0100;
static const uint32_t RA_MASK = 0x8000;
static const uint32_t RCODE_MASK = 0x000F;

/* Response Type */
enum {
  Ok_ResponseType = 0,
  FormatError_ResponseType = 1,
  ServerFailure_ResponseType = 2,
  NameError_ResponseType = 3,
  NotImplemented_ResponseType = 4,
  Refused_ResponseType = 5
};

/* Resource Record Types */
enum {
  A_Resource_RecordType = 1,
  NS_Resource_RecordType = 2,
  CNAME_Resource_RecordType = 5,
  SOA_Resource_RecordType = 6,
  PTR_Resource_RecordType = 12,
  MX_Resource_RecordType = 15,
  TXT_Resource_RecordType = 16,
  AAAA_Resource_RecordType = 28,
  SRV_Resource_RecordType = 33
};

/* Operation Code */
enum {
  QUERY_OperationCode = 0, /* standard query */
  IQUERY_OperationCode = 1, /* inverse query */
  STATUS_OperationCode = 2, /* server status request */
  NOTIFY_OperationCode = 4, /* request zone transfer */
  UPDATE_OperationCode = 5 /* change resource records */
};

/* Response Code */
enum {
  NoError_ResponseCode = 0,
  FormatError_ResponseCode = 1,
  ServerFailure_ResponseCode = 2,
  NameError_ResponseCode = 3
};

/* Query Type */
enum {
  IXFR_QueryType = 251,
  AXFR_QueryType = 252,
  MAILB_QueryType = 253,
  MAILA_QueryType = 254,
  STAR_QueryType = 255
};

/*
* Types.
*/

/* Question Section */
struct Question {
  char *qName;
  uint16_t qType;
  uint16_t qClass;
  struct Question* next; // for linked list
};

/* Data part of a Resource Record */
union ResourceData {
  struct {
    uint8_t txt_data_len;
    char *txt_data;
  } txt_record;
  struct {
    uint8_t addr[4];
  } a_record;
  struct {
    char* MName;
    char* RName;
    uint32_t serial;
    uint32_t refresh;
    uint32_t retry;
    uint32_t expire;
    uint32_t minimum;
  } soa_record;
  struct {
    char *name;
  } name_server_record;
  struct {
    char* name;
  } cname_record;
  struct {
    char *name;
  } ptr_record;
  struct {
    uint16_t preference;
    char *exchange;
  } mx_record;
  struct {
    uint8_t addr[16];
  } aaaa_record;
  struct {
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    char *target;
  } srv_record;
};

/* Resource Record Section */
struct ResourceRecord {
  char *name;
  uint16_t type;
  uint16_t class;
  uint32_t ttl;
  uint16_t rd_length;
  union ResourceData rd_data;
  struct ResourceRecord* next; // for linked list
};

struct Message {
  uint16_t id; /* Identifier */

  /* Flags */
  uint16_t qr; /* Query/Response Flag */
  uint16_t opcode; /* Operation Code */
  uint16_t aa; /* Authoritative Answer Flag */
  uint16_t tc; /* Truncation Flag */
  uint16_t rd; /* Recursion Desired */
  uint16_t ra; /* Recursion Available */
  uint16_t rcode; /* Response Code */

  uint16_t qdCount; /* Question Count */
  uint16_t anCount; /* Answer Record Count */
  uint16_t nsCount; /* Authority Record Count */
  uint16_t arCount; /* Additional Record Count */

  /* At least one question; questions are copied to the response 1:1 */
  struct Question* questions;

  /*
  * Resource records to be send back.
  * Every resource record can be in any of the following places.
  * But every place has a different semantic.
  */
  struct ResourceRecord* answers;
  struct ResourceRecord* authorities;
  struct ResourceRecord* additionals;
};

typedef struct {
  uint8_t addr[4];
  uint32_t ttl;
  char view[20];
} a_record;

a_record* get_A_Record(int16_t * rc, const char domain_name[]){
  // strcasecmp  不区分大小写
  // strcmp  区分大小写
  if (strcmp("a.bar.com", domain_name) == 0)
  {
    *rc = 3;

    a_record* p = calloc(*rc, sizeof(a_record));

    p->ttl = 0x11;
    p->addr[0] = 192;
    p->addr[1] = 168;
    p->addr[2] = 71;
    p->addr[3] = 2;
    strcpy(p->view, "ctc");

    (p + 1)->ttl = 0x12;
    (p + 1)->addr[0] = 192;
    (p + 1)->addr[1] = 168;
    (p + 1)->addr[2] = 71;
    (p + 1)->addr[3] = 3;
    strcpy((p + 1)->view, "ctc");

    (p + 2)->ttl = 0x13;
    (p + 2)->addr[0] = 192;
    (p + 2)->addr[1] = 168;
    (p + 2)->addr[2] = 71;
    (p + 2)->addr[3] = 111;
    strcpy((p + 2)->view, "cmc");

    return p;
  } else {

    *rc = -1;

    return NULL;
  }
}

typedef struct {
  uint8_t addr[16];
  uint32_t ttl;
  char view[20];
} aaaa_record;

aaaa_record* get_AAAA_Record(int16_t * rc, const char domain_name[])
{
  if (strcmp("aaaa.bar.com", domain_name) == 0)
  {
    *rc = 2;

    aaaa_record* p = calloc(*rc, sizeof(aaaa_record));

    p->ttl = 0x14;
    strcpy(p->view, "ctc");
    p->addr[0] = 0xfe;
    p->addr[1] = 0x80;
    p->addr[2] = 0x00;
    p->addr[3] = 0x00;
    p->addr[4] = 0x00;
    p->addr[5] = 0x00;
    p->addr[6] = 0x00;
    p->addr[7] = 0x00;
    p->addr[8] = 0x00;
    p->addr[9] = 0x00;
    p->addr[10] = 0x00;
    p->addr[11] = 0x00;
    p->addr[12] = 0x00;
    p->addr[13] = 0x00;
    p->addr[14] = 0x00;
    p->addr[15] = 0x01;

    (p + 1)->ttl = 0x15;
    strcpy((p + 1)->view, "ctc");
    (p + 1)->addr[0] = 0xfe;
    (p + 1)->addr[1] = 0x80;
    (p + 1)->addr[2] = 0x00;
    (p + 1)->addr[3] = 0x00;
    (p + 1)->addr[4] = 0x00;
    (p + 1)->addr[5] = 0x00;
    (p + 1)->addr[6] = 0x00;
    (p + 1)->addr[7] = 0x00;
    (p + 1)->addr[8] = 0x00;
    (p + 1)->addr[9] = 0x00;
    (p + 1)->addr[10] = 0x00;
    (p + 1)->addr[11] = 0x00;
    (p + 1)->addr[12] = 0x00;
    (p + 1)->addr[13] = 0x00;
    (p + 1)->addr[14] = 0x00;
    (p + 1)->addr[15] = 0x02;

    return p;
  }
  else
  {
    *rc = -1;
    return NULL;
  }
}

typedef struct {
  uint8_t txt_data_len;
  char txt_data[255];
  uint32_t ttl;
  char view[20];
} txt_record;

txt_record* get_TXT_Record(int16_t * rc, const char domain_name[])
{
  if (strcmp("txt.bar.com", domain_name) == 0)
  {
    *rc = 2;

    txt_record* p = calloc(*rc, sizeof(txt_record));

    p->ttl = 0x11;
    strcpy(p->view, "ctc");
    strcpy(p->txt_data, "12345");
    p->txt_data_len = strlen(p->txt_data);
    
    (p + 1)->ttl = 0x12;
    strcpy((p + 1)->view, "ctc");
    strcpy((p + 1)->txt_data, "54321");
    (p + 1)->txt_data_len = strlen((p + 1)->txt_data);

    return p;
  }
  else
  {
    *rc = -1;
    return NULL;
  }
}

typedef struct {
  uint8_t name_len;
  char name[253];  //  每一级域名长度的限制是63个字符, 域名总长度不能超过253个字符
  uint32_t ttl;
  char view[20];
} cname_record;

cname_record* get_CNAME_Record(int16_t * rc, const char domain_name[])
{
  if (strcmp("cname.bar.com", domain_name) == 0)
  {
    *rc = 1;

    cname_record* p = calloc(*rc, sizeof(cname_record));

    p->ttl = 0x13;
    strcpy(p->view, "ctc");
    strcpy(p->name, "abc.efg.com");
    p->name_len = strlen(p->name);

    return p;
  }
  else
  {
    *rc = -1;
    return NULL;
  }
}

typedef struct {
  uint8_t exchange_len;
  char exchange[253];  //  每一级域名长度的限制是63个字符, 域名总长度不能超过253个字符
  uint16_t preference;
  uint32_t ttl;
  char view[20];
} mx_record;

mx_record* get_MX_Record(int16_t * rc, const char domain_name[])
{
  if (strcmp("mx.bar.com", domain_name) == 0)
  {
    *rc = 2;

    mx_record* p = calloc(*rc, sizeof(mx_record));

    p->ttl = 0x14;
    p->preference = 0x0a;
    strcpy(p->view, "ctc");
    strcpy(p->exchange, "aaa.efg.com");
    p->exchange_len = strlen(p->exchange);

    (p + 1)->ttl = 0x14;
    (p + 1)->preference = 0x0a;
    strcpy((p + 1)->view, "ctc");
    strcpy((p + 1)->exchange, "bbb.efg.com");
    (p + 1)->exchange_len = strlen(p->exchange);

    return p;
  }
  else
  {
    *rc = -1;
    return NULL;
  }
}

typedef struct {
  uint8_t MName_len;
  char MName[253];  //  每一级域名长度的限制是63个字符, 域名总长度不能超过253个字符
  uint8_t RName_len;
  char RName[253];  //  每一级域名长度的限制是63个字符, 域名总长度不能超过253个字符
  uint32_t serial;
  uint32_t refresh;
  uint32_t retry;
  uint32_t expire;
  uint32_t minimum;
  uint32_t ttl;
  char view[20];
} soa_record;

soa_record* get_SOA_Record(int16_t *rc, const char domain_name[])
{
  if (strcmp("soa.bar.com", domain_name) == 0)
  {
    *rc = 1;

    soa_record* p = calloc(*rc, sizeof(soa_record));

    strcpy(p->MName, "ns.xxx.com");
    p->MName_len = strlen(p->MName);

    strcpy(p->RName, "admin.xxx.com");
    p->RName_len = strlen(p->RName);

    p->serial = 0x01;
    p->refresh = 0x02;
    p->retry = 0x03;
    p->expire = 0x04;
    p->minimum = 0x05;

    p->ttl = 0x14;
    strcpy(p->view, "ctc");

    return p;
  }
  else
  {
    *rc = -1;
    return NULL;
  }
}

typedef struct {
  uint8_t name_len;
  char name[253];  //  每一级域名长度的限制是63个字符, 域名总长度不能超过253个字符
  uint32_t ttl;
  char view[20];
} ns_record;

ns_record* get_NS_Record(int16_t *rc, const char domain_name[])
{
  if (strcmp("bar.com", domain_name) == 0)
  {
    *rc = 2;

    ns_record* p = calloc(*rc, sizeof(ns_record));

    p->ttl = 0x13;
    strcpy(p->view, "ctc");
    strcpy(p->name, "ns1.abc.com");
    p->name_len = strlen(p->name);

    (p + 1)->ttl = 0x13;
    strcpy((p + 1)->view, "ctc");
    strcpy((p + 1)->name, "ns2.abc.com");
    (p + 1)->name_len = strlen((p + 1)->name);

    return p;
  }
  else
  {
    *rc = -1;
    return NULL;
  }
}

/*
* Debugging functions.
*/

void print_hex(uint8_t* buf, size_t len)
{
  int i;
  printf("%zu bytes:\n", len);
  for(i = 0; i < len; ++i)
    printf("%02x ", buf[i]);
  printf("\n");
}

void print_resource_record(struct ResourceRecord* rr)
{
  int i;
  while (rr)
  {
    printf("  ResourceRecord { name '%s', type %u, class %u, ttl %u, rd_length %u, ",
        rr->name,
        rr->type,
        rr->class,
        rr->ttl,
        rr->rd_length
   );

    union ResourceData *rd = &rr->rd_data;
    switch (rr->type)
    {
      case A_Resource_RecordType:
        printf("Address Resource Record { address ");

        for(i = 0; i < 4; ++i)
          printf("%s%u", (i ? "." : ""), rd->a_record.addr[i]);

        printf(" }");
        break;
      case NS_Resource_RecordType:
        printf("Name Server Resource Record { name %s }",
          rd->name_server_record.name
       );
        break;
      case CNAME_Resource_RecordType:
        printf("Canonical Name Resource Record { name %s }",
          rd->cname_record.name
       );
        break;
      case SOA_Resource_RecordType:
        printf("SOA { MName '%s', RName '%s', serial %u, refresh %u, retry %u, expire %u, minimum %u }",
          rd->soa_record.MName,
          rd->soa_record.RName,
          rd->soa_record.serial,
          rd->soa_record.refresh,
          rd->soa_record.retry,
          rd->soa_record.expire,
          rd->soa_record.minimum
       );
        break;
      case PTR_Resource_RecordType:
        printf("Pointer Resource Record { name '%s' }",
          rd->ptr_record.name
       );
        break;
      case MX_Resource_RecordType:
        printf("Mail Exchange Record { preference %u, exchange '%s' }",
          rd->mx_record.preference,
          rd->mx_record.exchange
       );
        break;
      case TXT_Resource_RecordType:
        printf("Text Resource Record { txt_data '%s' }",
          rd->txt_record.txt_data
       );
        break;
      case AAAA_Resource_RecordType:
        printf("AAAA Resource Record { address ");

        for(i = 0; i < 16; ++i)
          printf("%s%02x", (i ? ":" : ""), rd->aaaa_record.addr[i]);

        printf(" }");
        break;
      default:
        printf("Unknown Resource Record { ??? }");
    }
    printf("}\n");

    printf("🍁--3---rr->next: %p\n", rr->next);

    rr = rr->next;
  }
}

void print_query(struct Message* msg)
{
  struct Question* q;

  printf("QUERY { ID: %02x", msg->id);
  printf(". FIELDS: [ QR: %u, OpCode: %u ]", msg->qr, msg->opcode);
  printf(", QDcount: %u", msg->qdCount);
  printf(", ANcount: %u", msg->anCount);
  printf(", NScount: %u", msg->nsCount);
  printf(", ARcount: %u,\n", msg->arCount);

  q = msg->questions;
  while (q)
  {
    printf("  Question { qName '%s', qType %u, qClass %u }\n",
      q->qName,
      q->qType,
      q->qClass
   );
    q = q->next;
  }

  print_resource_record(msg->answers);
  print_resource_record(msg->authorities);
  print_resource_record(msg->additionals);

  printf("}\n");
}

/*
* Basic memory operations.
*/

size_t get16bits(const uint8_t** buffer)
{
  uint16_t value;

  memcpy(&value, *buffer, 2);
  *buffer += 2;

  return ntohs(value);
}

void put8bits(uint8_t** buffer, uint8_t value)
{
  memcpy(*buffer, &value, 1);
  *buffer += 1;
}

void put16bits(uint8_t** buffer, uint16_t value)
{
  value = htons(value);
  memcpy(*buffer, &value, 2);
  *buffer += 2;
}

void put32bits(uint8_t** buffer, uint32_t value)
{
  value = htonl(value);
  memcpy(*buffer, &value, 4);
  *buffer += 4;
}

/*
* Deconding/Encoding functions.
*/

// 3foo3bar3com0 => foo.bar.com
char* decode_domain_name(const uint8_t** buffer)
{
  char name[256];
  const uint8_t* buf = *buffer;
  int j = 0;
  int i = 0;

  while (buf[i] != 0)
  {
    //if (i >= buflen || i > sizeof(name))
    //  return NULL;

    if (i != 0)
    {
      name[j] = '.';
      j += 1;
    }

    int len = buf[i];
    i += 1;

    memcpy(name+j, buf+i, len);
    i += len;
    j += len;
  }

  name[j] = '\0';

  *buffer += i + 1; //also jump over the last 0

  return strdup(name);
}

// foo.bar.com => 3foo3bar3com0
void encode_domain_name(uint8_t** buffer, const char* domain)
{
  uint8_t* buf = *buffer;
  const char* beg = domain;
  const char* pos;
  int len = 0;
  int i = 0;

  while ((pos = strchr(beg, '.')))
  {
    len = pos - beg;
    buf[i] = len;
    i += 1;
    memcpy(buf+i, beg, len);
    i += len;

    beg = pos + 1;
  }

  len = strlen(domain) - (beg - domain);

  buf[i] = len;
  i += 1;

  memcpy(buf + i, beg, len);
  i += len;

  buf[i] = 0;
  i += 1;

  *buffer += i;
}


void decode_header(struct Message* msg, const uint8_t** buffer)
{
  msg->id = get16bits(buffer);

  uint32_t fields = get16bits(buffer);
  msg->qr = (fields & QR_MASK) >> 15;
  msg->opcode = (fields & OPCODE_MASK) >> 11;
  msg->aa = (fields & AA_MASK) >> 10;
  msg->tc = (fields & TC_MASK) >> 9;
  msg->rd = (fields & RD_MASK) >> 8;
  msg->ra = (fields & RA_MASK) >> 7;
  msg->rcode = (fields & RCODE_MASK) >> 0;

  msg->qdCount = get16bits(buffer);
  msg->anCount = get16bits(buffer);
  msg->nsCount = get16bits(buffer);
  msg->arCount = get16bits(buffer);
}

void encode_header(struct Message* msg, uint8_t** buffer)
{
  put16bits(buffer, msg->id);

  int fields = 0;
  fields |= (msg->qr << 15) & QR_MASK;
  fields |= (msg->rcode << 0) & RCODE_MASK;
  // TODO: insert the rest of the fields
  put16bits(buffer, fields);

  put16bits(buffer, msg->qdCount);
  put16bits(buffer, msg->anCount);
  put16bits(buffer, msg->nsCount);
  put16bits(buffer, msg->arCount);
}

int decode_msg(struct Message* msg, const uint8_t* buffer, int size)
{
  int i;

  decode_header(msg, &buffer);

  if (msg->anCount != 0 || msg->nsCount != 0)
  {
    printf("Only questions expected!\n");
    return -1;
  }

  // parse questions
  uint32_t qcount = msg->qdCount;
  for (i = 0; i < qcount; ++i)
  {
    struct Question* q = malloc(sizeof(struct Question));

    q->qName = decode_domain_name(&buffer);
    q->qType = get16bits(&buffer);
    q->qClass = get16bits(&buffer);

    // prepend question to questions list
    q->next = msg->questions;
    msg->questions = q;
  }

  // We do not expect any resource records to parse here.

  return 0;
}

// For every question in the message add a appropiate resource record
// in either section 'answers', 'authorities' or 'additionals'.
void resolver_process(struct Message* msg)
{
  // struct ResourceRecord* beg;
  struct ResourceRecord* rr;
  struct Question* q;
  int16_t rc;

  // leave most values intact for response
  msg->qr = 1; // this is a response
  msg->aa = 1; // this server is authoritative
  msg->ra = 0; // no recursion available
  msg->rcode = Ok_ResponseType;

  // should already be 0
  msg->anCount = 0;
  msg->nsCount = 0;
  msg->arCount = 0;

  // for every question append resource records
  q = msg->questions;
  
  // uint16_t j = 0;

  while (q)
  {
    printf("Query for '%s'\n", q->qName);
 
    // We only can only answer two question types so far
    // and the answer (resource records) will be all put
    // into the answers list.
    // This behavior is probably non-standard!
    switch (q->qType)
    {
      case A_Resource_RecordType:
      {
        a_record* record_a_arr = get_A_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          // struct ResourceRecord* rr_new = malloc(sizeof(struct ResourceRecord));
          // memset(rr_new, 0, sizeof(struct ResourceRecord));
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_a_arr + i)->ttl; 

          rr_new->rd_length = 4;

          memcpy(rr_new->rd_data.a_record.addr, (record_a_arr + i)->addr, 4);

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }
        }

        free(record_a_arr);

        record_a_arr = NULL;

        break;
      }
 
      case AAAA_Resource_RecordType:
      {
        aaaa_record* record_aaaa_arr = get_AAAA_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_aaaa_arr + i)->ttl; 

          rr_new->rd_length = 16;

          memcpy(rr_new->rd_data.aaaa_record.addr, (record_aaaa_arr + i)->addr, 16);

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }

        }

        free(record_aaaa_arr);
        record_aaaa_arr = NULL;

        break;
      }
      case TXT_Resource_RecordType:
      {
        txt_record* record_txt_arr = get_TXT_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_txt_arr + i)->ttl; 

          rr_new->rd_data.txt_record.txt_data = (char*)calloc((record_txt_arr + i)->txt_data_len, sizeof(char));
          memcpy(rr_new->rd_data.txt_record.txt_data, (record_txt_arr + i)->txt_data, (record_txt_arr + i)->txt_data_len);

          rr_new->rd_data.txt_record.txt_data_len = (record_txt_arr + i)->txt_data_len;

          rr_new->rd_length = (record_txt_arr + i)->txt_data_len + 1; //  1: length

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }

        }

        free(record_txt_arr);
        record_txt_arr = NULL;

        break;
      }
      case CNAME_Resource_RecordType:
      {
        cname_record* record_cname_arr = get_CNAME_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_cname_arr + i)->ttl; 

          rr_new->rd_data.cname_record.name = (char*)calloc((record_cname_arr + i)->name_len, sizeof(char));
          memcpy(rr_new->rd_data.cname_record.name, (record_cname_arr + i)->name, (record_cname_arr + i)->name_len);

          rr_new->rd_length = (record_cname_arr + i)->name_len + 2; //  1: name length; 1: 0x00 end

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }

        }

        free(record_cname_arr);
        record_cname_arr = NULL;

        break;
      }
      case MX_Resource_RecordType:
      {
        mx_record* record_arr = get_MX_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_arr + i)->ttl; 

          rr_new->rd_data.mx_record.preference = (record_arr + i)->preference; 

          rr_new->rd_data.mx_record.exchange = (char*)calloc((record_arr + i)->exchange_len, sizeof(char));
          memcpy(rr_new->rd_data.mx_record.exchange, (record_arr + i)->exchange, (record_arr + i)->exchange_len);

          rr_new->rd_length = (record_arr + i)->exchange_len + 4; // 1: name length; 1: 0x00 end 2: preference(uint16_t)

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }

        }

        free(record_arr);
        record_arr = NULL;

        break;
      }
      case SOA_Resource_RecordType:
      {
        soa_record* record_arr = get_SOA_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_arr + i)->ttl; 

          rr_new->rd_data.soa_record.serial = (record_arr + i)->serial;
          rr_new->rd_data.soa_record.refresh = (record_arr + i)->refresh;
          rr_new->rd_data.soa_record.retry = (record_arr + i)->retry;
          rr_new->rd_data.soa_record.expire = (record_arr + i)->expire;
          rr_new->rd_data.soa_record.minimum = (record_arr + i)->minimum;

          rr_new->rd_data.soa_record.MName = (char*)calloc((record_arr + i)->MName_len, sizeof(char));
          memcpy(rr_new->rd_data.soa_record.MName, (record_arr + i)->MName, (record_arr + i)->MName_len);

          rr_new->rd_data.soa_record.RName = (char*)calloc((record_arr + i)->RName_len, sizeof(char));
          memcpy(rr_new->rd_data.soa_record.RName, (record_arr + i)->RName, (record_arr + i)->RName_len);

          rr_new->rd_length = (record_arr + i)->MName_len + (record_arr + i)->RName_len + 4 + 4 * 5; // 4: uint32_t (ttl); 4 * 5: uint32_t * 5;

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }

        }

        free(record_arr);
        record_arr = NULL;

        break;
      }
      case NS_Resource_RecordType:
      {
        ns_record* record_arr = get_NS_Record(&rc, q->qName);

        if (rc <= 0)
        {
          goto next;
        }

        msg->anCount = rc;

        for (int i = 0; i < rc; i++)
        {
          struct ResourceRecord* rr_new = (struct ResourceRecord*)calloc(1, sizeof(struct ResourceRecord));

          rr_new->name = strdup(q->qName);
          rr_new->type = q->qType;
          rr_new->class = q->qClass;

          rr_new->ttl = (record_arr + i)->ttl; 

          rr_new->rd_data.name_server_record.name = (char*)calloc((record_arr + i)->name_len, sizeof(char));
          memcpy(rr_new->rd_data.name_server_record.name, (record_arr + i)->name, (record_arr + i)->name_len);

          rr_new->rd_length = (record_arr + i)->name_len + 2; // 1: name length; 1: 0x00 end

          rr_new->next = NULL;

          if(i == 0){
            rr = rr_new;
            msg->answers = rr;
          } else {
            rr->next = rr_new;
            rr = rr_new;
          }

        }

        free(record_arr);
        record_arr = NULL;

        break;
      }

      /*      
      case PTR_Resource_RecordType:
      */
      default:
        free(rr);
        msg->rcode = NotImplemented_ResponseType;
        printf("Cannot answer question of type %d.\n", q->qType);
        goto next;
    }

    // msg->anCount++;

    // // prepend resource record to answers list
    // beg = msg->answers;
    // msg->answers = rr;
    // rr->next = beg;

    // jump here to omit question
    next:

    printf("\n🏀---------- next");

    // process next question
    q = q->next;
  }

  printf("🍁 --------- msg->anCount: %d\n", msg->anCount);
}

/* @return 0 upon failure, 1 upon success */
int encode_resource_records(struct ResourceRecord* rr, uint8_t** buffer)
{
  int tmp_Count = 0;

  int i;
  while (rr)
  {
    // Answer questions by attaching resource sections.

    // encode_domain_name(buffer, rr->name);
    
    //  compress answer name (same as query name)
    put16bits(buffer, 0xC00C);

    put16bits(buffer, rr->type);
    put16bits(buffer, rr->class);
    put32bits(buffer, rr->ttl);
    put16bits(buffer, rr->rd_length);

    switch (rr->type)
    {
      // case A_Resource_RecordType:
      //   for(i = 0; i < 4; ++i)
      //     put8bits(buffer, rr->rd_data.a_record.addr[i]);
      //   break;

      case A_Resource_RecordType:
        for(i = 0; i < 4; ++i)
          put8bits(buffer, rr->rd_data.a_record.addr[i]);

        printf("\n🍎-----addr: %d.%d.%d.%d\n", rr->rd_data.a_record.addr[0], rr->rd_data.a_record.addr[1], rr->rd_data.a_record.addr[2], rr->rd_data.a_record.addr[3]);

        
        break;
      case AAAA_Resource_RecordType:
        for(i = 0; i < 16; ++i)
          put8bits(buffer, rr->rd_data.aaaa_record.addr[i]);
        break;
      case TXT_Resource_RecordType:
        put8bits(buffer, rr->rd_data.txt_record.txt_data_len);
        for(i = 0; i < rr->rd_data.txt_record.txt_data_len; i++)
          put8bits(buffer, rr->rd_data.txt_record.txt_data[i]);
        break;
      case CNAME_Resource_RecordType:
        encode_domain_name(buffer, rr->rd_data.cname_record.name);
        break;
      case MX_Resource_RecordType:
        put16bits(buffer, rr->rd_data.mx_record.preference);
        encode_domain_name(buffer, rr->rd_data.mx_record.exchange);
        break;
      case SOA_Resource_RecordType:
        encode_domain_name(buffer, rr->rd_data.soa_record.MName);
        encode_domain_name(buffer, rr->rd_data.soa_record.RName);
        put32bits(buffer, rr->rd_data.soa_record.serial);
        put32bits(buffer, rr->rd_data.soa_record.refresh);
        put32bits(buffer, rr->rd_data.soa_record.retry);
        put32bits(buffer, rr->rd_data.soa_record.expire);
        put32bits(buffer, rr->rd_data.soa_record.minimum);
        break;
      case NS_Resource_RecordType:
        encode_domain_name(buffer, rr->rd_data.name_server_record.name);
        break;

      default:
        fprintf(stderr, "Unknown type %u. => Ignore resource record.\n", rr->type);
      return 1;
    }

    rr = rr->next;

    tmp_Count++;
  }

  printf("\n🍁--------------tmp_Count: %d\n", tmp_Count);

  return 0;
}

/* @return 0 upon failure, 1 upon success */
int encode_msg(struct Message* msg, uint8_t** buffer)
{
  struct Question* q;
  int rc;

  encode_header(msg, buffer);

  q = msg->questions;
  while (q)
  {
    encode_domain_name(buffer, q->qName);
    put16bits(buffer, q->qType);
    put16bits(buffer, q->qClass);

    q = q->next;
  }

  rc = 0;
  rc |= encode_resource_records(msg->answers, buffer);
  rc |= encode_resource_records(msg->authorities, buffer);
  rc |= encode_resource_records(msg->additionals, buffer);

  return rc;
}

void free_resource_records(struct ResourceRecord* rr)
{
  struct ResourceRecord* next;

  while (rr) {

    switch(rr->type)
    {
      case TXT_Resource_RecordType:
      {
        free(rr->rd_data.txt_record.txt_data);
        rr->rd_data.txt_record.txt_data = NULL;
        break;
      }
      case CNAME_Resource_RecordType:
      {
        free(rr->rd_data.cname_record.name);
        rr->rd_data.cname_record.name = NULL;
        break;
      }
      case MX_Resource_RecordType:
      {
        free(rr->rd_data.mx_record.exchange);
        rr->rd_data.mx_record.exchange = NULL;
        break;
      }
      case SOA_Resource_RecordType:
      {
        free(rr->rd_data.soa_record.MName);
        rr->rd_data.soa_record.MName = NULL;

        free(rr->rd_data.soa_record.RName);
        rr->rd_data.soa_record.RName = NULL;
        break;
      }
      case NS_Resource_RecordType:
      {
        free(rr->rd_data.name_server_record.name);
        rr->rd_data.name_server_record.name = NULL;
        break;
      }
      default:
        printf("\n no need specified inner free \n");
    }
 
    free(rr->name);
    next = rr->next;
    free(rr);
    rr = next;
  }
}

void free_questions(struct Question* qq)
{
  struct Question* next;

  while (qq) {
    free(qq->qName);
    next = qq->next;
    free(qq);
    qq = next;
  }
}

int main()
{
  // buffer for input/output binary packet
  uint8_t buffer[BUF_SIZE];
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in addr;
  int nbytes, rc;
  int sock;
  int port = 9000;

  struct Message msg;
  memset(&msg, 0, sizeof(struct Message));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  sock = socket(AF_INET, SOCK_DGRAM, 0);

  rc = bind(sock, (struct sockaddr*) &addr, addr_len);

  if (rc != 0)
  {
    printf("Could not bind: %s\n", strerror(errno));
    return 1;
  }

  printf("Listening on port %u.\n", port);

  while (1)
  {
    free_questions(msg.questions);
    free_resource_records(msg.answers);
    free_resource_records(msg.authorities);
    free_resource_records(msg.additionals);
    memset(&msg, 0, sizeof(struct Message));

    nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &client_addr, &addr_len);

    if (decode_msg(&msg, buffer, nbytes) != 0) {
      continue;
    }

    /* Print query */
    print_query(&msg);

    resolver_process(&msg);

    /* Print response */
    print_query(&msg);

    uint8_t *p = buffer;
    if (encode_msg(&msg, &p) != 0) {
      continue;
    }

    int buflen = p - buffer;
    sendto(sock, buffer, buflen, 0, (struct sockaddr*) &client_addr, addr_len);
  }
}







// struct aaa_record{
//   uint8_t addr[4];
//   uint32_t ttl;
//   char *view;
// };

// typedef struct node{
//   a_record data;
//   struct node *next;
// } Node;

// a_record record_arr[] = {
// {
//   {192, 168, 1, 1},
//   0x11,
//   "hh"
// },
// {
//   {192, 168, 1, 2},
//   0x10,
//   "hh"
//   },
// }; 

// a_record* get_My_A_Record(int* num,  const char domain_name[]){

//   a_record record_arr[] = {
//   {
//     {192, 168, 1, 1},
//     0x11,
//     "hh"
//   },
//   {
//     {192, 168, 1, 2},
//     0x10,
//     "hh"
//     },
//   }; 

//   return record_arr;
// }











/*
        //  新建
         Node *head = get_My_A_Record2(&t1, q->qName);
         Node *cur = head;
         Node *head2 = head;
         
         while(cur->next != NULL){
           cur = cur -> next;
           printf("\n 遍历 ---ttl: %d; view: %s; ip: %d.%d.%d.%d\n", cur -> data.ttl, cur -> data.view, cur -> data.addr[0], cur -> data.addr[1], cur -> data.addr[2], cur -> data.addr[3]);
         }


         //  释放
        while(head != NULL){
          cur = head->next;

          // free(&((*head).data.view));

          // free(&((*head).data.view));
          head->data.view = NULL;
          free(head);
          head = cur;
        }

         cur = head2;
         while(cur->next != NULL){
           cur = cur -> next;
           printf("\n 释放后 ---ttl: %d; view: %s; ip: %d.%d.%d.%d\n", cur -> data.ttl, cur -> data.view, cur -> data.addr[0], cur -> data.addr[1], cur -> data.addr[2], cur -> data.addr[3]);
         }
*/

/*
         for (int i = 0; i < 2; i++)
         {
           struct ResourceRecord* rr_new = malloc(sizeof(struct ResourceRecord));
           memset(rr_new, 0, sizeof(struct ResourceRecord));
      

           rr_new->name = strdup(q->qName);
           rr_new->type = q->qType;
           rr_new->class = q->qClass;

           rr_new->ttl = record_arr[i].ttl; 

           rr_new->rd_length = 4;

           rr_new->rd_data.a_record.addr[0] = record_arr[i].addr[0];
           rr_new->rd_data.a_record.addr[1] = record_arr[i].addr[1];
           rr_new->rd_data.a_record.addr[2] = record_arr[i].addr[2];
           rr_new->rd_data.a_record.addr[3] = record_arr[i].addr[3];

           rr_new->next = NULL;

           if(i == 0){
             rr = rr_new;
             msg->answers = rr;
           }

           if(i > 0){
             rr->next = rr_new;
             rr = rr_new;
           }

         }

        
        msg->anCount = 2;

        rc = 2;

        if (rc < 0)
        {
          free(rr->name);
          free(rr);
          goto next;
        }
        */





// int get_Multi_A_Record(struct ResourceRecord** rr, struct Question* q, const char domain_name[])
// {

//   if (strcmp("foo.bar.com", domain_name) == 0)
//   {
//     a_record record_arr[] = {
//       {
//         {192, 168, 1, 1},
//         0x11,
//         "hh"
//       },
//       {
//         {192, 168, 1, 2},
//         0x10,
//         "hh"
//       },
//     }; 

//     int length = sizeof(record_arr)/sizeof(record_arr[0]);
 

//     struct ResourceRecord* rr_new;
//     rr_new = *rr;

//     for (int i = 0; i < length; i++)
//     {
// /*
//       if(i == 0){
//         rr_new->name = strdup(q->qName);
//         rr_new->type = q->qType;
//         rr_new->class = q->qClass;

//         rr_new->ttl = record_arr[i].ttl; 

//         rr_new->rd_length = 4;

//         rr_new->rd_data.a_record.addr[0] = record_arr[i].addr[0];
//         rr_new->rd_data.a_record.addr[1] = record_arr[i].addr[1];
//         rr_new->rd_data.a_record.addr[2] = record_arr[i].addr[2];
//         rr_new->rd_data.a_record.addr[3] = record_arr[i].addr[3];

//         rr_new->next = NULL;

//       } else
//       {

//         rr_new = malloc(sizeof(struct ResourceRecord));
//         memset(rr_new, 0, sizeof(struct ResourceRecord));

//         rr_new->name = strdup(q->qName);
//         rr_new->type = q->qType;
//         rr_new->class = q->qClass;

//         rr_new->ttl = record_arr[i].ttl; 

//         rr_new->rd_length = 4;

//         rr_new->rd_data.a_record.addr[0] = record_arr[i].addr[0];
//         rr_new->rd_data.a_record.addr[1] = record_arr[i].addr[1];
//         rr_new->rd_data.a_record.addr[2] = record_arr[i].addr[2];
//         rr_new->rd_data.a_record.addr[3] = record_arr[i].addr[3];

//         rr_new->next = NULL;

//         rr->next = rr_new;

//         rr = rr_new;
//       }
//       */

      
//       rr_new = malloc(sizeof(struct ResourceRecord));
//       memset(rr_new, 0, sizeof(struct ResourceRecord));
      

//       rr_new->name = strdup(q->qName);
//       rr_new->type = q->qType;
//       rr_new->class = q->qClass;

//       rr_new->ttl = record_arr[i].ttl; 

//       rr_new->rd_length = 4;

//       rr_new->rd_data.a_record.addr[0] = record_arr[i].addr[0];
//       rr_new->rd_data.a_record.addr[1] = record_arr[i].addr[1];
//       rr_new->rd_data.a_record.addr[2] = record_arr[i].addr[2];
//       rr_new->rd_data.a_record.addr[3] = record_arr[i].addr[3];

//       rr_new->next = NULL;

//       if(i > 0){
//         (*rr)->next = rr_new;
//         *rr = rr_new;
//       }


//     }

// /*   
//     for (int i = 0; i < length; i++)
//     {

//       if(i == 0){
//         rr->name = strdup(q->qName);
//         rr->type = q->qType;
//         rr->class = q->qClass;

//         rr->ttl = record_arr[i].ttl; 

//         rr->rd_length = 4;

//         rr->rd_data.a_record.addr[0] = record_arr[i].addr[0];
//         rr->rd_data.a_record.addr[1] = record_arr[i].addr[1];
//         rr->rd_data.a_record.addr[2] = record_arr[i].addr[2];
//         rr->rd_data.a_record.addr[3] = record_arr[i].addr[3];

//         rr->next = NULL;

//       } else
//       {
//         struct ResourceRecord* rr2;
//         rr2 = malloc(sizeof(struct ResourceRecord));
//         memset(rr2, 0, sizeof(struct ResourceRecord));

//         rr2->name = strdup(q->qName);
//         rr2->type = q->qType;
//         rr2->class = q->qClass;

//         rr2->ttl = record_arr[i].ttl; 

//         rr2->rd_length = 4;

//         rr2->rd_data.a_record.addr[0] = record_arr[i].addr[0];
//         rr2->rd_data.a_record.addr[1] = record_arr[i].addr[1];
//         rr2->rd_data.a_record.addr[2] = record_arr[i].addr[2];
//         rr2->rd_data.a_record.addr[3] = record_arr[i].addr[3];

//         rr2->next = NULL;

//         rr->next = rr2;

//         rr = rr2;
//       }

//     }
// */
//     printf("\n-------length: %d-----\n", length);

//     return length;
//   }
//   else
//   {
//     return -1;
//   }
// }


// Node* get_My_A_Record2(int* num,  const char domain_name[]){

//   a_record record_arr[] = {
//   {
//     {192, 168, 1, 1},
//     0x11,
//     "hh"
//   },
//   {
//     {192, 168, 1, 2},
//     0x10,
//     "hh"
//     },
//   }; 

//   int length = sizeof(record_arr)/sizeof(record_arr[0]);

//   Node *head = (Node *)malloc(sizeof(Node));
//   if(head == NULL){
//     exit(-1);
//   }
//   head -> next = NULL;

//     // while(-1 != num)
//   for (int i = 0; i < length; i++)
//   {
//     Node *node = (Node*)malloc(sizeof(Node));

//     node -> data.ttl = record_arr[i].ttl;
//     // node -> data.view = record_arr[i].view;
//     node -> data.addr[0] = record_arr[i].addr[0];
//     node -> data.addr[1] = record_arr[i].addr[1];
//     node -> data.addr[2] = record_arr[i].addr[2];
//     node -> data.addr[3] = record_arr[i].addr[3];

//     node -> next = head -> next;
//     head -> next = node;
//   }

//   return head;
// }



// int get_A_Record(uint8_t addr[4], const char domain_name[])
// {
//   if (strcmp("foo.bar.com", domain_name) == 0)
//   {
//     addr[0] = 192;
//     addr[1] = 168;
//     addr[2] = 1;
//     addr[3] = 1;
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }



// int get_AAAA_Record(uint8_t addr[16], const char domain_name[])
// {
//   if (strcmp("foo.bar.com", domain_name) == 0)
//   {
//     addr[0] = 0xfe;
//     addr[1] = 0x80;
//     addr[2] = 0x00;
//     addr[3] = 0x00;
//     addr[4] = 0x00;
//     addr[5] = 0x00;
//     addr[6] = 0x00;
//     addr[7] = 0x00;
//     addr[8] = 0x00;
//     addr[9] = 0x00;
//     addr[10] = 0x00;
//     addr[11] = 0x00;
//     addr[12] = 0x00;
//     addr[13] = 0x00;
//     addr[14] = 0x00;
//     addr[15] = 0x01;
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }


// int get_TXT_Record(char **addr, const char domain_name[])
// {
//   if (strcmp("txt.bar.com", domain_name) == 0)
//   {
//     *addr = "abcdefg";
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }

// int get_CNAME_Record(char **name, const char domain_name[])
// {
//   if (strcmp("cname.bar.com", domain_name) == 0)
//   {
//     *name = "abc.efg.com";
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }

// int get_MX_Record(char **exchange, const char domain_name[])
// {
//   if (strcmp("mx.bar.com", domain_name) == 0)
//   {
//     *exchange = "abc.efg.com";
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }


// int get_SOA_Record(char **MName, char **RName,const char domain_name[])
// {
//   if (strcmp("soa.bar.com", domain_name) == 0)
//   {
//     *MName = "ns.xxx.com";
//     *RName = "admin.xxx.com";
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }

// int get_NS_Record(char **name, const char domain_name[])
// {
//   if (strcmp("bar.com", domain_name) == 0)
//   {
//     *name = "ns1.abc.com";
//     return 0;
//   }
//   else
//   {
//     return -1;
//   }
// }