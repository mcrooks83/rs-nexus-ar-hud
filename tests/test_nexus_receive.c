#include "nexus_receive.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static struct nexus_receive r;
int main(void)
{
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"{",1,NEXUS_TEXT,4)==NEXUS_RX_MORE);
    assert(nexus_receive_feed(&r,"\"a\":",4,NEXUS_TEXT,0)==NEXUS_RX_MORE);
    assert(r.active);
    // Ping between fragments, with partial control payload reads.
    assert(nexus_receive_feed(&r,"p",1,NEXUS_PING|NEXUS_FINAL,1)==NEXUS_RX_MORE);
    assert(nexus_receive_feed(&r,"q",1,NEXUS_PING|NEXUS_FINAL,0)==NEXUS_RX_PING);
    assert(r.control_size==2 && !memcmp(r.control,"pq",2));
    assert(nexus_receive_feed(&r,"1}",2,NEXUS_FINAL,0)==NEXUS_RX_MESSAGE);
    assert(r.size==7 && !strcmp(r.data,"{\"a\":1}"));
    assert(nexus_receive_feed(&r,"",0,NEXUS_FINAL|NEXUS_PONG,0)==NEXUS_RX_PONG);
    assert(nexus_receive_feed(&r,"{}",2,NEXUS_FINAL|NEXUS_TEXT,0)==NEXUS_RX_MESSAGE);
    assert(r.size==2 && !strcmp(r.data,"{}"));
    assert(nexus_receive_feed(&r,"",0,NEXUS_FINAL|NEXUS_CLOSE,0)==NEXUS_RX_CLOSE);
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"",0,NEXUS_FINAL,0)==-EPROTO);
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"",0,NEXUS_BINARY|NEXUS_FINAL,0)==-EPROTO);
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"",0,NEXUS_PING,0)==-EPROTO);
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"",0,NEXUS_PING|NEXUS_FINAL,126)<0);
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"x",1,NEXUS_TEXT|NEXUS_FINAL,NEXUS_MESSAGE_MAX)==-EMSGSIZE);
    nexus_receive_reset(&r);
    assert(nexus_receive_feed(&r,"x",1,NEXUS_TEXT,0)==NEXUS_RX_MORE);
    assert(nexus_receive_feed(&r,"x",1,NEXUS_TEXT|NEXUS_FINAL,0)==-EPROTO);
    nexus_receive_reset(&r);
    char block[1024]; memset(block,'x',sizeof(block));
    for(unsigned i=0;i<8;++i)
        assert(nexus_receive_feed(&r,block,sizeof(block),i?0:NEXUS_TEXT,0)==NEXUS_RX_MORE);
    assert(nexus_receive_feed(&r,"x",1,NEXUS_FINAL,0)==-EMSGSIZE);
    // Installed Zephyr can omit metadata when a timed receive returns a
    // partial payload. Reuse the established frame metadata in that case.
    nexus_receive_reset(&r);
    uint64_t remaining = 0;
    assert(nexus_receive_read(&r, "{", 1, NEXUS_TEXT|NEXUS_FINAL, 6, &remaining)==NEXUS_RX_MORE);
    assert(nexus_receive_read(&r, "\"a", 2, UINT32_MAX, UINT64_MAX, &remaining)==NEXUS_RX_MORE);
    assert(remaining==4);
    assert(nexus_receive_read(&r, "\":1}", 4, NEXUS_TEXT|NEXUS_FINAL, 0, &remaining)==NEXUS_RX_MESSAGE);
    assert(!strcmp(r.data,"{\"a\":1}"));
    assert(nexus_receive_read(&r, "x", 1, UINT32_MAX, UINT64_MAX, &remaining)==-EPROTO);
    char host[NEXUS_HOST_MAX];
    assert(nexus_destination(false,"192.168.4.1","",host,sizeof(host))==-ENETDOWN && !host[0]);
    assert(nexus_destination(true,"192.168.4.1","",host,sizeof(host))==0 && !strcmp(host,"192.168.4.1"));
    assert(nexus_destination(true,"10.20.30.1","",host,sizeof(host))==0 && !strcmp(host,"10.20.30.1"));
    const char *bad[]={"", "0.0.0.0", "255.255.255.255", "224.0.0.1", "127.0.0.1", "192.168.4.999", "1.2.3.4x"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i)
        assert(nexus_destination(true,bad[i],"",host,sizeof(host))<0 && !host[0]);
    assert(nexus_destination(true,"","neia.example.local",host,sizeof(host))==0 && !strcmp(host,"neia.example.local"));
    assert(nexus_destination(true,"192.168.4.1","10.0.0.8",host,sizeof(host))==0 && !strcmp(host,"10.0.0.8"));
    assert(nexus_destination(true,"","ws://bad/path",host,sizeof(host))==-EINVAL);
    assert(nexus_destination(true,"10.0.0.1","",host,4)==-ENAMETOOLONG);
    puts("NEIA receive: partial reads, fragmentation/control interleaving, size/protocol bounds and gateway/override selection passed");
}
