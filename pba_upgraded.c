#include "contiki.h"
// #include "net/rime/trickle.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "sys/clock.h"
#include "sys/timer.h"

#include <stdio.h>
#include <string.h>

#define MAX_WAITING_TIME 19
#define MIN_WAITING_TIME 10
#define WAITS(a) (a % (MAX_WAITING_TIME - MIN_WAITING_TIME + 1) + MIN_WAITING_TIME)
#define MAX_NEIGHBORS 20

static const float dummy_prob = 0.3;
static const float variance = 0.15;
static float dummy_step_dec;
static float dummy_step_inc;
static const float real_prob = 0.9;

/*---------------------------------------------------------------------------*/
PROCESS(example_trickle_process, "Trickle example");
PROCESS(sending_messages_process, "Sending messages process");
AUTOSTART_PROCESSES(&example_trickle_process, &sending_messages_process);
/*---------------------------------------------------------------------------*/
static int to_sink = 0;
static int my_seed;
static int seeds[MAX_NEIGHBORS];
static uint8_t hops[MAX_NEIGHBORS];
static uint8_t ids[MAX_NEIGHBORS];
static float cur_dummy_prob;
static int next_iteration[MAX_NEIGHBORS];
static int neigbors = 0;
static uint8_t start_sending_messages = 0;
static clock_time_t start;
static uint8_t dummy;
static process_event_t event_data_ready;
/*---------------------------------------------------------------------------*/
static void
trickle_recv(struct trickle_conn *c)
{
  char *temp = (char *)packetbuf_dataptr();
  if (strncmp(temp, "Initial", 7) == 0) {
    to_sink = packetbuf_attr(PACKETBUF_ATTR_HOPS);
    printf("Distance to sink = %d\n", packetbuf_attr(PACKETBUF_ATTR_HOPS));
  }
}
const static struct trickle_callbacks trickle_call = {trickle_recv};
static struct trickle_conn trickle;
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  char *temp = (char *)packetbuf_dataptr();
  if (strncmp(temp, "Seed", 4) == 0) {
    int his_seed;
    uint8_t his_sink;
    sscanf(temp, "Seed: %d %hhu", &his_seed, &his_sink);
    // if (his_sink <= to_sink) {
      seeds[neigbors] = his_seed;
      hops[neigbors] = his_sink;
      ids[neigbors] = from->u8[0];
      next_iteration[neigbors] = 0;
      neigbors++;
    // }
    printf("From %d.%d neighbor = %d %d, n_neighbors = %d\n", from->u8[0], from->u8[1], his_seed, his_sink, neigbors);
  }
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
static void send_message();
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  char *msg = (char *)packetbuf_dataptr();
  if (strncmp(msg, "Dummy:", 6) != 0) {
    if (linkaddr_node_addr.u8[0] == 1) {
      printf("Received a message '%s'\n", msg);
    } else {
      dummy = 0;
      // printf("Got a message '%s' for retransmission\n", msg);
    }
  }
}
static const struct unicast_callbacks unicast_call = {recv_uc};
static struct unicast_conn unicast;
/*---------------------------------------------------------------------------*/
static void update_next_iterations() {
  uint8_t i;
  clock_time_t cur_time = clock_time() - start;
  for (i = 0; i < neigbors; ++i) {
    random_init(seeds[i]);
    while (next_iteration[i] * CLOCK_SECOND < cur_time) {
      int waits = random_rand();
      seeds[i] = waits;
      next_iteration[i] += WAITS(waits);
    }
  }
}
/*---------------------------------------------------------------------------*/
static uint8_t find_neighbor() {
  update_next_iterations();
  uint8_t i;
  int cand = -1;
  for (i = 0; i < neigbors; ++i) {
    if (hops[i] < to_sink) {
      if (cand == -1 || next_iteration[i] < next_iteration[cand]) {
        cand = i;
      }
    }
  }
  return ids[cand];
}
static void
send_message() {
  // float cur_time = (clock_time() - start) * 1. / CLOCK_SECOND;
  random_init(my_seed);
  if (linkaddr_node_addr.u8[0] == 2 && dummy && random_rand() <= real_prob * RANDOM_RAND_MAX) {
    char message[] = "Hello, world!";
    packetbuf_copyfrom(message, 13);
    dummy = 0;
  }
  if (!dummy) {
        linkaddr_t addr;
        addr.u8[0] = find_neighbor();
        addr.u8[1] = 0;
        unicast_send(&unicast, &addr);
        printf("Real: %d -> %d\n", linkaddr_node_addr.u8[0], addr.u8[0]);
        // cur_dummy_prob -= dummy_step;
  } else {
        random_init(my_seed);
        float bounded_prob = cur_dummy_prob;
        if (bounded_prob < 0) {
          bounded_prob = 0;
        } else if (bounded_prob > 1.0) {
          bounded_prob = 1.0;
        }
        if (1. * random_rand() <= 1. * bounded_prob * RANDOM_RAND_MAX) {
          linkaddr_t addr;
          addr.u8[0] = find_neighbor();
          addr.u8[1] = 0;
          packetbuf_copyfrom("Dummy: data", 11);
          unicast_send(&unicast, &addr);
          printf("Dummy: %d -> %d\n", linkaddr_node_addr.u8[0], addr.u8[0]);
          cur_dummy_prob -= dummy_step_dec;
        } else {
          cur_dummy_prob += dummy_step_inc;
        }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_trickle_process, ev, data)
{
  static struct etimer et;
  PROCESS_EXITHANDLER(trickle_close(&trickle);)
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_EXITHANDLER(unicast_close(&unicast);)
  PROCESS_BEGIN();
  cur_dummy_prob = dummy_prob;
  dummy_step_dec = -variance * dummy_prob + variance;
  dummy_step_inc = variance - dummy_step_dec;
  trickle_open(&trickle, CLOCK_SECOND, 145, &trickle_call);

  etimer_set(&et, CLOCK_SECOND * 4);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  if (linkaddr_node_addr.u8[0] == 1) {
    packetbuf_copyfrom("Initial", 7);
    trickle_send(&trickle);
    packetbuf_clear();
  }

  broadcast_open(&broadcast, 129, &broadcast_call);

  etimer_set(&et, CLOCK_SECOND * 8);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));


  {
    random_init(123);
    uint8_t i;
    for (i = 0; i < linkaddr_node_addr.u8[0]; ++i, my_seed = random_rand());
    char buf[20];
    sprintf(buf, "Seed: %d %d\n", my_seed, to_sink);
    packetbuf_copyfrom(buf, strlen(buf));
    broadcast_send(&broadcast);
    packetbuf_clear();
  }
  etimer_set(&et, CLOCK_SECOND * 4);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));


  unicast_open(&unicast, 155, &unicast_call);
  etimer_set(&et, CLOCK_SECOND * 4);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  start_sending_messages = 1;
  event_data_ready = process_alloc_event();
  process_post(&sending_messages_process, event_data_ready, NULL);
  // printf("Set up finished, neighbors = %d\n", neigbors);
  SENSORS_ACTIVATE(button_sensor);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
			     data == &button_sensor);
    char message[] = "Hello, world!";
    packetbuf_copyfrom(message, 13);
    dummy = 0;
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sending_messages_process, ev, data) {
  static struct etimer et;
  PROCESS_BEGIN();
  PROCESS_WAIT_EVENT_UNTIL(event_data_ready);
  clock_init();
  start = clock_time();

  printf("Sending messages initiated\n");

  while(1) {
      dummy = 1;
      random_init(my_seed);
      int to_wait = random_rand();
      my_seed = to_wait;
      etimer_set(&et, CLOCK_SECOND * WAITS(to_wait));
      PROCESS_WAIT_EVENT_UNTIL((ev != event_data_ready && ev != sensors_event) && etimer_expired(&et));
      send_message();
  }

  PROCESS_END();
}
