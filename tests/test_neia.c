#include "nexus_event_parser.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static struct nexus_parser parser;
static struct hud_model m;
static char buffer[NEXUS_MESSAGE_MAX + 2];
static unsigned pulses;
static int parse(const char *s, struct hud_event *e)
{ return nexus_event_parse(&parser, s, strlen(s), e); }
static int fixture(const char *name, struct hud_event *e)
{
    char path[128]; snprintf(path,sizeof(path),"tests/fixtures/%s.json",name);
    FILE *f=fopen(path,"rb"); assert(f);
    size_t n=fread(buffer,1,sizeof(buffer)-1,f); assert(!ferror(f)); fclose(f);
    return nexus_event_parse(&parser,buffer,n,e);
}
static void apply(const struct hud_event *e)
{
    assert(hud_model_apply(&m,e,100)==0);
    if (m.pulse) ++pulses;
    assert(m.outgoing==HUD_CMD_NONE);
}
static void load(const char *name)
{ struct hud_event e; assert(fixture(name,&e)==1); apply(&e); }
static void test_error_messages(void)
{
    const char *formats[] = {
        "{\"type\":\"error\",\"payload\":\"%s\"}",
        "{\"type\":\"error\",\"payload\":{\"message\":\"%s\"}}"
    };
    char message[400], json[1024];
    strcpy(message, "Sensor stream failed: ");
    memset(message + strlen(message), 'x', sizeof(message) - strlen(message) - 1);
    message[sizeof(message) - 1] = 0;
    for (unsigned i = 0; i < 2; ++i) {
        struct hud_event e;
        snprintf(json, sizeof(json), formats[i], "Error message");
        assert(parse(json, &e) == 1 && e.type == HUD_EVENT_ERROR);
        assert(!strcmp(e.message, "Error message"));
        snprintf(json, sizeof(json), formats[i], message);
        assert(parse(json, &e) == 1 && e.type == HUD_EVENT_ERROR);
        assert(strlen(e.message) == sizeof(e.message) - 1);
        assert(e.message[sizeof(e.message) - 1] == 0);
        assert(!strncmp(e.message, message, sizeof(e.message) - 1));
        struct hud_event queued = e;
        memset(json, 'z', sizeof(json));
        struct hud_model local;
        hud_model_init(&local, false);
        assert(hud_model_apply(&local, &queued, 0) == 0);
        assert(local.state == HUD_STATE_ERROR && !local.pulse);
        assert(!strcmp(local.error, queued.message));
        struct hud_screen screen;
        hud_model_screen(&local, 0, false, &screen);
        assert(!strcmp(screen.lines[0], "ERROR"));
        assert(!strncmp(screen.lines[1], message, HUD_COLS));
        /* Escapes are decoded before truncation; no borrowed JSON bytes. */
        message[0] = 0;
        for (unsigned j = 0; j < 60; ++j) strcat(message, "\\u0041");
        snprintf(json, sizeof(json), formats[i], message);
        assert(parse(json, &e) == 1 && strlen(e.message) == sizeof(e.message) - 1);
        for (unsigned j = 0; j < sizeof(e.message) - 1; ++j) assert(e.message[j] == 'A');
        strcpy(message, "Sensor stream failed: ");
        memset(message + strlen(message), 'x', sizeof(message) - strlen(message) - 1);
        message[sizeof(message) - 1] = 0;
    }
    struct hud_event e;
    assert(parse("{\"type\":\"error\",\"payload\":null}", &e) == -EINVAL);
    assert(parse("{\"type\":\"error\",\"payload\":123}", &e) == -EINVAL);
    assert(parse("{\"type\":\"error\",\"payload\":[]}", &e) == -EINVAL);
}
int main(void)
{
    hud_model_init(&m,false);
    struct hud_event e;
    assert(parse("{\"type\":\"voice_status\",\"payload\":{}}",&e)==0);
    assert(parse("{\"type\":\"node_initialized\",\"payload\":{\"initialization_complete\":true}}",&e)==0);
    assert(parse("{\"type\":\"system_initialized\",\"payload\":{\"initialization_complete\":false}}",&e)==0);
    load("initialized_standalone"); assert(m.known_session && !strcmp(m.subject_id,"subject-01") && !m.count);
    load("sensor_left"); assert(m.count==1 && !m.results[0].has_value);
    load("sensor_right"); assert(m.count==2 && !m.results[1].has_value);
    load("compute_left"); assert(m.results[0].value==12.48f && !m.results[1].has_value && pulses==1);
    e=(struct hud_event){.type=HUD_EVENT_BUTTON}; apply(&e); assert(m.selected==1);
    load("compute_zero"); assert(m.selected==1 && m.results[0].value==0 && pulses==2);
    load("compute_right"); assert(m.results[1].value==11.76f && pulses==3);
    load("sensors_both"); assert(m.count==2 && m.results[0].has_value && m.results[1].has_value && m.selected==1 && pulses==3);
    assert(fixture("compute_left",&e)==1);
    // Mutating the receive buffer after parsing cannot change queued identities.
    struct hud_event queued=e; memset(buffer,'X',sizeof(buffer)); memset(&e,0,sizeof(e));
    apply(&queued); assert(!strcmp(m.results[0].address,"AA:BB:CC:DD:EE:01") && pulses==4);
    strcpy(queued.subject_id,"other-subject");
    assert(hud_model_apply(&m,&queued,100)==-ENOENT && !m.pulse);
    strcpy(queued.subject_id,"subject-01"); strcpy(queued.address,"unknown-sensor");
    assert(hud_model_apply(&m,&queued,100)==-ENOENT && !m.pulse && m.count==2);
    queued=(struct hud_event){.type=HUD_EVENT_SENSOR_CONNECTED,.subject_id="subject-01",.sensor_count=1,
        .sensors={{.address="third",.location="HEAD"}}};
    assert(hud_model_apply(&m,&queued,100)==-ENOSPC && m.count==2 && m.results[0].has_value);
    load("initialized_master"); assert(m.count==0 && m.selected==0 && pulses==4);
    load("sensors_both"); assert(m.count==2 && !m.results[0].has_value && !m.results[1].has_value);
    assert(parse("{\"type\":\"system_initialized\",\"payload\":{\"initialization_complete\":true,\"configuration_state\":\"installed\",\"subject_ids\":[\"a\",\"b\"]}}",&e)==-ENOSPC);
    const char *ignored[]={
        "{\"type\":\"compute_performance\",\"payload\":{\"result\":{}}}",
        "{\"type\":\"compute_result\",\"payload\":{\"algorithm_name\":\"other\"}}",
        "{\"type\":\"compute_result\",\"payload\":{\"algorithm_name\":\"standard_loading_intensity\",\"result\":{\"stage\":\"intermediate_time\"}}}",
        "{\"type\":\"compute_result\",\"payload\":{\"algorithm_name\":\"standard_loading_intensity\",\"result\":{\"stage\":\"consolidated_time\"}}}"};
    for (unsigned i=0;i<sizeof(ignored)/sizeof(ignored[0]);++i) assert(parse(ignored[i],&e)==0);
    const char *bad[]={"{", "[]", "{\"type\":\"x\",}", "{\"type\":\"x\"}false", "{\"x\":NaN}",
        "{\"x\":01}","{\"x\":1.}","{\"x\":1e}","{\"x\":\"bad\\q\"}","{\"x\":\"bad\n\"}"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) assert(parse(bad[i],&e)<0);
    // Mutate a source-derived result fixture to nonfinite/incorrect/oversized fields.
    const char *mags[]={"1e400", "NaN", "\"12.48\"", "null"};
    for(unsigned i=0;i<4;++i) {
        fixture("compute_left",&e); buffer[parser.length]=0;
        char *mag=strstr(buffer,"\"mag\": 12.48"); assert(mag);
        char mutated[NEXUS_MESSAGE_MAX+1];
        size_t prefix=(size_t)(mag-buffer)+7;
        snprintf(mutated,sizeof(mutated),"%.*s%s%s",(int)prefix,buffer,mags[i],mag+12);
        assert(parse(mutated,&e)<0);
    }
    memset(buffer,' ',sizeof(buffer));
    assert(nexus_event_parse(&parser,buffer,NEXUS_MESSAGE_MAX+1,&e)==-EMSGSIZE);
    assert(parse("{\"type\":\"sensor_connected\",\"payload\":{\"subjects\":[{\"subject_id\":\"s\",\"connected_sensors\":[{\"status\":\"CONNECTED\",\"address\":\"a\",\"location\":\"x\"},{\"status\":\"CONNECTED\",\"address\":\"b\",\"location\":\"y\"},{\"status\":\"CONNECTED\",\"address\":\"c\",\"location\":\"z\"}]}]}}",&e)==-ENOSPC);
    char long_id[200]; memset(long_id,'a',sizeof(long_id)); long_id[sizeof(long_id)-1]=0;
    snprintf(buffer,sizeof(buffer),"{\"type\":\"system_initialized\",\"payload\":{\"initialization_complete\":true,\"configuration_state\":\"installed\",\"subject_ids\":[\"%s\"]}}",long_id);
    assert(parse(buffer,&e)==-ENAMETOOLONG);
    // Deep/large metadata has a hard bound even on unrelated events.
    strcpy(buffer,"{\"type\":\"x\",\"meta\":");
    for(unsigned i=0;i<20;++i) strcat(buffer,"[");
    strcat(buffer,"0"); for(unsigned i=0;i<20;++i) strcat(buffer,"]"); strcat(buffer,"}");
    assert(parse(buffer,&e)==-E2BIG);
    assert(pulses==4);
    test_error_messages();
    puts("NEIA fixtures: init/reset, sensor merging, address routing, band/zero, ignores/rejections, pulse decisions and ownership passed");
}
